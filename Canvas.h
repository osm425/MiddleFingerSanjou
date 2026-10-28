#pragma once

#include "RenderTypes.h"

class Canvas {
public:
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

	void resize(int width, int height) {
		frameWidth_ = std::max(1, width);
		frameHeight_ = std::max(1, height);
		scale_ = std::min(frameWidth_ / 1280.0f, frameHeight_ / 720.0f);
		offsetX_ = (frameWidth_ - 1280.0f * scale_) * 0.5f;
		offsetY_ = (frameHeight_ - 720.0f * scale_) * 0.5f;
	}

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

	void end() {
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_SCISSOR_TEST);
	}

	void rect(float x, float y, float w, float h, Tint color) {
		quad(x, y, w, h, color, 0, false);
	}

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

	void image(const Texture& texture, float x, float y, float w, float h, Tint tint = { 1, 1, 1, 1 }) {
		if (texture.id) quad(x, y, w, h, tint, texture.id, true);
	}

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

	void imageMasked(const Texture& texture, const Texture& mask, float x, float y, float w, float h,
		const Rect& maskBounds, Tint tint = { 1, 1, 1, 1 }) {
		if (!texture.id || !mask.id || maskBounds.w <= 0.0f || maskBounds.h <= 0.0f) return;
		quad(x, y, w, h, tint, texture.id, true, mask.id,
			w / maskBounds.w, h / maskBounds.h,
			(x - maskBounds.x) / maskBounds.w, (y - maskBounds.y) / maskBounds.h);
	}

	void clipTo(Rect rect) {
		const int x = static_cast<int>(std::floor(offsetX_ + rect.x * scale_));
		const int y = static_cast<int>(std::floor(frameHeight_ - (offsetY_ + (rect.y + rect.h) * scale_)));
		const int w = static_cast<int>(std::ceil(rect.w * scale_));
		const int h = static_cast<int>(std::ceil(rect.h * scale_));
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, w, h);
	}

	void endClip() { glDisable(GL_SCISSOR_TEST); }

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

	void end3D() {
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glViewport(0, 0, frameWidth_, frameHeight_);
		glUseProgram(program_);
		glUniform2f(glGetUniformLocation(program_, "uViewport"), static_cast<float>(frameWidth_), static_cast<float>(frameHeight_));
		glBindVertexArray(vao_);
	}

	void toDesign(double cursorX, double cursorY, int windowWidth, int windowHeight, float& x, float& y) const {
		const float px = static_cast<float>(cursorX) * frameWidth_ / std::max(1, windowWidth);
		const float py = static_cast<float>(cursorY) * frameHeight_ / std::max(1, windowHeight);
		x = (px - offsetX_) / std::max(0.001f, scale_);
		y = (py - offsetY_) / std::max(0.001f, scale_);
	}

	void release() {
		if (vbo_) glDeleteBuffers(1, &vbo_);
		if (vao_) glDeleteVertexArrays(1, &vao_);
		if (program_) glDeleteProgram(program_);
		vbo_ = vao_ = program_ = 0;
	}

private:
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

