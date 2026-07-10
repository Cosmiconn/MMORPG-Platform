#include "block_allocator.h"

#include <atomic>
#include <cstring>

// ---------------------------------------------------------------------------
// Platform-specific OS allocation
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace seed::memory {

// ---------------------------------------------------------------------------
// OS allocation helpers
// ---------------------------------------------------------------------------
void* BlockAllocator::os_alloc(size_t size, size_t alignment) {
    SEED_ZONE("BlockAllocator::os_alloc");

#ifdef _WIN32
    (void)alignment; // VirtualAlloc aligns to allocation granularity automatically
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ptr) {
        // Fallback: let std::terminate handle it or return nullptr
        return nullptr;
    }
    return ptr;
#else
    // mmap with requested alignment.  We over-allocate and align manually,
    // or rely on mmap returning page-aligned memory (usually 4KiB / 64KiB).
    // For 64 MiB blocks, page alignment is sufficient.
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return nullptr;
    }
    return ptr;
#endif
}

void BlockAllocator::os_free(void* ptr, size_t size) {
    SEED_ZONE("BlockAllocator::os_free");

#ifdef _WIN32
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

// ---------------------------------------------------------------------------
// BlockAllocator implementation
// ---------------------------------------------------------------------------
BlockAllocator::BlockAllocator(size_t blockSize, size_t alignment)
    : m_blockSize(blockSize), m_alignment(alignment) {}

BlockAllocator::~BlockAllocator() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& block : m_blocks) {
        if (block.base) {
            os_free(block.base, block.size);
            SEED_FREE(block.base);
        }
    }
    m_blocks.clear();
}

void* BlockAllocator::allocate(size_t size, size_t alignment) {
    SEED_ZONE("BlockAllocator::allocate");

    // We only allocate whole blocks; size must be <= blockSize
    if (size > m_blockSize) {
        return nullptr; // Too large – caller should handle or use fallback
    }

    // Use blockSize if caller asks for less (we always hand out whole blocks)
    size_t allocSize = m_blockSize;
    size_t align     = (alignment > m_alignment) ? alignment : m_alignment;

    void* ptr = os_alloc(allocSize, align);
    if (!ptr) {
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_blocks.push_back({ptr, allocSize, true});
    }

    m_totalAllocated.fetch_add(allocSize, std::memory_order_relaxed);
    m_totalUsed.fetch_add(allocSize, std::memory_order_relaxed);

    SEED_ALLOC(ptr, allocSize);
    return ptr;
}

void BlockAllocator::deallocate(void* ptr, size_t size) {
    SEED_ZONE("BlockAllocator::deallocate");

    if (!ptr) return;

    size_t freedSize = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& block : m_blocks) {
            if (block.base == ptr) {
                freedSize = block.size;
                block.inUse = false;
                break;
            }
        }
    }

    if (freedSize == 0) {
        // Unknown pointer – ignore or assert in debug
        return;
    }

    os_free(ptr, freedSize);
    SEED_FREE(ptr);

    m_totalUsed.fetch_sub(freedSize, std::memory_order_relaxed);
    // Note: totalAllocated stays the same (cumulative counter)

    if (size > 0 && size != freedSize) {
        // Size mismatch – debug builds could assert here
    }
}

size_t BlockAllocator::getAllocationSize(const void* ptr) const {
    if (!ptr) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& block : m_blocks) {
        if (block.base == ptr) {
            return block.size;
        }
    }
    return 0;
}

size_t BlockAllocator::numBlocks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.size();
}

std::vector<BlockAllocator::BlockInfo> BlockAllocator::blockList() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BlockInfo> list;
    list.reserve(m_blocks.size());
    for (const auto& b : m_blocks) {
        list.push_back({b.base, b.size, b.inUse});
    }
    return list;
}

} // namespace seed::memory
