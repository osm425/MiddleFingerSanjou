#pragma once

#pragma once

#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "common.cpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mmsystem.h>
#include <sstream>
#include <vector>

#pragma comment(lib, "gdiplus.lib")


namespace fs = std::filesystem;

static constexpr float SCENE_CAMERA_DISTANCE = 9.0f;
static constexpr float SCENE_CAMERA_FOV = 38.0f;
static constexpr float FIELD_Y = -3.27f;
static constexpr float FIELD_HALF_EXTENT = 50.0f;
static constexpr float CAMERA_GROUND_CLEARANCE = 0.35f;
static constexpr float CHARACTER_FIELD_MARGIN = 4.0f;
static constexpr float CHARACTER_MOVE_SPEED = 2.8f;

static glm::vec3 sceneCameraPosition(const glm::vec3& target, float yaw, float pitch) {
	const float horizontal = SCENE_CAMERA_DISTANCE * std::cos(pitch);
	const glm::vec3 offset(horizontal * std::sin(yaw), SCENE_CAMERA_DISTANCE * std::sin(pitch), horizontal * std::cos(yaw));
	return target + offset;
}

static glm::mat4 sceneViewMatrix(const glm::vec3& target, float yaw, float pitch) {
	return glm::lookAt(sceneCameraPosition(target, yaw, pitch), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

static glm::mat4 sceneProjectionMatrix(float aspect) {
	return glm::perspective(glm::radians(SCENE_CAMERA_FOV), aspect, 0.1f, 150.0f);
}

struct Tint {
	float r, g, b, a;
};

static Tint rgba(int r, int g, int b, int a = 255) {
	return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
}

struct Texture {
	GLuint id = 0;
	int width = 0;
	int height = 0;
};

struct Rect {
	float x, y, w, h;
};

static GLuint compileStage(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (ok == GL_FALSE) {
		GLint length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		std::string log(static_cast<size_t>(std::max(1, length)), '\0');
		glGetShaderInfoLog(shader, length, nullptr, log.data());
		std::cerr << "Shader compile error: " << log << std::endl;
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint makeUiProgram() {
	static const char* vertexSource = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aUv;
uniform vec2 uViewport;
out vec2 vUv;
void main() {
	vec2 clip = vec2((aPosition.x / uViewport.x) * 2.0 - 1.0,
		1.0 - (aPosition.y / uViewport.y) * 2.0);
	gl_Position = vec4(clip, 0.0, 1.0);
	vUv = aUv;
}
)GLSL";
	static const char* fragmentSource = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uTexture;
uniform sampler2D uMaskTexture;
uniform vec4 uTint;
uniform int uTextured;
uniform int uMasked;
uniform vec2 uMaskScale;
uniform vec2 uMaskOffset;
out vec4 outColor;
void main() {
	vec4 color = uTint;
	if (uTextured != 0) color *= texture(uTexture, vUv);
	if (uMasked != 0) color.a *= texture(uMaskTexture, vUv * uMaskScale + uMaskOffset).a;
	if (color.a < 0.004) discard;
	outColor = color;
}
)GLSL";

	GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSource);
	GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSource);
	if (!vs || !fs) {
		if (vs) glDeleteShader(vs);
		if (fs) glDeleteShader(fs);
		return 0;
	}
	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint ok = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (ok == GL_FALSE) {
		GLint length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		std::string log(static_cast<size_t>(std::max(1, length)), '\0');
		glGetProgramInfoLog(program, length, nullptr, log.data());
		std::cerr << "UI program link error: " << log << std::endl;
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

