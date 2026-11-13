#pragma once

#include "RenderResources.h"
#include <atomic>

static constexpr float VESPA_MODEL_EXTENT = 4.5f;
// 적의 크기·타격 범위, 캐릭터 접근 거리와 레바테인 동작 시간을 모은다.
static constexpr float ENEMY_LOCKON_RADIUS = 5.0f;
static constexpr float ENEMY_SKILL_HIT_RADIUS = 5.0f;
static constexpr float ENEMY_LUNGE_DISTANCE = 1.2f;
static constexpr double ENEMY_LUNGE_SECONDS = 0.3;
static constexpr double LAEVATEINN_SKILL3_FLIGHT_TIME = 1.0;
static constexpr double LAEVATEINN_SKILL5_DASH_START = 0.12 + 0.5 + 0.5;
static constexpr float LAEVATEINN_SKILL6_KIRA_FORWARD_OFFSET = 2.0f;
static constexpr float LAEVATEINN_SKILL6_STOP_DISTANCE_FROM_KIRA = 2.5f;

// 캐릭터 기준 위치, 방향, 관절과 몸통·무기·망토 회전값을 저장한다.
struct CharacterPose {
	glm::vec3 rootPosition{ 0.35f, 0.0f, 0.0f };
	float rootYaw = 0.0f;
	// Optional character-local direction for the held sword; zero keeps forearm alignment.
	glm::vec3 heldSwordDirection{ 0.0f };
	// Additional rotation around the held sword object's local Y axis, in radians.
	float heldSwordRotationY = 0.0f;
	// Additional cape rotation around its local X axis, in radians.
	float capeRotationX = 0.0f;
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

// 관절 사이 길이는 자세에서 계산하고 X/Z 값으로 팔다리 굵기를 조정한다.
struct CharacterRig {
	glm::vec2 upperArmWidth{ 0.234f, 0.234f };
	glm::vec2 lowerArmWidth{ 0.299f, 0.286f };
	glm::vec2 leftThighWidth{ 0.221f, 0.221f };
	glm::vec2 rightThighWidth{ 0.221f, 0.221f };
	glm::vec2 leftLowerLegWidth{ 0.286f, 0.260f };
	glm::vec2 rightLowerLegWidth{ 0.2145f, 0.208f };
};

// 현재 스킬 프레임, 이동·타격 상태, 던진 검의 비행 정보를 보관한다.
struct SkillAnimationState {
	int skillIndex = -1;
	int swordStageAtStart = 0;
	double elapsed = 0.0;
	double duration = 0.0;
	unsigned voiceCueMask = 0;
	unsigned soundEffectCueMask = 0;
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

// 보이스 또는 효과음 한 개의 waveOut 장치와 샘플 재생 상태를 보관한다.
struct VoicePlayback {
	HWAVEOUT device = nullptr;
	WAVEHDR header{};
	std::vector<char> audioData;
	std::vector<char> outputData;
	WAVEFORMATEX format{};
	std::atomic<bool> completed{ true };
	double startedAt = 0.0;
	std::wstring fileName;
	float gain = 1.0f;
	bool spatialized = false;
	glm::vec3 sourcePosition{ 0.0f };
};

// 화면 전환과 ESC 확인창 상태를 구분하는 열거형이다.
enum class Screen { Start, Main, ConfirmReturn, ConfirmExit };

// 게임 한 세션의 렌더링, 입력, 캐릭터, 적, 오디오 상태를 모은다.
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
	std::array<VoicePlayback, 16> soundEffectPlayback;
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
	bool soundEffectsEnabled = true;
	int framebufferWidth = 1280;
	int framebufferHeight = 720;
};

// UI 버튼 좌표와 6개 스킬 아이콘의 화면 배치를 정의한다.
static const Rect START_BUTTON{ 480, 526, 320, 72 };
static const Rect MUSIC_TOGGLE_BUTTON{ 190, 356, 220, 52 };
static const Rect VOICE_TOGGLE_BUTTON{ 530, 356, 220, 52 };
static const Rect SOUND_EFFECT_TOGGLE_BUTTON{ 870, 356, 220, 52 };
static const Rect CONFIRM_RETURN_BUTTON{ 470, 438, 150, 58 };
static const Rect CANCEL_RETURN_BUTTON{ 660, 438, 150, 58 };
static const Rect CONFIRM_EXIT_BUTTON{ 470, 438, 150, 58 };
static const Rect CANCEL_EXIT_BUTTON{ 660, 438, 150, 58 };

// 스킬 배열 인덱스, 아이콘 프레임 번호와 화면 사각형을 연결한다.
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



// 입력·스킬: 좌표 판정, 사용 가능 여부, 쿨타임과 시작·취소 처리를 담당한다.
bool inside(float x, float y, const Rect& rect);
bool skillAvailableForStage(int swordStage, int skillIndex);
void activateSkill(App& app, int skillIndex, double now);
void confirmReturnToStart(App& app);
void cancelReturnConfirmation(App& app, double now);
// 창 크기와 UI 좌표계 사이의 변환 및 키보드·마우스 콜백이다.
void cursorToDesign(App& app, double x, double y, float& designX, float& designY);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPositionCallback(GLFWwindow* window, double x, double y);
// 매 프레임 이동·애니메이션 갱신 및 오디오 재생 제어를 제공한다.
void updateCharacterMovement(App& app, float deltaTime);
void updateSkillAnimation(App& app, float deltaTime, double now);
// 보이스와 효과음 파일의 재생·공간화·음량 조절·정지 인터페이스다.
void playSkillVoice(App& app, const wchar_t* fileName, double now);
void updateVoicePlayback(App& app);
void stopVoicePlayback(App& app);
void playSkillSoundEffect(App& app, const wchar_t* fileName, double now);
void playSpatialSkillSoundEffect(App& app, const wchar_t* fileName, double now);
void setSkillSoundEffectVolume(App& app, const wchar_t* fileName, float gain);
void stopSkillSoundEffect(App& app, const wchar_t* fileName);
void updateSoundEffectPlayback(App& app);
void stopSoundEffectPlayback(App& app);
// 스킬 애니메이션 시간·검 궤적과 캐릭터 화면 출력 함수를 선언한다.
double skillCooldownSeconds(int swordStage, int skillIndex);
double skillAnimationSeconds(int skillIndex, int swordStage);
double laevateinnSkill3ThrowTime();
double laevateinnSkill3KickStartTime();
double laevateinnSkill3RetrievalTime();
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
// 실행 위치와 작업 폴더에서 게임 에셋 루트를 찾는다.
fs::path findAssetRoot();
