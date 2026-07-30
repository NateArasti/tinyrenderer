#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace tr::Data {
    struct Transform {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 eulerAngles = glm::vec3(0);
        glm::vec3 scale = glm::vec3(1);

        glm::mat4 getMatrix() const {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, position);
            model = glm::rotate(model, glm::radians(eulerAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(eulerAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(eulerAngles.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, scale);
            return model;
        }

        static Transform fromTransform(const glm::mat4& matrix) {
            Transform result;
            glm::quat orientation;
            glm::vec3 skew;
            glm::vec4 perspective;
            if (!glm::decompose(
                matrix,
                result.scale,
                orientation,
                result.position,
                skew,
                perspective
            )) {
                result.position = glm::vec3(matrix[3]);
                return result;
            }

            float yaw = 0.0f;
            float pitch = 0.0f;
            float roll = 0.0f;
            glm::extractEulerAngleYXZ(
                glm::mat4_cast(orientation),
                yaw,
                pitch,
                roll
            );
            result.eulerAngles = glm::degrees(glm::vec3(pitch, yaw, roll));
            return result;
        }
    };
}
