#include "GameState.h"
#include <random>

namespace {
struct SkillActionPose {
	float armLift = 0.0f;
	float armReach = 0.0f;
	float elbowBend = 0.0f;
	float armYaw = 0.0f;
	float torsoRotationX = 0.0f;
	float torsoRotationY = 0.0f;
	float torsoRotationZ = 0.0f;
	float leftLegPitch = 0.0f;
	float rightLegPitch = 0.0f;
	float leftKneeFlex = 0.0f;
	float rightKneeFlex = 0.0f;
	float rootY = 0.0f;
	float forward = 0.0f;
};

struct SkillFrame {
	SkillActionPose pose;
	double duration;
	const wchar_t* voiceOnEnter = nullptr;
	unsigned voiceCueBit = 0;
};

constexpr float unsealedSwordForwardSpeed = 20.0f;
constexpr float unsealedSwordFlightDistance = 20.0f;
constexpr float thrownSwordGravity = 9.8f;

SkillActionPose action(float armLift = 0.0f, float armReach = 0.0f, float elbowBend = 0.0f,
	float leftLegPitch = 0.0f, float rightLegPitch = 0.0f, float leftKneeFlex = 0.0f,
	float rightKneeFlex = 0.0f, float rootY = 0.0f, float forward = 0.0f,
	float armYaw = 0.0f, float torsoRotationX = 0.0f,
	float torsoRotationY = 0.0f, float torsoRotationZ = 0.0f) {
	return { armLift, armReach, elbowBend, armYaw, torsoRotationX, torsoRotationY, torsoRotationZ,
		leftLegPitch, rightLegPitch, leftKneeFlex, rightKneeFlex, rootY, forward };
}

// Shared leg poses keep the repeated kick sequences consistent across skills.
const SkillActionPose& rightLegRaisedToFace() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, 0.0f, 2.50f, 0.0f, -1.00f);
	return pose;
}

const SkillActionPose& rightLegStompAndLeftKneel() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, 0.20f, 1.20f, -1.20f, -1.20f, -0.6f, 0.0f, 0.0f, 0.18f);
	return pose;
}

const SkillActionPose& rightLegTuckedFootDown() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, 0.0f, 1.40f, 0.0f, -1.40f);
	return pose;
}

const SkillActionPose& rightLegExtendedKick() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, -0.08f, 2.22f, -0.55f, 0.0f, 0.0f, 0.0f, 0.0f, -0.06f,0.0f,-0.5f);
	return pose;
}

const SkillActionPose& rightLegHorizontal() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, 0.12f, 1.57f, -0.80f, 0.0f, 0.0f, 0.0f, 0.0f, -0.03f,0.0f,-0.2f);
	return pose;
}

const std::array<SkillActionPose, 7>& swordComboPoses() {
	static const std::array<SkillActionPose, 7> poses = {{
		action(0.62f, 1.93f, -1.93f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.10f, -0.04f), // bend the arm back so the hand reaches the sword grip on the back
		action(2.18f, 0.60f, 0.00f, -0.22f, 0.38f, 0.0f, 0.15f, 0.0f, 0.0f, 1.00f, 0.0f), // extend the sword arm upward with a slight -X sweep
		action(-0.72f, -1.05f, 0.00f, -0.18f, 0.32f, -0.30f, 0.10f, 0.0f, 0.08f, 0.00f, 0.34f), // swing the sword down in front
		action(2.76f, 1.60f, 1.57f, -0.12f, 0.22f, -0.25f, 0.10f, 0.0f, 0.0f, 0.0f, 0.30f), // raise the elbow and bend the forearm 90 degrees to the character's right
		action(-1.05f, 1.25f, 0.15f, -0.20f, 0.28f, 0.0f, 0.12f, 0.0f, 0.0f, -0.94f, -0.05f), // swing horizontally to the right
		action(-0.22f, 1.25f, 0.00f, -0.16f, 0.24f, -0.22f, 0.0f, 0.0f, 0.05f, -0.22f, 0.32f), // extend the sword arm backward
		action(2.72f, 0.85f, 0.00f, -0.18f, 0.30f, 0.0f, 0.12f, 0.0f, -0.05f, 0.22f, -0.08f) // swing forward and upward
	}};
	return poses;
}

const SkillActionPose& swordThrowPose() {
	static const SkillActionPose pose = action(-0.36f, -0.78f, 0.0f, 0.0f, 0.12f, 0.0f, 0.0f, 0.0f, 0.0f, 1.12f, 0.08f);
	return pose;
}

