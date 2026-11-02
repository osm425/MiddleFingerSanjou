#include "GameState.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

namespace {
constexpr int INITIAL_SAMPLE_SKILL = 0; // zero-based: skill 1 starts automatically

GLFWwindow* createSampleWindow(int width, int height, const char* title) {
	if (!glfwInit()) return nullptr;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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

void startSampleSkill(App& app, int skillIndex, double now) {
	if (skillIndex < 0 || skillIndex >= 6) return;
	if (!skillAvailableForStage(app.swordStage, skillIndex)) {
		std::cerr << "That skill has no animation for the current sword stage.\n";
		return;
	}
	app.skillAnimation = {};
	app.cameraDragging = false;
	app.characterPose.rootPosition = app.initialCharacterPosition;
	app.characterPose.rootYaw = app.initialCharacterYaw;
	app.runAnimationPhase = 0.0f;
	app.runAnimationBlend = 0.0f;
	app.enemyYawSet = false;
	app.enemyYaw = 0.0f;
	app.enemyHasBeenHit = false;
	app.cooldownEndsAt.fill(0.0);
	app.cooldownDurations.fill(0.0);
	activateSkill(app, skillIndex, now);
}

void sampleMouseButtonCallback(GLFWwindow* window, int button, int action, int) {
	if (button != GLFW_MOUSE_BUTTON_LEFT) return;
	App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (!app) return;
	if (action == GLFW_RELEASE) {
		app->cameraDragging = false;
		return;
	}
	if (action != GLFW_PRESS || app->screen != Screen::Main) return;
	double x = 0.0, y = 0.0;
	glfwGetCursorPos(window, &x, &y);
	float designX = 0.0f, designY = 0.0f;
	cursorToDesign(*app, x, y, designX, designY);
	for (const SkillSlot& slot : SKILL_SLOTS) {
		if (inside(designX, designY, slot.bounds)) {
			startSampleSkill(*app, slot.skillIndex, glfwGetTime());
			return;
		}
	}
	if (inside(designX, designY, MODEL_VIEW_RECT)) {
		app->cameraDragging = true;
		app->lastCameraDragX = designX;
		app->lastCameraDragY = designY;
	}
}

void sampleKeyCallback(GLFWwindow* window, int key, int, int action, int mods) {
	if (action != GLFW_PRESS) return;
	App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
	if (!app) return;
	if (key == GLFW_KEY_ESCAPE) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return;
	}
	if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT)
		&& key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
		if (skillAnimationActive(*app)) return;
		app->swordStage = key - GLFW_KEY_1;
		app->cooldownEndsAt.fill(0.0);
		app->cooldownDurations.fill(0.0);
		std::cout << "Sword stage: " << app->swordStage + 1 << "\n";
		return;
	}
	if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6) {
		startSampleSkill(*app, key - GLFW_KEY_1, glfwGetTime());
	}
	else if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_6) {
		startSampleSkill(*app, key - GLFW_KEY_KP_1, glfwGetTime());
	}
}

