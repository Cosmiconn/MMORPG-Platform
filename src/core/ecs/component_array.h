#pragma once

#include "core/profiling/seed_assert.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/entity.h"
#include "core/memory/allocator.h"
#include "core/profiling/tracy_seed.h"
#include "core/diagnostics/event_timeline.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace seed::ecs {

class IComponentArray {
public:
    virtual ~IComponentArray() = default;

    virtual void* get(size_t index) = 0;
    virtual const void* get(size_t index) const = 0;
    virtual void remove(size_t index) = 0;
    virtual void move(size_t dstIndex, size_t srcIndex) = 0;
    virtual void moveFrom(size_t dstIndex, IComponentArray* src, size_t srcIndex) = 0;
    virtual void copy(size_t dstIndex, const void* srcData) = 0;
    virtual void defaultConstruct(size_t index) = 0;
    virtual void destructAt(size_t index) = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual void reserve(size_t n) = 0;
    virtual ComponentType componentType() const = 0;
    virtual const ComponentMeta& meta() const = 0;
    virtual void clear() = 0;
};

template<typename T>
class ComponentArray : public IComponentArray {
    static constexpr size_t ELEMENTS_PER_CHUNK = 1024;

public:
    explicit ComponentArray(seed::memory::Allocator* alloc)
        : m_allocator(alloc), m_size(0), m_capacity(0)
    {
        SEED_ZONE("ComponentArray::ctor");
        SEED_ASSERT(alloc != nullptr, "ComponentArray requires a valid allocator");
    }

    ~ComponentArray() override {
        SEED_ZONE("ComponentArray::dtor");
        clear();
        for (auto* chunk : m_chunks) {
            m_allocator->deallocate(chunk, ELEMENTS_PER_CHUNK * sizeof(T));
            SEED_DIAG_EVENT_MEM(seed::diagnostics::EventType::MemoryDeallocate,
                INVALID_ENTITY, 0, ComponentTraits<T>::id, 0,
                "chunk deallocate", __FILE__, __LINE__, ELEMENTS_PER_CHUNK * sizeof(T));
        }
    }

    ComponentArray(const ComponentArray&) = delete;
    ComponentArray& operator=(const ComponentArray&) = delete;

    void* get(size_t index) override {
        return const_cast<void*>(static_cast<const ComponentArray*>(this)->get(index));
    }

    const void* get(size_t index) const override {
        SEED_ASSERT(index < m_size, "ComponentArray index out of bounds");
        const size_t chunkIdx = index / ELEMENTS_PER_CHUNK;
        const size_t elemIdx = index % ELEMENTS_PER_CHUNK;
        return &m_chunks[chunkIdx][elemIdx];
    }

    void remove(size_t index) override {
        SEED_ASSERT(index < m_size, "ComponentArray remove out of bounds");
        SEED_DIAG_EVENT(seed::diagnostics::EventType::ComponentDestruct, 
            INVALID_ENTITY, 0, ComponentTraits<T>::id, static_cast<uint32_t>(index),
            "remove: destruct + swap", __FILE__, __LINE__);

        if (index != m_size - 1) {
            T* dst = static_cast<T*>(get(index));
            T* src = static_cast<T*>(get(m_size - 1));

            meta().destruct(dst);
            meta().move(dst, src);
            // src is now destroyed by move() - DO NOT destruct again
        } else {
            meta().destruct(static_cast<T*>(get(index)));
        }
        --m_size;
    }

    void destructAt(size_t index) override {
        SEED_ASSERT(index < m_size, "destructAt index out of bounds");
        SEED_DIAG_EVENT(seed::diagnostics::EventType::ComponentDestruct,
            INVALID_ENTITY, 0, ComponentTraits<T>::id, static_cast<uint32_t>(index),
            "destructAt", __FILE__, __LINE__);
        meta().destruct(static_cast<T*>(get(index)));
    }

    void move(size_t dstIndex, size_t srcIndex) override {
        SEED_ASSERT(dstIndex < m_size && srcIndex < m_size, "move out of bounds");
        T* dst = static_cast<T*>(get(dstIndex));
        T* src = static_cast<T*>(get(srcIndex));
        meta().destruct(dst);
        meta().move(dst, src);
        // src already destroyed by meta().move()
    }