const std::array<std::vector<SkillFrame>, 6>& sealedSkillFrames() {
	static const auto& combo = swordComboPoses();
	static const std::array<std::vector<SkillFrame>, 6> frames = {{
		{ // 1. Lift, stomp and kneel, tuck, then kick high
			{ rightLegRaisedToFace(), 0.5 },
			{ rightLegStompAndLeftKneel(), 0.5 },
			{ rightLegTuckedFootDown(), 0.5 },
			{ rightLegExtendedKick(), 1.0 }
		},
		{ // 2. Seven-part sword combo
			{ combo[0], 0.1 }, { combo[1], 0.5 }, { combo[2], 0.5 }, { combo[3], 0.5 },
			{ combo[4], 0.6 }, { combo[5], 0.6 }, { combo[6], 1.0 }
		},
		{ // 3. Repeated kicks, jump, and landing
			{ rightLegTuckedFootDown(), 0.1 },
			{ rightLegExtendedKick(), 0.2 },
			{ rightLegTuckedFootDown(), 0.1 },
			{ rightLegHorizontal(), 0.5 },
			{ rightLegStompAndLeftKneel(), 0.2, L"bored_to_play_2.wav", 0x2u },
			{ action(0.0f, 0.0f, 0.0f, -0.35f, 1.48f, -0.85f, 0.0f, 3.0f, 0.0f, 0.0f, -0.05f), 0.8 },
			{ action(0.0f, 0.0f, 0.0f, -0.30f, 1.78f, -0.55f, 0.0f, 3.0f, 0.0f, 0.0f, -0.03f), 0.1,
				L"bored_to_play_3.wav", 0x4u },
			{ rightLegStompAndLeftKneel(), 0.1 },
			{ rightLegStompAndLeftKneel(), 1.0 }
		},
		{ // 4. Reuse tuck / kick / tuck / horizontal kick poses
			{ rightLegTuckedFootDown(), 0.2 },
			{ rightLegExtendedKick(), 0.2 },
			{ rightLegTuckedFootDown(), 0.2 },
			{ rightLegHorizontal(), 0.5 }
		},
		{ // 5. Reuse the stomp setup, then dash with a horizontal kick
			{ rightLegRaisedToFace(), 0.5 },
			{ rightLegStompAndLeftKneel(), 0.5 },
			{ rightLegTuckedFootDown(), 0.5 },
			{ rightLegHorizontal(), 1.0 }
		},
		{ // 6. Reuse the sword combo, then transition and throw at the 3_10 pose
			{ combo[0], 0.1 }, { combo[1], 0.5 }, { combo[2], 0.5 }, { combo[3], 0.5 },
			{ combo[4], 0.6 }, { combo[5], 0.6 }, { combo[6], 1.2 }, { swordThrowPose(), 0.35 }
		}
	}};
	return frames;
}

const SkillActionPose& laevateinnReadyPose() {
	static const SkillActionPose pose = action(-0.28f, 0.72f, 0.12f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.12f, -0.08f);
	return pose;
}

const SkillActionPose& laevateinnHighKickPose() {
	static const SkillActionPose pose = action(0.18f, 0.58f, 0.0f, -0.08f, 2.22f,
		-0.55f, 0.0f, 0.0f, 0.0f, 0.16f, -0.06f);
	return pose;
}

const SkillActionPose& laevateinnVerticalKickPose() {
	static const SkillActionPose pose = action(0.24f, 0.54f, 0.0f, 0.0f, 2.50f,
		0.0f, -1.0f, 0.0f, 0.0f, 0.12f, -0.04f);
	return pose;
}

const SkillActionPose& laevateinnHorizontalSlashPose() {
	static const SkillActionPose pose = action(-1.0f, 0.32f, 0.30f, -0.18f, 0.26f,
		0.0f, 0.10f, 0.0f, 0.0f, -0.88f, 0.18f);
	return pose;
}

const SkillActionPose& laevateinnUpSlashPose() {
	static const SkillActionPose pose = action(1.68f, 0.62f, 0.10f, -0.14f, 0.24f,
		-0.15f, 0.12f, 0.0f, 0.0f, 0.42f, 0.12f);
	return pose;
}

const SkillActionPose& laevateinnStabPose() {
	static const SkillActionPose pose = action(-0.76f, -0.58f, 0.26f, -0.08f, 0.16f,
		-0.12f, 0.08f, 0.0f, 0.12f, 0.0f, 0.10f);
	return pose;
}

const SkillActionPose& laevateinnJumpPrepPose() {
	static const SkillActionPose pose = action(-0.08f, 1.05f, 0.12f, -0.20f, 0.38f,
		-0.30f, 0.10f, 0.18f, 0.0f, 0.15f, -0.12f);
	return pose;
}

const SkillActionPose& laevateinnKickToSwordPose() {
	static const SkillActionPose pose = action(0.12f, 0.62f, 0.0f, -0.12f, 2.35f,
		-0.35f, -0.10f, 0.0f, 0.0f, 0.20f, -0.08f);
	return pose;
}

const SkillActionPose& laevateinnKneeBendPose() {
	static const SkillActionPose pose = action(0.24f, 0.76f, 0.0f, -0.12f, 1.95f,
		-0.20f, -0.75f, 0.0f, 0.0f, 0.12f, 0.18f);
	return pose;
}

