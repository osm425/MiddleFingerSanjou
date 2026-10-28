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

double skillCooldownSeconds(int swordStage, int skillIndex) {
	static constexpr std::array<double, 6> sealedSwordCooldowns = { 4.0, 6.0, 8.0, 3.0, 5.0, 10.0 };
	if (swordStage == 0 && skillIndex >= 0 && skillIndex < static_cast<int>(sealedSwordCooldowns.size())) {
		return sealedSwordCooldowns[skillIndex];
	}
	return SKILL_COOLDOWN_SECONDS;
}

static void CALLBACK voicePlaybackCallback(HWAVEOUT, UINT message, DWORD_PTR instance, DWORD_PTR, DWORD_PTR) {
	if (message != WOM_DONE || !instance) return;
	auto* playback = reinterpret_cast<VoicePlayback*>(instance);
	playback->completed.store(true, std::memory_order_release);
}

static void releaseVoicePlayback(VoicePlayback& playback, bool stop) {
	if (playback.device) {
		if (stop) waveOutReset(playback.device);
		if ((playback.header.dwFlags & WHDR_PREPARED) != 0) {
			waveOutUnprepareHeader(playback.device, &playback.header, sizeof(playback.header));
		}
		waveOutClose(playback.device);
	}
	playback.device = nullptr;
	playback.header = {};
	playback.audioData.clear();
	playback.completed.store(true, std::memory_order_release);
	playback.startedAt = 0.0;
}

static bool readPcmWave(const fs::path& path, WAVEFORMATEX& format, std::vector<char>& samples) {
	std::ifstream input(path, std::ios::binary);
	char container[4]{}, waveId[4]{};
	uint32_t riffSize = 0;
	input.read(container, sizeof(container));
	input.read(reinterpret_cast<char*>(&riffSize), sizeof(riffSize));
	input.read(waveId, sizeof(waveId));
	if (!input || std::string(container, 4) != "RIFF" || std::string(waveId, 4) != "WAVE") return false;

	bool haveFormat = false;
	bool haveSamples = false;
	while (input) {
		char chunkId[4]{};
		uint32_t chunkSize = 0;
		input.read(chunkId, sizeof(chunkId));
		input.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
		if (!input) break;
		const std::streamoff chunkStart = input.tellg();
		const std::string id(chunkId, sizeof(chunkId));
		if (id == "fmt ") {
			if (chunkSize < 16) return false;
			input.read(reinterpret_cast<char*>(&format.wFormatTag), sizeof(format.wFormatTag));
			input.read(reinterpret_cast<char*>(&format.nChannels), sizeof(format.nChannels));
			input.read(reinterpret_cast<char*>(&format.nSamplesPerSec), sizeof(format.nSamplesPerSec));
			input.read(reinterpret_cast<char*>(&format.nAvgBytesPerSec), sizeof(format.nAvgBytesPerSec));
			input.read(reinterpret_cast<char*>(&format.nBlockAlign), sizeof(format.nBlockAlign));
			input.read(reinterpret_cast<char*>(&format.wBitsPerSample), sizeof(format.wBitsPerSample));
			format.cbSize = 0;
			haveFormat = static_cast<bool>(input);
		}
		else if (id == "data") {
			samples.resize(chunkSize);
			input.read(samples.data(), static_cast<std::streamsize>(samples.size()));
			haveSamples = static_cast<bool>(input);
		}
		input.seekg(chunkStart + chunkSize + (chunkSize & 1u), std::ios::beg);
		if (haveFormat && haveSamples) break;
	}
	return haveFormat && haveSamples && format.wFormatTag == WAVE_FORMAT_PCM
		&& format.nChannels > 0 && format.nSamplesPerSec > 0 && format.nBlockAlign > 0 && !samples.empty();
}

void updateVoicePlayback(App& app) {
	for (VoicePlayback& playback : app.voicePlayback) {
		if (playback.device && playback.completed.load(std::memory_order_acquire)) {
			releaseVoicePlayback(playback, false);
		}
	}
}

void stopVoicePlayback(App& app) {
	for (VoicePlayback& playback : app.voicePlayback) releaseVoicePlayback(playback, true);
}

