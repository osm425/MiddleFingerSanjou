#include "GameState.h"
#include <cstdint>
#include <cstring>

// 화면 좌표가 버튼이나 UI 사각형 안에 들어오는지 판정한다.
bool inside(float x, float y, const Rect& rect) {
	return x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h;
}

// 숫자열 또는 숫자 키패드 입력을 스킬 배열 인덱스로 바꾼다.
static int skillIndexForKey(int key) {
	if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6) return key - GLFW_KEY_1;
	if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_6) return key - GLFW_KEY_KP_1;
	return -1;
}

// 디버그 숫자 입력을 검 단계 인덱스로 바꾼다.
static int swordStageForDebugKey(int key) {
	if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) return key - GLFW_KEY_1;
	if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_4) return key - GLFW_KEY_KP_1;
	return -1;
}

// 현재 검 단계에서 해당 스킬을 사용할 수 있는지 확인한다.
bool skillAvailableForStage(int swordStage, int skillIndex) {
	if (swordStage < 0 || swordStage > 3 || skillIndex < 0 || skillIndex >= 6) return false;
	return !(swordStage == 3 && skillIndex == 3);
}

// 검 단계와 스킬 번호에 해당하는 쿨타임을 초 단위로 반환한다.
double skillCooldownSeconds(int swordStage, int skillIndex) {
	static constexpr std::array<double, 6> sealedSwordCooldowns = { 4.0, 6.0, 8.0, 3.0, 5.0, 10.0 };
	static constexpr std::array<double, 6> laevateinnCooldowns = { 4.0, 8.0, 15.0, 0.0, 5.0, 16.0 };
	if (skillIndex < 0 || skillIndex >= static_cast<int>(sealedSwordCooldowns.size())) return SKILL_COOLDOWN_SECONDS;
	if (swordStage >= 0 && swordStage <= 2) {
		return sealedSwordCooldowns[skillIndex];
	}
	if (swordStage == 3) return laevateinnCooldowns[skillIndex];
	return SKILL_COOLDOWN_SECONDS;
}

// waveOut이 재생 완료를 알리면 슬롯을 완료 상태로 표시한다.
static void CALLBACK voicePlaybackCallback(HWAVEOUT, UINT message, DWORD_PTR instance, DWORD_PTR, DWORD_PTR) {
	if (message != WOM_DONE || !instance) return;
	auto* playback = reinterpret_cast<VoicePlayback*>(instance);
	playback->completed.store(true, std::memory_order_release);
}

// 재생 장치를 닫고, 필요하면 현재 소리를 먼저 중단한다.
static void closeVoicePlaybackDevice(VoicePlayback& playback, bool stop) {
	if (playback.device) {
		if (stop) waveOutReset(playback.device);
		if ((playback.header.dwFlags & WHDR_PREPARED) != 0) {
			waveOutUnprepareHeader(playback.device, &playback.header, sizeof(playback.header));
		}
		waveOutClose(playback.device);
	}
	playback.device = nullptr;
	playback.header = {};
}

// 재생 장치와 샘플 버퍼를 해제하고 슬롯 상태를 초기화한다.
static void releaseVoicePlayback(VoicePlayback& playback, bool stop) {
	closeVoicePlaybackDevice(playback, stop);
	playback.audioData.clear();
	playback.outputData.clear();
	playback.format = {};
	playback.completed.store(true, std::memory_order_release);
	playback.startedAt = 0.0;
	playback.fileName.clear();
	playback.gain = 1.0f;
	playback.spatialized = false;
	playback.sourcePosition = glm::vec3(0.0f);
}

// RIFF/WAVE 파일에서 PCM 형식 정보와 오디오 샘플을 읽는다.
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

// 준비된 샘플 버퍼를 waveOut 장치에 연결해 재생한다.
static bool openVoicePlaybackDevice(VoicePlayback& playback) {
	playback.completed.store(false, std::memory_order_release);
	const MMRESULT opened = waveOutOpen(&playback.device, WAVE_MAPPER, &playback.format,
		reinterpret_cast<DWORD_PTR>(voicePlaybackCallback), reinterpret_cast<DWORD_PTR>(&playback), CALLBACK_FUNCTION);
	if (opened != MMSYSERR_NOERROR) return false;
	playback.header = {};
	playback.header.lpData = playback.outputData.data();
	playback.header.dwBufferLength = static_cast<DWORD>(playback.outputData.size());
	if (waveOutPrepareHeader(playback.device, &playback.header, sizeof(playback.header)) != MMSYSERR_NOERROR
		|| waveOutWrite(playback.device, &playback.header, sizeof(playback.header)) != MMSYSERR_NOERROR) {
		closeVoicePlaybackDevice(playback, true);
		return false;
	}
	return true;
}

