#pragma once

#include "RenderTypes.h"

// 고정된 1280x720 디자인 좌표계에서 2D UI와 일부 3D 영역을 그린다.
class Canvas {
public:
	// 셰이더와 사각형 그리기에 필요한 OpenGL 버퍼를 준비한다.
	bool initialize() {
		program_ = makeUiProgram();
		if (!program_) return false;
		glGenVertexArrays(1, &vao_);
		glGenBuffers(1, &vbo_);
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
		glEnableVertexAttribArray(1);
		glBindVertexArray(0);
		return true;
	}

	// 창 크기에 맞춰 UI의 축척과 가운데 정렬 여백을 다시 계산한다.
	void resize(int width, int height) {
		frameWidth_ = std::max(1, width);
		frameHeight_ = std::max(1, height);
		scale_ = std::min(frameWidth_ / 1280.0f, frameHeight_ / 720.0f);
		offsetX_ = (frameWidth_ - 1280.0f * scale_) * 0.5f;
		offsetY_ = (frameHeight_ - 720.0f * scale_) * 0.5f;
	}

	// UI 렌더링을 시작하고 viewport·blend·셰이더 상태를 설정한다.
	void begin() {
		glViewport(0, 0, frameWidth_, frameHeight_);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glClearColor(0.025f, 0.026f, 0.035f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(program_);
		glUniform2f(glGetUniformLocation(program_, "uViewport"), static_cast<float>(frameWidth_), static_cast<float>(frameHeight_));
		glUniform1i(glGetUniformLocation(program_, "uTexture"), 0);
		glUniform1i(glGetUniformLocation(program_, "uMaskTexture"), 1);
		glBindVertexArray(vao_);
	}

	// UI 렌더링 종료 후 공유 OpenGL 상태를 정리한다.
	void end() {
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_SCISSOR_TEST);
	}

	// 단색 사각형을 그린다.
	void rect(float x, float y, float w, float h, Tint color) {
		quad(x, y, w, h, color, 0, false);
	}

