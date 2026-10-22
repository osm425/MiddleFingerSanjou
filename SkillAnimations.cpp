#include "GameState.h"
#include <random>

namespace {
struct SkillActionPose {
	float armLift = 0.0f;
	float armReach = 0.0f;
	float elbowBend = 0.0f;
	float armYaw = 0.0f;
	float torsoLean = 0.0f;
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
};

SkillActionPose action(float armLift = 0.0f, float armReach = 0.0f, float elbowBend = 0.0f,
	float leftLegPitch = 0.0f, float rightLegPitch = 0.0f, float leftKneeFlex = 0.0f,
	float rightKneeFlex = 0.0f, float rootY = 0.0f, float forward = 0.0f,
	float armYaw = 0.0f, float torsoLean = 0.0f) {
	return { armLift, armReach, elbowBend, armYaw, torsoLean, leftLegPitch, rightLegPitch,
		leftKneeFlex, rightKneeFlex, rootY, forward };
}

// Shared leg poses keep the repeated kick sequences consistent across skills.
const SkillActionPose& rightLegRaisedToFace() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, 0.0f, 2.50f, 0.0f, -1.00f);
	return pose;
}

const SkillActionPose& rightLegStompAndLeftKneel() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, -0.60f, 0.60f, -0.60f, -0.40f, -0.2f, 0.0f, 0.0f, 0.18f);
	return pose;
}

const SkillActionPose& rightLegTuckedFootDown() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, 0.0f, 1.40f, 0.0f, -1.40f);
	return pose;
}

const SkillActionPose& rightLegExtendedKick() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, -0.08f, 2.02f, -0.55f, 0.0f, 0.0f, 0.0f, 0.0f, -0.06f);
	return pose;
}

const SkillActionPose& rightLegHorizontal() {
	static const SkillActionPose pose = action(0.0f, 0.0f, 0.0f, 0.12f, 1.52f, -0.80f, 0.0f, 0.0f, 0.0f, 0.0f, -0.03f);
	return pose;
}