const SkillActionPose& laevateinnSwordPullPose() {
	static const SkillActionPose pose = action(-0.35f, -0.25f, 0.12f, 0.10f, 1.50f,
		-0.15f, -0.85f, 0.0f, 0.0f, -0.30f, 0.10f);
	return pose;
}

const SkillActionPose& laevateinnSkill6WindupPose() {
	static const SkillActionPose pose = action(2.35f, -0.28f, -0.10f, -0.06f, 0.12f,
		0.0f, 0.05f, 0.0f, 0.0f, 0.34f, 0.18f);
	return pose;
}

const SkillActionPose& laevateinnSkill6SlashPose() {
	static const SkillActionPose pose = action(-1.12f, 0.76f, 0.20f, -0.18f, 0.22f,
		0.0f, 0.08f, 0.0f, 0.0f, -1.05f, 0.22f);
	return pose;
}

const std::array<std::vector<SkillFrame>, 6>& laevateinnSkillFrames() {
	static const auto& combo = swordComboPoses();
	static const std::array<std::vector<SkillFrame>, 6> frames = [] {
		std::array<std::vector<SkillFrame>, 6> result{};
		result[0] = { // 1. Lower, high kick, raise the sword, dash and cut down.
			{ laevateinnReadyPose(), 0.2 }, { laevateinnHighKickPose(), 1.0 },
			{ combo[1], 0.4 }, { combo[2], 0.45 }
		};
		result[1] = { // 2. Two high kicks, one vertical kick, then two three-stab batches and retreat.
			{ rightLegTuckedFootDown(), 0.1 }, { laevateinnHighKickPose(), 0.1 },
			{ rightLegTuckedFootDown(), 0.1 }, { laevateinnHighKickPose(), 0.1 },
			{ rightLegTuckedFootDown(), 0.1 }, { laevateinnVerticalKickPose(), 0.1 },
			{ SkillActionPose{}, 0.1 }
		};
		for (int batch = 0; batch < 2; ++batch) {
			result[1].push_back({ laevateinnHorizontalSlashPose(), 0.1 }); // skill2_7
			result[1].push_back({ laevateinnStabPose(), 0.1 }); // skill2_15
			result[1].push_back({ laevateinnStabPose(), 0.1, batch == 0 ? L"is_hot_1.wav" : nullptr,
				batch == 0 ? 0x2u : 0u }); // plant the sword
			for (int stab = 0; stab < 3; ++stab) {
				result[1].push_back({ laevateinnHorizontalSlashPose(), 0.1 });
				result[1].push_back({ laevateinnStabPose(), 0.1 });
			}
			result[1].push_back({ SkillActionPose{}, 0.1 });
		}
		result[1].push_back({ action(1.20f, 0.78f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f,
			0.0f, -0.30f, 0.10f, -0.05f), 0.25, L"is_hot_2.wav", 0x4u }); // withdraw with the sword

		result[2] = { // 3. Down, right-to-left, up, heavy down, throw, leap-kick and retrieve.
			{ combo[1], 0.12 }, { combo[2], 0.12 }, { combo[2], 0.2 },
			{ laevateinnHorizontalSlashPose(), 0.12 }, { laevateinnHorizontalSlashPose(), 0.3 },
			{ laevateinnUpSlashPose(), 0.12 }, { laevateinnUpSlashPose(), 0.3 },
			{ combo[1], 0.12 }, { swordThrowPose(), 0.12 }, { combo[2], 0.12 }, { combo[2], 0.4 },
			{ swordThrowPose(), 0.12 }, { laevateinnHorizontalSlashPose(), 0.12 },
			{ laevateinnJumpPrepPose(), 1.0 },
			{ laevateinnKickToSwordPose(), 1.0 }, { laevateinnKneeBendPose(), 1.2,
				L"bulls_eye_1.wav", 0x2u },
			{ laevateinnSwordPullPose(), 2.0 }
		};
		result[4] = { // 5. Downward strike, then a dashing horizontal cut.
			{ combo[1], 0.12 }, { combo[2], 0.12, L"come_on_2.wav", 0x2u }, { combo[2], 0.5 },
			{ laevateinnHorizontalSlashPose(), 0.6 }
		};
		return result;
	}();
	return frames;
}

glm::vec3 rotateX(const glm::vec3& vector, float angle) {
	const float c = std::cos(angle), s = std::sin(angle);
	return { vector.x, vector.y * c - vector.z * s, vector.y * s + vector.z * c };
}

glm::vec3 rotateZ(const glm::vec3& vector, float angle) {
	const float c = std::cos(angle), s = std::sin(angle);
	return { vector.x * c - vector.y * s, vector.x * s + vector.y * c, vector.z };
}

glm::vec3 rotateY(const glm::vec3& vector, float angle) {
	const float c = std::cos(angle), s = std::sin(angle);
	return { vector.x * c + vector.z * s, vector.y, -vector.x * s + vector.z * c };
}

