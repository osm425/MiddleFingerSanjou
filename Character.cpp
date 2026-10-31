#include "GameState.h"



static glm::mat4 alignPositiveY(const glm::vec3& direction) {
	const glm::vec3 sourceAxis(0.0f, 1.0f, 0.0f);
	const glm::vec3 targetAxis = glm::normalize(direction);
	const float cosine = std::clamp(glm::dot(sourceAxis, targetAxis), -1.0f, 1.0f);
	if (cosine > 0.999999f) return glm::mat4(1.0f);
	if (cosine < -0.999999f) return glm::rotate(glm::mat4(1.0f), 3.14159265f, glm::vec3(1.0f, 0.0f, 0.0f));
	const glm::vec3 axis = glm::normalize(glm::cross(sourceAxis, targetAxis));
	return glm::rotate(glm::mat4(1.0f), std::acos(cosine), axis);
}

static glm::mat4 boneTransform(const glm::vec3& upperJoint, const glm::vec3& lowerJoint,
	const glm::vec3& localUpperAnchor, const glm::vec3& localLowerAnchor,
	const glm::vec2& crossSectionScale, const glm::vec3& characterForward) {
	const glm::vec3 boneAxis = upperJoint - lowerJoint;
	const float worldLength = glm::length(boneAxis);
	const float localLength = glm::length(localUpperAnchor - localLowerAnchor);
	const glm::vec3 up = worldLength > 1e-6f ? boneAxis / worldLength : glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 forward = characterForward - up * glm::dot(characterForward, up);
	if (glm::length(forward) < 1e-6f) {
		const glm::vec3 fallback = std::abs(up.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
		forward = fallback - up * glm::dot(fallback, up);
	}
	forward = glm::normalize(forward);
	const glm::vec3 right = glm::normalize(glm::cross(up, forward));
	forward = glm::normalize(glm::cross(right, up));
	glm::mat4 rotation(1.0f);
	rotation[0] = glm::vec4(right, 0.0f);
	rotation[1] = glm::vec4(up, 0.0f);
	rotation[2] = glm::vec4(forward, 0.0f);

	const float safeLocalLength = std::max(localLength, 1e-6f);
	const glm::vec3 scale(crossSectionScale.x, std::max(worldLength, 1e-4f) / safeLocalLength, crossSectionScale.y);
	const glm::vec3 anchorOffset = glm::vec3(rotation * glm::vec4(localUpperAnchor * scale, 0.0f));
	return glm::translate(glm::mat4(1.0f), upperJoint - anchorOffset)
		* rotation * glm::scale(glm::mat4(1.0f), scale);
}

static glm::mat4 backSwordTransform(const ObjModel& sword, const glm::mat4& rootTransform) {
	// The grip rests above the character's left shoulder, with the blade angled down across the back.
	const glm::vec3 gripPosition(0.48f, 1.12f, -1.0f);
	const glm::vec3 bladeDirection(-0.30f, -0.95f, 0.02f);
	glm::mat4 rotation = alignPositiveY(glm::normalize(bladeDirection));
	rotation = rotation * glm::rotate(glm::mat4(1.0f), 1.57079632679f, glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::vec3 rotatedGripAnchor = glm::vec3(rotation * glm::vec4(sword.swordGripAnchor(), 0.0f));
	const glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), gripPosition - rotatedGripAnchor) * rotation;
	return rootTransform * localTransform;
}

static glm::mat4 handSwordTransform(const ObjModel& sword, const glm::mat4& rootTransform,
	const CharacterPose& pose) {
	// Keep the grip at the animated hand; point the blade along the forearm's current extension.
	glm::vec3 bladeDirection = pose.leftHandTip - pose.leftElbow;
	if (glm::length(bladeDirection) < 1e-6f) bladeDirection = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::mat4 rotation = alignPositiveY(glm::normalize(bladeDirection));
	rotation = rotation * glm::rotate(glm::mat4(1.0f), 1.57079632679f, glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::vec3 rotatedGripAnchor = glm::vec3(rotation * glm::vec4(sword.swordGripAnchor(), 0.0f));
	const glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), pose.leftHandTip - rotatedGripAnchor) * rotation;
	return rootTransform * localTransform;
}

static glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point) {
	return glm::vec3(transform * glm::vec4(point, 1.0f));
}

static glm::vec3 rotateAroundLocalX(const glm::vec3& vector, float angle) {
	const float cosine = std::cos(angle);
	const float sine = std::sin(angle);
	return glm::vec3(vector.x, vector.y * cosine - vector.z * sine, vector.y * sine + vector.z * cosine);
}

