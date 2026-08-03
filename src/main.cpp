// ============================================================
//  OracleHelper - Destiny2 VOG Oracle Tone Helper
//  Pure user-mode audio capture + template matching
//  Output to secondary monitor overlay window
// ============================================================
//
//  ⚠️ COMPLIANCE NOTICE ⚠️
//  This tool is provided SOLELY for accessibility assistance
//  (hearing-impaired players). Using it to gain unfair advantage
//  in raids, PvP, or speedruns violates Bungie's Terms of Service
//  and may result in account penalties. Use at your own risk.
//
//  Technical: WASAPI loopback capture (user-mode, no injection,
//  no driver install, no memory reading). Anticheat-safe.
// ============================================================

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "AudioAnalyzer.h"
#include "OverlayWindow.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

// ============================================================
//  WASAPI Audio Capture (Loopback mode)
// ============================================================
class WasapiCapture {
public:
    WasapiCapture() = default;
    ~WasapiCapture() { Shutdown(); }

    bool Initialize(AudioAnalyzer* analyzer) {
        analyzer_ = analyzer;

        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&deviceEnumerator_
        );
        if (FAILED(hr)) return false;

        hr = deviceEnumerator_->GetDefaultAudioEndpoint(
            eRender, eConsole, &device_
        );
        if (FAILED(hr)) return false;

        hr = device_->Activate(
            __uuidof(IAudioClient), CLSCTX_ALL,
            nullptr, (void**)&audioClient_
        );
        if (FAILED(hr)) return false;

        hr = audioClient_->GetMixFormat(&mixFormat_);
        if (FAILED(hr)) return false;

        // 只支持 32-bit float 或 16-bit int
        if (mixFormat_->wFormatTag != WAVE_FORMAT_FLOAT &&
            mixFormat_->wFormatTag != WAVE_FORMAT_PCM &&
            mixFormat_->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
            return false;
        }

        REFERENCE_TIME bufferDuration = 10000000; // 1 second
        hr = audioClient_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            bufferDuration, 0, mixFormat_, nullptr
        );
        if (FAILED(hr)) return false;

        hr = audioClient_->GetService(
            __uuidof(IAudioCaptureClient),
            (void**)&captureClient_
        );
        if (FAILED(hr)) return false;

        return true;
    }

    void Start() {
        if (running_.exchange(true)) return;
        audioClient_->Start();
        captureThread_ = std::thread([this]() { CaptureLoop(); });
        captureThread_.detach();
    }

    void Stop() {
        running_.store(false);
        if (audioClient_) audioClient_->Stop();
    }

    void Shutdown() {
        Stop();
        if (mixFormat_) {
            CoTaskMemFree(mixFormat_);
            mixFormat_ = nullptr;
        }
        if (captureClient_) { captureClient_->Release(); captureClient_ = nullptr; }
        if (audioClient_)  { audioClient_->Release();  audioClient_  = nullptr; }
        if (device_)       { device_->Release();       device_       = nullptr; }
        if (deviceEnumerator_) { deviceEnumerator_->Release(); deviceEnumerator_ = nullptr; }
    }

    WAVEFORMATEX* GetMixFormat() const { return mixFormat_; }

private:
    void CaptureLoop() {
        // 设置高优先级
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        // 设置 MMCSS 音频任务
        DWORD taskIndex = 0;
        HANDLE avrtHandle = AvSetMmThreadCharacteristicsW(L"Audio", &taskIndex);

        std::vector<float> conversionBuffer;
        conversionBuffer.reserve(4096);

        while (running_.load()) {
            UINT32 packetLength = 0;
            HRESULT hr = captureClient_->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;

            if (packetLength == 0) {
                // 短暂等待
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            while (packetLength > 0 && running_.load()) {
                BYTE*  data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;

                hr = captureClient_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                if (FAILED(hr)) break;

                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && analyzer_) {
                    int channels = (int)mixFormat_->nChannels;
                    conversionBuffer.resize(frames);

                    if (mixFormat_->wFormatTag == WAVE_FORMAT_FLOAT) {
                        const float* src = (const float*)data;
                        for (UINT32 i = 0; i < frames; ++i) {
                            conversionBuffer[i] = src[i * channels]; // 取第一通道
                        }
                    } else if (mixFormat_->wBitsPerSample == 16) {
                        const int16_t* src = (const int16_t*)data;
                        for (UINT32 i = 0; i < frames; ++i) {
                            conversionBuffer[i] = (float)src[i * channels] / 32768.0f;
                        }
                    } else if (mixFormat_->wBitsPerSample == 32 &&
                               mixFormat_->wFormatTag == WAVE_FORMAT_PCM) {
                        const int32_t* src = (const int32_t*)data;
                        for (UINT32 i = 0; i < frames; ++i) {
                            conversionBuffer[i] = (float)src[i * channels] / 2147483648.0f;
                        }
                    }

                    analyzer_->WriteAudio(conversionBuffer.data(), frames);
                }

                captureClient_->ReleaseBuffer(frames);
                hr = captureClient_->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }
        }

        if (avrtHandle) AvRevertMmThreadCharacteristics(avrtHandle);
    }