// 끝난 보이스 재생 슬롯을 프레임 갱신 중 정리한다.
void updateVoicePlayback(App& app) {
	for (VoicePlayback& playback : app.voicePlayback) {
		if (playback.device && playback.completed.load(std::memory_order_acquire)) {
			releaseVoicePlayback(playback, false);
		}
	}
}

// 진행 중인 보이스를 모두 중단하고 슬롯을 비운다.
void stopVoicePlayback(App& app) {
	for (VoicePlayback& playback : app.voicePlayback) releaseVoicePlayback(playback, true);
}

// 키라 모델의 배치 규칙에 맞춰 공간 음향의 발생 위치를 구한다.
static glm::vec3 kiraSoundSourcePosition(const App& app) {
	glm::vec3 towardEnemy = app.enemyPosition - app.characterPose.rootPosition;
	towardEnemy.y = 0.0f;
	if (glm::length(towardEnemy) < 1e-5f) towardEnemy = glm::vec3(0.0f, 0.0f, 1.0f);
	towardEnemy = glm::normalize(towardEnemy);
	return app.enemyPosition - towardEnemy * LAEVATEINN_SKILL6_KIRA_FORWARD_OFFSET;
}

// 효과음 PCM 데이터에 개별 음량과 키라 기준 좌우·거리 감쇠를 입힌다.
static void renderSoundEffectSamples(VoicePlayback& playback, const App& app) {
	// Apply per-sound gain and pan to PCM samples; waveOutSetVolume affects unrelated streams too.
	playback.outputData = playback.audioData;
	float left = std::clamp(playback.gain, 0.0f, 1.0f);
	float right = left;
	if (playback.spatialized) {
		const glm::vec3 target = app.characterPose.rootPosition;
		const glm::vec3 listener = sceneCameraPosition(target, app.cameraYaw, app.cameraPitch);
		glm::vec3 towardTarget = target - listener;
		if (glm::length(towardTarget) < 1e-5f) towardTarget = glm::vec3(0.0f, 0.0f, -1.0f);
		towardTarget = glm::normalize(towardTarget);
		glm::vec3 cameraRight = glm::cross(towardTarget, glm::vec3(0.0f, 1.0f, 0.0f));
		if (glm::length(cameraRight) < 1e-5f) cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
		cameraRight = glm::normalize(cameraRight);
		const glm::vec3 sourceDirection = playback.sourcePosition - listener;
		const float distance = glm::length(sourceDirection);
		const float pan = distance > 1e-5f
			? std::clamp(glm::dot(sourceDirection / distance, cameraRight), -1.0f, 1.0f) : 0.0f;
		const float attenuation = 1.0f / (1.0f + (distance / 12.0f) * (distance / 12.0f));
		left *= attenuation * (pan > 0.0f ? 1.0f - pan : 1.0f);
		right *= attenuation * (pan < 0.0f ? 1.0f + pan : 1.0f);
	}
	if (playback.format.wFormatTag != WAVE_FORMAT_PCM || playback.format.wBitsPerSample != 16
		|| playback.format.nBlockAlign < playback.format.nChannels * sizeof(int16_t)) return;

	const size_t sampleCount = playback.outputData.size() / sizeof(int16_t);
	for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
		const int channel = static_cast<int>(sampleIndex % playback.format.nChannels);
		const float channelGain = playback.spatialized && playback.format.nChannels >= 2
			? (channel == 0 ? left : channel == 1 ? right : (left + right) * 0.5f)
			: playback.gain;
		int16_t sample = 0;
		std::memcpy(&sample, playback.audioData.data() + sampleIndex * sizeof(sample), sizeof(sample));
		const int16_t scaled = static_cast<int16_t>(std::clamp(
			std::lround(static_cast<float>(sample) * channelGain), -32768L, 32767L));
		std::memcpy(playback.outputData.data() + sampleIndex * sizeof(scaled), &scaled, sizeof(scaled));
	}
}

// 완료된 효과음 재생 슬롯을 정리한다.
void updateSoundEffectPlayback(App& app) {
	for (VoicePlayback& playback : app.soundEffectPlayback) {
		if (playback.device && playback.completed.load(std::memory_order_acquire)) {
			releaseVoicePlayback(playback, false);
		}
	}
}

// 활성화된 모든 효과음을 중단한다.
void stopSoundEffectPlayback(App& app) {
	for (VoicePlayback& playback : app.soundEffectPlayback) releaseVoicePlayback(playback, true);
}