static bool projectToDesign(const glm::vec3& point, const glm::mat4& viewProjection, glm::vec2& output) {
	const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
	if (clip.w <= 0.001f) return false;
	const glm::vec3 ndc = glm::vec3(clip) / clip.w;
	if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
	output.x = MODEL_VIEW_RECT.x + (ndc.x + 1.0f) * 0.5f * MODEL_VIEW_RECT.w;
	output.y = MODEL_VIEW_RECT.y + (1.0f - ndc.y) * 0.5f * MODEL_VIEW_RECT.h;
	return true;
}

// Shared vertices around the open cut boundary in Ki.obj and Ra.obj.
static const glm::vec3 KIRA_CUT_JOIN_SOURCE(-0.03792675f, 2.53850525f, -1.6860115f);

CharacterPose animatedCharacterPose(const App& app) {
	if (skillAnimationActive(app)) return animatedSkillPose(app);
	const CharacterPose& base = app.characterPose;
	CharacterPose animated = base;
	const float blend = std::clamp(app.runAnimationBlend, 0.0f, 1.0f);
	if (blend <= 0.0f) return animated;

	constexpr float HALF_TURN = 3.14159265359f;
	const float leftStride = std::sin(app.runAnimationPhase);
	const float rightStride = std::sin(app.runAnimationPhase + HALF_TURN);
	const float leftThighAngle = -leftStride * 0.48f * blend;
	const float rightThighAngle = -rightStride * 0.48f * blend;
	const float leftKneeBend = std::max(0.0f, leftStride) * 0.68f * blend;
	const float rightKneeBend = std::max(0.0f, rightStride) * 0.68f * blend;

	animated.leftKnee = base.leftHip + rotateAroundLocalX(base.leftKnee - base.leftHip, leftThighAngle);
	animated.leftFootBase = animated.leftKnee
		+ rotateAroundLocalX(base.leftFootBase - base.leftKnee, leftThighAngle + leftKneeBend);
	animated.rightKnee = base.rightHip + rotateAroundLocalX(base.rightKnee - base.rightHip, rightThighAngle);
	animated.rightFootBase = animated.rightKnee
		+ rotateAroundLocalX(base.rightFootBase - base.rightKnee, rightThighAngle + rightKneeBend);

	// The character has one arm; swing it opposite the left leg and bend it at the elbow.
	const float armSwing = leftStride * 0.38f * blend;
	animated.leftElbow = base.leftShoulder
		+ rotateAroundLocalX(base.leftElbow - base.leftShoulder, armSwing);
	animated.leftHandTip = animated.leftElbow
		+ rotateAroundLocalX(base.leftHandTip - base.leftElbow, armSwing - 0.60f * blend);
	return animated;
}