	// 두 점을 잇는 두께 있는 선을 사각형 삼각형으로 그린다.
	void line(float x1, float y1, float x2, float y2, float width, Tint color) {
		const float dx = x2 - x1;
		const float dy = y2 - y1;
		const float length = std::sqrt(dx * dx + dy * dy);
		if (length < 0.001f || width <= 0.0f) return;
		const float halfWidth = width * 0.5f;
		const float ox = -dy / length * halfWidth;
		const float oy = dx / length * halfWidth;
		const float leftTopX = offsetX_ + (x1 + ox) * scale_;
		const float leftTopY = offsetY_ + (y1 + oy) * scale_;
		const float leftBottomX = offsetX_ + (x1 - ox) * scale_;
		const float leftBottomY = offsetY_ + (y1 - oy) * scale_;
		const float rightTopX = offsetX_ + (x2 + ox) * scale_;
		const float rightTopY = offsetY_ + (y2 + oy) * scale_;
		const float rightBottomX = offsetX_ + (x2 - ox) * scale_;
		const float rightBottomY = offsetY_ + (y2 - oy) * scale_;
		const float vertices[24] = {
			leftTopX, leftTopY, 0, 0, leftBottomX, leftBottomY, 0, 1, rightBottomX, rightBottomY, 1, 1,
			leftTopX, leftTopY, 0, 0, rightBottomX, rightBottomY, 1, 1, rightTopX, rightTopY, 1, 0
		};
		glUniform4f(glGetUniformLocation(program_, "uTint"), color.r, color.g, color.b, color.a);
		glUniform1i(glGetUniformLocation(program_, "uTextured"), 0);
		glUniform1i(glGetUniformLocation(program_, "uMasked"), 0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		if (sizeof(vertices) > bufferCapacity_) {
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), nullptr, GL_DYNAMIC_DRAW);
			bufferCapacity_ = sizeof(vertices);
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	// 모서리가 둥근 사각형을 직사각형과 원호 조각으로 그린다.
	void roundedRect(float x, float y, float w, float h, float radius, Tint color) {
		if (w <= 0.0f || h <= 0.0f) return;
		const float r = std::clamp(radius, 0.0f, std::min(w, h) * 0.5f);
		if (r <= 0.0f) {
			rect(x, y, w, h, color);
			return;
		}
		rect(x + r, y, w - 2.0f * r, h, color);
		rect(x, y + r, r, h - 2.0f * r, color);
		rect(x + w - r, y + r, r, h - 2.0f * r, color);
		const float quarterTurn = 1.5707963267948966192f;
		sector(x + r, y + r, r, 3.1415926535897932385f, quarterTurn, color);
		sector(x + w - r, y + r, r, -quarterTurn, quarterTurn, color);
		sector(x + w - r, y + h - r, r, 0.0f, quarterTurn, color);
		sector(x + r, y + h - r, r, quarterTurn, quarterTurn, color);
	}

	// 지정한 각도 구간의 원형 부채꼴을 그린다.
	void sector(float centerX, float centerY, float radius, float startAngle, float sweepAngle, Tint color) {
		if (radius <= 0.0f || std::abs(sweepAngle) < 0.0001f) return;
		const int segments = std::max(1, static_cast<int>(std::ceil(std::abs(sweepAngle) * radius * scale_ / 3.0f)));
		std::vector<float> vertices;
		vertices.reserve(static_cast<size_t>(segments + 2) * 4);
		const auto addVertex = [this, &vertices](float x, float y) {
			vertices.push_back(offsetX_ + x * scale_);
			vertices.push_back(offsetY_ + y * scale_);
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
		};
		addVertex(centerX, centerY);
		for (int i = 0; i <= segments; ++i) {
			const float angle = startAngle + sweepAngle * static_cast<float>(i) / static_cast<float>(segments);
			addVertex(centerX + std::cos(angle) * radius, centerY + std::sin(angle) * radius);
		}

		glUniform4f(glGetUniformLocation(program_, "uTint"), color.r, color.g, color.b, color.a);
		glUniform1i(glGetUniformLocation(program_, "uTextured"), 0);
		glUniform1i(glGetUniformLocation(program_, "uMasked"), 0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		const size_t bytes = vertices.size() * sizeof(float);
		if (bytes > bufferCapacity_) {
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), nullptr, GL_DYNAMIC_DRAW);
			bufferCapacity_ = bytes;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), vertices.data());
		glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
	}

	// 텍스처를 지정한 UI 사각형에 배치한다.
	void image(const Texture& texture, float x, float y, float w, float h, Tint tint = { 1, 1, 1, 1 }) {
		if (texture.id) quad(x, y, w, h, tint, texture.id, true);
	}

	// 텍스처 비율을 보존하면서 화면 viewport를 빈틈없이 덮는다.
	void imageCoverViewport(const Texture& texture) {
		if (!texture.id || texture.width <= 0 || texture.height <= 0) return;
		const float designWidth = frameWidth_ / scale_;
		const float designHeight = frameHeight_ / scale_;
		const float designX = -offsetX_ / scale_;
		const float designY = -offsetY_ / scale_;
		const float coverScale = std::max(designWidth / texture.width, designHeight / texture.height);
		const float drawWidth = texture.width * coverScale;
		const float drawHeight = texture.height * coverScale;
		image(texture,
			designX + (designWidth - drawWidth) * 0.5f,
			designY + (designHeight - drawHeight) * 0.5f,
			drawWidth,
			drawHeight);
	}

	// 별도 마스크 텍스처의 지정 영역으로 이미지를 잘라 그린다.
	void imageMasked(const Texture& texture, const Texture& mask, float x, float y, float w, float h,
		const Rect& maskBounds, Tint tint = { 1, 1, 1, 1 }) {
		if (!texture.id || !mask.id || maskBounds.w <= 0.0f || maskBounds.h <= 0.0f) return;
		quad(x, y, w, h, tint, texture.id, true, mask.id,
			w / maskBounds.w, h / maskBounds.h,
			(x - maskBounds.x) / maskBounds.w, (y - maskBounds.y) / maskBounds.h);
	}

	// UI 렌더링을 지정한 사각형 범위로 자른다.
	void clipTo(Rect rect) {
		const int x = static_cast<int>(std::floor(offsetX_ + rect.x * scale_));
		const int y = static_cast<int>(std::floor(frameHeight_ - (offsetY_ + (rect.y + rect.h) * scale_)));
		const int w = static_cast<int>(std::ceil(rect.w * scale_));
		const int h = static_cast<int>(std::ceil(rect.h * scale_));
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, w, h);
	}

	// 앞서 설정한 UI 자르기 상태를 해제한다.
	void endClip() { glDisable(GL_SCISSOR_TEST); }

	// 지정 사각형에 3D viewport와 깊이 버퍼를 설정한다.
	void begin3D(Rect rect) {
		const int x = static_cast<int>(std::floor(offsetX_ + rect.x * scale_));
		const int y = static_cast<int>(std::floor(frameHeight_ - (offsetY_ + (rect.y + rect.h) * scale_)));
		const int width = static_cast<int>(std::ceil(rect.w * scale_));
		const int height = static_cast<int>(std::ceil(rect.h * scale_));
		glViewport(x, y, width, height);
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, width, height);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	// 3D viewport를 끝내고 UI viewport와 렌더링 상태를 복구한다.
	void end3D() {
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glViewport(0, 0, frameWidth_, frameHeight_);
		glUseProgram(program_);
		glUniform2f(glGetUniformLocation(program_, "uViewport"), static_cast<float>(frameWidth_), static_cast<float>(frameHeight_));
		glBindVertexArray(vao_);
	}

	// 실제 창 커서 좌표를 UI 디자인 좌표로 변환한다.
	void toDesign(double cursorX, double cursorY, int windowWidth, int windowHeight, float& x, float& y) const {
		const float px = static_cast<float>(cursorX) * frameWidth_ / std::max(1, windowWidth);
		const float py = static_cast<float>(cursorY) * frameHeight_ / std::max(1, windowHeight);
		x = (px - offsetX_) / std::max(0.001f, scale_);
		y = (py - offsetY_) / std::max(0.001f, scale_);
	}

	// Canvas가 소유한 OpenGL 버퍼와 셰이더를 해제한다.
	void release() {
		if (vbo_) glDeleteBuffers(1, &vbo_);
		if (vao_) glDeleteVertexArrays(1, &vao_);
		if (program_) glDeleteProgram(program_);
		vbo_ = vao_ = program_ = 0;
	}

