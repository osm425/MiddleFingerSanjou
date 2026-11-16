#include "GameState.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

namespace {
// PCM 16비트 WAV의 샘플 진폭을 현재 음량의 4/5로 낮춰 메모리에 준비한다.
bool loadBackgroundMusicAtReducedVolume(const fs::path& path, std::vector<char>& waveData) {
	std::ifstream input(path, std::ios::binary);
	if (!input) return false;
	waveData.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	if (waveData.size() < 12 || std::memcmp(waveData.data(), "RIFF", 4) != 0
		|| std::memcmp(waveData.data() + 8, "WAVE", 4) != 0) return false;

	uint16_t formatTag = 0;
	uint16_t channelCount = 0;
	uint16_t blockAlign = 0;
	uint16_t bitsPerSample = 0;
	size_t sampleDataOffset = 0;
	size_t sampleDataSize = 0;
	bool hasFormat = false;
	bool hasSampleData = false;
	for (size_t chunkOffset = 12; chunkOffset + 8 <= waveData.size();) {
		uint32_t chunkSize = 0;
		std::memcpy(&chunkSize, waveData.data() + chunkOffset + 4, sizeof(chunkSize));
		const size_t payloadOffset = chunkOffset + 8;
		if (chunkSize > waveData.size() - payloadOffset) return false;
		if (std::memcmp(waveData.data() + chunkOffset, "fmt ", 4) == 0) {
			if (chunkSize < 16) return false;
			std::memcpy(&formatTag, waveData.data() + payloadOffset, sizeof(formatTag));
			std::memcpy(&channelCount, waveData.data() + payloadOffset + 2, sizeof(channelCount));
			std::memcpy(&blockAlign, waveData.data() + payloadOffset + 12, sizeof(blockAlign));
			std::memcpy(&bitsPerSample, waveData.data() + payloadOffset + 14, sizeof(bitsPerSample));
			hasFormat = true;
		}
		else if (std::memcmp(waveData.data() + chunkOffset, "data", 4) == 0) {
			sampleDataOffset = payloadOffset;
			sampleDataSize = chunkSize;
			hasSampleData = true;
		}
		const size_t paddedChunkSize = static_cast<size_t>(chunkSize) + (chunkSize & 1u);
		if (paddedChunkSize > waveData.size() - payloadOffset) break;
		chunkOffset = payloadOffset + paddedChunkSize;
	}
	if (!hasFormat || !hasSampleData || formatTag != WAVE_FORMAT_PCM || bitsPerSample != 16
		|| channelCount == 0 || blockAlign < channelCount * sizeof(int16_t)) return false;

	for (size_t offset = sampleDataOffset; offset + sizeof(int16_t) <= sampleDataOffset + sampleDataSize;
		offset += sizeof(int16_t)) {
		int16_t sample = 0;
		std::memcpy(&sample, waveData.data() + offset, sizeof(sample));
		const int16_t reducedSample = static_cast<int16_t>(std::lround(static_cast<double>(sample) * 0.6));
		std::memcpy(waveData.data() + offset, &reducedSample, sizeof(reducedSample));
	}
	return true;
}
}

// GLFW 창과 OpenGL 3.3 렌더링 문맥을 준비한다.
GLFWwindow* createOpenGLWindow(int width, int height, const char* title) {
	if (!glfwInit()) return nullptr;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return nullptr;
	}
	glfwMakeContextCurrent(window);
	glViewport(0, 0, width, height);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		glfwDestroyWindow(window);
		glfwTerminate();
		return nullptr;
	}
	return window;
}


