#pragma once

#include "RenderTypes.h"

class Assets {
	using WebPDecodeBgra = uint8_t* (__cdecl*)(const uint8_t*, size_t, int*, int*);
	using WebPFree = void (__cdecl*)(void*);

public:
	explicit Assets(fs::path root) : root_(std::move(root)) {
		webpModule_ = LoadLibraryW((root_ / L"libwebp-7.dll").c_str());
		if (webpModule_) {
			webpDecodeBgra_ = reinterpret_cast<WebPDecodeBgra>(GetProcAddress(webpModule_, "WebPDecodeBGRA"));
			webpFree_ = reinterpret_cast<WebPFree>(GetProcAddress(webpModule_, "WebPFree"));
		}
		if (!webpDecodeBgra_ || !webpFree_) {
			if (webpModule_) FreeLibrary(webpModule_);
			webpModule_ = nullptr;
			std::wcerr << L"WebP decoder missing: " << (root_ / L"libwebp-7.dll").wstring() << std::endl;
		}
	}

	~Assets() {
		if (webpModule_) FreeLibrary(webpModule_);
	}

	Texture loadImage(const wchar_t* name) {
		const fs::path path = root_ / name;
		if (isWebpFile(path)) return loadWebp(path);

		Gdiplus::Bitmap bitmap(path.c_str());
		if (bitmap.GetLastStatus() != Gdiplus::Ok) {
			std::wcerr << L"Could not load image: " << path.wstring() << std::endl;
			return {};
		}
		const UINT width = bitmap.GetWidth();
		const UINT height = bitmap.GetHeight();
		Gdiplus::Rect bounds(0, 0, static_cast<INT>(width), static_cast<INT>(height));
		Gdiplus::BitmapData data{};
		if (bitmap.LockBits(&bounds, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok) return {};
		std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
		for (UINT y = 0; y < height; ++y) {
			const auto* row = reinterpret_cast<const unsigned char*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride;
			std::memcpy(pixels.data() + static_cast<size_t>(y) * width * 4, row, static_cast<size_t>(width) * 4);
		}
		bitmap.UnlockBits(&data);
		return upload(static_cast<int>(width), static_cast<int>(height), pixels.data());
	}

	const Texture& text(const std::wstring& value, int fontSize, Tint color, bool bold = false) {
		std::wostringstream keyStream;
		keyStream << value << L'|' << fontSize << L'|' << bold << L'|'
			<< static_cast<int>(color.r * 255) << L'|'
			<< static_cast<int>(color.g * 255) << L'|'
			<< static_cast<int>(color.b * 255);
		const std::wstring key = keyStream.str();
		auto found = textCache_.find(key);
		if (found != textCache_.end()) return found->second;

		Gdiplus::FontFamily malgun(L"Malgun Gothic");
		const Gdiplus::FontFamily* family = malgun.GetLastStatus() == Gdiplus::Ok
			? &malgun : Gdiplus::FontFamily::GenericSansSerif();
		Gdiplus::Font font(family, static_cast<Gdiplus::REAL>(fontSize), bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		Gdiplus::StringFormat format;
		format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
		Gdiplus::Bitmap measureBitmap(2, 2, PixelFormat32bppARGB);
		Gdiplus::Graphics measureGraphics(&measureBitmap);
		measureGraphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
		Gdiplus::RectF layout(0, 0, 4096, 512), measured{};
		measureGraphics.MeasureString(value.c_str(), -1, &font, layout, &format, &measured);
		const int width = std::max(4, static_cast<int>(std::ceil(measured.Width)) + 10);
		const int height = std::max(4, static_cast<int>(std::ceil(measured.Height)) + 8);
		Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
		Gdiplus::Graphics graphics(&bitmap);
		graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
		graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
		Gdiplus::SolidBrush brush(Gdiplus::Color(255,
			static_cast<BYTE>(color.r * 255), static_cast<BYTE>(color.g * 255), static_cast<BYTE>(color.b * 255)));
		Gdiplus::RectF drawRect(2, 1, static_cast<Gdiplus::REAL>(width - 4), static_cast<Gdiplus::REAL>(height - 2));
		graphics.DrawString(value.c_str(), -1, &font, drawRect, &format, &brush);
		Gdiplus::Rect lockRect(0, 0, width, height);
		Gdiplus::BitmapData data{};
		if (bitmap.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok) {
			return textCache_.emplace(key, Texture{}).first->second;
		}
		std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
		for (int y = 0; y < height; ++y) {
			const auto* row = reinterpret_cast<const unsigned char*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride;
			std::memcpy(pixels.data() + static_cast<size_t>(y) * width * 4, row, static_cast<size_t>(width) * 4);
		}
		bitmap.UnlockBits(&data);
		return textCache_.emplace(key, upload(width, height, pixels.data())).first->second;
	}

	void release() {
		for (GLuint texture : ownedTextures_) glDeleteTextures(1, &texture);
		ownedTextures_.clear();
		textCache_.clear();
	}

private:
	static bool isWebpFile(const fs::path& path) {
		std::ifstream input(path, std::ios::binary);
		char header[12]{};
		if (!input.read(header, sizeof(header))) return false;
		return std::memcmp(header, "RIFF", 4) == 0 && std::memcmp(header + 8, "WEBP", 4) == 0;
	}

	Texture loadWebp(const fs::path& path) {
		if (!webpDecodeBgra_ || !webpFree_) return {};
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		if (!input) return {};
		const std::streamoff fileSize = static_cast<std::streamoff>(input.tellg());
		if (fileSize <= 0) return {};
		std::vector<uint8_t> encoded(static_cast<size_t>(fileSize));
		input.seekg(0, std::ios::beg);
		if (!input.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()))) return {};

		int width = 0, height = 0;
		uint8_t* decoded = webpDecodeBgra_(encoded.data(), encoded.size(), &width, &height);
		if (!decoded || width <= 0 || height <= 0) {
			if (decoded) webpFree_(decoded);
			std::wcerr << L"Could not decode WebP image: " << path.wstring() << std::endl;
			return {};
		}
		Texture texture = upload(width, height, decoded);
		webpFree_(decoded);
		return texture;
	}

	Texture upload(int width, int height, const unsigned char* bgra) {
		Texture texture{};
		texture.width = width;
		texture.height = height;
		glGenTextures(1, &texture.id);
		glBindTexture(GL_TEXTURE_2D, texture.id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, bgra);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		ownedTextures_.push_back(texture.id);
		return texture;
	}

	fs::path root_;
	HMODULE webpModule_ = nullptr;
	WebPDecodeBgra webpDecodeBgra_ = nullptr;
	WebPFree webpFree_ = nullptr;
	std::map<std::wstring, Texture> textCache_;
	std::vector<GLuint> ownedTextures_;
};

