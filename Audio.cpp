#include "GameState.h"

static void CALLBACK voicePlaybackCallback(HWAVEOUT, UINT message, DWORD_PTR instance, DWORD_PTR, DWORD_PTR) {
	if (message != WOM_DONE || !instance) return;
	auto* playback = reinterpret_cast<VoicePlayback*>(instance);
	playback->completed.store(true, std::memory_order_release);
}

static bool releaseVoicePlayback(VoicePlayback& playback, bool stop) {
	if (playback.device) {
		if (stop && waveOutReset(playback.device) != MMSYSERR_NOERROR) {
			OutputDebugStringW(L"waveOutReset failed while stopping voice playback.\n");
			return false;
		}
		if ((playback.header.dwFlags & WHDR_PREPARED) != 0) {
			MMRESULT result = waveOutUnprepareHeader(playback.device, &playback.header, sizeof(playback.header));
			while (result == WAVERR_STILLPLAYING) {
				if (stop && waveOutReset(playback.device) != MMSYSERR_NOERROR) {
					OutputDebugStringW(L"waveOutReset failed while releasing a playing voice.\n");
					return false;
				}
				Sleep(1);
				result = waveOutUnprepareHeader(playback.device, &playback.header, sizeof(playback.header));
			}
			if (result != MMSYSERR_NOERROR) {
				OutputDebugStringW(L"waveOutUnprepareHeader failed; retaining voice buffer and device state.\n");
				return false;
			}
		}
		if (waveOutClose(playback.device) != MMSYSERR_NOERROR) {
			OutputDebugStringW(L"waveOutClose failed; retaining voice buffer and device state.\n");
			return false;
		}
	}
	playback.device = nullptr;
	playback.header = {};
	playback.audioData.clear();
	playback.completed.store(true, std::memory_order_release);
	playback.startedAt = 0.0;
	return true;
}

static bool readPcmWave(const fs::path& path, WAVEFORMATEX& format, std::vector<char>& samples) {
	std::ifstream input(path, std::ios::binary);
	char container[4]{}, waveId[4]{};
	uint32_t riffSize = 0;
	input.read(container, sizeof(container));
	input.read(reinterpret_cast<char*>(&riffSize), sizeof(riffSize));
	input.read(waveId, sizeof(waveId));
	if (!input || std::string(container, 4) != "RIFF" || std::string(waveId, 4) != "WAVE") return false;
	input.seekg(0, std::ios::end);
	const std::streamoff fileSize = input.tellg();
	const std::streamoff riffEnd = 8 + static_cast<std::streamoff>(riffSize);
	if (fileSize < 12 || riffSize < 4 || riffEnd > fileSize) return false;
	input.seekg(12, std::ios::beg);

	bool haveFormat = false;
	bool haveSamples = false;
	while (input && input.tellg() >= 0 && input.tellg() + 8 <= riffEnd) {
		char chunkId[4]{};
		uint32_t chunkSize = 0;
		input.read(chunkId, sizeof(chunkId));
		input.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
		if (!input) break;
		const std::streamoff chunkStart = input.tellg();
		if (chunkSize > static_cast<uint64_t>(riffEnd - chunkStart)) return false;
		const std::string id(chunkId, sizeof(chunkId));
		if (id == "fmt ") {
			if (chunkSize < 16) return false;
			input.read(reinterpret_cast<char*>(&format.wFormatTag), sizeof(format.wFormatTag));
			input.read(reinterpret_cast<char*>(&format.nChannels), sizeof(format.nChannels));
			input.read(reinterpret_cast<char*>(&format.nSamplesPerSec), sizeof(format.nSamplesPerSec));
			input.read(reinterpret_cast<char*>(&format.nAvgBytesPerSec), sizeof(format.nAvgBytesPerSec));
			input.read(reinterpret_cast<char*>(&format.nBlockAlign), sizeof(format.nBlockAlign));
			input.read(reinterpret_cast<char*>(&format.wBitsPerSample), sizeof(format.wBitsPerSample));
			format.cbSize = 0;
			haveFormat = static_cast<bool>(input);
		}
		else if (id == "data") {
			samples.resize(chunkSize);
			input.read(samples.data(), static_cast<std::streamsize>(samples.size()));
			haveSamples = static_cast<bool>(input);
		}
		input.seekg(chunkStart + chunkSize + (chunkSize & 1u), std::ios::beg);
		if (haveFormat && haveSamples) break;
	}
	return haveFormat && haveSamples && format.wFormatTag == WAVE_FORMAT_PCM
		&& format.nChannels > 0 && format.nSamplesPerSec > 0 && format.nBlockAlign > 0 && !samples.empty();
}

void updateVoicePlayback(App& app) {
	for (VoicePlayback& playback : app.voicePlayback) {
		if (playback.device && playback.completed.load(std::memory_order_acquire)) {
			releaseVoicePlayback(playback, false);
		}
	}
}

void stopVoicePlayback(App& app) {
	for (VoicePlayback& playback : app.voicePlayback) releaseVoicePlayback(playback, true);
}

void playSkillVoice(App& app, const wchar_t* fileName, double now) {
	if (!app.voiceEnabled) return;
	const fs::path path = app.assetRoot / L"voice" / fileName;
	WAVEFORMATEX format{};
	std::vector<char> samples;
	if (!readPcmWave(path, format, samples)) {
		std::wcerr << L"Could not read PCM voice: " << path.wstring() << L'\n';
		return;
	}
	updateVoicePlayback(app);
	VoicePlayback* slot = nullptr;
	for (VoicePlayback& playback : app.voicePlayback) {
		if (!playback.device) { slot = &playback; break; }
	}
	if (!slot) {
		slot = &*std::min_element(app.voicePlayback.begin(), app.voicePlayback.end(),
			[](const VoicePlayback& a, const VoicePlayback& b) { return a.startedAt < b.startedAt; });
		if (!releaseVoicePlayback(*slot, true)) return;
	}
	slot->audioData = std::move(samples);
	slot->completed.store(false, std::memory_order_release);
	slot->startedAt = now;
	const MMRESULT opened = waveOutOpen(&slot->device, WAVE_MAPPER, &format,
		reinterpret_cast<DWORD_PTR>(voicePlaybackCallback), reinterpret_cast<DWORD_PTR>(slot), CALLBACK_FUNCTION);
	if (opened != MMSYSERR_NOERROR) {
		releaseVoicePlayback(*slot, false);
		std::wcerr << L"Could not open voice output: " << path.wstring() << L'\n';
		return;
	}
	slot->header = {};
	slot->header.lpData = slot->audioData.data();
	slot->header.dwBufferLength = static_cast<DWORD>(slot->audioData.size());
	if (waveOutPrepareHeader(slot->device, &slot->header, sizeof(slot->header)) != MMSYSERR_NOERROR
		|| waveOutWrite(slot->device, &slot->header, sizeof(slot->header)) != MMSYSERR_NOERROR) {
		releaseVoicePlayback(*slot, true);
		std::wcerr << L"Could not play voice: " << path.wstring() << L'\n';
	}
}