SkillActionPose mixPose(const SkillActionPose& a, const SkillActionPose& b, float t) {
	return {
		a.armLift + (b.armLift - a.armLift) * t,
		a.armReach + (b.armReach - a.armReach) * t,
		a.elbowBend + (b.elbowBend - a.elbowBend) * t,
		a.armYaw + (b.armYaw - a.armYaw) * t,
		a.torsoRotationX + (b.torsoRotationX - a.torsoRotationX) * t,
		a.torsoRotationY + (b.torsoRotationY - a.torsoRotationY) * t,
		a.torsoRotationZ + (b.torsoRotationZ - a.torsoRotationZ) * t,
		a.leftLegPitch + (b.leftLegPitch - a.leftLegPitch) * t,
		a.rightLegPitch + (b.rightLegPitch - a.rightLegPitch) * t,
		a.leftKneeFlex + (b.leftKneeFlex - a.leftKneeFlex) * t,
		a.rightKneeFlex + (b.rightKneeFlex - a.rightKneeFlex) * t,
		a.rootY + (b.rootY - a.rootY) * t,
		a.forward + (b.forward - a.forward) * t
	};
}

double frameSequenceDuration(const std::vector<SkillFrame>& frames) {
	double total = 0.0;
	for (const SkillFrame& frame : frames) total += frame.duration;
	return total;
}

constexpr double swordPullbackTransitionSeconds = 0.35;
constexpr double swordStrainSeconds = 1.8;
constexpr double swordReleasePoseTransitionSeconds = 0.25;
constexpr float swordStrainArmRotation = 0.18f;
constexpr double swordStrainOscillations = 3.0;
constexpr double laevateinnSkill6HoldBeforeSlash = 2.0;
constexpr double laevateinnSkill6PostSlashWait = 5.0;

double unsealedSwordReleaseTime() {
	return frameSequenceDuration(sealedSkillFrames()[1])
		+ swordPullbackTransitionSeconds + swordStrainSeconds + swordReleasePoseTransitionSeconds;
}

double enemyHitTime(int skillIndex, int swordStage, double approachDuration) {
	if (swordStage == 3) {
		switch (skillIndex) {
		case 0: return 1.6; // downward dash slash
		case 1: return 0.5; // first stab
		case 2: return 0.2; // opening downward cut
		case 4: return 0.2; // downward strike
		case 5: return approachDuration + laevateinnSkill6HoldBeforeSlash;
		default: return 0.0;
		}
	}
	switch (skillIndex) {
	case 0: return 0.5; // stomp impact
	case 1: return 0.6; // first sword slash
	case 2: return 0.1; // first kick
	case 3: return 0.2; // kick impact
	case 4: return 1.5; // forward dash kick
	case 5: return swordStage == 0 ? 4.0 : unsealedSwordReleaseTime();
	default: return 0.0;
	}
}

const SkillActionPose& unsealedSwordPullbackPose() {
	static const SkillActionPose pose = action(
		1.57f, -1.57f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.32f);
	return pose;
}

SkillActionPose poseAtFrames(const std::vector<SkillFrame>& frames, double elapsed) {
	const SkillActionPose neutral{};
	double cursor = 0.0;
	SkillActionPose previous = neutral;
	for (const SkillFrame& frame : frames) {
		if (elapsed < cursor + frame.duration) {
			const double transition = std::min(0.14, frame.duration * 0.55);
			const float t = transition > 0.0
				? static_cast<float>(std::clamp((elapsed - cursor) / transition, 0.0, 1.0)) : 1.0f;
			const float smooth = t * t * (3.0f - 2.0f * t);
			return mixPose(previous, frame.pose, smooth);
		}
		cursor += frame.duration;
		previous = frame.pose;
	}
	const float t = static_cast<float>(std::clamp((elapsed - cursor) / 0.16, 0.0, 1.0));
	return mixPose(previous, neutral, t * t * (3.0f - 2.0f * t));
}

SkillActionPose laevateinnSkill6Pose(double elapsed, double approachDuration) {
	const SkillActionPose walkPose = action(0.12f, 0.32f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.02f);
	const double slashTime = approachDuration + laevateinnSkill6HoldBeforeSlash;
	if (elapsed < approachDuration) {
		SkillActionPose pose = walkPose;
		const float stride = std::sin(static_cast<float>(elapsed * 10.6814));
		pose.leftLegPitch = stride * 0.42f;
		pose.rightLegPitch = -stride * 0.42f;
		pose.leftKneeFlex = -std::max(0.0f, -stride) * 0.55f;
		pose.rightKneeFlex = -std::max(0.0f, stride) * 0.55f;
		return pose;
	}
	if (elapsed < approachDuration + 0.25) {
		const float t = static_cast<float>(std::clamp((elapsed - approachDuration) / 0.25, 0.0, 1.0));
		return mixPose(walkPose, laevateinnSkill6WindupPose(), t * t * (3.0f - 2.0f * t));
	}
	if (elapsed < slashTime) return laevateinnSkill6WindupPose();
	if (elapsed < slashTime + 0.3) {
		const float t = static_cast<float>(std::clamp((elapsed - slashTime) / 0.3, 0.0, 1.0));
		return mixPose(laevateinnSkill6WindupPose(), laevateinnSkill6SlashPose(), t * t * (3.0f - 2.0f * t));
	}
	if (elapsed < slashTime + 0.6) {
		const float t = static_cast<float>(std::clamp((elapsed - slashTime - 0.3) / 0.3, 0.0, 1.0));
		return mixPose(laevateinnSkill6SlashPose(), SkillActionPose{}, t * t * (3.0f - 2.0f * t));
	}
	return {};
}