private:
    AudioAnalyzer*     analyzer_ = nullptr;
    IMMDeviceEnumerator* deviceEnumerator_ = nullptr;
    IMMDevice*         device_ = nullptr;
    IAudioClient*      audioClient_ = nullptr;
    IAudioCaptureClient* captureClient_ = nullptr;
    WAVEFORMATEX*      mixFormat_ = nullptr;
    std::atomic<bool>  running_{false};
    std::thread        captureThread_;
};

// ============================================================
//  Main
// ============================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // COM 初始化
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"COM 初始化失败", L"错误", MB_ICONERROR);
        return 1;
    }

    // 获取 exe 所在目录
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDirW(exePath);
    auto pos = exeDirW.find_last_of(L"\\/");
    std::string exeDir;
    if (pos != std::wstring::npos) {
        std::wstring dirW = exeDirW.substr(0, pos);
        int len = WideCharToMultiByte(CP_UTF8, 0, dirW.c_str(), -1, nullptr, 0, nullptr, nullptr);
        exeDir.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, dirW.c_str(), -1, &exeDir[0], len, nullptr, nullptr);
        if (!exeDir.empty() && exeDir.back() == '\0') exeDir.pop_back();
        exeDir += "\\";
    }

    // ---- 初始化分析器 ----
    AudioAnalyzer analyzer;
    std::string templateDir = exeDir + "templates";

    if (!analyzer.Initialize(templateDir)) {
        std::string msg = "模板加载失败！\n\n请将预言者音调模板文件放入以下目录：\n"
                          + templateDir + "\n\n"
                          "文件名格式：oracle_1.wav ~ oracle_7.wav\n"
                          "（WAV 16-bit PCM 或 32-bit float，单/双声道均可）";
        MessageBoxA(nullptr, msg.c_str(), "OracleHelper - 错误", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    // ---- 初始化 WASAPI 捕获 ----
    WasapiCapture capture;
    if (!capture.Initialize(&analyzer)) {
        MessageBoxW(nullptr, L"音频捕获初始化失败！\n请检查系统音频设备。",
                    L"OracleHelper - 错误", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    // ---- 创建覆盖窗口 ----
    OverlayWindow overlay;
    if (!overlay.Create(500, 560)) {
        MessageBoxW(nullptr, L"窗口创建失败！", L"OracleHelper - 错误", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    // 设置模板样式
    int tplCount = analyzer.GetTemplateCount();
    std::vector<OracleStyle> styles;
    for (int i = 0; i < tplCount; ++i) {
        OracleStyle s;
        s.name  = analyzer.GetTemplateName(i);
        s.color = analyzer.GetTemplateColor(i);
        styles.push_back(s);
    }
    overlay.SetStyles(styles);
    overlay.MoveToSecondMonitor(); // 自动移到第二屏幕
    overlay.SetStatusText("正在监听音频... 请进入 VOG 预言者房间");

    // ---- 启动捕获和分析 ----
    analyzer.Start();
    capture.Start();

    // ---- UI 更新线程 ----
    std::atomic<bool> uiRunning{true};
    std::thread uiThread([&]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        while (uiRunning.load()) {
            int match = analyzer.GetCurrentMatch();
            float conf = analyzer.GetConfidence();
            overlay.SetPrompt(match);
            overlay.SetConfidence(conf);
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
        }
    });

    // ---- 主线程跑窗口消息循环 ----
    overlay.MessageLoop();

    // ---- 清理 ----
    uiRunning.store(false);
    if (uiThread.joinable()) uiThread.join();
    capture.Stop();
    analyzer.Stop();

    CoUninitialize();
    return 0;
}
