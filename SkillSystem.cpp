#include "GameState.h"

bool skillAvailableForStage(int swordStage, int skillIndex) {
	if (swordStage < SwordStageId::Sealed || swordStage >= SwordStageId::Count
		|| skillIndex < SkillId::Stomp || skillIndex >= SkillId::Count) return false;
	return !(swordStage == SwordStageId::Laevateinn && skillIndex == SkillId::Punting);
}

double skillCooldownSeconds(int swordStage, int skillIndex) {
	static constexpr std::array<double, SkillId::Count> sealedSwordCooldowns = { 4.0, 6.0, 8.0, 3.0, 5.0, 10.0 };
	static constexpr std::array<double, SkillId::Count> laevateinnCooldowns = { 4.0, 8.0, 10.0, 0.0, 5.0, 16.0 };
	if (skillIndex < SkillId::Stomp || skillIndex >= SkillId::Count) {
		return SKILL_COOLDOWN_SECONDS;
	}
	if (swordStage >= SwordStageId::Sealed && swordStage <= SwordStageId::SecondUnsealed) {
		return sealedSwordCooldowns[skillIndex];
	}
	if (swordStage == SwordStageId::Laevateinn) return laevateinnCooldowns[skillIndex];
	return SKILL_COOLDOWN_SECONDS;
}

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
	if (stageAtActivation >= SwordStageId::Count) return;
	app.skillAnimation = {};
	app.skillAnimation.skillIndex = skillIndex;
	app.skillAnimation.swordStageAtStart = stageAtActivation;
	app.skillAnimation.duration = skillAnimationSeconds(skillIndex, stageAtActivation);
	if (stageAtActivation == SwordStageId::Laevateinn && enemyInLockOnRange) {
		app.skillAnimation.enemyHitPending = true;
	}
	if (enemyInLockOnRange && stageAtActivation < SwordStageId::Laevateinn) {
		app.skillAnimation.lungeDirection = enemyApproachDirection;
		app.skillAnimation.lungeDistance = ENEMY_LUNGE_DISTANCE;
		app.skillAnimation.lungeDuration = ENEMY_LUNGE_SECONDS;
		app.skillAnimation.enemyHitPending = true;
	}
	if (stageAtActivation == SwordStageId::Laevateinn && app.enemyModel.loaded() && (skillIndex == SkillId::VengeanceStamp || skillIndex == SkillId::Unbox)) {
		const glm::vec2 toEnemy(app.enemyPosition.x - app.characterPose.rootPosition.x,
			app.enemyPosition.z - app.characterPose.rootPosition.z);
		const float distance = glm::length(toEnemy);
		if (distance > 1e-4f) {
			const glm::vec3 direction(toEnemy.x / distance, 0.0f, toEnemy.y / distance);
			app.skillAnimation.lungeDirection = direction;
			app.characterPose.rootYaw = std::atan2(direction.x, direction.z);
			if (skillIndex == SkillId::VengeanceStamp) {
				// Move during the kick toward the sword planted at the enemy, so the right foot can reach it.
				app.skillAnimation.lungeDistance = std::max(0.0f, distance - 1.7f);
				app.skillAnimation.lungeStart = LAEVATEINN_SKILL3_KICK_START;
				app.skillAnimation.lungeDuration = 1.0;
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
	if (stageAtActivation < SwordStageId::Laevateinn && skillIndex == SkillId::VengeanceStamp) {
		app.skillAnimation.voiceCueMask |= 0x1u;
		playSkillVoice(app, L"bored_to_play_1.wav", now);
	}
	else if (stageAtActivation < SwordStageId::Laevateinn && skillIndex == SkillId::RuleViolation) {
		app.skillAnimation.voiceCueMask |= 0x1u;
		playSkillVoice(app, L"come_on_1.wav", now);
	}
	else if (stageAtActivation < SwordStageId::Laevateinn && skillIndex == SkillId::Unbox && stageAtActivation == SwordStageId::Sealed) {
		app.skillAnimation.voiceCueMask |= 0x1u;
		playSkillVoice(app, L"pojangji_1.wav", now);
	}
	else if (stageAtActivation < SwordStageId::Laevateinn && skillIndex == SkillId::Unbox && stageAtActivation == SwordStageId::FirstUnsealed) {
		app.skillAnimation.voiceCueMask |= 0x1u;
		playSkillVoice(app, L"pojangji_2.wav", now);
	}
	else if (stageAtActivation < SwordStageId::Laevateinn && skillIndex == SkillId::Unbox && stageAtActivation == SwordStageId::SecondUnsealed) {
		app.skillAnimation.voiceCueMask |= 0x1u;
		playSkillVoice(app, L"pojangji_3.wav", now);
	}
}

