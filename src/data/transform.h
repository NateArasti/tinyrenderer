#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace tr::Data {
    struct Transform {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 eulerAngles = glm::vec3(0);
        glm::vec3 scale = glm::vec3(1);

        glm::mat4 getModelMatrix() const {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, position);
            model = glm::rotate(model, eulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, eulerAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, eulerAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, scale);
            return model;
        }
    };
}
