#pragma once

#include "RenderResources.h"
#include <atomic>

static constexpr float VESPA_MODEL_EXTENT = 3.8f;
static constexpr float ENEMY_LOCKON_RADIUS = 14.0f;
static constexpr float ENEMY_SKILL_HIT_RADIUS = 5.0f;
static constexpr float ENEMY_LUNGE_DISTANCE = 1.2f;
static constexpr double ENEMY_LUNGE_SECONDS = 0.3;
static constexpr double LAEVATEINN_SKILL3_THROW_TIME = 2.28;
static constexpr double LAEVATEINN_SKILL3_FLIGHT_TIME = 1.0;
static constexpr double LAEVATEINN_SKILL3_KICK_START = LAEVATEINN_SKILL3_THROW_TIME + LAEVATEINN_SKILL3_FLIGHT_TIME;
static constexpr double LAEVATEINN_SKILL3_KNEE_BEND_TIME = LAEVATEINN_SKILL3_KICK_START + 1.0;
static constexpr double LAEVATEINN_SKILL3_RETRIEVAL_TIME = LAEVATEINN_SKILL3_KNEE_BEND_TIME + 1.2;
static constexpr double LAEVATEINN_SKILL5_DASH_START = 0.12 + 0.12 + 0.5;

// Joint coordinates are local to the character root and can be updated by an animation later.
struct CharacterPose {
	glm::vec3 rootPosition{ 0.35f, 0.0f, 0.0f };
	float rootYaw = 0.0f;
	float torsoRotationX = 0.0f;
	float torsoRotationY = 0.0f;
	float torsoRotationZ = 0.0f;
	glm::vec3 leftShoulder{ 0.52f, 0.76f, 0.00f };
	glm::vec3 leftElbow{ 0.819f, 0.019f, 0.00f };
	glm::vec3 leftHandTip{ 1.209f, -0.930f, 0.013f };

	glm::vec3 leftHip{ 0.00f, -1.20f, 0.00f };
	glm::vec3 rightHip{ -0.60f, -1.20f, 0.00f };
	glm::vec3 leftKnee{ 0.00f, -2.1152f, 0.00f };
	glm::vec3 rightKnee{ -0.60f, -2.1152f, 0.00f };
	glm::vec3 leftFootBase{ 0.00f, -3.2592f, 0.00f };
	glm::vec3 rightFootBase{ -0.60f, -3.2592f, 0.00f };
};

// X/Z values control the thickness of each segment; its length comes from the pose joints.
struct CharacterRig {
	glm::vec2 upperArmWidth{ 0.234f, 0.234f };
	glm::vec2 lowerArmWidth{ 0.299f, 0.286f };
	glm::vec2 leftThighWidth{ 0.221f, 0.221f };
	glm::vec2 rightThighWidth{ 0.221f, 0.221f };
	glm::vec2 leftLowerLegWidth{ 0.286f, 0.260f };
	glm::vec2 rightLowerLegWidth{ 0.2145f, 0.208f };
};

struct SkillAnimationState {
	int skillIndex = -1;
	int swordStageAtStart = 0;
	double elapsed = 0.0;
	double duration = 0.0;
	unsigned voiceCueMask = 0;
	bool swordThrown = false;
	bool thrownSwordVisible = false;
	int thrownSwordStage = 0;
	double thrownSwordElapsed = 0.0;
	double thrownSwordFlightDuration = 0.0;
	glm::vec3 lungeDirection{ 0.0f };
	float lungeDistance = 0.0f;
	double lungeStart = 0.0;
	double lungeDuration = 0.0;
	bool enemyHitPending = false;
	bool enemyHitResolved = false;
	glm::vec3 thrownSwordStart{ 0.0f };
	glm::vec3 thrownSwordTarget{ 0.0f };
	glm::vec3 thrownSwordVelocity{ 0.0f };
	glm::vec3 thrownSwordDirection{ 0.0f, 1.0f, 0.0f };
	float thrownSwordSpinTurns = 0.0f;
	bool thrownSwordPlanted = false;
};

struct VoicePlayback {
	HWAVEOUT device = nullptr;
	WAVEHDR header{};
	std::vector<char> audioData;
	std::atomic<bool> completed{ true };
	double startedAt = 0.0;
};

enum class Screen { Start, Main, ConfirmReturn, ConfirmExit };

