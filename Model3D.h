#pragma once

#include "RenderTypes.h"

struct ObjVertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
};

struct ObjReference {
	int position = -1;
	int texcoord = -1;
	int normal = -1;
};

class ObjModel {
public:
	bool load(const fs::path& path, const Texture& albedoTexture,
		const glm::vec3& defaultColor = glm::vec3(0.78f, 0.68f, 0.55f),
		float targetExtent = 4.4f, float fixedModelScale = 0.0f) {
		std::ifstream input(path);
		if (!input) {
			std::wcerr << L"Could not open OBJ model: " << path.wstring() << std::endl;
			return false;
		}

		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> texcoords;
		std::vector<ObjVertex> triangles;
		std::string line;
		while (std::getline(input, line)) {
			std::istringstream row(line);
			std::string kind;
			row >> kind;
			if (kind == "v") {
				glm::vec3 position;
				if (row >> position.x >> position.y >> position.z) positions.push_back(position);
			}
			else if (kind == "vn") {
				glm::vec3 normal;
				if (row >> normal.x >> normal.y >> normal.z) normals.push_back(normal);
			}
			else if (kind == "vt") {
				glm::vec2 texcoord;
				if (row >> texcoord.x >> texcoord.y) texcoords.push_back(texcoord);
			}
			else if (kind == "f") {
				std::vector<ObjReference> face;
				std::string token;
				while (row >> token) {
					if (!token.empty() && token[0] == '#') break;
					const size_t firstSlash = token.find('/');
					const size_t secondSlash = firstSlash == std::string::npos ? std::string::npos : token.find('/', firstSlash + 1);
					const std::string positionPart = token.substr(0, firstSlash);
					const std::string texcoordPart = firstSlash == std::string::npos ? std::string()
						: token.substr(firstSlash + 1, secondSlash == std::string::npos ? std::string::npos : secondSlash - firstSlash - 1);
					const std::string normalPart = secondSlash == std::string::npos ? std::string() : token.substr(secondSlash + 1);
					auto resolve = [](const std::string& part, size_t count) {
						if (part.empty()) return -1;
						try {
							const int objIndex = std::stoi(part);
							const int index = objIndex > 0 ? objIndex - 1 : static_cast<int>(count) + objIndex;
							return index >= 0 && index < static_cast<int>(count) ? index : -1;
						}
						catch (...) { return -1; }
					};
					face.push_back({ resolve(positionPart, positions.size()), resolve(texcoordPart, texcoords.size()), resolve(normalPart, normals.size()) });
				}
				if (face.size() < 3) continue;
				for (size_t i = 1; i + 1 < face.size(); ++i) {
					const ObjReference refs[3] = { face[0], face[i], face[i + 1] };
					if (refs[0].position < 0 || refs[1].position < 0 || refs[2].position < 0) continue;
					const glm::vec3 p0 = positions[refs[0].position];
					const glm::vec3 p1 = positions[refs[1].position];
					const glm::vec3 p2 = positions[refs[2].position];
					glm::vec3 faceNormal = glm::cross(p1 - p0, p2 - p0);
					if (glm::dot(faceNormal, faceNormal) < 1e-10f) faceNormal = glm::vec3(0, 0, 1);
					else faceNormal = glm::normalize(faceNormal);
					for (const auto& ref : refs) {
						glm::vec3 normal = ref.normal >= 0 ? normals[ref.normal] : faceNormal;
						if (glm::dot(normal, normal) < 1e-10f) normal = faceNormal;
						else normal = glm::normalize(normal);
						const glm::vec2 texcoord = ref.texcoord >= 0 ? texcoords[ref.texcoord] : glm::vec2(0.0f);
						triangles.push_back({ positions[ref.position], normal, texcoord });
					}
				}
			}
		}

		if (positions.empty() || triangles.empty()) {
			std::wcerr << L"OBJ model has no renderable faces: " << path.wstring() << std::endl;
			return false;
		}
		glm::vec3 minimum(std::numeric_limits<float>::max());
		glm::vec3 maximum(std::numeric_limits<float>::lowest());
		for (const ObjVertex& vertex : triangles) {
			minimum = glm::min(minimum, vertex.position);
			maximum = glm::max(maximum, vertex.position);
		}
		const glm::vec3 center = (minimum + maximum) * 0.5f;
		const glm::vec3 extent = maximum - minimum;
		const float largestExtent = std::max({ extent.x, extent.y, extent.z });
		if (largestExtent <= 1e-6f) return false;
		const float modelScale = fixedModelScale > 1e-6f ? fixedModelScale : targetExtent / largestExtent;
		sourceCenter_ = center;
		modelScale_ = modelScale;
		localBoundsMin_ = (minimum - center) * modelScale;
		localBoundsMax_ = (maximum - center) * modelScale;
		localOrigin_ = -center * modelScale;
		std::vector<float> vertexData;
		vertexData.reserve(triangles.size() * 8);
		for (const ObjVertex& vertex : triangles) {
			const glm::vec3 position = (vertex.position - center) * modelScale;
			vertexData.insert(vertexData.end(), { position.x, position.y, position.z,
				vertex.normal.x, vertex.normal.y, vertex.normal.z, vertex.texcoord.x, vertex.texcoord.y });
		}

		static const char* vertexSource = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform int uShadowPass;
uniform float uGroundY;
uniform float uShadowOffset;
uniform float uShadowOpacity;
uniform vec3 uShadowDirection;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vTexcoord;
out float vShadowAlpha;
void main() {
	vec4 worldPosition = uModel * vec4(aPosition, 1.0);
	vShadowAlpha = 0.0;
	if (uShadowPass != 0) {
		float sourceHeight = max(0.0, worldPosition.y - uGroundY);
		vec3 rayDirection = normalize(uShadowDirection);
		float distanceToGround = max(0.0, (uGroundY - worldPosition.y) / rayDirection.y);
		worldPosition.xyz += rayDirection * distanceToGround;
		worldPosition.y = uGroundY + uShadowOffset;
		vShadowAlpha = uShadowOpacity * exp(-sourceHeight * 0.08);
	}
	vWorldPosition = worldPosition.xyz;
	vWorldNormal = mat3(transpose(inverse(uModel))) * aNormal;
	vTexcoord = aTexcoord;
	gl_Position = uProjection * uView * worldPosition;
}
)GLSL";
		static const char* fragmentSource = R"GLSL(
#version 330 core
in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vTexcoord;
in float vShadowAlpha;
uniform int uShadowPass;
uniform vec3 uCameraPosition;
uniform sampler2D uAlbedoTexture;
uniform int uHasAlbedoTexture;
uniform vec3 uDefaultColor;
out vec4 outColor;
void main() {
	if (uShadowPass != 0) {
		outColor = vec4(0.008, 0.007, 0.011, vShadowAlpha);
		return;
	}
	vec3 normal = normalize(vWorldNormal);
	vec3 lightDirection = normalize(vec3(-0.45, 0.82, 0.56));
	vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
	vec3 halfDirection = normalize(lightDirection + viewDirection);
	float diffuse = max(dot(normal, lightDirection), 0.0);
	float specular = pow(max(dot(normal, halfDirection), 0.0), 28.0) * 0.16;
	vec4 albedo = uHasAlbedoTexture != 0
		? texture(uAlbedoTexture, vec2(vTexcoord.x, 1.0 - vTexcoord.y))
		: vec4(uDefaultColor, 1.0);
	vec3 litColor = albedo.rgb * (0.30 + diffuse * 0.68) + vec3(specular);
	outColor = vec4(litColor, albedo.a);
}
)GLSL";
		const GLuint vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource);
		const GLuint fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource);
		if (!vertexShader || !fragmentShader) return false;
		program_ = glCreateProgram();
		glAttachShader(program_, vertexShader);
		glAttachShader(program_, fragmentShader);
		glLinkProgram(program_);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		GLint linked = GL_FALSE;
		glGetProgramiv(program_, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			GLint length = 0;
			glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
			std::string log(static_cast<size_t>(std::max(1, length)), '\0');
			glGetProgramInfoLog(program_, length, nullptr, log.data());
			std::cerr << "3D model shader link error: " << log << std::endl;
			glDeleteProgram(program_);
			program_ = 0;
			return false;
		}

		glGenVertexArrays(1, &vao_);
		glGenBuffers(1, &vbo_);
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexData.size() * sizeof(float)), vertexData.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, reinterpret_cast<void*>(0));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, reinterpret_cast<void*>(sizeof(float) * 3));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, reinterpret_cast<void*>(sizeof(float) * 6));
		glEnableVertexAttribArray(2);
		glBindVertexArray(0);
		vertexCount_ = static_cast<GLsizei>(triangles.size());
		albedoTexture_ = albedoTexture.id;
		defaultColor_ = defaultColor;
		return true;
	}

	void draw(float aspect, float cameraYaw, float cameraPitch, const glm::vec3& cameraTarget, const glm::mat4& modelTransform) const {
		if (!program_ || !vao_ || vertexCount_ == 0) return;
		const glm::vec3 cameraPosition = sceneCameraPosition(cameraTarget, cameraYaw, cameraPitch);
		const glm::mat4 view = sceneViewMatrix(cameraTarget, cameraYaw, cameraPitch);
		const glm::mat4 projection = sceneProjectionMatrix(aspect);
		glUseProgram(program_);
		glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE, glm::value_ptr(modelTransform));
		glUniformMatrix4fv(glGetUniformLocation(program_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(program_, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniform1i(glGetUniformLocation(program_, "uShadowPass"), 1);
		glUniform1f(glGetUniformLocation(program_, "uGroundY"), FIELD_Y);
		glUniform1f(glGetUniformLocation(program_, "uShadowOffset"), 0.018f);
		glUniform1f(glGetUniformLocation(program_, "uShadowOpacity"), 0.18f);
		glUniform3f(glGetUniformLocation(program_, "uShadowDirection"), 0.45f, -1.0f, -0.56f);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
		glDepthMask(GL_TRUE);
		glUniform1i(glGetUniformLocation(program_, "uShadowPass"), 0);
		glUniform3fv(glGetUniformLocation(program_, "uCameraPosition"), 1, glm::value_ptr(cameraPosition));
		glUniform3fv(glGetUniformLocation(program_, "uDefaultColor"), 1, glm::value_ptr(defaultColor_));
		glUniform1i(glGetUniformLocation(program_, "uAlbedoTexture"), 0);
		glUniform1i(glGetUniformLocation(program_, "uHasAlbedoTexture"), albedoTexture_ ? 1 : 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, albedoTexture_);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
	}

	bool loaded() const { return program_ && vao_ && vertexCount_ > 0; }
	glm::vec3 upperOriginAnchor() const { return { localOrigin_.x, localBoundsMax_.y, localOrigin_.z }; }
	glm::vec3 lowerOriginAnchor() const { return { localOrigin_.x, localBoundsMin_.y, localOrigin_.z }; }
	float modelScale() const { return modelScale_; }
	glm::vec3 sourceCenter() const { return sourceCenter_; }
	glm::vec3 scaledSourceCenter() const { return sourceCenter_ * modelScale_; }
	glm::vec3 swordGripAnchor() const {
		const float handleOffset = (localBoundsMax_.y - localBoundsMin_.y) * 0.13f;
		return { localOrigin_.x, localBoundsMin_.y + handleOffset, localOrigin_.z };
	}

	void release() {
		if (vbo_) glDeleteBuffers(1, &vbo_);
		if (vao_) glDeleteVertexArrays(1, &vao_);
		if (program_) glDeleteProgram(program_);
		vbo_ = vao_ = program_ = 0;
		vertexCount_ = 0;
	}

private:
	GLuint vao_ = 0;
	GLuint vbo_ = 0;
	GLuint program_ = 0;
	GLuint albedoTexture_ = 0;
	GLsizei vertexCount_ = 0;
	glm::vec3 defaultColor_{ 0.78f, 0.68f, 0.55f };
	glm::vec3 localBoundsMin_{ 0.0f };
	glm::vec3 localBoundsMax_{ 0.0f };
	glm::vec3 localOrigin_{ 0.0f };
	glm::vec3 sourceCenter_{ 0.0f };
	float modelScale_ = 1.0f;
};

class GroundPlane {
public:
	bool initialize() {
		static const char* vertexSource = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
void main() {
	gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)GLSL";
		static const char* fragmentSource = R"GLSL(
#version 330 core
uniform vec3 uColor;
out vec4 outColor;
void main() {
	outColor = vec4(uColor, 1.0);
}
)GLSL";
		const GLuint vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource);
		const GLuint fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource);
		if (!vertexShader || !fragmentShader) {
			if (vertexShader) glDeleteShader(vertexShader);
			if (fragmentShader) glDeleteShader(fragmentShader);
			return false;
		}
		program_ = glCreateProgram();
		glAttachShader(program_, vertexShader);
		glAttachShader(program_, fragmentShader);
		glLinkProgram(program_);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		GLint linked = GL_FALSE;
		glGetProgramiv(program_, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			GLint length = 0;
			glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
			std::string log(static_cast<size_t>(std::max(1, length)), '\0');
			glGetProgramInfoLog(program_, length, nullptr, log.data());
			std::cerr << "Ground shader link error: " << log << std::endl;
			glDeleteProgram(program_);
			program_ = 0;
			return false;
		}

		const float vertices[] = {
			-1.0f, 0.0f, -1.0f,  -1.0f, 0.0f,  1.0f,   1.0f, 0.0f, -1.0f,
			-1.0f, 0.0f,  1.0f,   1.0f, 0.0f,  1.0f,   1.0f, 0.0f, -1.0f
		};
		glGenVertexArrays(1, &vao_);
		glGenBuffers(1, &vbo_);
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
		initializeShadow();
		return true;
	}

	void draw(float aspect, float cameraYaw, float cameraPitch, const glm::vec3& cameraTarget) const {
		if (!program_ || !vao_) return;
		const glm::mat4 modelTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, FIELD_Y, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(FIELD_HALF_EXTENT, 1.0f, FIELD_HALF_EXTENT));
		const glm::mat4 view = sceneViewMatrix(cameraTarget, cameraYaw, cameraPitch);
		const glm::mat4 projection = sceneProjectionMatrix(aspect);
		glUseProgram(program_);
		glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE, glm::value_ptr(modelTransform));
		glUniformMatrix4fv(glGetUniformLocation(program_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(program_, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniform3f(glGetUniformLocation(program_, "uColor"), 0.17f, 0.18f, 0.19f);
		glBindVertexArray(vao_);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void drawShadow(float aspect, float cameraYaw, float cameraPitch, const glm::vec3& cameraTarget,
		const glm::vec3& characterPosition, float characterYaw, float footClearance) const {
		if (!shadowProgram_ || !shadowVao_) return;
		const float height = std::clamp(footClearance, 0.0f, 6.0f);
		const float radiusX = 0.72f + height * 0.10f;
		const float radiusZ = 0.42f + height * 0.08f;
		const float opacity = std::clamp(0.20f - height * 0.02f, 0.08f, 0.20f);
		const glm::vec3 shadowPosition(characterPosition.x, FIELD_Y + 0.012f, characterPosition.z);
		const glm::mat4 modelTransform = glm::translate(glm::mat4(1.0f), shadowPosition)
			* glm::rotate(glm::mat4(1.0f), characterYaw, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(radiusX, 1.0f, radiusZ));
		const glm::mat4 view = sceneViewMatrix(cameraTarget, cameraYaw, cameraPitch);
		const glm::mat4 projection = sceneProjectionMatrix(aspect);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		glUseProgram(shadowProgram_);
		glUniformMatrix4fv(glGetUniformLocation(shadowProgram_, "uModel"), 1, GL_FALSE, glm::value_ptr(modelTransform));
		glUniformMatrix4fv(glGetUniformLocation(shadowProgram_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(shadowProgram_, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniform1f(glGetUniformLocation(shadowProgram_, "uOpacity"), opacity);
		glBindVertexArray(shadowVao_);
		glDrawArrays(GL_TRIANGLE_FAN, 0, shadowVertexCount_);
		glBindVertexArray(0);
		glUseProgram(0);
		glDepthMask(GL_TRUE);
	}

	void release() {
		if (vbo_) glDeleteBuffers(1, &vbo_);
		if (vao_) glDeleteVertexArrays(1, &vao_);
		if (program_) glDeleteProgram(program_);
		if (shadowVbo_) glDeleteBuffers(1, &shadowVbo_);
		if (shadowVao_) glDeleteVertexArrays(1, &shadowVao_);
		if (shadowProgram_) glDeleteProgram(shadowProgram_);
		vbo_ = vao_ = program_ = 0;
		shadowVbo_ = shadowVao_ = shadowProgram_ = 0;
		shadowVertexCount_ = 0;
	}

private:
	void initializeShadow() {
		static const char* vertexSource = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec2 vLocal;
void main() {
	vLocal = aPosition.xz;
	gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)GLSL";
		static const char* fragmentSource = R"GLSL(
#version 330 core
in vec2 vLocal;
uniform float uOpacity;
out vec4 outColor;
void main() {
	float edge = length(vLocal);
	float alpha = (1.0 - smoothstep(0.42, 1.0, edge)) * uOpacity;
	if (alpha < 0.003) discard;
	outColor = vec4(0.012, 0.010, 0.016, alpha);
}
)GLSL";
		const GLuint vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource);
		const GLuint fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource);
		if (!vertexShader || !fragmentShader) {
			if (vertexShader) glDeleteShader(vertexShader);
			if (fragmentShader) glDeleteShader(fragmentShader);
			return;
		}
		shadowProgram_ = glCreateProgram();
		glAttachShader(shadowProgram_, vertexShader);
		glAttachShader(shadowProgram_, fragmentShader);
		glLinkProgram(shadowProgram_);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		GLint linked = GL_FALSE;
		glGetProgramiv(shadowProgram_, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			GLint length = 0;
			glGetProgramiv(shadowProgram_, GL_INFO_LOG_LENGTH, &length);
			std::string log(static_cast<size_t>(std::max(1, length)), '\0');
			glGetProgramInfoLog(shadowProgram_, length, nullptr, log.data());
			std::cerr << "Shadow shader link error: " << log << std::endl;
			glDeleteProgram(shadowProgram_);
			shadowProgram_ = 0;
			return;
		}

		constexpr int segments = 48;
		std::vector<float> vertices;
		vertices.reserve(static_cast<size_t>(segments + 2) * 3);
		vertices.insert(vertices.end(), { 0.0f, 0.0f, 0.0f });
		for (int i = 0; i <= segments; ++i) {
			const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(segments);
			vertices.insert(vertices.end(), { std::cos(angle), 0.0f, std::sin(angle) });
		}
		shadowVertexCount_ = static_cast<GLsizei>(segments + 2);
		glGenVertexArrays(1, &shadowVao_);
		glGenBuffers(1, &shadowVbo_);
		glBindVertexArray(shadowVao_);
		glBindBuffer(GL_ARRAY_BUFFER, shadowVbo_);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
	}

	GLuint vao_ = 0;
	GLuint vbo_ = 0;
	GLuint program_ = 0;
	GLuint shadowVao_ = 0;
	GLuint shadowVbo_ = 0;
	GLuint shadowProgram_ = 0;
	GLsizei shadowVertexCount_ = 0;
};

