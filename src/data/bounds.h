#pragma once

#include <cstdint>
#include <limits>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>

namespace tr::Data {
    struct Bounds {
        bool valid = false;
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

        static Bounds zero() {
            return Bounds{
                .valid = true,
                .min = glm::vec3(0),
                .max = glm::vec3(0)
            };
        }

        glm::vec3 getSize() const { return max - min; }
        glm::vec3 getCenter() const { return min + 0.5f * getSize(); }

        void encapsulate(const glm::vec3& point) {
            if (!valid) {
                min = point;
                max = point;
            }
            else {
                min = glm::min(min, point);
                max = glm::max(max, point);
            }
            valid = true;
        }

        void encapsulate(const Bounds& bounds, const glm::mat4& transform = glm::mat4(1.0f)) {
            if (!bounds.valid) {
                return;
            }

            for (uint32_t corner = 0; corner < 8; ++corner) {
                const glm::vec3 localPosition{
                    (corner & 1) ? bounds.max.x : bounds.min.x,
                    (corner & 2) ? bounds.max.y : bounds.min.y,
                    (corner & 4) ? bounds.max.z : bounds.min.z
                };
                const glm::vec3 worldPosition = transform * glm::vec4(localPosition, 1.0f);
                encapsulate(worldPosition);
            }
        }
    };
}
