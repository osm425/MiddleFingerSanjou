#include "GameState.h"

namespace {
bool projectToDesign(const glm::vec3& point, const glm::mat4& viewProjection, glm::vec2& output) {
	const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
	if (clip.w <= 0.001f) return false;
	const glm::vec3 ndc = glm::vec3(clip) / clip.w;
	if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
	output.x = MODEL_VIEW_RECT.x + (ndc.x + 1.0f) * 0.5f * MODEL_VIEW_RECT.w;
	output.y = MODEL_VIEW_RECT.y + (1.0f - ndc.y) * 0.5f * MODEL_VIEW_RECT.h;
	return true;
}
}

void drawLaevateinnSkill6SlashEffect(App& app, const CharacterPose& pose, float aspect) {
	const SkillAnimationState& skill = app.skillAnimation;
	if (!skillAnimationActive(app) || skill.swordStageAtStart != SwordStageId::Laevateinn
		|| skill.skillIndex != SkillId::Unbox
		|| skill.elapsed < skill.lungeDuration + LAEVATEINN_SKILL6_HOLD_BEFORE_SLASH
		|| skill.elapsed >= skill.lungeDuration + LAEVATEINN_SKILL6_HOLD_BEFORE_SLASH
			+ LAEVATEINN_SKILL6_SLASH_EFFECT_DURATION) return;

	glm::vec3 towardEnemy = app.enemyPosition - app.characterPose.rootPosition;
	towardEnemy.y = 0.0f;
	if (glm::length(towardEnemy) < 1e-5f) towardEnemy = glm::vec3(0.0f, 0.0f, 1.0f);
	towardEnemy = glm::normalize(towardEnemy);
	const glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), towardEnemy));
	const glm::vec3 slashCenter = app.enemyPosition - towardEnemy * 0.12f;
	const glm::vec3 slashStart = slashCenter - side * 2.65f;
	const glm::vec3 slashEnd = slashCenter + side * 2.65f;
	const glm::mat4 viewProjection = sceneProjectionMatrix(aspect)
		* sceneViewMatrix(pose.rootPosition, app.cameraYaw, app.cameraPitch);
	glm::vec2 screenStart{}, screenEnd{};
	if (!projectToDesign(slashStart, viewProjection, screenStart)
		|| !projectToDesign(slashEnd, viewProjection, screenEnd)) return;

	const float fade = 1.0f - std::clamp(static_cast<float>(skill.elapsed - skill.lungeDuration
		- LAEVATEINN_SKILL6_HOLD_BEFORE_SLASH) / static_cast<float>(LAEVATEINN_SKILL6_SLASH_EFFECT_DURATION), 0.0f, 1.0f);
	app.canvas.clipTo(MODEL_VIEW_RECT);
	app.canvas.line(screenStart.x, screenStart.y, screenEnd.x, screenEnd.y,
		34.0f, rgba(150, 0, 12, static_cast<int>(210 * fade)));
	app.canvas.line(screenStart.x, screenStart.y, screenEnd.x, screenEnd.y,
		18.0f, rgba(255, 22, 24, static_cast<int>(235 * fade)));
	app.canvas.line(screenStart.x, screenStart.y, screenEnd.x, screenEnd.y,
		5.0f, rgba(255, 170, 145, static_cast<int>(245 * fade)));
	app.canvas.endClip();
}
