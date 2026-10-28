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

static const wchar_t* skillDisplayName(int swordStage, int skillIndex) {
	static constexpr const wchar_t* names[4][6] = {
		{ L"짓밟기", L"고작 이거에 부서지지 말라고!", L"원한 스탬핑", L"걷어차기", L"규칙 위반", L"포장 하나 뜯는다!" },
		{ L"짓밟기", L"후끈해지지 않나?!", L"원한 스탬핑", L"걷어차기", L"규칙 위반", L"두번째 포장지까지 벗기게 되는군." },
		{ L"짓밟기!", L"슬슬 녹아 죽을 차례다", L"즉결 처형 스탬핑!", L"걷어차기!", L"규칙 위반!", L"레바테인 - 여기까지 꺼내는 것도 오랜만이군!" },
		{ L"배떼지를 갈라주지", L"칼빵 [레바테인]", L"...즉결처형[레바테인]", L"", L"규칙 위반!!", L"애절단 - 가로" }
	};
	if (swordStage < 0 || swordStage >= 4 || skillIndex < 0 || skillIndex >= 6) return L"";
	return names[swordStage][skillIndex];
}

static void drawSkillKeyLabel(Canvas& canvas, Assets& assets, const SkillSlot& slot) {
	constexpr float labelWidth = 22.0f;
	constexpr float labelHeight = 28.0f;
	const float x = slot.bounds.x - 25.0f;
	const float y = slot.bounds.y + (slot.bounds.h - labelHeight) * 0.5f;
	canvas.roundedRect(x, y, labelWidth, labelHeight, 6.0f, rgba(13, 14, 19, 230));
	const Texture& number = assets.text(std::to_wstring(slot.skillIndex + 1), 16, rgba(255, 247, 230), true);
	canvas.image(number, x + (labelWidth - number.width) * 0.5f, y + (labelHeight - number.height) * 0.5f,
		static_cast<float>(number.width), static_cast<float>(number.height));
}

static void drawSkillHoverName(App& app, const SkillSlot& slot) {
	if (app.hoveredSkillIndex != slot.skillIndex) return;
	const wchar_t* name = skillDisplayName(app.swordStage, slot.skillIndex);
	if (!name || !*name) return;
	Assets& assets = *app.assets;
	const Tint textColor = rgba(255, 247, 230);
	int fontSize = 20;
	const Texture* text = nullptr;
	while (fontSize >= 14) {
		const Texture& candidate = assets.text(name, fontSize, textColor, true);
		text = &candidate;
		if (candidate.width <= 1200) break;
		--fontSize;
	}
	if (!text) return;
	const float boxWidth = static_cast<float>(text->width) + 24.0f;
	const float boxHeight = std::max(34.0f, static_cast<float>(text->height) + 10.0f);
	const float iconCenterX = slot.bounds.x + slot.bounds.w * 0.5f;
	const float boxX = std::clamp(iconCenterX - boxWidth * 0.5f, 12.0f, 1268.0f - boxWidth);
	const float boxY = std::max(12.0f, slot.bounds.y - boxHeight - 8.0f);
	app.canvas.roundedRect(boxX, boxY, boxWidth, boxHeight, 8.0f, rgba(15, 16, 21, 238));
	app.canvas.image(*text, boxX + (boxWidth - text->width) * 0.5f,
		boxY + (boxHeight - text->height) * 0.5f, static_cast<float>(text->width), static_cast<float>(text->height));
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
	drawSkillKeyLabel(canvas, *app.assets, slot);
	drawSkillHoverName(app, slot);

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

void drawExitConfirmation(App& app) {
	Canvas& canvas = app.canvas;
	Assets& assets = *app.assets;
	canvas.rect(0, 0, 1280, 720, rgba(0, 0, 0, 170));
	drawCentered(canvas, assets, L"게임을 종료하시겠습니까?", 30, rgba(245, 239, 228), 640, 300, true);
	canvas.rect(CONFIRM_EXIT_BUTTON.x, CONFIRM_EXIT_BUTTON.y, CONFIRM_EXIT_BUTTON.w, CONFIRM_EXIT_BUTTON.h, rgba(106, 34, 42));
	canvas.rect(CANCEL_EXIT_BUTTON.x, CANCEL_EXIT_BUTTON.y, CANCEL_EXIT_BUTTON.w, CANCEL_EXIT_BUTTON.h, rgba(52, 53, 61));
	drawCentered(canvas, assets, L"확인", 22, rgba(255, 247, 230), CONFIRM_EXIT_BUTTON.x + CONFIRM_EXIT_BUTTON.w * 0.5f, CONFIRM_EXIT_BUTTON.y + 17, true);
	drawCentered(canvas, assets, L"취소", 22, rgba(240, 240, 240), CANCEL_EXIT_BUTTON.x + CANCEL_EXIT_BUTTON.w * 0.5f, CANCEL_EXIT_BUTTON.y + 17, true);
}

