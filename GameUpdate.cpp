#include "GameState.h"

void updateCharacterMovement(App& app, float deltaTime) {
	if (app.screen != Screen::Main || deltaTime <= 0.0f) return;
	float forwardInput = 0.0f;
	float rightInput = 0.0f;
	if (glfwGetKey(app.window, GLFW_KEY_W) == GLFW_PRESS) forwardInput += 1.0f;
	if (glfwGetKey(app.window, GLFW_KEY_S) == GLFW_PRESS) forwardInput -= 1.0f;
	if (glfwGetKey(app.window, GLFW_KEY_D) == GLFW_PRESS) rightInput += 1.0f;
	if (glfwGetKey(app.window, GLFW_KEY_A) == GLFW_PRESS) rightInput -= 1.0f;

	const glm::vec3 forward(-std::sin(app.cameraYaw), 0.0f, -std::cos(app.cameraYaw));
	const glm::vec3 right(std::cos(app.cameraYaw), 0.0f, -std::sin(app.cameraYaw));
	glm::vec3 movement = forward * forwardInput + right * rightInput;
	const float movementLength = glm::length(movement);
	const bool hasMovementInput = movementLength > 1e-6f;
	const glm::vec3 currentPosition = app.characterPose.rootPosition;
	glm::vec3 nextPosition = currentPosition;
	if (hasMovementInput) {
		movement /= movementLength;
		nextPosition = currentPosition + movement * (CHARACTER_MOVE_SPEED * deltaTime);
		const float fieldLimit = FIELD_HALF_EXTENT - CHARACTER_FIELD_MARGIN;
		nextPosition.x = std::clamp(nextPosition.x, -fieldLimit, fieldLimit);
		nextPosition.z = std::clamp(nextPosition.z, -fieldLimit, fieldLimit);
	}

	const glm::vec3 actualMovement = nextPosition - currentPosition;
	const float actualMovementLength = glm::length(actualMovement);
	const bool isMoving = actualMovementLength > 1e-6f;
	const float blendStep = 1.0f - std::exp(-deltaTime * 10.0f);
	app.runAnimationBlend += ((isMoving ? 1.0f : 0.0f) - app.runAnimationBlend) * blendStep;
	if (app.runAnimationBlend < 0.001f) app.runAnimationBlend = 0.0f;
	if (hasMovementInput) {
		constexpr float FULL_TURN = 6.28318530718f;
		const float targetYaw = std::atan2(movement.x, movement.z); // Use requested direction, not boundary-clamped displacement.
		const float yawDelta = std::remainder(targetYaw - app.characterPose.rootYaw, FULL_TURN);
		const float turnBlend = 1.0f - std::exp(-deltaTime * 12.0f);
		app.characterPose.rootYaw += yawDelta * turnBlend;
	}
	if (isMoving) {
		constexpr float RUN_CYCLE_RATE = 10.6814f; // 1.7 cycles per second.
		constexpr float FULL_TURN = 6.28318530718f;
		app.runAnimationPhase = std::fmod(app.runAnimationPhase + RUN_CYCLE_RATE * deltaTime, FULL_TURN);
		app.characterPose.rootPosition = nextPosition;
	}
}

void updateGame(App& app, float deltaTime) {
	if (app.screen != Screen::Main || deltaTime <= 0.0f) return;
	if (skillAnimationActive(app)) {
		updateSkillAnimation(app, deltaTime, glfwGetTime());
		return;
	}
	updateCharacterMovement(app, deltaTime);
}