void drawMainScreen(App& app, double now) {
	Canvas& canvas = app.canvas;
	canvas.rect(0, 0, 1280, 720, rgba(9, 10, 15));
	if (app.model.loaded()) {
		canvas.begin3D(MODEL_VIEW_RECT);
		const float aspect = MODEL_VIEW_RECT.w / MODEL_VIEW_RECT.h;
		const CharacterPose pose = animatedCharacterPose(app);
		const CharacterRig& rig = app.characterRig;
		app.ground.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition);
		if (app.enemyModel.loaded()) {
			glm::mat4 enemyTransform = glm::translate(glm::mat4(1.0f), app.enemyPosition);
			if (app.enemyYawSet) {
				enemyTransform *= glm::rotate(glm::mat4(1.0f), app.enemyYaw, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			app.enemyModel.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, enemyTransform);
		}
		if (skillAnimationActive(app) && app.skillAnimation.swordStageAtStart == 3
			&& app.skillAnimation.skillIndex == 5 && app.kiModel.loaded() && app.raModel.loaded()) {
			glm::vec3 towardEnemy = app.enemyPosition - app.characterPose.rootPosition;
			towardEnemy.y = 0.0f;
			if (glm::length(towardEnemy) < 1e-5f) towardEnemy = glm::vec3(0.0f, 0.0f, 1.0f);
			towardEnemy = glm::normalize(towardEnemy);
			const float yaw = std::atan2(towardEnemy.x, towardEnemy.z);
			const float slashStart = static_cast<float>(app.skillAnimation.lungeDuration + 2.0);
			const float split = std::clamp((static_cast<float>(app.skillAnimation.elapsed) - slashStart) / 0.45f, 0.0f, 1.0f);
			const float separation = 1.15f * split;
			const glm::vec3 center = app.enemyPosition - towardEnemy * 1.0f;
			const glm::mat4 kiraRotation = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			const float commonScale = app.kiModel.modelScale();
			const glm::vec3 kiAnchorOffset = (app.kiModel.sourceCenter() - KIRA_CUT_JOIN_SOURCE) * commonScale;
			const glm::vec3 raAnchorOffset = (app.raModel.sourceCenter() - KIRA_CUT_JOIN_SOURCE) * commonScale;
			const glm::mat4 kiTransform = glm::translate(glm::mat4(1.0f), center + glm::vec3(0.0f, separation, 0.0f))
				* kiraRotation * glm::translate(glm::mat4(1.0f), kiAnchorOffset);
			const glm::mat4 raTransform = glm::translate(glm::mat4(1.0f), center - glm::vec3(0.0f, separation, 0.0f))
				* kiraRotation * glm::translate(glm::mat4(1.0f), raAnchorOffset);
			app.kiModel.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, kiTransform);
			app.raModel.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, raTransform);
		}
		const glm::mat4 characterTransform = glm::translate(glm::mat4(1.0f), pose.rootPosition)
			* glm::rotate(glm::mat4(1.0f), pose.rootYaw, glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::vec3 torsoPivot = (pose.leftHip + pose.rightHip) * 0.5f;
		const glm::mat4 torsoTransform = characterTransform
			* glm::translate(glm::mat4(1.0f), torsoPivot)
			* glm::rotate(glm::mat4(1.0f), pose.torsoRotationZ, glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::rotate(glm::mat4(1.0f), pose.torsoRotationY, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::rotate(glm::mat4(1.0f), pose.torsoRotationX, glm::vec3(1.0f, 0.0f, 0.0f))
			* glm::translate(glm::mat4(1.0f), -torsoPivot);
		app.model.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, torsoTransform);
		if (app.coat.loaded()) {
			const glm::vec3 coatOffset = app.coat.scaledSourceCenter() - app.model.scaledSourceCenter() + vec3(0.0f, -0.4f, -0.4f);
			const float runBlend = std::clamp(app.runAnimationBlend, 0.0f, 1.0f);
			const float capeFlutter = std::sin(app.runAnimationPhase * 2.0f) * 0.035f;
			const float capeTiltBack = runBlend * (0.12f + capeFlutter);
			const glm::vec3 capePivot = app.coat.upperOriginAnchor();
			const glm::mat4 coatTransform = torsoTransform
				* glm::translate(glm::mat4(1.0f), coatOffset + capePivot)
				* glm::rotate(glm::mat4(1.0f), capeTiltBack, glm::vec3(1.0f, 0.0f, 0.0f))
				* glm::translate(glm::mat4(1.0f), -capePivot);
			app.coat.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, coatTransform);
		}
		// Arms follow torso rotation; legs use only the character's position and facing direction.
		const glm::vec3 torsoForward = glm::normalize(glm::vec3(
			torsoTransform * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
		const glm::vec3 characterForward = glm::normalize(glm::vec3(
			characterTransform * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
		const glm::vec3 leftShoulder = transformPoint(torsoTransform, pose.leftShoulder);
		const glm::vec3 leftElbow = transformPoint(torsoTransform, pose.leftElbow);
		const glm::vec3 leftHandTip = transformPoint(torsoTransform, pose.leftHandTip);
		const glm::mat4 leftUpperArm = boneTransform(leftShoulder, leftElbow,
			app.limbSegments.upperOriginAnchor(), app.limbSegments.lowerOriginAnchor(), rig.upperArmWidth, torsoForward);
		const glm::mat4 leftLowerArm = boneTransform(leftElbow, leftHandTip,
			app.hands.upperOriginAnchor(), app.hands.lowerOriginAnchor(), rig.lowerArmWidth, torsoForward);
		app.limbSegments.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, leftUpperArm);
		app.hands.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, leftLowerArm);
		if (app.swordStage >= 0 && app.swordStage < static_cast<int>(app.swordModels.size())) {
			const ObjModel& sword = app.swordModels[app.swordStage];
			const bool laevateinnAnimation = skillAnimationActive(app) && app.skillAnimation.swordStageAtStart == 3;
			const bool handHeldSkill = laevateinnAnimation
				? (app.skillAnimation.skillIndex == 0 || app.skillAnimation.skillIndex == 1
					|| app.skillAnimation.skillIndex == 2 || app.skillAnimation.skillIndex == 4
					|| app.skillAnimation.skillIndex == 5)
				: (skillAnimationActive(app)
					&& (app.skillAnimation.skillIndex == 1 || app.skillAnimation.skillIndex == 5));
			const bool swordIsInFlight = laevateinnAnimation && app.skillAnimation.skillIndex == 2
				&& app.skillAnimation.thrownSwordVisible;
			if (sword.loaded() && !swordIsInFlight) {
				sword.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition,
					handHeldSkill ? handSwordTransform(sword, torsoTransform, pose)
						: backSwordTransform(sword, torsoTransform));
			}
		}
		const bool legacyStageUpThrow = app.skillAnimation.skillIndex == 5
			&& app.skillAnimation.swordStageAtStart < 3;
		const bool laevateinnSkill3Throw = app.skillAnimation.skillIndex == 2
			&& app.skillAnimation.swordStageAtStart == 3;
		if ((legacyStageUpThrow || laevateinnSkill3Throw) && app.skillAnimation.thrownSwordVisible
			&& app.skillAnimation.thrownSwordStage >= 0
			&& app.skillAnimation.thrownSwordStage < static_cast<int>(app.thrownSwordModels.size())) {
			const ObjModel& thrownSword = app.thrownSwordModels[app.skillAnimation.thrownSwordStage];
			if (thrownSword.loaded()) {
				thrownSword.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition,
					thrownSkillSwordTransform(app));
			}
		}

		const glm::vec3 leftHip = transformPoint(characterTransform, pose.leftHip);
		const glm::vec3 rightHip = transformPoint(characterTransform, pose.rightHip);
		const glm::vec3 leftKnee = transformPoint(characterTransform, pose.leftKnee);
		const glm::vec3 rightKnee = transformPoint(characterTransform, pose.rightKnee);
		const glm::vec3 leftFootBase = transformPoint(characterTransform, pose.leftFootBase);
		const glm::vec3 rightFootBase = transformPoint(characterTransform, pose.rightFootBase);
		const glm::mat4 leftThigh = boneTransform(leftHip, leftKnee,
			app.limbSegments.upperOriginAnchor(), app.limbSegments.lowerOriginAnchor(), rig.leftThighWidth, characterForward);
		const glm::mat4 rightThigh = boneTransform(rightHip, rightKnee,
			app.limbSegments.upperOriginAnchor(), app.limbSegments.lowerOriginAnchor(), rig.rightThighWidth, characterForward);
		const glm::mat4 leftLowerLeg = boneTransform(leftKnee, leftFootBase,
			app.leftFoot.upperOriginAnchor(), app.leftFoot.lowerOriginAnchor(), rig.leftLowerLegWidth, characterForward);
		const glm::mat4 rightLowerLeg = boneTransform(rightKnee, rightFootBase,
			app.rightFoot.upperOriginAnchor(), app.rightFoot.lowerOriginAnchor(), rig.rightLowerLegWidth, characterForward);
		app.limbSegments.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, leftThigh);
		app.limbSegments.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, rightThigh);
		app.leftFoot.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, leftLowerLeg);
		app.rightFoot.draw(aspect, app.cameraYaw, app.cameraPitch, pose.rootPosition, rightLowerLeg);
		canvas.end3D();
		if (skillAnimationActive(app) && app.skillAnimation.swordStageAtStart == 3
			&& app.skillAnimation.skillIndex == 5 && app.skillAnimation.elapsed >= app.skillAnimation.lungeDuration + 2.0
			&& app.skillAnimation.elapsed < app.skillAnimation.lungeDuration + 2.45) {
			glm::vec3 towardEnemy = app.enemyPosition - app.characterPose.rootPosition;
			towardEnemy.y = 0.0f;
			if (glm::length(towardEnemy) < 1e-5f) towardEnemy = glm::vec3(0.0f, 0.0f, 1.0f);
			towardEnemy = glm::normalize(towardEnemy);
			const glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), towardEnemy));
			const glm::vec3 slashCenter = app.enemyPosition - towardEnemy * 0.12f;
			const glm::vec3 slashA = slashCenter - side * 2.65f;
			const glm::vec3 slashB = slashCenter + side * 2.65f;
			const glm::mat4 viewProjection = sceneProjectionMatrix(aspect)
				* sceneViewMatrix(pose.rootPosition, app.cameraYaw, app.cameraPitch);
			glm::vec2 screenA{}, screenB{};
			if (projectToDesign(slashA, viewProjection, screenA) && projectToDesign(slashB, viewProjection, screenB)) {
				const float fade = 1.0f - std::clamp(static_cast<float>(app.skillAnimation.elapsed
					- app.skillAnimation.lungeDuration - 2.0) / 0.45f, 0.0f, 1.0f);
				canvas.clipTo(MODEL_VIEW_RECT);
				canvas.line(screenA.x, screenA.y, screenB.x, screenB.y, 34.0f, rgba(150, 0, 12, static_cast<int>(210 * fade)));
				canvas.line(screenA.x, screenA.y, screenB.x, screenB.y, 18.0f, rgba(255, 22, 24, static_cast<int>(235 * fade)));
				canvas.line(screenA.x, screenA.y, screenB.x, screenB.y, 5.0f, rgba(255, 170, 145, static_cast<int>(245 * fade)));
				canvas.endClip();
			}
		}
	}
	for (const SkillSlot& slot : SKILL_SLOTS) drawSkillIcon(app, slot, now);
	
}

