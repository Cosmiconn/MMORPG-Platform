#pragma once

#include "core/profiling/seed_assert.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/entity.h"
#include "core/memory/allocator.h"
#include "core/profiling/tracy_seed.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace seed::ecs {

// ---------------------------------------------------------------------------
// IComponentArray
// ---------------------------------------------------------------------------
// Polymorphic interface for archetype column storage.
// ---------------------------------------------------------------------------
class IComponentArray {
public:
    virtual ~IComponentArray() = default;

    virtual void* get(size_t index) = 0;
    virtual const void* get(size_t index) const = 0;
    virtual void remove(size_t index) = 0; // swap-with-last
    virtual void move(size_t dstIndex, size_t srcIndex) = 0;
    virtual void copy(size_t dstIndex, const void* srcData) = 0;
    virtual void defaultConstruct(size_t index) = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual void reserve(size_t n) = 0;
    virtual ComponentType componentType() const = 0;
    virtual const ComponentMeta& meta() const = 0;
    virtual void clear() = 0;
};

// ---------------------------------------------------------------------------
// ComponentArray<T>
// ---------------------------------------------------------------------------
// Dense, tightly packed array of T.
// Uses a simple page-based growth strategy (4 KiB pages via Allocator).
// Swap-with-last for O(1) removal.
//
// NOT thread-safe – intended for single-threaded archetype use.
// ---------------------------------------------------------------------------
template<typename T>
class ComponentArray : public IComponentArray {
public:
    explicit ComponentArray(seed::memory::Allocator* alloc)
        : m_allocator(alloc)
        , m_data(nullptr)
        , m_size(0)
        , m_capacity(0)
    {
        SEED_ZONE("ComponentArray::ctor");
        SEED_ASSERT(alloc != nullptr, "ComponentArray requires a valid allocator");
    }

    ~ComponentArray() override {
        SEED_ZONE("ComponentArray::dtor");
        clear();
        if (m_data) {
            m_allocator->deallocate(m_data, m_capacity * sizeof(T));
        }
    }

    ComponentArray(const ComponentArray&) = delete;
    ComponentArray& operator=(const ComponentArray&) = delete;

    void* get(size_t index) override {
        return const_cast<void*>(static_cast<const ComponentArray*>(this)->get(index));
    }

    const void* get(size_t index) const override {
        SEED_ASSERT(index < m_size, "ComponentArray index out of bounds");
        return &m_data[index];
    }

    void remove(size_t index) override {
        SEED_ASSERT(index < m_size, "ComponentArray remove out of bounds");
        if (index != m_size - 1) {
            T* dst = &m_data[index];
            T* src = &m_data[m_size - 1];
            meta().move(dst, src);
        }
        meta().destruct(&m_data[m_size - 1]);
        --m_size;
    }

    void move(size_t dstIndex, size_t srcIndex) override {
        SEED_ASSERT(dstIndex < m_size && srcIndex < m_size, "move out of bounds");
        meta().move(&m_data[dstIndex], &m_data[srcIndex]);
    }

    void copy(size_t dstIndex, const void* srcData) override {
        SEED_ASSERT(dstIndex < m_size, "copy out of bounds");
        meta().copy(&m_data[dstIndex], srcData);
    }

    void defaultConstruct(size_t index) override {
        SEED_ASSERT(index <= m_size, "defaultConstruct index out of bounds");
        if (m_size >= m_capacity) {
            reserve(m_capacity == 0 ? 64 : m_capacity * 2);
        }
        meta().construct(&m_data[index]);
        if (index == m_size) {
            ++m_size;
        }
    }

    size_t size() const override { return m_size; }
    size_t capacity() const override { return m_capacity; }

    void reserve(size_t n) override {
        if (n <= m_capacity) return;
        size_t newCapacity = m_capacity == 0 ? 64 : m_capacity;
        while (newCapacity < n) {
            newCapacity *= 2;
        }

        T* newData = static_cast<T*>(m_allocator->allocate(newCapacity * sizeof(T), alignof(T)));
        SEED_ASSERT(newData != nullptr, "ComponentArray allocation failed");

        for (size_t i = 0; i < m_size; ++i) {
            meta().construct(&newData[i]);
            meta().move(&newData[i], &m_data[i]);
            meta().destruct(&m_data[i]);
        }

        if (m_data) {
            m_allocator->deallocate(m_data, m_capacity * sizeof(T));
        }

        m_data = newData;
        m_capacity = newCapacity;
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
            meta().destruct(&m_data[i]);
        }
        m_size = 0;
    }

    template<typename... Args>
    T* emplaceBack(Args&&... args) {
        if (m_size >= m_capacity) {
            reserve(m_capacity == 0 ? 64 : m_capacity * 2);
        }
        T* slot = &m_data[m_size];
        new (slot) T(std::forward<Args>(args)...);
        ++m_size;
        return slot;
    }

    T* pushBack(const T& value) {
        return emplaceBack(value);
    }

    T* pushBack(T&& value) {
        return emplaceBack(std::move(value));
    }

    T& operator[](size_t index) {
        return *static_cast<T*>(get(index));
    }

    const T& operator[](size_t index) const {
        return *static_cast<const T*>(get(index));
    }

    T* data() {
        return m_data;
    }

    const T* data() const {
        return m_data;
    }

private:
    seed::memory::Allocator* m_allocator;
    T* m_data;
    size_t m_size;
    size_t m_capacity;
};

} // namespace seed::ecs