private:
	// 색상·텍스처·마스크를 적용한 사각형 두 개의 삼각형을 그린다.
	void quad(float x, float y, float w, float h, Tint color, GLuint texture, bool textured,
		GLuint maskTexture = 0, float maskScaleX = 1.0f, float maskScaleY = 1.0f,
		float maskOffsetX = 0.0f, float maskOffsetY = 0.0f) {
		const float left = offsetX_ + x * scale_;
		const float top = offsetY_ + y * scale_;
		const float right = left + w * scale_;
		const float bottom = top + h * scale_;
		const float vertices[24] = {
			left, top, 0, 0,  right, top, 1, 0,  right, bottom, 1, 1,
			left, top, 0, 0,  right, bottom, 1, 1,  left, bottom, 0, 1
		};
		glUniform4f(glGetUniformLocation(program_, "uTint"), color.r, color.g, color.b, color.a);
		glUniform1i(glGetUniformLocation(program_, "uTextured"), textured ? 1 : 0);
		glUniform1i(glGetUniformLocation(program_, "uMasked"), maskTexture ? 1 : 0);
		glUniform2f(glGetUniformLocation(program_, "uMaskScale"), maskScaleX, maskScaleY);
		glUniform2f(glGetUniformLocation(program_, "uMaskOffset"), maskOffsetX, maskOffsetY);
		if (textured) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);
		}
		if (maskTexture) {
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, maskTexture);
		}
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		if (sizeof(vertices) > bufferCapacity_) {
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), nullptr, GL_DYNAMIC_DRAW);
			bufferCapacity_ = sizeof(vertices);
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	GLuint program_ = 0;
	GLuint vao_ = 0;
	GLuint vbo_ = 0;
	int frameWidth_ = 1280;
	int frameHeight_ = 720;
	float scale_ = 1.0f;
	float offsetX_ = 0.0f;
	float offsetY_ = 0.0f;
	size_t bufferCapacity_ = sizeof(float) * 24;
};