// 파일 이름이 일치하는 특정 효과음만 중단한다.
void stopSkillSoundEffect(App& app, const wchar_t* fileName) {
	for (VoicePlayback& playback : app.soundEffectPlayback) {
		if (playback.device && playback.fileName == fileName) releaseVoicePlayback(playback, true);
	}
}

// 특정 효과음의 남은 구간을 새 음량으로 이어 재생한다.
void setSkillSoundEffectVolume(App& app, const wchar_t* fileName, float gain) {
	for (VoicePlayback& playback : app.soundEffectPlayback) {
		if (playback.device && playback.fileName == fileName) {
			MMTIME position{};
			position.wType = TIME_BYTES;
			if (waveOutGetPosition(playback.device, &position, sizeof(position)) != MMSYSERR_NOERROR
				|| position.wType != TIME_BYTES) continue;
			size_t consumedBytes = std::min<size_t>(position.u.cb, playback.audioData.size());
			consumedBytes -= consumedBytes % playback.format.nBlockAlign;
			if (consumedBytes >= playback.audioData.size()) {
				releaseVoicePlayback(playback, true);
				continue;
			}
			playback.gain = std::clamp(gain, 0.0f, 1.0f);
			// Resume the active clip from its playback cursor so only this sound changes gain.
			closeVoicePlaybackDevice(playback, true);
			playback.audioData.erase(playback.audioData.begin(), playback.audioData.begin() + consumedBytes);
			if (playback.spatialized) playback.sourcePosition = kiraSoundSourcePosition(app);
			renderSoundEffectSamples(playback, app);
			if (!openVoicePlaybackDevice(playback)) releaseVoicePlayback(playback, false);
		}
	}
}

// 효과음 파일을 불러와 재생 슬롯을 배정하고 일반/공간 재생을 시작한다.
static void playSkillSoundEffectInternal(App& app, const wchar_t* fileName, double now,
	bool spatialized, float gain) {
	if (!app.soundEffectsEnabled) return;
	const fs::path path = app.assetRoot / L"soundeffects" / fileName;
	WAVEFORMATEX format{};
	std::vector<char> samples;
	if (!readPcmWave(path, format, samples)) {
		std::wcerr << L"Could not read PCM sound effect: " << path.wstring() << L'\n';
		return;
	}
	updateSoundEffectPlayback(app);
	VoicePlayback* slot = nullptr;
	for (VoicePlayback& playback : app.soundEffectPlayback) {
		if (!playback.device) { slot = &playback; break; }
	}
	if (!slot) {
		slot = &*std::min_element(app.soundEffectPlayback.begin(), app.soundEffectPlayback.end(),
			[](const VoicePlayback& a, const VoicePlayback& b) { return a.startedAt < b.startedAt; });
		releaseVoicePlayback(*slot, true);
	}
	slot->audioData = std::move(samples);
	slot->format = format;
	slot->startedAt = now;
	slot->fileName = fileName;
	slot->gain = std::clamp(gain, 0.0f, 1.0f);
	slot->spatialized = spatialized;
	if (spatialized) slot->sourcePosition = kiraSoundSourcePosition(app);
	renderSoundEffectSamples(*slot, app);
	if (!openVoicePlaybackDevice(*slot)) {
		releaseVoicePlayback(*slot, false);
		std::wcerr << L"Could not open sound effect output: " << path.wstring() << L'\n';
		return;
	}
}

// 지정한 효과음을 중심 음량으로 재생한다.
void playSkillSoundEffect(App& app, const wchar_t* fileName, double now) {
	playSkillSoundEffectInternal(app, fileName, now, false, 1.0f);
}

// 키라 위치를 기준으로 좌우감과 거리에 따른 음량을 적용해 재생한다.
void playSpatialSkillSoundEffect(App& app, const wchar_t* fileName, double now) {
	playSkillSoundEffectInternal(app, fileName, now, true, 1.0f);
}

// 보이스 설정이 켜져 있을 때 스킬 음성 파일을 재생 슬롯에 올린다.
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
	slot->outputData = slot->audioData;
	slot->format = format;
	slot->startedAt = now;
	if (!openVoicePlaybackDevice(*slot)) {
		releaseVoicePlayback(*slot, false);
		std::wcerr << L"Could not open voice output: " << path.wstring() << L'\n';
		return;
	}
}

