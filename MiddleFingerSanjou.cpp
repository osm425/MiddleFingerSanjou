#include "GameState.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

int WINAPI wWinMain(HINSTANCE , HINSTANCE, PWSTR, int) {
	GLFWwindow* window = initwindow(1280, 720, "Middle Finger Sanjou - UI Prototype");
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
	const Texture sealedSwordTexture = assets.loadImage(L"models/Sealedsword.png");
	app.thrownSealedSword.load(modelRoot / L"sealedsword_nal.obj", sealedSwordTexture, glm::vec3(0.82f), 2.8f);
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
	Screen musicScreen = Screen::Start;
	bool musicPlaying = false;
	bool previousMusicEnabled = false;
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
			musicPlaying = PlaySoundW(requestedPath.c_str(), nullptr,
				SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT) != FALSE;
			if (!musicPlaying) {
				std::wcerr << L"Could not play background music: " << requestedPath << L'\n';
			}
		}
	};
	updateBackgroundMusic();

	double previousFrameTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		updateVoicePlayback(app);
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

	stopVoicePlayback(app);
	PlaySoundW(nullptr, nullptr, 0);
	app.ground.release();
	app.model.release();
	app.limbSegments.release();
	app.hands.release();
	app.leftFoot.release();
	app.rightFoot.release();
	app.coat.release();
	for (ObjModel& sword : app.swordModels) sword.release();
	app.thrownSealedSword.release();
	assets.release();
	app.canvas.release();
	Gdiplus::GdiplusShutdown(gdiplusToken);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
