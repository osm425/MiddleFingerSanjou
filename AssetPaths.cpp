#include "GameState.h"

// 실행 파일 위치와 작업 폴더를 바탕으로 게임 에셋 폴더를 찾는다.
fs::path findAssetRoot() {
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	const fs::path exeDir = fs::path(path).parent_path();
	const std::array<fs::path, 4> candidates = {
		exeDir / L"assets",
		exeDir.parent_path().parent_path() / L"assets",
		fs::current_path() / L"assets",
		fs::current_path().parent_path() / L"assets"
	};
	for (const auto& candidate : candidates) {
		if (fs::exists(candidate / L"models" / L"Matthias.obj")
			&& fs::exists(candidate / L"models" / L"bodytexture.png")) return candidate;
	}
	return exeDir / L"assets";
}