const std::array<SkillActionPose, 7>& swordComboPoses() {
	static const std::array<SkillActionPose, 7> poses = {{
		action(0.35f, 1.15f, -0.20f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.10f, -0.04f), // draw from back
		action(2.18f, 0.08f, -0.12f, -0.22f, 0.38f, 0.0f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f), // raise overhead and brace
		action(-0.62f, -0.90f, 0.25f, 0.18f, 0.32f, 0.30f, 0.10f, 0.0f, 0.08f, -0.18f, 0.34f), // downward cut and lean
		action(-0.28f, -0.52f, 1.05f, 0.12f, 0.22f, 0.25f, 0.10f, 0.0f, 0.0f, 0.82f, 0.30f), // fold arm inward
		action(0.18f, -0.62f, 0.10f, -0.20f, 0.28f, 0.0f, 0.12f, 0.0f, 0.0f, -1.20f, -0.05f), // sweep left and straighten
		action(-0.22f, 1.05f, 0.18f, 0.16f, 0.24f, 0.22f, 0.0f, 0.0f, 0.05f, -0.22f, 0.32f), // lean and draw back
		action(2.12f, -0.22f, -0.16f, -0.18f, 0.30f, 0.0f, 0.12f, 0.0f, 0.0f, 0.22f, -0.08f) // rising cut
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
			{ rightLegStompAndLeftKneel(), 0.2 },
			{ action(0.0f, 0.0f, 0.0f, -0.35f, 1.48f, -0.85f, 0.0f, 0.72f, 0.0f, 0.0f, -0.05f), 0.8 },
			{ action(0.0f, 0.0f, 0.0f, -0.30f, 1.78f, -0.55f, 0.0f, 0.72f, 0.0f, 0.0f, -0.03f), 0.1 },
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
		a.torsoLean + (b.torsoLean - a.torsoLean) * t,
		a.leftLegPitch + (b.leftLegPitch - a.leftLegPitch) * t,
		a.rightLegPitch + (b.rightLegPitch - a.rightLegPitch) * t,
		a.leftKneeFlex + (b.leftKneeFlex - a.leftKneeFlex) * t,
		a.rightKneeFlex + (b.rightKneeFlex - a.rightKneeFlex) * t,
		a.rootY + (b.rootY - a.rootY) * t,
		a.forward + (b.forward - a.forward) * t
	};
}

SkillActionPose poseAt(int skillIndex, double elapsed) {
	const auto& frames = sealedSkillFrames()[skillIndex];
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
	pose.rootPitch = base.rootPitch + skillPose.torsoLean;
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

double skillAnimationSeconds(int skillIndex) {
	if (skillIndex < 0 || skillIndex >= 6) return 0.0;
	double total = 0.16; // blend back to the idle pose after the final reference frame
	for (const SkillFrame& frame : sealedSkillFrames()[skillIndex]) total += frame.duration;
	return total;
}

bool skillAnimationActive(const App& app) {
	return app.skillAnimation.skillIndex >= 0;
}

CharacterPose animatedSkillPose(const App& app) {
	if (!skillAnimationActive(app)) return app.characterPose;
	return applyPose(app.characterPose, poseAt(app.skillAnimation.skillIndex, app.skillAnimation.elapsed));
}

void updateSkillAnimation(App& app, float deltaTime, double now) {
	if (!skillAnimationActive(app) || deltaTime <= 0.0f) return;
	SkillAnimationState& animation = app.skillAnimation;
	const int skillIndex = animation.skillIndex;
	const double previousElapsed = animation.elapsed;
	animation.elapsed = std::min(animation.duration, animation.elapsed + static_cast<double>(deltaTime));
	if (skillIndex == 4) {
		// Skill 5's final horizontal kick is a one-second forward dash, not a pose offset.
		constexpr double dashStart = 1.5;
		constexpr double dashDuration = 1.0;
		constexpr float dashDistance = 5.0f;
		const double previousProgress = std::clamp((previousElapsed - dashStart) / dashDuration, 0.0, 1.0);
		const double currentProgress = std::clamp((animation.elapsed - dashStart) / dashDuration, 0.0, 1.0);
		const float distance = dashDistance * static_cast<float>(currentProgress - previousProgress);
		if (distance > 0.0f) {
			const glm::vec3 forward(std::sin(app.characterPose.rootYaw), 0.0f,
				std::cos(app.characterPose.rootYaw));
			glm::vec3 nextPosition = app.characterPose.rootPosition + forward * distance;
			const float fieldLimit = FIELD_HALF_EXTENT - CHARACTER_FIELD_MARGIN;
			nextPosition.x = std::clamp(nextPosition.x, -fieldLimit, fieldLimit);
			nextPosition.z = std::clamp(nextPosition.z, -fieldLimit, fieldLimit);
			app.characterPose.rootPosition = nextPosition;
		}
	}

	if (skillIndex == 2) {
		if (animation.elapsed >= 0.9 && (animation.voiceCueMask & 0x2u) == 0) {
			animation.voiceCueMask |= 0x2u;
			playSkillVoice(app, L"bored_to_play_2.wav", now);
		}
		if (animation.elapsed >= 1.9 && (animation.voiceCueMask & 0x4u) == 0) {
			animation.voiceCueMask |= 0x4u;
			playSkillVoice(app, L"bored_to_play_3.wav", now);
		}
	}

	constexpr double swordReleaseTime = 4.0;
	if (skillIndex == 5 && !animation.swordThrown && previousElapsed < swordReleaseTime && animation.elapsed >= swordReleaseTime) {
		const CharacterPose releasePose = applyPose(app.characterPose, poseAt(skillIndex, animation.elapsed));
		const glm::mat4 root = glm::translate(glm::mat4(1.0f), releasePose.rootPosition)
			* glm::rotate(glm::mat4(1.0f), releasePose.rootYaw, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::rotate(glm::mat4(1.0f), releasePose.rootPitch, glm::vec3(1.0f, 0.0f, 0.0f));
		animation.thrownSwordStart = transformPoint(root, releasePose.leftHandTip);
		std::random_device randomDevice;
		std::mt19937 generator(randomDevice());
		std::uniform_real_distribution<float> angleDistribution(-3.14159265f, 3.14159265f);
		const float angle = releasePose.rootYaw + angleDistribution(generator);
		const glm::vec3 horizontal(std::sin(angle), 0.0f, std::cos(angle));
		animation.thrownSwordVelocity = horizontal * 3.0f + glm::vec3(0.0f, 3.5f, 0.0f);
		animation.thrownSwordDirection = glm::normalize(animation.thrownSwordVelocity);
		animation.thrownSwordElapsed = std::max(0.0, animation.elapsed - swordReleaseTime);
		animation.thrownSwordVisible = true;
		animation.swordThrown = true;
		// The previous sealed sword is airborne as the equipped model advances to stage 1.
		app.swordStage = 1;
		const float dropHeight = std::max(0.0f, animation.thrownSwordStart.y - (FIELD_Y + 1.35f));
		const float flightTime = (animation.thrownSwordVelocity.y
			+ std::sqrt(animation.thrownSwordVelocity.y * animation.thrownSwordVelocity.y + 2.0f * 9.8f * dropHeight)) / 9.8f;
		animation.duration = swordReleaseTime + flightTime + 0.16;
	}
	else if (animation.swordThrown && animation.thrownSwordVisible) {
		animation.thrownSwordElapsed += static_cast<double>(deltaTime);
	}

	if (animation.swordThrown && animation.thrownSwordVisible) {
		constexpr float gravity = 9.8f;
		const float t = static_cast<float>(animation.thrownSwordElapsed);
		const glm::vec3 currentVelocity = animation.thrownSwordVelocity + glm::vec3(0.0f, -gravity * t, 0.0f);
		animation.thrownSwordDirection = glm::normalize(currentVelocity);
		const glm::vec3 position = animation.thrownSwordStart + animation.thrownSwordVelocity * t
			+ glm::vec3(0.0f, -0.5f * gravity * t * t, 0.0f);
		if (position.y <= FIELD_Y + 1.35f) animation.thrownSwordVisible = false;
	}
	if (animation.elapsed >= animation.duration) app.skillAnimation = {};
}

glm::mat4 thrownSkillSwordTransform(const App& app) {
	const SkillAnimationState& animation = app.skillAnimation;
	const float t = static_cast<float>(animation.thrownSwordElapsed);
	constexpr float gravity = 9.8f;
	const glm::vec3 position = animation.thrownSwordStart + animation.thrownSwordVelocity * t
		+ glm::vec3(0.0f, -0.5f * gravity * t * t, 0.0f);
	const glm::mat4 rotation = alignPositiveY(animation.thrownSwordDirection);
	return glm::translate(glm::mat4(1.0f), position) * rotation;
}
