#pragma once

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
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t ELEMENTS_PER_PAGE = PAGE_SIZE / sizeof(T);
    static_assert(ELEMENTS_PER_PAGE > 0, "T too large for page size");

public:
    explicit ComponentArray(seed::memory::Allocator* alloc)
        : m_allocator(alloc)
        , m_size(0)
        , m_capacity(0)
    {
        SEED_ZONE("ComponentArray::ctor");
    }

    ~ComponentArray() override {
        SEED_ZONE("ComponentArray::dtor");
        clear();
        for (auto* page : m_pages) {
            m_allocator->deallocate(page, PAGE_SIZE);
        }
    }

    ComponentArray(const ComponentArray&) = delete;
    ComponentArray& operator=(const ComponentArray&) = delete;

    // -----------------------------------------------------------------------
    // IComponentArray interface
    // -----------------------------------------------------------------------
    void* get(size_t index) override {
        return const_cast<void*>(static_cast<const ComponentArray*>(this)->get(index));
    }

    const void* get(size_t index) const override {
        SEED_ASSERT(index < m_size, "ComponentArray index out of bounds");
        const size_t pageIdx = index / ELEMENTS_PER_PAGE;
        const size_t elemIdx = index % ELEMENTS_PER_PAGE;
        return &m_pages[pageIdx][elemIdx];
    }

    void remove(size_t index) override {
        SEED_ASSERT(index < m_size, "ComponentArray remove out of bounds");
        if (index != m_size - 1) {
            // Swap-with-last
            T* dst = static_cast<T*>(get(index));
            T* src = static_cast<T*>(get(m_size - 1));
            meta().move(dst, src);
        }
        // Destruct last element
        T* last = static_cast<T*>(get(m_size - 1));
        meta().destruct(last);
        --m_size;
    }

    void move(size_t dstIndex, size_t srcIndex) override {
        SEED_ASSERT(dstIndex < m_size && srcIndex < m_size, "move out of bounds");
        T* dst = static_cast<T*>(get(dstIndex));
        T* src = static_cast<T*>(get(srcIndex));
        meta().move(dst, src);
    }

    void copy(size_t dstIndex, const void* srcData) override {
        SEED_ASSERT(dstIndex < m_size, "copy out of bounds");
        T* dst = static_cast<T*>(get(dstIndex));
        meta().copy(dst, srcData);
    }

    size_t size() const override { return m_size; }
    size_t capacity() const override { return m_capacity; }

    void reserve(size_t n) override {
        if (n <= m_capacity) return;
        const size_t neededPages = (n + ELEMENTS_PER_PAGE - 1) / ELEMENTS_PER_PAGE;
        const size_t currentPages = m_pages.size();
        for (size_t i = currentPages; i < neededPages; ++i) {
            T* page = static_cast<T*>(m_allocator->allocate(PAGE_SIZE, alignof(T)));
            SEED_ASSERT(page != nullptr, "ComponentArray page allocation failed");
            m_pages.push_back(page);
        }
        m_capacity = neededPages * ELEMENTS_PER_PAGE;
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

    // -----------------------------------------------------------------------
    // Typed interface (preferred)
    // -----------------------------------------------------------------------
    template<typename... Args>
    T* emplaceBack(Args&&... args) {
        if (m_size >= m_capacity) {
            reserve(m_capacity + ELEMENTS_PER_PAGE);
        }
        T* slot = static_cast<T*>(get(m_size));
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
        return m_pages.empty() ? nullptr : m_pages[0];
    }

    const T* data() const {
        return m_pages.empty() ? nullptr : m_pages[0];
    }

private:
    seed::memory::Allocator* m_allocator;
    std::vector<T*> m_pages;
    size_t m_size;
    size_t m_capacity;
};

} // namespace seed::ecs