// 프로그램 초기화, 게임 루프, 리소스 정리를 담당하는 진입 함수다.
int WINAPI wWinMain(HINSTANCE , HINSTANCE, PWSTR, int) {
	GLFWwindow* window = createOpenGLWindow(1280, 720, "Middle Finger Sanjou - UI Prototype");
	if (!window) {
		std::cerr << "Could not create the OpenGL window." << std::endl;
		return -1;
	}
	glfwSwapInterval(1);

	Gdiplus::GdiplusStartupInput startupInput;
	ULONG_PTR gdiplusToken = 0;
	if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok) {
		std::cerr << "Could not initialize GDI+." << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	const fs::path assetRoot = findAssetRoot();
	Assets assets(assetRoot);
	App app;
	app.window = window;
	app.assets = &assets;
	app.assetRoot = assetRoot;
	app.canvas.initialize();
	app.startBackground = assets.loadImage(L"start_screen.png");
	app.ground.initialize();
	const Texture modelTexture = assets.loadImage(L"models/bodytexture.png");
	app.model.load(assetRoot / L"models" / L"Matthias.obj", modelTexture);
	const fs::path modelRoot = assetRoot / L"models";
	const Texture bookshelfTexture = assets.loadImage(L"models/books.png");
	app.bookshelf.load(modelRoot / L"bookshelf.obj", bookshelfTexture, glm::vec3(1.0f), 8.0f);
	const Texture enemyTexture = assets.loadImage(L"models/vespa1.png");
	app.enemyModel.load(modelRoot / L"Vespa.obj", enemyTexture, glm::vec3(0.78f, 0.68f, 0.55f), VESPA_MODEL_EXTENT);
	const Texture kiTexture = assets.loadImage(L"models/Kira_upper.png");
	const Texture raTexture = assets.loadImage(L"models/Kira_lower.png");
	app.kiModel.load(modelRoot / L"Ki.obj", kiTexture, glm::vec3(1.0f), VESPA_MODEL_EXTENT/1.5);
	app.raModel.load(modelRoot / L"Ra.obj", raTexture, glm::vec3(1.0f), VESPA_MODEL_EXTENT/1.5,
		app.kiModel.modelScale());
	app.coat.load(modelRoot / L"coat.obj", Texture{}, glm::vec3(0.14f, 0.09f, 0.22f), 4.4f, app.model.modelScale());
	const Texture handTexture = assets.loadImage(L"models/hand.png");
	const Texture leftArmTexture = assets.loadImage(L"models/arm_and_leg.png");
	const Texture leftLegTexture = assets.loadImage(L"models/left_leg.png");
	const Texture rightLegTexture = assets.loadImage(L"models/right_leg.png");
	app.limbSegments.load(modelRoot / L"Arm_and_Leg.obj", leftArmTexture);
	app.hands.load(modelRoot / L"Hand.obj", handTexture);
	app.leftFoot.load(modelRoot / L"left_feet.obj", leftLegTexture);
	app.rightFoot.load(modelRoot / L"right_feet.obj", rightLegTexture);
	const std::array<const wchar_t*, 4> swordFiles = {
		L"sealedsword.obj",
		L"firstsealsword.obj",
		L"secondsealedsword.obj",
		L"Laevateinn.obj"
	};
	const std::array<const wchar_t*, 4> swordTextureFiles = {
		L"models/Sealedsword.png",
		L"models/FirstSealsword.png",
		L"models/Secomd_seal_sword.png",
		L"models/Laevateinn.png"
	};
	for (size_t i = 0; i < app.swordModels.size(); ++i) {
		const Texture swordTexture = assets.loadImage(swordTextureFiles[i]);
		app.swordModels[i].load(modelRoot / swordFiles[i], swordTexture, glm::vec3(0.82f), 2.8f);
	}
	const std::array<const wchar_t*, 4> thrownSwordFiles = {
		L"sealedsword_nal.obj",
		L"Firstsealsword_nal.obj",
		L"secondsealedsword_nal.obj",
		L"Laevateinn.obj"
	};
	const std::array<const wchar_t*, 4> thrownSwordTextureFiles = {
		L"models/Sealedsword.png",
		L"models/FirstSealsword.png",
		L"models/Secomd_seal_sword.png",
		L"models/Laevateinn.png"
	};
	for (size_t i = 0; i < app.thrownSwordModels.size(); ++i) {
		const Texture swordTexture = assets.loadImage(thrownSwordTextureFiles[i]);
		app.thrownSwordModels[i].load(modelRoot / thrownSwordFiles[i], swordTexture, glm::vec3(0.82f), 2.8f);
	}
	const std::array<std::array<const wchar_t*, 6>, 4> skillFiles = {{
		{{
			L"wrath1/111px-Stomping_1_Matthias.png",
			L"pride2/111px-Don't_Let_Somethin'_Like_This_Break_Ya_Matthias.png",
			L"envy3/111px-Stamp_of_Vengeance_Matthias.png",
			L"wrath1/111px-Punting_1_Matthias.png",
			L"envy1/111px-Rule_Violation_1_Matthias.png",
			L"envy3/111px-Gonna_Unbox_a_Layer_of_Packaging_Matthias.png"
		}},
		{{
			L"wrath1/111px-Stomping_2_Matthias.png",
			L"pride2/111px-Are_Ya_Feelin'_the_Heat_Matthias.png",
			L"envy3/111px-Stamp_of_Vengeance_2_Matthias.png",
			L"wrath1/111px-Punting_2_Matthias.png",
			L"envy1/111px-Rule_Violation_2_Matthias.png",
			L"envy3/111px-Looks_Like_I_Gotta_Unbox_the_Second_Layer,_too._Matthias.png"
		}},
		{{
			L"wrath1/111px-Stomping_3_Matthias.png",
			L"pride2/111px-Time_to_Melt_Away_and_Die_Matthias.png",
			L"envy3/111px-Stamp_of_Complete_and_Total_Extermination_Matthias.png",
			L"wrath1/111px-Punting_3_Matthias.png",
			L"envy1/111px-Rule_Violation_3_Matthias.png",
			L"envy3/111px-Lævateinn_-_Been_a_While_Since_I_Had_to_Unbox_the_Whole_Thing!_Matthias.png"
		}},
		{{
			L"wrath1/111px-I'll_Gut_Ya_Like_a_Fish_Matthias.png",
			L"pride2/111px-Gut_Stab_Lævateinn_Matthias.png",
			L"envy3/111px-Complete_and_Total_Extermination_Lævateinn_Matthias.png",
			nullptr,
			L"envy1/111px-Rule_Violation_4_Matthias.png",
			L"envy3/111px-Good_Girl._Your_Sacrifice_for_the_Family_Won't_Be_Forgotten._Outis_Icon.png"
		}}
	}};
	for (size_t stage = 0; stage < skillFiles.size(); ++stage) {
		for (size_t skill = 0; skill < skillFiles[stage].size(); ++skill) {
			if (skillFiles[stage][skill]) app.skills[stage][skill] = assets.loadImage(skillFiles[stage][skill]);
		}
	}
	const std::array<const wchar_t*, 4> frameFiles = {
		L"wrath1/168px-Wrath1.png",
		L"pride2/168px-Pride2.png",
		L"envy1/168px-Envy1.png",
		L"envy3/168px-Envy3.png"
	};
	for (size_t i = 0; i < frameFiles.size(); ++i) app.skillFrames[i] = assets.loadImage(frameFiles[i]);
	const std::array<const wchar_t*, 4> backgroundFiles = {
		L"wrath1/168px-Wrath1BG.png",
		L"pride2/168px-Pride2BG.png",
		L"envy1/168px-Envy1BG.png",
		L"envy3/168px-Envy3BG.png"
	};
	for (size_t i = 0; i < backgroundFiles.size(); ++i) app.skillBackgrounds[i] = assets.loadImage(backgroundFiles[i]);
	glfwGetFramebufferSize(window, &app.framebufferWidth, &app.framebufferHeight);
	app.canvas.resize(app.framebufferWidth, app.framebufferHeight);
	glfwSetWindowUserPointer(window, &app);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetFramebufferSizeCallback(window, framebufferCallback);

	const fs::path startMusicPath = assetRoot / L"Middle_Finger_Sanjou_Cut.wav";
	const fs::path mainMusicPath = assetRoot / L"Matthias_Theme.wav";
	std::vector<char> startMusicWave;
	std::vector<char> mainMusicWave;
	if (!loadBackgroundMusicAtReducedVolume(startMusicPath, startMusicWave)) {
		startMusicWave.clear();
		std::wcerr << L"Could not load background music: " << startMusicPath << L'\n';
	}
	if (!loadBackgroundMusicAtReducedVolume(mainMusicPath, mainMusicWave)) {
		mainMusicWave.clear();
		std::wcerr << L"Could not load background music: " << mainMusicPath << L'\n';
	}
	Screen musicScreen = Screen::Start;
	bool musicPlaying = false;
	bool previousMusicEnabled = false;
	// 화면과 설정 상태에 맞춰 배경 음악을 시작하거나 멈춘다.
	auto updateBackgroundMusic = [&]() {
		Screen requestedScreen = musicScreen;
		if (app.screen == Screen::Start || app.screen == Screen::Main) requestedScreen = app.screen;
		const bool trackChanged = requestedScreen != musicScreen;
		musicScreen = requestedScreen;
		const bool musicJustEnabled = app.musicEnabled && !previousMusicEnabled;
		previousMusicEnabled = app.musicEnabled;

		if (!app.musicEnabled) {
			if (musicPlaying) PlaySoundW(nullptr, nullptr, 0);
			musicPlaying = false;
			return;
		}

		if (trackChanged || musicJustEnabled) {
			const fs::path& requestedPath = musicScreen == Screen::Main ? mainMusicPath : startMusicPath;
			const std::vector<char>& requestedWave = musicScreen == Screen::Main ? mainMusicWave : startMusicWave;
			musicPlaying = !requestedWave.empty() && PlaySoundW(
				reinterpret_cast<LPCWSTR>(requestedWave.data()), nullptr,
				SND_MEMORY | SND_ASYNC | SND_LOOP | SND_NODEFAULT) != FALSE;
			if (!musicPlaying) {
				std::wcerr << L"Could not play background music: " << requestedPath << L'\n';
			}
		}
	};
	updateBackgroundMusic();

	// 입력 처리, 게임 상태 갱신, 화면 그리기를 프레임마다 반복한다.
	double previousFrameTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		updateVoicePlayback(app);
		updateSoundEffectPlayback(app);
		const double now = glfwGetTime();
		const float deltaTime = std::clamp(static_cast<float>(now - previousFrameTime), 0.0f, 0.05f);
		previousFrameTime = now;
		updateCharacterMovement(app, deltaTime);

		double cursorX = 0, cursorY = 0;
		glfwGetCursorPos(window, &cursorX, &cursorY);
		float designX = 0, designY = 0;
		cursorToDesign(app, cursorX, cursorY, designX, designY);
		app.startHovered = app.screen == Screen::Start && inside(designX, designY, START_BUTTON);
		updateBackgroundMusic();

		app.canvas.begin();
		if (app.screen == Screen::Start) { 
			drawStartScreen(app); 
		}
		else if (app.screen == Screen::Main) {
			drawMainScreen(app, now);
		}
		else if (app.screen == Screen::ConfirmReturn) {
			drawMainScreen(app, app.pauseStartedAt);
			drawReturnConfirmation(app);
		}
		else {
			drawStartScreen(app);
			drawExitConfirmation(app);
		}
		app.canvas.end();
		glfwSwapBuffers(window);
	}

	// 종료 시 재생·그래픽·창 리소스를 생성 순서에 맞춰 해제한다.
	stopVoicePlayback(app);
	stopSoundEffectPlayback(app);
	PlaySoundW(nullptr, nullptr, 0);
	app.ground.release();
	app.model.release();
	app.bookshelf.release();
	app.enemyModel.release();
	app.kiModel.release();
	app.raModel.release();
	app.limbSegments.release();
	app.hands.release();
	app.leftFoot.release();
	app.rightFoot.release();
	app.coat.release();
	for (ObjModel& sword : app.swordModels) sword.release();
	for (ObjModel& sword : app.thrownSwordModels) sword.release();
	assets.release();
	app.canvas.release();
	Gdiplus::GdiplusShutdown(gdiplusToken);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