struct App {
	GLFWwindow* window = nullptr;
	Canvas canvas;
	Assets* assets = nullptr;
	Screen screen = Screen::Start;
	Texture startBackground;
	std::array<std::array<Texture, 6>, 4> skills{};
	std::array<Texture, 4> skillFrames{};
	std::array<Texture, 4> skillBackgrounds{};
	std::array<double, 6> cooldownEndsAt{};
	std::array<double, 6> cooldownDurations{};
	fs::path assetRoot;
	GroundPlane ground;
	ObjModel model;
	ObjModel enemyModel;
	ObjModel kiModel;
	ObjModel raModel;
	glm::vec3 enemyPosition{ 4.0f, FIELD_Y + VESPA_MODEL_EXTENT * 0.5f, 0.0f };
	float enemyYaw = 0.0f;
	bool enemyYawSet = false;
	bool enemyHasBeenHit = false;
	ObjModel limbSegments;
	ObjModel hands;
	ObjModel leftFoot;
	ObjModel rightFoot;
	ObjModel coat;
	std::array<ObjModel, 4> swordModels;
	std::array<ObjModel, 4> thrownSwordModels;
	int swordStage = 0;
	SkillAnimationState skillAnimation;
	std::array<VoicePlayback, 3> voicePlayback;
	CharacterPose characterPose;
	glm::vec3 initialCharacterPosition = characterPose.rootPosition;
	float initialCharacterYaw = characterPose.rootYaw;
	CharacterRig characterRig;
	float cameraYaw = 0.25f;
	float cameraPitch = 0.43f;
	bool cameraDragging = false;
	float lastCameraDragX = 0.0f;
	float lastCameraDragY = 0.0f;
	float runAnimationPhase = 0.0f;
	float runAnimationBlend = 0.0f;
	double pauseStartedAt = 0.0;
	bool startHovered = false;
	int hoveredSkillIndex = -1;
	bool musicEnabled = true;
	bool voiceEnabled = true;
	int framebufferWidth = 1280;
	int framebufferHeight = 720;
};

static const Rect START_BUTTON{ 480, 526, 320, 72 };
static const Rect MUSIC_TOGGLE_BUTTON{ 400, 356, 220, 52 };
static const Rect VOICE_TOGGLE_BUTTON{ 660, 356, 220, 52 };
static const Rect CONFIRM_RETURN_BUTTON{ 470, 438, 150, 58 };
static const Rect CANCEL_RETURN_BUTTON{ 660, 438, 150, 58 };
static const Rect CONFIRM_EXIT_BUTTON{ 470, 438, 150, 58 };
static const Rect CANCEL_EXIT_BUTTON{ 660, 438, 150, 58 };

struct SkillSlot {
	int skillIndex;
	int frameIndex;
	Rect bounds;
};

static constexpr std::array<SkillSlot, 6> SKILL_SLOTS = {{
	{ 3, 0, { 927, 440, 91, 91 } },  // 4: Punting / Wrath
	{ 4, 2, { 1044, 440, 91, 91 } }, // 5: Rule Violation / Envy1
	{ 5, 3, { 1161, 440, 91, 91 } }, // 6: Unbox / Envy3
	{ 0, 0, { 927, 591, 91, 91 } }, // 1: Stomping / Wrath
	{ 1, 1, { 1044, 591, 91, 91 } },// 2: Don't Let This Break Ya / Pride
	{ 2, 3, { 1161, 591, 91, 91 } } // 3: Stamp of Vengeance / Envy3
}};

static const Rect MODEL_VIEW_RECT{ 0, 0, 1280, 720 };

static constexpr double SKILL_COOLDOWN_SECONDS = 5.0;



bool inside(float x, float y, const Rect& rect);
bool skillAvailableForStage(int swordStage, int skillIndex);
void activateSkill(App& app, int skillIndex, double now);
void confirmReturnToStart(App& app);
void cancelReturnConfirmation(App& app, double now);
void cursorToDesign(App& app, double x, double y, float& designX, float& designY);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPositionCallback(GLFWwindow* window, double x, double y);
void updateCharacterMovement(App& app, float deltaTime);
void updateSkillAnimation(App& app, float deltaTime, double now);
void playSkillVoice(App& app, const wchar_t* fileName, double now);
void updateVoicePlayback(App& app);
void stopVoicePlayback(App& app);
double skillCooldownSeconds(int swordStage, int skillIndex);
double skillAnimationSeconds(int skillIndex, int swordStage);
bool skillAnimationActive(const App& app);
void framebufferCallback(GLFWwindow* window, int width, int height);
void drawText(Canvas& canvas, Assets& assets, const std::wstring& value, int size, Tint color, float x, float y, bool bold = false);
void drawCentered(Canvas& canvas, Assets& assets, const std::wstring& value, int size, Tint color, float centerX, float y, bool bold = false);
void drawStartScreen(App& app);
void drawSkillIcon(App& app, const SkillSlot& slot, double now);
CharacterPose animatedCharacterPose(const App& app);
CharacterPose animatedSkillPose(const App& app);
glm::mat4 thrownSkillSwordTransform(const App& app);
void drawMainScreen(App& app, double now);
void drawReturnConfirmation(App& app);
void drawExitConfirmation(App& app);
fs::path findAssetRoot();
