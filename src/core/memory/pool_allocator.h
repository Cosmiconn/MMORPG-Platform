#pragma once

#include "core/memory/allocator.h"
#include "core/memory/block_allocator.h"
#include "core/profiling/tracy_seed.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace seed::memory {

// ---------------------------------------------------------------------------
// PoolAllocator
// ---------------------------------------------------------------------------
// Fixed-size object pool with per-thread caching.
//
// Design:
//   - Thread-local cache (hot): 64 items max, no atomics
//   - Global free list (cold): mutex-protected for safety in Phase 0.
//     (Can be upgraded to lock-free later once the algorithm is proven.)
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

    struct alignas(64) ThreadCache {       // one per (thread, instance), cache-line isolated
        FreeNode* freeList = nullptr;
        uint32_t count = 0;
        static constexpr uint32_t MAX_CACHE = 64;
        static constexpr uint32_t BATCH_SIZE = 32;
    };

    // ---------------------------------------------------------------------
    // BUGFIX (gefunden ueber Job-System-Stresstests, verifiziert per ASan-
    // Minimal-Repro): s_threadCache war zuvor ein einzelnes
    // `static thread_local ThreadCache` - geteilt von ALLEN Instanzen von
    // PoolAllocator<T, ObjectsPerPage> auf einem Thread, nicht nur von EINER
    // Instanz! Erzeugt man nacheinander zwei PoolAllocator<Task>-Objekte
    // (z.B. zwei JobSystem-Instanzen hintereinander), teilen sie sich
    // denselben thread-lokalen Cache. Wird die erste Instanz zerstoert
    // (BlockAllocator unmapped seine Seiten), bleiben im geteilten Cache
    // Zeiger auf jetzt unmapped Speicher zurueck - die zweite Instanz liefert
    // beim naechsten allocate() diese toten Zeiger aus -> SIGSEGV.
    //
    // Fix: jede Instanz bekommt eine eindeutige, NIE wiederverwendete ID
    // (monotoner Zaehler statt this-Zeiger - eine neu erzeugte Instanz kann
    // durchaus dieselbe Adresse wie eine zuvor zerstoerte bekommen, eine ID
    // dagegen nie). Der thread-lokale Zustand ist eine kleine Registry
    // (ID -> ThreadCache), mit einem 1-Eintrag-Fastpath fuer den haeufigen
    // Fall, dass ein Thread wiederholt dieselbe Instanz benutzt (z.B. ein
    // JobSystem-Worker-Thread, der immer denselben taskPool bedient) - dafuer
    // kostet es nur einen Zeigervergleich, keine Suche.
    // ---------------------------------------------------------------------
    struct ThreadCacheRegistry {
        uint64_t lastId = 0;
        bool lastValid = false;
        ThreadCache* lastCache = nullptr;
        std::vector<std::pair<uint64_t, std::unique_ptr<ThreadCache>>> entries;

        ThreadCache& get(uint64_t id) {
            if (lastValid && id == lastId) {
                return *lastCache;
            }
            for (auto& [key, cachePtr] : entries) {
                if (key == id) {
                    lastId = id;
                    lastValid = true;
                    lastCache = cachePtr.get();
                    return *lastCache;
                }
            }
            entries.emplace_back(id, std::make_unique<ThreadCache>());
            lastId = id;
            lastValid = true;
            lastCache = entries.back().second.get();
            return *lastCache;
        }
    };

public:
    explicit PoolAllocator(BlockAllocator* blockAlloc)
        : m_blockAlloc(blockAlloc)
        , m_numAllocated(0)
        , m_instanceId(s_nextInstanceId.fetch_add(1, std::memory_order_relaxed))
    {
        SEED_ZONE("PoolAllocator::ctor");
    }

    ~PoolAllocator() override {
        SEED_ZONE("PoolAllocator::dtor");
        // Pages are owned by BlockAllocator; we just leak them for now.
        // In Phase 1 we can add a proper cleanup path if needed.
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

        // 1. Thread-local cache (fast path, lock-free)
        if (cache.count > 0) {
            FreeNode* node = cache.freeList;
            cache.freeList = node->next;
            --cache.count;
            m_numAllocated.fetch_add(1, std::memory_order_relaxed);
            SEED_ALLOC(node, sizeof(T));
            return reinterpret_cast<void*>(node);
        }

        // 2. Refill from global list under mutex
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
        SEED_FREE(ptr);

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
    const uint64_t m_instanceId;

    static inline std::atomic<uint64_t> s_nextInstanceId{1};

    // Global free list – mutex-protected for correctness in Phase 0.
    // All operations on m_globalFreeList must hold m_globalMutex.
    alignas(64) std::mutex m_globalMutex;
    FreeNode* m_globalFreeList = nullptr;

    // Page list (for cleanup / tracking)
    alignas(64) std::mutex m_pageMutex;
    Page* m_pageList = nullptr;

    // Thread-local Registry, isoliert nach Instanz-ID (siehe BUGFIX-Kommentar
    // bei ThreadCacheRegistry oben).
    static thread_local ThreadCacheRegistry s_registry;

    ThreadCache& getThreadCache() {
        return s_registry.get(m_instanceId);
    }

    // -----------------------------------------------------------------------
    // Global list operations (mutex-protected)
    // -----------------------------------------------------------------------
    bool tryRefillCache(ThreadCache& cache) {
        SEED_ZONE("PoolAllocator::tryRefillCache");

        std::lock_guard<std::mutex> lock(m_globalMutex);
        if (!m_globalFreeList) return false;

        FreeNode* head = m_globalFreeList;
        FreeNode* tail = head;
        uint32_t count = 1;

        while (tail->next && count < ThreadCache::BATCH_SIZE) {
            tail = tail->next;
            ++count;
        }

        m_globalFreeList = tail->next;
        tail->next = cache.freeList;
        cache.freeList = head;
        cache.count += count;
        return true;
    }

    void flushCacheToGlobal(ThreadCache& cache) {
        SEED_ZONE("PoolAllocator::flushCacheToGlobal");

        if (!cache.freeList || cache.count == 0) return;

        // Move half to global (or all if count is small)
        uint32_t toMove = cache.count / 2;
        if (toMove == 0) toMove = 1;

        FreeNode* moveHead = cache.freeList;
        FreeNode* moveTail = moveHead;
        for (uint32_t i = 1; i < toMove; ++i) {
            if (!moveTail->next) break;
            moveTail = moveTail->next;
        }

        FreeNode* newCacheHead = moveTail->next;
        moveTail->next = nullptr;

        {
            std::lock_guard<std::mutex> lock(m_globalMutex);
            moveTail->next = m_globalFreeList;
            m_globalFreeList = moveHead;
        }

        cache.freeList = newCacheHead;
        cache.count -= toMove;
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

        // Carve page into free nodes
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

        // Prepend chain to global list under mutex
        {
            std::lock_guard<std::mutex> lock(m_globalMutex);
            last->next = m_globalFreeList;
            m_globalFreeList = first;
        }

        // Link page for cleanup
        {
            std::lock_guard<std::mutex> lock(m_pageMutex);
            page->nextPage = m_pageList;
            m_pageList = page;
        }
    }
};

// Thread-local storage definition
template<typename T, size_t N>
thread_local typename PoolAllocator<T, N>::ThreadCacheRegistry
    PoolAllocator<T, N>::s_registry;

} // namespace seed::memory

#ifdef _MSC_VER
#pragma warning(pop)
#endif
