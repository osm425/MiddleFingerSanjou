#pragma once

#include "RenderTypes.h"
#include <memory>

// 이미지와 글자 텍스처를 로드·캐시하고 사설 글꼴/WebP 디코더를 관리한다.
class Assets {
	using WebPDecodeBgra = uint8_t* (__cdecl*)(const uint8_t*, size_t, int*, int*);
	using WebPFree = void (__cdecl*)(void*);

public:
	// 에셋 경로를 저장하고 KOTRA 글꼴·선택적 WebP 디코더를 초기화한다.
	explicit Assets(fs::path root) : root_(std::move(root)) {
		loadPrivateFont();

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

	// 객체가 소멸할 때 동적으로 연 WebP 라이브러리를 닫는다.
	~Assets() {
		if (webpModule_) FreeLibrary(webpModule_);
	}

	// PNG 등 GDI+ 지원 이미지를 읽고 BGRA 픽셀을 GPU 텍스처로 올린다.
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

	// 글자를 현재 폰트 우선순위로 래스터화하고 결과 텍스처를 캐시한다.
	const Texture& text(const std::wstring& value, int fontSize, Tint color, bool bold = false) {
		std::wostringstream keyStream;
		keyStream << fontFamilyName_ << L'|' << value << L'|' << fontSize << L'|' << bold << L'|'
			<< static_cast<int>(color.r * 255) << L'|'
			<< static_cast<int>(color.g * 255) << L'|'
			<< static_cast<int>(color.b * 255);
		const std::wstring key = keyStream.str();
		auto found = textCache_.find(key);
		if (found != textCache_.end()) return found->second;

		auto cacheText = [this, &key](const Texture& texture) -> const Texture& {
			return textCache_.emplace(key, texture).first->second;
		};

		if (privateFonts_ && !fontFamilyName_.empty()) {
			Gdiplus::FontFamily kotra(fontFamilyName_.c_str(), privateFonts_.get());
			if (kotra.GetLastStatus() == Gdiplus::Ok) {
				const Texture rendered = rasterizeText(value, fontSize, color, bold, &kotra);
				if (rendered.id) return cacheText(rendered);
			}
		}

		Gdiplus::FontFamily malgun(L"Malgun Gothic");
		if (malgun.GetLastStatus() == Gdiplus::Ok) {
			const Texture rendered = rasterizeText(value, fontSize, color, bold, &malgun);
			if (rendered.id) return cacheText(rendered);
		}
		const Texture rendered = rasterizeText(value, fontSize, color, bold, Gdiplus::FontFamily::GenericSansSerif());
		return cacheText(rendered);
	}

	// 캐시된 텍스처와 사설 글꼴 리소스를 정리한다.
	void release() {
		for (GLuint texture : ownedTextures_) glDeleteTextures(1, &texture);
		ownedTextures_.clear();
		textCache_.clear();
		privateFonts_.reset();
	}

private:
	// 프로젝트 내부 KOTRA_BOLD 폰트를 읽고 사용 가능 여부를 확인한다.
	void loadPrivateFont() {
		const fs::path fontPath = root_ / L"KOTRA_BOLD.ttf";
		std::error_code fileError;
		if (!fs::is_regular_file(fontPath, fileError)) {
			std::wcerr << L"Font not found: " << fontPath.wstring() << L"; using the system fallback." << std::endl;
			return;
		}

		auto collection = std::make_unique<Gdiplus::PrivateFontCollection>();
		Gdiplus::Status status = collection->AddFontFile(fontPath.c_str());
		if (status != Gdiplus::Ok) {
			std::wcerr << L"Could not load font file: " << fontPath.wstring()
				<< L" (GDI+ status " << static_cast<int>(status) << L"); using the system fallback." << std::endl;
			return;
		}

		Gdiplus::FontFamily families[1];
		INT familyCount = 0;
		status = collection->GetFamilies(1, families, &familyCount);
		if (status != Gdiplus::Ok || familyCount == 0) {
			std::wcerr << L"Could not read font family from: " << fontPath.wstring()
				<< L" (GDI+ status " << static_cast<int>(status) << L"); using the system fallback." << std::endl;
			return;
		}

		WCHAR familyName[LF_FACESIZE]{};
		status = families[0].GetFamilyName(familyName);
		if (status != Gdiplus::Ok || familyName[0] == L'\0') {
			std::wcerr << L"Could not read font family name from: " << fontPath.wstring()
				<< L" (GDI+ status " << static_cast<int>(status) << L"); using the system fallback." << std::endl;
			return;
		}

		Gdiplus::FontFamily testFamily(familyName, collection.get());
		if (testFamily.GetLastStatus() != Gdiplus::Ok) {
			std::wcerr << L"Could not activate font family " << familyName << L" from: " << fontPath.wstring()
				<< L"; using the system fallback." << std::endl;
			return;
		}

		fontFamilyName_ = familyName;
		privateFonts_ = std::move(collection);
	}

	// 선택된 글꼴로 문자열을 비트맵에 그린 뒤 텍스처로 업로드한다.
	Texture rasterizeText(const std::wstring& value, int fontSize, Tint color, bool bold,
		const Gdiplus::FontFamily* family) {
		if (!family) return {};
		const INT fontStyle = bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular;
		Gdiplus::Font font(family, static_cast<Gdiplus::REAL>(fontSize), fontStyle, Gdiplus::UnitPixel);
		if (font.GetLastStatus() != Gdiplus::Ok) return {};

		Gdiplus::StringFormat format;
		format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
		Gdiplus::Bitmap measureBitmap(2, 2, PixelFormat32bppARGB);
		if (measureBitmap.GetLastStatus() != Gdiplus::Ok) return {};
		Gdiplus::Graphics measureGraphics(&measureBitmap);
		if (measureGraphics.GetLastStatus() != Gdiplus::Ok) return {};
		measureGraphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
		Gdiplus::RectF layout(0, 0, 4096, 512), measured{};
		if (measureGraphics.MeasureString(value.c_str(), -1, &font, layout, &format, &measured) != Gdiplus::Ok
			|| !std::isfinite(measured.Width) || !std::isfinite(measured.Height)) return {};

		const int width = std::clamp(static_cast<int>(std::ceil(measured.Width)) + 10, 4, 4096);
		const int height = std::clamp(static_cast<int>(std::ceil(measured.Height)) + 8, 4, 512);
		Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
		if (bitmap.GetLastStatus() != Gdiplus::Ok) return {};
		Gdiplus::Graphics graphics(&bitmap);
		if (graphics.GetLastStatus() != Gdiplus::Ok) return {};
		graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
		graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
		Gdiplus::SolidBrush brush(Gdiplus::Color(255,
			static_cast<BYTE>(color.r * 255), static_cast<BYTE>(color.g * 255), static_cast<BYTE>(color.b * 255)));
		Gdiplus::RectF drawRect(2, 1, static_cast<Gdiplus::REAL>(width - 4), static_cast<Gdiplus::REAL>(height - 2));
		if (graphics.DrawString(value.c_str(), -1, &font, drawRect, &format, &brush) != Gdiplus::Ok) return {};

		Gdiplus::Rect lockRect(0, 0, width, height);
		Gdiplus::BitmapData data{};
		if (bitmap.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok) return {};
		std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
		for (int y = 0; y < height; ++y) {
			const auto* row = reinterpret_cast<const unsigned char*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride;
			std::memcpy(pixels.data() + static_cast<size_t>(y) * width * 4, row, static_cast<size_t>(width) * 4);
		}
		bitmap.UnlockBits(&data);
		return upload(width, height, pixels.data());
	}

	// 파일 헤더를 확인해 WebP 형식인지 판별한다.
	static bool isWebpFile(const fs::path& path) {
		std::ifstream input(path, std::ios::binary);
		char header[12]{};
		if (!input.read(header, sizeof(header))) return false;
		return std::memcmp(header, "RIFF", 4) == 0 && std::memcmp(header + 8, "WEBP", 4) == 0;
	}

	// 동적 WebP 라이브러리로 이미지를 디코딩하고 GPU 텍스처로 만든다.
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

	// BGRA 픽셀 데이터를 OpenGL 텍스처로 올리고 소유 목록에 등록한다.
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
	std::unique_ptr<Gdiplus::PrivateFontCollection> privateFonts_;
	std::wstring fontFamilyName_;
	HMODULE webpModule_ = nullptr;
	WebPDecodeBgra webpDecodeBgra_ = nullptr;
	WebPFree webpFree_ = nullptr;
	std::map<std::wstring, Texture> textCache_;
	std::vector<GLuint> ownedTextures_;
};