// 입력된 스킬의 사용 가능 여부를 확인하고 단계별 애니메이션을 시작한다.
void activateSkill(App& app, int skillIndex, double now) {
	if (!skillAvailableForStage(app.swordStage, skillIndex)) return;
	if (skillAnimationActive(app) || app.cooldownEndsAt[skillIndex] > now) return;
	bool enemyInLockOnRange = false;
	glm::vec3 enemyApproachDirection(0.0f);
	if (app.enemyModel.loaded()) {
		const glm::vec2 toEnemy(app.enemyPosition.x - app.characterPose.rootPosition.x,
			app.enemyPosition.z - app.characterPose.rootPosition.z);
		const float distanceSquared = glm::dot(toEnemy, toEnemy);
		if (distanceSquared > 1e-6f && distanceSquared <= ENEMY_LOCKON_RADIUS * ENEMY_LOCKON_RADIUS) {
			app.characterPose.rootYaw = std::atan2(toEnemy.x, toEnemy.y);
			enemyApproachDirection = glm::vec3(toEnemy.x, 0.0f, toEnemy.y) / std::sqrt(distanceSquared);
			enemyInLockOnRange = true;
		}
	}
	const int stageAtActivation = app.swordStage;
	const double cooldown = skillCooldownSeconds(stageAtActivation, skillIndex);
	app.cooldownDurations[skillIndex] = cooldown;
	app.cooldownEndsAt[skillIndex] = now + cooldown;
	// Laevateinn has its own timing; earlier stages share the sealed sword's animation and cooldowns.
	if (stageAtActivation <= 3) {
		app.skillAnimation = {};
		app.skillAnimation.skillIndex = skillIndex;
		app.skillAnimation.swordStageAtStart = stageAtActivation;
		app.skillAnimation.duration = skillAnimationSeconds(skillIndex, stageAtActivation);
		if (skillIndex == 5) {
			app.skillAnimation.thrownSwordStart = app.characterPose.rootPosition;
			app.skillAnimation.thrownSwordTarget = app.characterPose.rootPosition;
		}
		if (stageAtActivation == 3 && enemyInLockOnRange) {
			app.skillAnimation.enemyHitPending = true;
		}
		if (enemyInLockOnRange && stageAtActivation < 3) {
			app.skillAnimation.lungeDirection = enemyApproachDirection;
			app.skillAnimation.lungeDistance = ENEMY_LUNGE_DISTANCE;
			app.skillAnimation.lungeDuration = ENEMY_LUNGE_SECONDS;
			app.skillAnimation.enemyHitPending = true;
		}
		if (stageAtActivation == 3 && app.enemyModel.loaded() && (skillIndex == 2 || skillIndex == 5)) {
			const glm::vec2 toEnemy(app.enemyPosition.x - app.characterPose.rootPosition.x,
				app.enemyPosition.z - app.characterPose.rootPosition.z - 0.6f);
			const float distance = glm::length(toEnemy);
			if (distance > 1e-4f) {
				const glm::vec3 direction(toEnemy.x / distance, 0.0f, toEnemy.y / distance);
				app.skillAnimation.lungeDirection = direction;
				app.characterPose.rootYaw = std::atan2(direction.x, direction.z);
				if (skillIndex == 2) {
					// Move during the kick toward the sword planted at the enemy, so the right foot can reach it.
					app.skillAnimation.lungeDistance = std::max(0.0f, distance - 3.4f);
					app.skillAnimation.lungeStart = laevateinnSkill3KickStartTime();
					app.skillAnimation.lungeDuration = 0.2;
					app.skillAnimation.enemyHitPending = true;
				}
				else {
					// Approach the target before charging the horizontal cut.
					app.enemyYaw = std::atan2(-direction.x, -direction.z);
					app.enemyYawSet = true;
					const glm::vec3 kiraCenter = app.enemyPosition
						- direction * LAEVATEINN_SKILL6_KIRA_FORWARD_OFFSET;
					const glm::vec2 toKira(kiraCenter.x - app.characterPose.rootPosition.x,
						kiraCenter.z - app.characterPose.rootPosition.z);
					const float distanceToKira = glm::length(toKira);
					if (distanceToKira > 1e-4f) {
						app.skillAnimation.lungeDirection = glm::vec3(
							toKira.x / distanceToKira, 0.0f, toKira.y / distanceToKira);
					}
					app.skillAnimation.lungeDistance = std::max(0.0f,
						distanceToKira - LAEVATEINN_SKILL6_STOP_DISTANCE_FROM_KIRA);
					app.skillAnimation.lungeDuration = app.skillAnimation.lungeDistance / 3.5;
					app.skillAnimation.duration += app.skillAnimation.lungeDuration;
					app.skillAnimation.enemyHitPending = true;
				}
			}
		}
		if (stageAtActivation < 3 && skillIndex == 2) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"bored_to_play_1.wav", now);
		}
		else if (stageAtActivation < 3 && skillIndex == 4) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"come_on_1.wav", now);
		}
		else if (stageAtActivation < 3 && skillIndex == 5 && stageAtActivation == 0) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"pojangji_1.wav", now);
		}
		else if (stageAtActivation < 3 && skillIndex == 5 && stageAtActivation == 1) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"pojangji_2.wav", now);
		}
		else if (stageAtActivation < 3 && skillIndex == 5 && stageAtActivation == 2) {
			app.skillAnimation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"pojangji_3.wav", now);
		}
	}
}

