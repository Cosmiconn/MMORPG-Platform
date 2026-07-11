#pragma once

#include "core/memory/allocator.h"
#include "core/memory/block_allocator.h"
#include <atomic>
#include <cstdint>
#include <new>

// Tracy integration
#if __has_include(<tracy/Tracy.hpp>)
#  include <tracy/Tracy.hpp>
#  define SEED_ZONE(name) ZoneScopedN(name)
#else
#  define SEED_ZONE(name) ((void)sizeof(name))
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace seed::memory {

// ---------------------------------------------------------------------------
// PoolAllocator
// ---------------------------------------------------------------------------
// Lock-free fixed-size object pool with per-thread caching.
//
// Design:
//   - Thread-local cache (hot): 64 items max, no atomics
//   - Global free list (cold): lock-free stack via compare_exchange_weak
//   - BlockAllocator backing: 4 KiB pages carved into objects
//
// Thread-safety: allocate()/deallocate() may be called from any thread.
// ---------------------------------------------------------------------------
template<typename T, size_t ObjectsPerPage = 512>
class PoolAllocator : public Allocator {
    static_assert(sizeof(T) >= sizeof(void*), "T must be at least pointer-sized");
    static_assert(alignof(T) >= alignof(void*), "T alignment must be at least pointer-sized");

    struct FreeNode {
        FreeNode* next;
    };

    struct alignas(64) Page {              // cache-line aligned page header
        static constexpr size_t DATA_SIZE = ObjectsPerPage * sizeof(T);
        alignas(alignof(T)) uint8_t data[DATA_SIZE];
        Page* nextPage;
    };

    struct alignas(64) ThreadCache {       // one per thread, cache-line isolated
        FreeNode* freeList = nullptr;
        uint32_t count = 0;
        static constexpr uint32_t MAX_CACHE = 64;
        static constexpr uint32_t BATCH_SIZE = 32;
    };

public:
    explicit PoolAllocator(BlockAllocator* blockAlloc)
        : m_blockAlloc(blockAlloc)
        , m_numAllocated(0)
    {
        SEED_ZONE("PoolAllocator::ctor");
    }

    ~PoolAllocator() override {
        SEED_ZONE("PoolAllocator::dtor");
        // Pages are owned by BlockAllocator; we just leak them for now
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // -----------------------------------------------------------------------
    // Fixed-type interface (preferred)
    // -----------------------------------------------------------------------
    template<typename... Args>
    T* construct(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    void destroy(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        deallocate(ptr, sizeof(T));
    }

    // -----------------------------------------------------------------------
    // Allocator interface (for polymorphic use)
    // -----------------------------------------------------------------------
    void* allocate(size_t size, size_t alignment) override {
        SEED_ZONE("PoolAllocator::allocate");
        (void)size;
        (void)alignment;

        ThreadCache& cache = getThreadCache();

        // 1. Thread-local cache (fast path)
        if (cache.count > 0) {
            FreeNode* node = cache.freeList;
            cache.freeList = node->next;
            --cache.count;
            m_numAllocated.fetch_add(1, std::memory_order_relaxed);
            return reinterpret_cast<void*>(node);
        }

        // 2. Try to refill from global list
        if (tryRefillCache(cache)) {
            return allocate(size, alignment); // retry
        }

        // 3. Allocate new page from BlockAllocator
        allocateNewPage();

        // 4. Retry (global list now has nodes)
        if (tryRefillCache(cache)) {
            return allocate(size, alignment);
        }

        return nullptr; // OOM
    }

    void deallocate(void* ptr, size_t /*size*/) override {
        SEED_ZONE("PoolAllocator::deallocate");
        if (!ptr) return;

        ThreadCache& cache = getThreadCache();
        FreeNode* node = reinterpret_cast<FreeNode*>(ptr);

        // 1. Push to thread-local cache
        if (cache.count < ThreadCache::MAX_CACHE) {
            node->next = cache.freeList;
            cache.freeList = node;
            ++cache.count;
            m_numAllocated.fetch_sub(1, std::memory_order_relaxed);
            return;
        }

        // 2. Cache full – flush half to global list
        flushCacheToGlobal(cache);

        // 3. Push to (now emptier) cache
        node->next = cache.freeList;
        cache.freeList = node;
        ++cache.count;
        m_numAllocated.fetch_sub(1, std::memory_order_relaxed);
    }

    size_t numAllocated() const {
        return m_numAllocated.load(std::memory_order_relaxed);
    }

private:
    BlockAllocator* m_blockAlloc;
    std::atomic<size_t> m_numAllocated;

    // Global free list (lock-free stack)
    alignas(64) std::atomic<FreeNode*> m_globalFreeList{nullptr};

    // Page list (for cleanup / tracking)
    std::atomic<Page*> m_pageList{nullptr};

    // Thread-local cache
    static thread_local ThreadCache s_threadCache;

    ThreadCache& getThreadCache() {
        return s_threadCache;
    }

    // -----------------------------------------------------------------------
    // Count nodes in a chain (for safety)
    // -----------------------------------------------------------------------
    static uint32_t countChain(FreeNode* head, uint32_t maxCount) {
        uint32_t count = 0;
        FreeNode* current = head;
        while (current && count < maxCount) {
            current = current->next;
            ++count;
        }
        return count;
    }

    // -----------------------------------------------------------------------
    // Lock-free global list operations
    // -----------------------------------------------------------------------
    bool tryRefillCache(ThreadCache& cache) {
        SEED_ZONE("PoolAllocator::tryRefillCache");

        FreeNode* head = m_globalFreeList.load(std::memory_order_seq_cst);
        if (!head) return false;

        // Walk up to BATCH_SIZE nodes, with null checks
        FreeNode* tail = head;
        uint32_t count = 1;
        while (tail->next && count < ThreadCache::BATCH_SIZE) {
            tail = tail->next;
            ++count;
        }

        FreeNode* newHead = tail->next;
        if (m_globalFreeList.compare_exchange_weak(
                head, newHead,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            // Success – prepend to cache
            tail->next = cache.freeList;
            cache.freeList = head;
            cache.count += count;
            return true;
        }

        return false; // CAS failed, retry on next allocate()
    }

    void flushCacheToGlobal(ThreadCache& cache) {
        SEED_ZONE("PoolAllocator::flushCacheToGlobal");

        if (!cache.freeList || cache.count == 0) return;

        // Safety: verify actual chain length matches count
        uint32_t actualCount = countChain(cache.freeList, cache.count + 1);
        if (actualCount == 0) {
            cache.freeList = nullptr;
            cache.count = 0;
            return;
        }

        // Use the smaller of actual count and recorded count
        uint32_t effectiveCount = (actualCount < cache.count) ? actualCount : cache.count;

        // Move half to global (or all if count is small)
        uint32_t toMove = effectiveCount / 2;
        if (toMove == 0) toMove = 1;
        if (toMove > effectiveCount) toMove = effectiveCount;

        // Split cache list
        FreeNode* moveHead = cache.freeList;
        FreeNode* moveTail = moveHead;
        for (uint32_t i = 1; i < toMove; ++i) {
            if (!moveTail->next) break; // Safety: don't walk past end
            moveTail = moveTail->next;
        }

        FreeNode* newCacheHead = moveTail->next;
        moveTail->next = nullptr;

        // Atomically prepend to global list
        FreeNode* oldGlobal = m_globalFreeList.load(std::memory_order_seq_cst);
        do {
            moveTail->next = oldGlobal;
        } while (!m_globalFreeList.compare_exchange_weak(
                    oldGlobal, moveHead,
                    std::memory_order_release,
                    std::memory_order_relaxed));

        cache.freeList = newCacheHead;
        cache.count = effectiveCount - toMove;
    }

    // -----------------------------------------------------------------------
    // Page allocation
    // -----------------------------------------------------------------------
    void allocateNewPage() {
        SEED_ZONE("PoolAllocator::allocateNewPage");

        void* mem = m_blockAlloc->allocate(sizeof(Page), alignof(Page));
        if (!mem) return;

        Page* page = new (mem) Page();
        page->nextPage = nullptr;

        // Carve page into free nodes, push to global list
        FreeNode* first = nullptr;
        FreeNode* last = nullptr;

        for (size_t i = 0; i < ObjectsPerPage; ++i) {
            void* objAddr = &page->data[i * sizeof(T)];
            FreeNode* node = reinterpret_cast<FreeNode*>(objAddr);
            node->next = nullptr;

            if (!first) {
                first = node;
                last = node;
            } else {
                last->next = node;
                last = node;
            }
        }

        // Atomically prepend chain to global list
        FreeNode* oldGlobal = m_globalFreeList.load(std::memory_order_seq_cst);
        do {
            last->next = oldGlobal;
        } while (!m_globalFreeList.compare_exchange_weak(
                    oldGlobal, first,
                    std::memory_order_release,
                    std::memory_order_relaxed));

        // Link page for cleanup
        Page* oldPageList = m_pageList.load(std::memory_order_relaxed);
        do {
            page->nextPage = oldPageList;
        } while (!m_pageList.compare_exchange_weak(
                    oldPageList, page,
                    std::memory_order_release,
                    std::memory_order_relaxed));
    }
};

// Thread-local storage definition
template<typename T, size_t N>
thread_local typename PoolAllocator<T, N>::ThreadCache
    PoolAllocator<T, N>::s_threadCache;

} // namespace seed::memory

#ifdef _MSC_VER
#pragma warning(pop)
#endif
