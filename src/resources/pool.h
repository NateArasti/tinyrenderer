#pragma once

#include <memory>
#include <cstdint>
#include <vector>
#include <utility>

#include "handle.h"

namespace tr::Resources {
    template<typename T>
    class Pool {
    private:
        struct Slot {
            std::unique_ptr<T> value;
            uint32_t generation;
            bool alive;
        };

        std::vector<Slot> _slots;
        std::vector<uint32_t> _freeList;

    public:
        Pool() = default;
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;
        Pool(Pool&&) = default;
        Pool& operator=(Pool&&) = default;

        bool isValid(Handle<T> handle) const noexcept {
            return handle.isValid()
                && handle.index < _slots.size()
                && _slots[handle.index].alive
                && _slots[handle.index].generation == handle.generation;
        }

        Handle<T> add(std::unique_ptr<T> value) {
            uint32_t idx;
            if (!_freeList.empty()) {
                idx = _freeList.back();
                _freeList.pop_back();
                _slots[idx].value = std::move(value);
                _slots[idx].alive = true;
            } else {
                idx = static_cast<uint32_t>(_slots.size());
                _slots.push_back({std::move(value), 0, true});
            }
            return {idx, _slots[idx].generation};
        }

        T* get(Handle<T> handle) {
            if (!isValid(handle)) return nullptr;
            return _slots[handle.index].value.get();
        }

        void remove(Handle<T> handle) {
            if (!isValid(handle)) return;
            _slots[handle.index].value.reset();
            _slots[handle.index].alive = false;
            ++_slots[handle.index].generation;
            _freeList.push_back(handle.index);
        }

        void clear() {
            _freeList.clear();

            for (uint32_t i = 0; i < _slots.size(); ++i) {
                auto& slot = _slots[i];
                if (slot.alive) {
                    slot.value.reset();
                    slot.alive = false;
                    ++slot.generation;
                }
                _freeList.push_back(i);
            }
        }

        struct Iterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type = std::pair<Handle<T>, T*>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type;

            explicit Iterator(std::vector<Slot>* slots, size_t index)
                : _slots(slots), _index(index) {
                advance();
            }

            value_type operator*() const {
                auto& slot = (*_slots)[_index];
                return {{static_cast<uint32_t>(_index), slot.generation}, slot.value.get()};
            }

            Iterator& operator++() {
                ++_index;
                advance();
                return *this;
            }

            Iterator operator++(int) {
                auto tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) const { return _index == other._index; }
            bool operator!=(const Iterator& other) const { return _index != other._index; }

        private:
            void advance() {
                while (_index < _slots->size() && !(*_slots)[_index].alive)
                    ++_index;
            }

            std::vector<Slot>* _slots;
            size_t _index;
        };

        Iterator begin() { return Iterator(&_slots, 0); }
        Iterator end() { return Iterator(&_slots, _slots.size()); }
    };
}