    // CRITICAL FIX for move-only types:
    // After moveFrom(), the src slot is destroyed by meta().move().
    // We place a default-constructed placeholder so that subsequent
    // remove() can safely destroy it.
    void moveFrom(size_t dstIndex, IComponentArray* src, size_t srcIndex) override {
        SEED_ASSERT(dstIndex < m_size, "moveFrom dst out of bounds");
        SEED_ASSERT(src != nullptr, "moveFrom src is null");
        SEED_ASSERT(srcIndex < src->size(), "moveFrom srcIndex out of bounds");

        SEED_DIAG_EVENT(seed::diagnostics::EventType::ComponentMove,
            INVALID_ENTITY, 0, ComponentTraits<T>::id, static_cast<uint32_t>(dstIndex),
            "moveFrom start", __FILE__, __LINE__);

        T* dst = static_cast<T*>(get(dstIndex));
        T* srcPtr = static_cast<T*>(src->get(srcIndex));

        // Destroy the default-constructed object at dst
        meta().destruct(dst);

        // Move-construct from src to dst (this also destroys src)
        meta().move(dst, srcPtr);

        // PLACEHOLDER FIX: Reconstruct a default T at src so that
        // the source array's remove() can safely destroy it later.
        new (srcPtr) T();

        SEED_DIAG_EVENT(seed::diagnostics::EventType::ComponentMove,
            INVALID_ENTITY, 0, ComponentTraits<T>::id, static_cast<uint32_t>(dstIndex),
            "moveFrom complete (placeholder placed)", __FILE__, __LINE__);
    }

    void copy(size_t dstIndex, const void* srcData) override {
        SEED_ASSERT(dstIndex < m_size, "copy out of bounds");
        T* dst = static_cast<T*>(get(dstIndex));
        meta().copy(dst, srcData);
    }

    void defaultConstruct(size_t index) override {
        SEED_ASSERT(index <= m_size, "defaultConstruct index out of bounds");
        if (index >= m_capacity) {
            reserve(index + 1);
        }
        const size_t chunkIdx = index / ELEMENTS_PER_CHUNK;
        const size_t elemIdx = index % ELEMENTS_PER_CHUNK;
        T* slot = &m_chunks[chunkIdx][elemIdx];

        if (index < m_size) {
            meta().destruct(slot);
        }
        meta().construct(slot);
        if (index == m_size) {
            ++m_size;
        }

        SEED_DIAG_EVENT(seed::diagnostics::EventType::ComponentDefaultConstruct,
            INVALID_ENTITY, 0, ComponentTraits<T>::id, static_cast<uint32_t>(index),
            "defaultConstruct", __FILE__, __LINE__);
    }

    size_t size() const override { return m_size; }
    size_t capacity() const override { return m_capacity; }

    void reserve(size_t n) override {
        if (n <= m_capacity) return;
        const size_t neededChunks = (n + ELEMENTS_PER_CHUNK - 1) / ELEMENTS_PER_CHUNK;
        const size_t currentChunks = m_chunks.size();
        for (size_t i = currentChunks; i < neededChunks; ++i) {
            T* chunk = static_cast<T*>(m_allocator->allocate(
                ELEMENTS_PER_CHUNK * sizeof(T), alignof(T)));
            SEED_ASSERT(chunk != nullptr, "ComponentArray chunk allocation failed");
            m_chunks.push_back(chunk);

            SEED_DIAG_EVENT_MEM(seed::diagnostics::EventType::MemoryAllocate,
                INVALID_ENTITY, 0, ComponentTraits<T>::id, static_cast<uint32_t>(i),
                "chunk allocate", __FILE__, __LINE__, ELEMENTS_PER_CHUNK * sizeof(T));
        }
        m_capacity = neededChunks * ELEMENTS_PER_CHUNK;
    }

    ComponentType componentType() const override {
        return ComponentTraits<T>::id;
    }

    const ComponentMeta& meta() const override {
        static const ComponentMeta s_meta = getComponentMeta<T>();
        return s_meta;
    }

    void clear() override {
        for (size_t i = 0; i < m_size; ++i) {
            T* elem = static_cast<T*>(get(i));
            meta().destruct(elem);
        }
        m_size = 0;
    }

    template<typename... Args>
    T* emplaceBack(Args&&... args) {
        if (m_size >= m_capacity) {
            reserve(m_capacity + ELEMENTS_PER_CHUNK);
        }
        T* slot = static_cast<T*>(get(m_size));
        new (slot) T(std::forward<Args>(args)...);
        ++m_size;
        return slot;
    }

    T* pushBack(const T& value) { return emplaceBack(value); }
    T* pushBack(T&& value) { return emplaceBack(std::move(value)); }

    T& operator[](size_t index) {
        return *static_cast<T*>(get(index));
    }

    const T& operator[](size_t index) const {
        return *static_cast<const T*>(get(index));
    }

    T* data() { return m_chunks.empty() ? nullptr : m_chunks[0]; }
    const T* data() const { return m_chunks.empty() ? nullptr : m_chunks[0]; }

private:
    seed::memory::Allocator* m_allocator;
    std::vector<T*> m_chunks;
    size_t m_size;
    size_t m_capacity;
};

} // namespace seed::ecs