bool loadSampleModels(App& app, Assets& assets) {
	const fs::path modelRoot = app.assetRoot / L"models";
	const Texture bodyTexture = assets.loadImage(L"models/bodytexture.png");
	if (!app.model.load(modelRoot / L"Matthias.obj", bodyTexture)) return false;
	app.ground.initialize();

	const Texture enemyTexture = assets.loadImage(L"models/vespa1.png");
	app.enemyModel.load(modelRoot / L"Vespa.obj", enemyTexture,
		glm::vec3(0.78f, 0.68f, 0.55f), VESPA_MODEL_EXTENT);
	const Texture kiTexture = assets.loadImage(L"models/Kira_upper.png");
	const Texture raTexture = assets.loadImage(L"models/Kira_lower.png");
	app.kiModel.load(modelRoot / L"Ki.obj", kiTexture,
		glm::vec3(1.0f), VESPA_MODEL_EXTENT / 1.5f);
	app.raModel.load(modelRoot / L"Ra.obj", raTexture,
		glm::vec3(1.0f), VESPA_MODEL_EXTENT / 1.5f, app.kiModel.modelScale());
	app.coat.load(modelRoot / L"coat.obj", Texture{}, glm::vec3(0.14f, 0.09f, 0.22f),
		4.4f, app.model.modelScale());

	const Texture handTexture = assets.loadImage(L"models/hand.png");
	const Texture armTexture = assets.loadImage(L"models/arm_and_leg.png");
	const Texture leftLegTexture = assets.loadImage(L"models/left_leg.png");
	const Texture rightLegTexture = assets.loadImage(L"models/right_leg.png");
	if (!app.limbSegments.load(modelRoot / L"Arm_and_Leg.obj", armTexture)
		|| !app.hands.load(modelRoot / L"Hand.obj", handTexture)
		|| !app.leftFoot.load(modelRoot / L"left_feet.obj", leftLegTexture)
		|| !app.rightFoot.load(modelRoot / L"right_feet.obj", rightLegTexture)) return false;

	const std::array<const wchar_t*, 4> swordFiles = {
		L"sealedsword.obj", L"firstsealsword.obj", L"secondsealedsword.obj", L"Laevateinn.obj"
	};
	const std::array<const wchar_t*, 4> swordTextures = {
		L"models/Sealedsword.png", L"models/FirstSealsword.png",
		L"models/Secomd_seal_sword.png", L"models/Laevateinn.png"
	};
	const std::array<const wchar_t*, 4> thrownSwordFiles = {
		L"sealedsword_nal.obj", L"FirstSealsword_nal.obj",
		L"secondsealedsword_nal.obj", L"Laevateinn.obj"
	};
	for (size_t i = 0; i < app.swordModels.size(); ++i) {
		const Texture texture = assets.loadImage(swordTextures[i]);
		app.swordModels[i].load(modelRoot / swordFiles[i], texture, glm::vec3(0.82f), 2.8f);
		app.thrownSwordModels[i].load(modelRoot / thrownSwordFiles[i], texture, glm::vec3(0.82f), 2.8f);
	}
	return true;
}

void releaseSampleModels(App& app) {
	app.ground.release();
	app.model.release();
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
}
} // namespace

