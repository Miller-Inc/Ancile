//
// Created by James Miller on 9/28/2026.
//

#pragma once
#include "glm/glm.hpp"

namespace Ancile {
    using Vector3 = glm::vec3;
    using Quat = glm::quat;

    class Actor {
    public:
        Actor() = default;
        virtual ~Actor() = default;

        Vector3 Position();
        Quat Orientation();
        Vector3 Scale();
    };
}