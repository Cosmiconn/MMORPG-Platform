#pragma once

#include "core/memory/allocator.h"
#include <atomic>
#include <vector>

namespace seed::memory {

template<typename T, size_t BlockSize = 4096>
class PoolAllocator {
public:
    explicit PoolAllocator(Allocator* backing) : m_backing(backing), m_freeList(nullptr) {}

    T* allocate() {
        if (m_freeList) {
            Node* node = m_freeList;
            m_freeList = node->next;
            return reinterpret_cast<T*>(node);
        }

        if (m_currentBlock.empty() || m_blockOffset >= BlockSize) {
            void* block = m_backing->allocate(BlockSize * sizeof(T), alignof(T));
            m_currentBlock = static_cast<uint8_t*>(block);
            m_blockOffset = 0;
        }

        T* ptr = reinterpret_cast<T*>(m_currentBlock + m_blockOffset * sizeof(T));
        m_blockOffset++;
        new (ptr) T();
        return ptr;
    }

    void deallocate(T* ptr) {
        ptr->~T();
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = m_freeList;
        m_freeList = node;
    }

private:
    struct Node {
        Node* next;
    };

    Allocator* m_backing;
    uint8_t* m_currentBlock = nullptr;
    size_t m_blockOffset = 0;
    Node* m_freeList = nullptr;
    std::vector<uint8_t*> m_blocks;
};

} // namespace seed::memory