SkillActionPose poseAt(int skillIndex, double elapsed, int swordStageAtStart, double approachDuration) {
	if (skillIndex < 0 || skillIndex >= 6) return {};
	if (swordStageAtStart == 3) {
		if (skillIndex == 5) return laevateinnSkill6Pose(elapsed, approachDuration);
		return poseAtFrames(laevateinnSkillFrames()[skillIndex], elapsed);
	}
	if (skillIndex != 5 || swordStageAtStart < 1 || swordStageAtStart > 2) {
		return poseAtFrames(sealedSkillFrames()[skillIndex], elapsed);
	}

	const auto& comboFrames = sealedSkillFrames()[1];
	const auto& comboPoses = swordComboPoses();
	const double comboEnd = frameSequenceDuration(comboFrames);
	const double pullbackEnd = comboEnd + swordPullbackTransitionSeconds;
	const double strainEnd = pullbackEnd + swordStrainSeconds;
	const double releaseTime = strainEnd + swordReleasePoseTransitionSeconds;
	if (elapsed < comboEnd) return poseAtFrames(comboFrames, elapsed);
	if (elapsed < pullbackEnd) {
		const float t = static_cast<float>(std::clamp(
			(elapsed - comboEnd) / swordPullbackTransitionSeconds, 0.0, 1.0));
		return mixPose(comboPoses[6], unsealedSwordPullbackPose(), t * t * (3.0f - 2.0f * t));
	}
	if (elapsed < strainEnd) {
		SkillActionPose pose = unsealedSwordPullbackPose();
		const double progress = (elapsed - pullbackEnd) / swordStrainSeconds;
		const float oscillation = static_cast<float>(std::sin(
			progress * swordStrainOscillations * 6.283185307179586));
		// Swing the arm's X-axis reach angle back and forth while holding the sword behind.
		pose.armReach += swordStrainArmRotation * oscillation;
		return pose;
	}
	if (elapsed < releaseTime) {
		const float t = static_cast<float>(std::clamp(
			(elapsed - strainEnd) / swordReleasePoseTransitionSeconds, 0.0, 1.0));
		return mixPose(unsealedSwordPullbackPose(), comboPoses[6], t * t * (3.0f - 2.0f * t));
	}
	return comboPoses[6];
}

CharacterPose applyPose(const CharacterPose& base, const SkillActionPose& skillPose) {
	CharacterPose pose = base;
	const glm::vec3 upperArm = rotateY(rotateX(rotateZ(base.leftElbow - base.leftShoulder,
		skillPose.armLift), skillPose.armReach), skillPose.armYaw);
	const glm::vec3 lowerArm = rotateY(rotateX(rotateZ(base.leftHandTip - base.leftElbow,
		skillPose.armLift + skillPose.elbowBend), skillPose.armReach), skillPose.armYaw);
	pose.leftElbow = base.leftShoulder + upperArm;
	pose.leftHandTip = pose.leftElbow + lowerArm;
	pose.leftKnee = base.leftHip + rotateX(base.leftKnee - base.leftHip, -skillPose.leftLegPitch);
	pose.leftFootBase = pose.leftKnee + rotateX(base.leftFootBase - base.leftKnee,
		-(skillPose.leftLegPitch + skillPose.leftKneeFlex));
	pose.rightKnee = base.rightHip + rotateX(base.rightKnee - base.rightHip, -skillPose.rightLegPitch);
	pose.rightFootBase = pose.rightKnee + rotateX(base.rightFootBase - base.rightKnee,
		-(skillPose.rightLegPitch + skillPose.rightKneeFlex));
	const glm::vec3 forward(std::sin(base.rootYaw), 0.0f, std::cos(base.rootYaw));
	pose.rootPosition += glm::vec3(0.0f, skillPose.rootY, 0.0f) + forward * skillPose.forward;
	pose.torsoRotationX = base.torsoRotationX + skillPose.torsoRotationX;
	pose.torsoRotationY = base.torsoRotationY + skillPose.torsoRotationY;
	pose.torsoRotationZ = base.torsoRotationZ + skillPose.torsoRotationZ;
	return pose;
}

glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point) {
	return glm::vec3(transform * glm::vec4(point, 1.0f));
}