int main() {
	GLFWwindow* window = createSampleWindow(1280, 720, "Skill Animation Sample");
	if (!window) {
		std::cerr << "Could not create the OpenGL window.\n";
		return 1;
	}
	glfwSwapInterval(1);

	Gdiplus::GdiplusStartupInput startupInput;
	ULONG_PTR gdiplusToken = 0;
	if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok) {
		std::cerr << "Could not initialize GDI+.\n";
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	const fs::path assetRoot = findAssetRoot();
	Assets assets(assetRoot);
	App app;
	app.window = window;
	app.assets = &assets;
	app.assetRoot = assetRoot;
	app.screen = Screen::Main;
	app.musicEnabled = false;
	app.voiceEnabled = false;
	if (!app.canvas.initialize() || !loadSampleModels(app, assets)) {
		std::cerr << "Could not initialize the character preview. Check the assets folder.\n";
		releaseSampleModels(app);
		assets.release();
		app.canvas.release();
		Gdiplus::GdiplusShutdown(gdiplusToken);
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	// Keep the regular skill HUD available so a skill can also be started by clicking its icon.
	const std::array<std::array<const wchar_t*, 6>, 4> skillFiles = {{
		{{ L"wrath1/111px-Stomping_1_Matthias.png", L"pride2/111px-Don't_Let_Somethin'_Like_This_Break_Ya_Matthias.png",
			L"envy3/111px-Stamp_of_Vengeance_Matthias.png", L"wrath1/111px-Punting_1_Matthias.png",
			L"envy1/111px-Rule_Violation_1_Matthias.png", L"envy3/111px-Gonna_Unbox_a_Layer_of_Packaging_Matthias.png" }},
		{{ L"wrath1/111px-Stomping_2_Matthias.png", L"pride2/111px-Are_Ya_Feelin'_the_Heat_Matthias.png",
			L"envy3/111px-Stamp_of_Vengeance_2_Matthias.png", L"wrath1/111px-Punting_2_Matthias.png",
			L"envy1/111px-Rule_Violation_2_Matthias.png", L"envy3/111px-Looks_Like_I_Gotta_Unbox_the_Second_Layer,_too._Matthias.png" }},
		{{ L"wrath1/111px-Stomping_3_Matthias.png", L"pride2/111px-Time_to_Melt_Away_and_Die_Matthias.png",
			L"envy3/111px-Stamp_of_Complete_and_Total_Extermination_Matthias.png", L"wrath1/111px-Punting_3_Matthias.png",
			L"envy1/111px-Rule_Violation_3_Matthias.png", L"envy3/111px-Lævateinn_-_Been_a_While_Since_I_Had_to_Unbox_the_Whole_Thing!_Matthias.png" }},
		{{ L"wrath1/111px-I'll_Gut_Ya_Like_a_Fish_Matthias.png", L"pride2/111px-Gut_Stab_Lævateinn_Matthias.png",
			L"envy3/111px-Complete_and_Total_Extermination_Lævateinn_Matthias.png", nullptr,
			L"envy1/111px-Rule_Violation_4_Matthias.png", L"envy3/111px-Good_Girl._Your_Sacrifice_for_the_Family_Won't_Be_Forgotten._Outis_Icon.png" }}
	}};
	for (size_t stage = 0; stage < skillFiles.size(); ++stage) {
		for (size_t skill = 0; skill < skillFiles[stage].size(); ++skill) {
			if (skillFiles[stage][skill]) app.skills[stage][skill] = assets.loadImage(skillFiles[stage][skill]);
		}
	}
	const std::array<const wchar_t*, 4> frameFiles = {
		L"wrath1/168px-Wrath1.png", L"pride2/168px-Pride2.png",
		L"envy1/168px-Envy1.png", L"envy3/168px-Envy3.png"
	};
	const std::array<const wchar_t*, 4> backgroundFiles = {
		L"wrath1/168px-Wrath1BG.png", L"pride2/168px-Pride2BG.png",
		L"envy1/168px-Envy1BG.png", L"envy3/168px-Envy3BG.png"
	};
	for (size_t i = 0; i < frameFiles.size(); ++i) {
		app.skillFrames[i] = assets.loadImage(frameFiles[i]);
		app.skillBackgrounds[i] = assets.loadImage(backgroundFiles[i]);
	}

	glfwGetFramebufferSize(window, &app.framebufferWidth, &app.framebufferHeight);
	app.canvas.resize(app.framebufferWidth, app.framebufferHeight);
	glfwSetWindowUserPointer(window, &app);
	glfwSetKeyCallback(window, sampleKeyCallback);
	glfwSetMouseButtonCallback(window, sampleMouseButtonCallback);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetFramebufferSizeCallback(window, framebufferCallback);

	std::cout << "1-6 or click a skill icon: play | Ctrl+Shift+1-4: sword stage | drag left mouse: orbit camera | Esc: exit\n";
	startSampleSkill(app, INITIAL_SAMPLE_SKILL, glfwGetTime());
	double previousFrameTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		const double now = glfwGetTime();
		const float deltaTime = std::clamp(static_cast<float>(now - previousFrameTime), 0.0f, 0.05f);
		previousFrameTime = now;
		updateSkillAnimation(app, deltaTime, now);

		app.canvas.begin();
		drawMainScreen(app, now);
		app.canvas.end();
		glfwSwapBuffers(window);
	}

	releaseSampleModels(app);
	assets.release();
	app.canvas.release();
	Gdiplus::GdiplusShutdown(gdiplusToken);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}





