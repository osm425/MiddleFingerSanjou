#include "GameState.h"

bool inside(float x, float y, const Rect& rect) {
	return x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h;
}

static int skillIndexForKey(int key) {
	if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6) return key - GLFW_KEY_1;
	if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_6) return key - GLFW_KEY_KP_1;
	return -1;
}

static int swordStageForDebugKey(int key) {
	if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) return key - GLFW_KEY_1;
	if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_4) return key - GLFW_KEY_KP_1;
	return -1;
}

bool skillAvailableForStage(int swordStage, int skillIndex) {
	if (swordStage < 0 || swordStage > 3 || skillIndex < 0 || skillIndex >= 6) return false;
	return !(swordStage == 3 && skillIndex == 3);
}

void activateSkill(App& app, int skillIndex, double now) {
	if (!skillAvailableForStage(app.swordStage, skillIndex)) return;
	if (app.cooldownEndsAt[skillIndex] <= now) {
		app.cooldownEndsAt[skillIndex] = now + SKILL_COOLDOWN_SECONDS;
		if (skillIndex == 5 && app.swordStage < 3) ++app.swordStage;
	}
}

void confirmReturnToStart(App& app) {
	app.cooldownEndsAt.fill(0.0);
	app.swordStage = 0;
	app.characterPose.rootPosition = app.initialCharacterPosition;
	app.characterPose.rootYaw = app.initialCharacterYaw;
	app.runAnimationPhase = 0.0f;
	app.runAnimationBlend = 0.0f;
	app.cameraDragging = false;
	app.screen = Screen::Start;
}


void cancelReturnConfirmation(App& app, double now) {
	const double pausedDuration = std::max(0.0, now - app.pauseStartedAt);
	for (double& cooldownEnd : app.cooldownEndsAt) {
		if (cooldownEnd > app.pauseStartedAt) cooldownEnd += pausedDuration;
	}
	app.cameraDragging = false;
	app.screen = Screen::Main;
}

void cursorToDesign(App& app, double x, double y, float& designX, float& designY) {
	int windowWidth = 1, windowHeight = 1;
	glfwGetWindowSize(app.window, &windowWidth, &windowHeight);
	app.canvas.toDesign(x, y, windowWidth, windowHeight, designX, designY);
}

void keyCallback(GLFWwindow* window, int key, int, int action, int mods) {
	if (action != GLFW_PRESS) return;
	App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (!app) return;
	if (app->screen == Screen::Main
		&& (mods & GLFW_MOD_CONTROL) != 0
		&& (mods & GLFW_MOD_SHIFT) != 0) {
		const int debugStage = swordStageForDebugKey(key);
		if (debugStage >= 0) {
			app->swordStage = debugStage;
			app->cooldownEndsAt.fill(0.0);
			return;
		}
	}
	if (app->screen == Screen::Start && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE)) {
		app->screen = Screen::Main;
	}
	else if (app->screen == Screen::Main && key == GLFW_KEY_ESCAPE) {
		app->cameraDragging = false;
		app->pauseStartedAt = glfwGetTime();
		app->screen = Screen::ConfirmReturn;
	}
	else if (app->screen == Screen::Main) {
		const int skillIndex = skillIndexForKey(key);
		if (skillIndex >= 0) activateSkill(*app, skillIndex, glfwGetTime());
	}
	else if (app->screen == Screen::ConfirmReturn) {
		if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_Y) {
			confirmReturnToStart(*app);
		}
		else if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_N) {
			cancelReturnConfirmation(*app, glfwGetTime());
		}
	}
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
	if (button != GLFW_MOUSE_BUTTON_LEFT) return;
	App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (!app) return;
	if (action == GLFW_RELEASE) {
		app->cameraDragging = false;
		return;
	}
	if (action != GLFW_PRESS) return;
	double x = 0, y = 0;
	glfwGetCursorPos(window, &x, &y);
	float designX = 0, designY = 0;
	cursorToDesign(*app, x, y, designX, designY);
	if (app->screen == Screen::Start) {
		if (inside(designX, designY, START_BUTTON)) app->screen = Screen::Main;
		return;
	}
	if (app->screen == Screen::ConfirmReturn) {
		if (inside(designX, designY, MUSIC_TOGGLE_BUTTON)) app->musicEnabled = !app->musicEnabled;
		else if (inside(designX, designY, VOICE_TOGGLE_BUTTON)) app->voiceEnabled = !app->voiceEnabled;
		else if (inside(designX, designY, CONFIRM_RETURN_BUTTON)) confirmReturnToStart(*app);
		else if (inside(designX, designY, CANCEL_RETURN_BUTTON)) cancelReturnConfirmation(*app, glfwGetTime());
		return;
	}
	for (const SkillSlot& slot : SKILL_SLOTS) {
		if (inside(designX, designY, slot.bounds)) {
			activateSkill(*app, slot.skillIndex, glfwGetTime());
			return;
		}
	}
	if (app->screen == Screen::Main && inside(designX, designY, MODEL_VIEW_RECT)) {
		app->cameraDragging = true;
		app->lastCameraDragX = designX;
		app->lastCameraDragY = designY;
	}
}

void cursorPositionCallback(GLFWwindow* window, double x, double y) {
	App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (!app || !app->cameraDragging) return;
	if (app->screen != Screen::Main) {
		app->cameraDragging = false;
		return;
	}
	float designX = 0.0f, designY = 0.0f;
	cursorToDesign(*app, x, y, designX, designY);
	const float deltaX = designX - app->lastCameraDragX;
	const float deltaY = designY - app->lastCameraDragY;
	app->cameraYaw -= deltaX * 0.008f;
	const float targetY = app->characterPose.rootPosition.y;
	const float minimumPitch = std::asin(std::clamp(
		(FIELD_Y + CAMERA_GROUND_CLEARANCE - targetY) / SCENE_CAMERA_DISTANCE,
		-1.0f, 1.0f));
	app->cameraPitch = std::clamp(app->cameraPitch + deltaY * 0.008f, minimumPitch, 1.35f);
	app->lastCameraDragX = designX;
	app->lastCameraDragY = designY;
}

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

void framebufferCallback(GLFWwindow* window, int width, int height) {
	App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (!app) return;
	app->framebufferWidth = width;
	app->framebufferHeight = height;
	app->canvas.resize(width, height);
}