void playSkillVoice(App& app, const wchar_t* fileName, double now) {
	if (!app.voiceEnabled) return;
	const fs::path path = app.assetRoot / L"voice" / fileName;
	WAVEFORMATEX format{};
	std::vector<char> samples;
	if (!readPcmWave(path, format, samples)) {
		std::wcerr << L"Could not read PCM voice: " << path.wstring() << L'\n';
		return;
	}
	updateVoicePlayback(app);
	VoicePlayback* slot = nullptr;
	for (VoicePlayback& playback : app.voicePlayback) {
		if (!playback.device) { slot = &playback; break; }
	}
	if (!slot) {
		slot = &*std::min_element(app.voicePlayback.begin(), app.voicePlayback.end(),
			[](const VoicePlayback& a, const VoicePlayback& b) { return a.startedAt < b.startedAt; });
		releaseVoicePlayback(*slot, true);
	}
	slot->audioData = std::move(samples);
	slot->completed.store(false, std::memory_order_release);
	slot->startedAt = now;
	const MMRESULT opened = waveOutOpen(&slot->device, WAVE_MAPPER, &format,
		reinterpret_cast<DWORD_PTR>(voicePlaybackCallback), reinterpret_cast<DWORD_PTR>(slot), CALLBACK_FUNCTION);
	if (opened != MMSYSERR_NOERROR) {
		releaseVoicePlayback(*slot, false);
		std::wcerr << L"Could not open voice output: " << path.wstring() << L'\n';
		return;
	}
	slot->header = {};
	slot->header.lpData = slot->audioData.data();
	slot->header.dwBufferLength = static_cast<DWORD>(slot->audioData.size());
	if (waveOutPrepareHeader(slot->device, &slot->header, sizeof(slot->header)) != MMSYSERR_NOERROR
		|| waveOutWrite(slot->device, &slot->header, sizeof(slot->header)) != MMSYSERR_NOERROR) {
		releaseVoicePlayback(*slot, true);
		std::wcerr << L"Could not play voice: " << path.wstring() << L'\n';
	}
}

void activateSkill(App& app, int skillIndex, double now) {
	if (!skillAvailableForStage(app.swordStage, skillIndex)) return;
	if (skillAnimationActive(app) || app.cooldownEndsAt[skillIndex] > now) return;
	const int stageAtActivation = app.swordStage;
	const double cooldown = skillCooldownSeconds(stageAtActivation, skillIndex);
	app.cooldownDurations[skillIndex] = cooldown;
	app.cooldownEndsAt[skillIndex] = now + cooldown;
	if (stageAtActivation == 0) {
		app.skillAnimation = {};
		app.skillAnimation.skillIndex = skillIndex;
		app.skillAnimation.duration = skillAnimationSeconds(skillIndex);
		if (skillIndex == 2) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"bored_to_play_1.wav", now);
		}
		else if (skillIndex == 4) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"come_on_1.wav", now);
		}
		else if (skillIndex == 5) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"pojangji_1.wav", now);
		}
	}
	else if (skillIndex == 5 && app.swordStage < 3) {
		// Other sword forms already have their own state handling, but no new animations are added here.
		++app.swordStage;
	}
}

void confirmReturnToStart(App& app) {
	app.cooldownEndsAt.fill(0.0);
	app.cooldownDurations.fill(0.0);
	app.skillAnimation = {};
	stopVoicePlayback(app);
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
		&& !skillAnimationActive(*app)
		&& (mods & GLFW_MOD_CONTROL) != 0
		&& (mods & GLFW_MOD_SHIFT) != 0) {
		const int debugStage = swordStageForDebugKey(key);
		if (debugStage >= 0) {
			app->swordStage = debugStage;
			app->cooldownEndsAt.fill(0.0);
			app->cooldownDurations.fill(0.0);
			return;
		}
	}
	if (app->screen == Screen::Start && key == GLFW_KEY_ESCAPE) {
		app->screen = Screen::ConfirmExit;
	}
	else if (app->screen == Screen::Start && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE)) {
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
	else if (app->screen == Screen::ConfirmExit) {
		if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_Y) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		else if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_N) {
			app->screen = Screen::Start;
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
	if (app->screen == Screen::ConfirmExit) {
		if (inside(designX, designY, CONFIRM_EXIT_BUTTON)) glfwSetWindowShouldClose(window, GLFW_TRUE);
		else if (inside(designX, designY, CANCEL_EXIT_BUTTON)) app->screen = Screen::Start;
		return;
	}
	if (app->screen == Screen::ConfirmReturn) {
		if (inside(designX, designY, MUSIC_TOGGLE_BUTTON)) app->musicEnabled = !app->musicEnabled;
		else if (inside(designX, designY, VOICE_TOGGLE_BUTTON)) {
			app->voiceEnabled = !app->voiceEnabled;
			if (!app->voiceEnabled) stopVoicePlayback(*app);
		}
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
	if (!app) return;
	float designX = 0.0f, designY = 0.0f;
	cursorToDesign(*app, x, y, designX, designY);
	app->hoveredSkillIndex = -1;
	if (app->screen == Screen::Main) {
		for (const SkillSlot& slot : SKILL_SLOTS) {
			if (skillAvailableForStage(app->swordStage, slot.skillIndex)
				&& inside(designX, designY, slot.bounds)) {
				app->hoveredSkillIndex = slot.skillIndex;
				break;
			}
		}
	}
	if (!app->cameraDragging) return;
	if (app->screen != Screen::Main) {
		app->cameraDragging = false;
		return;
	}
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
	if (skillAnimationActive(app)) {
		updateSkillAnimation(app, deltaTime, glfwGetTime());
		return;
	}
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

