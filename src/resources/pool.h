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
    };
}