glm::mat4 alignPositiveY(const glm::vec3& direction) {
	const glm::vec3 source(0.0f, 1.0f, 0.0f);
	const glm::vec3 target = glm::normalize(direction);
	const float cosine = std::clamp(glm::dot(source, target), -1.0f, 1.0f);
	if (cosine > 0.999999f) return glm::mat4(1.0f);
	if (cosine < -0.999999f) return glm::rotate(glm::mat4(1.0f), 3.14159265f, glm::vec3(1.0f, 0.0f, 0.0f));
	return glm::rotate(glm::mat4(1.0f), std::acos(cosine), glm::normalize(glm::cross(source, target)));
}
}

double skillAnimationSeconds(int skillIndex, int swordStage) {
	if (skillIndex < 0 || skillIndex >= 6) return 0.0;
	if (swordStage == 3) {
		if (skillIndex == 5) return laevateinnSkill6HoldBeforeSlash
			+ 0.6 + laevateinnSkill6PostSlashWait + 0.16;
		return 0.16 + frameSequenceDuration(laevateinnSkillFrames()[skillIndex]);
	}
	if (skillIndex == 5 && swordStage >= 1 && swordStage <= 2) {
		return unsealedSwordReleaseTime() + 0.16;
	}
	double total = 0.16; // blend back to the idle pose after the final reference frame
	return total + frameSequenceDuration(sealedSkillFrames()[skillIndex]);
}

bool skillAnimationActive(const App& app) {
	return app.skillAnimation.skillIndex >= 0;
}

CharacterPose animatedSkillPose(const App& app) {
	if (!skillAnimationActive(app)) return app.characterPose;
	return applyPose(app.characterPose, poseAt(app.skillAnimation.skillIndex,
		app.skillAnimation.elapsed, app.skillAnimation.swordStageAtStart, app.skillAnimation.lungeDuration));
}