// 처음 화면으로 돌아가며 쿨타임·애니메이션·캐릭터 상태를 초기화한다.
void confirmReturnToStart(App& app) {
	app.cooldownEndsAt.fill(0.0);
	app.cooldownDurations.fill(0.0);
	app.skillAnimation = {};
	stopVoicePlayback(app);
	stopSoundEffectPlayback(app);
	app.swordStage = 0;
	app.enemyYaw = 0.0f;
	app.enemyYawSet = false;
	app.enemyHasBeenHit = false;
	app.characterPose.rootPosition = app.initialCharacterPosition;
	app.characterPose.rootYaw = app.initialCharacterYaw;
	app.runAnimationPhase = 0.0f;
	app.runAnimationBlend = 0.0f;
	app.cameraDragging = false;
	app.screen = Screen::Start;
}


// 복귀를 취소하고 확인창이 떠 있던 시간만큼 쿨타임 기준을 보정한다.
void cancelReturnConfirmation(App& app, double now) {
	const double pausedDuration = std::max(0.0, now - app.pauseStartedAt);
	for (double& cooldownEnd : app.cooldownEndsAt) {
		if (cooldownEnd > app.pauseStartedAt) cooldownEnd += pausedDuration;
	}
	app.cameraDragging = false;
	app.screen = Screen::Main;
}

// 창 커서 좌표를 고정된 UI 디자인 좌표계로 변환한다.
void cursorToDesign(App& app, double x, double y, float& designX, float& designY) {
	int windowWidth = 1, windowHeight = 1;
	glfwGetWindowSize(app.window, &windowWidth, &windowHeight);
	app.canvas.toDesign(x, y, windowWidth, windowHeight, designX, designY);
}

// 시작·확인 화면, 스킬 단축키와 검 단계 디버그 입력을 처리한다.
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

// 마우스 클릭의 UI 동작과 카메라 드래그 시작·종료를 처리한다.
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
		if (inside(designX, designY, MUSIC_TOGGLE_BUTTON)) app->musicEnabled = !app->musicEnabled;
		else if (inside(designX, designY, VOICE_TOGGLE_BUTTON)) {
			app->voiceEnabled = !app->voiceEnabled;
			if (!app->voiceEnabled) stopVoicePlayback(*app);
		}
		else if (inside(designX, designY, SOUND_EFFECT_TOGGLE_BUTTON)) {
			app->soundEffectsEnabled = !app->soundEffectsEnabled;
			if (!app->soundEffectsEnabled) stopSoundEffectPlayback(*app);
		}
		else if (inside(designX, designY, CONFIRM_EXIT_BUTTON)) glfwSetWindowShouldClose(window, GLFW_TRUE);
		else if (inside(designX, designY, CANCEL_EXIT_BUTTON)) app->screen = Screen::Start;
		return;
	}
	if (app->screen == Screen::ConfirmReturn) {
		if (inside(designX, designY, MUSIC_TOGGLE_BUTTON)) app->musicEnabled = !app->musicEnabled;
		else if (inside(designX, designY, VOICE_TOGGLE_BUTTON)) {
			app->voiceEnabled = !app->voiceEnabled;
			if (!app->voiceEnabled) stopVoicePlayback(*app);
		}
		else if (inside(designX, designY, SOUND_EFFECT_TOGGLE_BUTTON)) {
			app->soundEffectsEnabled = !app->soundEffectsEnabled;
			if (!app->soundEffectsEnabled) stopSoundEffectPlayback(*app);
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

// 드래그 중 카메라 각도를 갱신하고 스킬 아이콘 호버 상태를 계산한다.
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

// WASD 이동과 달리기 전환, 스킬 애니메이션 및 카메라 추적을 갱신한다.
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

// 프레임버퍼 크기 변경 시 화면 표시 영역과 Canvas 크기를 갱신한다.
void framebufferCallback(GLFWwindow* window, int width, int height) {
	App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (!app) return;
	app->framebufferWidth = width;
	app->framebufferHeight = height;
	app->canvas.resize(width, height);
}

