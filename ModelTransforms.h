#pragma once

#include "RenderTypes.h"

namespace modelTransforms {
inline glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point) {
	return glm::vec3(transform * glm::vec4(point, 1.0f));
}

inline glm::mat4 alignPositiveY(const glm::vec3& direction) {
	if (glm::length(direction) < 1e-6f) return glm::mat4(1.0f);
	const glm::vec3 source(0.0f, 1.0f, 0.0f);
	const glm::vec3 target = glm::normalize(direction);
	const float cosine = std::clamp(glm::dot(source, target), -1.0f, 1.0f);
	if (cosine > 0.999999f) return glm::mat4(1.0f);
	if (cosine < -0.999999f) {
		return glm::rotate(glm::mat4(1.0f), 3.14159265359f, glm::vec3(1.0f, 0.0f, 0.0f));
	}
	const glm::vec3 axis = glm::normalize(glm::cross(source, target));
	return glm::rotate(glm::mat4(1.0f), std::acos(cosine), axis);
}
}