void updateSkillAnimation(App& app, float deltaTime, double now) {
	if (!skillAnimationActive(app) || deltaTime <= 0.0f) return;
	SkillAnimationState& animation = app.skillAnimation;
	const int skillIndex = animation.skillIndex;
	const double previousElapsed = animation.elapsed;
	animation.elapsed = std::min(animation.duration, animation.elapsed + static_cast<double>(deltaTime));
	if (animation.lungeDistance > 0.0f && animation.lungeDuration > 0.0) {
		const auto smoothProgress = [](double value) {
			const float t = static_cast<float>(std::clamp(value, 0.0, 1.0));
			return t * t * (3.0f - 2.0f * t);
		};
		const double previousProgress = smoothProgress((previousElapsed - animation.lungeStart) / animation.lungeDuration);
		const double currentProgress = smoothProgress((animation.elapsed - animation.lungeStart) / animation.lungeDuration);
		const float stepDistance = animation.lungeDistance
			* static_cast<float>(currentProgress - previousProgress);
		if (stepDistance > 0.0f) {
			glm::vec3 nextPosition = app.characterPose.rootPosition + animation.lungeDirection * stepDistance;
			const float fieldLimit = FIELD_HALF_EXTENT - CHARACTER_FIELD_MARGIN;
			nextPosition.x = std::clamp(nextPosition.x, -fieldLimit, fieldLimit);
			nextPosition.z = std::clamp(nextPosition.z, -fieldLimit, fieldLimit);
			app.characterPose.rootPosition = nextPosition;
		}
	}
	if (animation.enemyHitPending && !animation.enemyHitResolved
		&& animation.elapsed >= enemyHitTime(skillIndex, animation.swordStageAtStart, animation.lungeDuration)) {
		const glm::vec2 toEnemy(app.enemyPosition.x - app.characterPose.rootPosition.x,
			app.enemyPosition.z - app.characterPose.rootPosition.z);
		if (app.enemyModel.loaded() && glm::dot(toEnemy, toEnemy)
			<= ENEMY_SKILL_HIT_RADIUS * ENEMY_SKILL_HIT_RADIUS) {
			const glm::vec2 toPlayer = -toEnemy;
			app.enemyYaw = std::atan2(toPlayer.x, toPlayer.y);
			app.enemyYawSet = true;
			app.enemyHasBeenHit = true;
			animation.enemyHitResolved = true;
		}
	}
	const std::vector<SkillFrame>& actionFrames = animation.swordStageAtStart == 3
		? laevateinnSkillFrames()[skillIndex] : sealedSkillFrames()[skillIndex];
	double actionStart = 0.0;
	for (const SkillFrame& frame : actionFrames) {
		if (frame.voiceOnEnter) {
			const bool enteringAtStart = actionStart <= 0.0 && previousElapsed <= 0.0 && animation.elapsed > 0.0;
			const bool crossedActionStart = previousElapsed < actionStart && animation.elapsed >= actionStart;
			if ((enteringAtStart || crossedActionStart)
				&& (animation.voiceCueMask & frame.voiceCueBit) == 0) {
				animation.voiceCueMask |= frame.voiceCueBit;
				playSkillVoice(app, frame.voiceOnEnter, now);
			}
		}
		actionStart += frame.duration;
	}
	if (animation.swordStageAtStart == 3 && skillIndex == 5) {
		const auto crossedActionStart = [previousElapsed, &animation](double start) {
			return (previousElapsed < start && animation.elapsed >= start)
				|| (start <= 0.0 && previousElapsed <= 0.0 && animation.elapsed > 0.0);
		};
		// These cues are bound to the transition into the wind-up and horizontal-cut poses.
		if (crossedActionStart(animation.lungeDuration) && (animation.voiceCueMask & 0x1u) == 0) {
			animation.voiceCueMask |= 0x1u;
			playSkillVoice(app, L"kidslash1.wav", now);
		}
		if (crossedActionStart(animation.lungeDuration + laevateinnSkill6HoldBeforeSlash)
			&& (animation.voiceCueMask & 0x2u) == 0) {
			animation.voiceCueMask |= 0x2u;
			playSkillVoice(app, L"kidslash2.wav", now);
		}
	}

	const auto applyDash = [&app, &animation, previousElapsed](double start, double duration, float distance) {
		const double previousProgress = std::clamp((previousElapsed - start) / duration, 0.0, 1.0);
		const double currentProgress = std::clamp((animation.elapsed - start) / duration, 0.0, 1.0);
		const float step = distance * static_cast<float>(currentProgress - previousProgress);
		if (step <= 0.0f) return;
		const glm::vec3 forward(std::sin(app.characterPose.rootYaw), 0.0f, std::cos(app.characterPose.rootYaw));
		glm::vec3 nextPosition = app.characterPose.rootPosition + forward * step;
		const float fieldLimit = FIELD_HALF_EXTENT - CHARACTER_FIELD_MARGIN;
		nextPosition.x = std::clamp(nextPosition.x, -fieldLimit, fieldLimit);
		nextPosition.z = std::clamp(nextPosition.z, -fieldLimit, fieldLimit);
		app.characterPose.rootPosition = nextPosition;
	};
	if (animation.swordStageAtStart == 3 && skillIndex == 0) {
		applyDash(1.6, 0.45, 6.0f);
	}
	else if (animation.swordStageAtStart == 3 && skillIndex == 4) {
		applyDash(LAEVATEINN_SKILL5_DASH_START, 0.6, 6.0f);
	}
	else if (animation.swordStageAtStart != 3 && skillIndex == 4) {
		// Skill 5's final horizontal kick is a one-second forward dash, not a pose offset.
		applyDash(1.5, 1.0, 10.0f);
	}

	const bool unsealSkill = skillIndex == 5 && animation.swordStageAtStart >= 0
		&& animation.swordStageAtStart <= 2;
	const bool laevateinnThrowSkill = skillIndex == 2 && animation.swordStageAtStart == 3;
	const double swordReleaseTime = laevateinnThrowSkill ? LAEVATEINN_SKILL3_THROW_TIME
		: (animation.swordStageAtStart == 0 ? 4.0 : unsealedSwordReleaseTime());
	if ((unsealSkill || laevateinnThrowSkill) && !animation.swordThrown
		&& previousElapsed < swordReleaseTime && animation.elapsed >= swordReleaseTime) {
		const CharacterPose releasePose = applyPose(app.characterPose,
			poseAt(skillIndex, swordReleaseTime, animation.swordStageAtStart, animation.lungeDuration));
		const glm::vec3 torsoPivot = (releasePose.leftHip + releasePose.rightHip) * 0.5f;
		const glm::mat4 root = glm::translate(glm::mat4(1.0f), releasePose.rootPosition)
			* glm::rotate(glm::mat4(1.0f), releasePose.rootYaw, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::translate(glm::mat4(1.0f), torsoPivot)
			* glm::rotate(glm::mat4(1.0f), releasePose.torsoRotationZ, glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::rotate(glm::mat4(1.0f), releasePose.torsoRotationY, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::rotate(glm::mat4(1.0f), releasePose.torsoRotationX, glm::vec3(1.0f, 0.0f, 0.0f))
			* glm::translate(glm::mat4(1.0f), -torsoPivot);
		const glm::vec3 spawnPoint = laevateinnThrowSkill
			? releasePose.leftHandTip
			: releasePose.rootPosition + glm::vec3(-0.1f, 0.0f, 3.0f);
		animation.thrownSwordStart = transformPoint(root, spawnPoint);
		float flightTime = 0.0f;
		animation.thrownSwordSpinTurns = 0.0f;
		animation.thrownSwordPlanted = false;
		if (laevateinnThrowSkill) {
			animation.thrownSwordTarget = app.enemyPosition;
			animation.thrownSwordTarget.y = FIELD_Y + 1.35f;
			flightTime = LAEVATEINN_SKILL3_FLIGHT_TIME;
			const glm::vec3 displacement = animation.thrownSwordTarget - animation.thrownSwordStart;
			animation.thrownSwordVelocity = displacement / static_cast<float>(flightTime);
			animation.thrownSwordSpinTurns = 3.0f;
			animation.thrownSwordStage = 3;
		}
		else if (animation.swordStageAtStart == 0) {
			std::random_device randomDevice;
			std::mt19937 generator(randomDevice());
			std::uniform_real_distribution<float> angleDistribution(-3.14159265f, 3.14159265f);
			const float angle = releasePose.rootYaw + angleDistribution(generator);
			const glm::vec3 horizontal(std::sin(angle), 0.0f, std::cos(angle));
			animation.thrownSwordVelocity = horizontal * 3.0f + glm::vec3(0.0f, 3.5f, 0.0f);
			const float dropHeight = std::max(0.0f, animation.thrownSwordStart.y - (FIELD_Y + 1.35f));
			flightTime = (animation.thrownSwordVelocity.y
				+ std::sqrt(animation.thrownSwordVelocity.y * animation.thrownSwordVelocity.y
					+ 2.0f * thrownSwordGravity * dropHeight)) / thrownSwordGravity;
		}
		else {
			const glm::vec3 forward(std::sin(releasePose.rootYaw), 0.0f, std::cos(releasePose.rootYaw));
			animation.thrownSwordVelocity = forward * unsealedSwordForwardSpeed;
			flightTime = unsealedSwordFlightDistance / unsealedSwordForwardSpeed;
		}
		if (!laevateinnThrowSkill) animation.thrownSwordStage = animation.swordStageAtStart;
		animation.thrownSwordDirection = glm::normalize(animation.thrownSwordVelocity);
		animation.thrownSwordElapsed = std::max(0.0, animation.elapsed - swordReleaseTime);
		animation.thrownSwordFlightDuration = flightTime;
		animation.thrownSwordVisible = true;
		animation.swordThrown = true;
		if (unsealSkill) {
			// Throw the previous stage's blade as the equipped sword advances one form.
			app.swordStage = std::min(animation.swordStageAtStart + 1, 3);
			animation.duration = swordReleaseTime + flightTime + 0.16;
		}
	}
	else if (animation.swordThrown && animation.thrownSwordVisible) {
		animation.thrownSwordElapsed += static_cast<double>(deltaTime);
	}

	if (animation.swordThrown && animation.thrownSwordVisible) {
		const float gravity = animation.thrownSwordStage == 0 ? thrownSwordGravity : 0.0f;
		const float t = static_cast<float>(animation.thrownSwordElapsed);
		const glm::vec3 currentVelocity = animation.thrownSwordVelocity + glm::vec3(0.0f, -gravity * t, 0.0f);
		animation.thrownSwordDirection = glm::normalize(currentVelocity);
		const glm::vec3 position = animation.thrownSwordStart + animation.thrownSwordVelocity * t
			+ glm::vec3(0.0f, -0.5f * gravity * t * t, 0.0f);
		if (animation.swordStageAtStart == 3 && skillIndex == 2) {
			if (animation.thrownSwordElapsed >= animation.thrownSwordFlightDuration) {
				animation.thrownSwordPlanted = true;
				animation.thrownSwordDirection = glm::vec3(0.0f, -1.0f, 0.0f);
			}
			if (animation.elapsed >= LAEVATEINN_SKILL3_RETRIEVAL_TIME) animation.thrownSwordVisible = false;
		}
		else if (animation.thrownSwordStage == 0) {
			if (position.y <= FIELD_Y + 1.35f) animation.thrownSwordVisible = false;
		}
		else if (animation.thrownSwordElapsed >= animation.thrownSwordFlightDuration) {
			animation.thrownSwordVisible = false;
		}
	}
	if (animation.elapsed >= animation.duration) app.skillAnimation = {};
}

glm::mat4 thrownSkillSwordTransform(const App& app) {
	const SkillAnimationState& animation = app.skillAnimation;
	if (animation.thrownSwordPlanted) {
		const glm::mat4 vertical = alignPositiveY(glm::vec3(0.0f, -1.0f, 0.0f));
		return glm::translate(glm::mat4(1.0f), animation.thrownSwordTarget) * vertical;
	}
	const float t = static_cast<float>(animation.thrownSwordElapsed);
	const float gravity = animation.thrownSwordStage == 0 ? thrownSwordGravity : 0.0f;
	const glm::vec3 position = animation.thrownSwordStart + animation.thrownSwordVelocity * t
		+ glm::vec3(0.0f, -0.5f * gravity * t * t, 0.0f);
	glm::mat4 rotation = alignPositiveY(animation.thrownSwordDirection);
	if (animation.thrownSwordSpinTurns > 0.0f && animation.thrownSwordFlightDuration > 0.0) {
		const float progress = std::clamp(t / static_cast<float>(animation.thrownSwordFlightDuration), 0.0f, 1.0f);
		rotation *= glm::rotate(glm::mat4(1.0f), progress * animation.thrownSwordSpinTurns * 6.28318530718f,
			glm::vec3(0.0f, 1.0f, 0.0f));
	}
	return glm::translate(glm::mat4(1.0f), position) * rotation;
}
