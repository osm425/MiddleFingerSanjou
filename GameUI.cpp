#include "GameState.h"


void drawText(Canvas& canvas, Assets& assets, const std::wstring& value, int size, Tint color, float x, float y, bool bold) {
	const Texture& texture = assets.text(value, size, color, bold);
	canvas.image(texture, x, y, static_cast<float>(texture.width), static_cast<float>(texture.height));
}

void drawCentered(Canvas& canvas, Assets& assets, const std::wstring& value, int size, Tint color, float centerX, float y, bool bold) {
	const Texture& texture = assets.text(value, size, color, bold);
	canvas.image(texture, centerX - texture.width * 0.5f, y, static_cast<float>(texture.width), static_cast<float>(texture.height));
}

void drawStartScreen(App& app) {
	Canvas& canvas = app.canvas;
	Assets& assets = *app.assets;
	canvas.imageCoverViewport(app.startBackground);
	if (!app.startBackground.id) canvas.rect(0, 0, 1280, 720, rgba(9, 10, 15));
	drawCentered(canvas, assets, L"MIDDLE FINGER SANJOU", 52, rgba(232, 226, 213), 640, 36, true);
	const Tint button = app.startHovered ? rgba(125, 38, 45) : rgba(64, 25, 31);
	canvas.rect(START_BUTTON.x, START_BUTTON.y, START_BUTTON.w, START_BUTTON.h, button);
	drawCentered(canvas, assets, L"게임 시작", 29, rgba(250, 236, 215), START_BUTTON.x + START_BUTTON.w * 0.5f, START_BUTTON.y + 17, true);
}

void drawSkillIcon(App& app, const SkillSlot& slot, double now) {
	if (!skillAvailableForStage(app.swordStage, slot.skillIndex)) return;
	Canvas& canvas = app.canvas;
	const Rect bounds = slot.bounds;
	const Texture& icon = app.skills[app.swordStage][slot.skillIndex];
	const Texture& background = app.skillBackgrounds[slot.frameIndex];
	const float iconWidth = bounds.w * static_cast<float>(icon.width) / std::max(1, background.width);
	const float iconHeight = bounds.h * static_cast<float>(icon.height) / std::max(1, background.height);
	const float iconX = bounds.x + (bounds.w - iconWidth) * 0.5f;
	const float iconY = bounds.y + (bounds.h - iconHeight) * 0.5f;
	canvas.image(background, bounds.x, bounds.y, bounds.w, bounds.h);
	canvas.imageMasked(icon, background, iconX, iconY, iconWidth, iconHeight, bounds);
	canvas.image(app.skillFrames[slot.frameIndex], bounds.x, bounds.y, bounds.w, bounds.h);

	const double remaining = app.cooldownEndsAt[slot.skillIndex] - now;
	if (remaining <= 0.0) return;

	const double cooldownDuration = app.cooldownDurations[slot.skillIndex] > 0.0
		? app.cooldownDurations[slot.skillIndex] : skillCooldownSeconds(app.swordStage, slot.skillIndex);
	const float progress = static_cast<float>(std::clamp(1.0 - remaining / cooldownDuration, 0.0, 1.0));
	const float fullTurn = 6.2831853071795864769f;
	const float startAngle = -1.5707963267948966192f + progress * fullTurn;
	const float remainingSweep = (1.0f - progress) * fullTurn;
	canvas.clipTo(bounds);
	canvas.sector(bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f,
		bounds.w * 0.70710678f, startAngle, remainingSweep, rgba(0, 0, 0, 165));
	canvas.endClip();

	const double displayTime = std::ceil(remaining * 10.0) / 10.0;
	std::wostringstream timer;
	timer << std::fixed << std::setprecision(1) << displayTime;
	drawCentered(canvas, *app.assets, timer.str(), 22, rgba(255, 255, 255), bounds.x + bounds.w * 0.5f,
		bounds.y + bounds.h * 0.5f - 13, true);
}

void drawReturnConfirmation(App& app) {
	Canvas& canvas = app.canvas;
	Assets& assets = *app.assets;
	canvas.rect(0, 0, 1280, 720, rgba(0, 0, 0, 170));
	drawCentered(canvas, assets, L"처음 화면으로 돌아가시겠습니까?", 30, rgba(245, 239, 228), 640, 300, true);
	canvas.rect(MUSIC_TOGGLE_BUTTON.x, MUSIC_TOGGLE_BUTTON.y, MUSIC_TOGGLE_BUTTON.w, MUSIC_TOGGLE_BUTTON.h,
		app.musicEnabled ? rgba(48, 83, 64) : rgba(62, 62, 68));
	canvas.rect(VOICE_TOGGLE_BUTTON.x, VOICE_TOGGLE_BUTTON.y, VOICE_TOGGLE_BUTTON.w, VOICE_TOGGLE_BUTTON.h,
		app.voiceEnabled ? rgba(48, 83, 64) : rgba(62, 62, 68));
	canvas.rect(CONFIRM_RETURN_BUTTON.x, CONFIRM_RETURN_BUTTON.y, CONFIRM_RETURN_BUTTON.w, CONFIRM_RETURN_BUTTON.h, rgba(106, 34, 42));
	canvas.rect(CANCEL_RETURN_BUTTON.x, CANCEL_RETURN_BUTTON.y, CANCEL_RETURN_BUTTON.w, CANCEL_RETURN_BUTTON.h, rgba(52, 53, 61));
	drawCentered(canvas, assets, app.musicEnabled ? L"음악: 켜짐" : L"음악: 꺼짐", 20,
		rgba(255, 247, 230), MUSIC_TOGGLE_BUTTON.x + MUSIC_TOGGLE_BUTTON.w * 0.5f, MUSIC_TOGGLE_BUTTON.y + 15, true);
	drawCentered(canvas, assets, app.voiceEnabled ? L"보이스: 켜짐" : L"보이스: 꺼짐", 20,
		rgba(255, 247, 230), VOICE_TOGGLE_BUTTON.x + VOICE_TOGGLE_BUTTON.w * 0.5f, VOICE_TOGGLE_BUTTON.y + 15, true);
	drawCentered(canvas, assets, L"확인", 22, rgba(255, 247, 230), CONFIRM_RETURN_BUTTON.x + CONFIRM_RETURN_BUTTON.w * 0.5f, CONFIRM_RETURN_BUTTON.y + 17, true);
	drawCentered(canvas, assets, L"취소", 22, rgba(240, 240, 240), CANCEL_RETURN_BUTTON.x + CANCEL_RETURN_BUTTON.w * 0.5f, CANCEL_RETURN_BUTTON.y + 17, true);
}

