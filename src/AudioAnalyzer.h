#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>

// ============================================================
// 音频参数（默认 48kHz，与命运2默认采样率一致）
// ============================================================
constexpr int  SAMPLE_RATE    = 48000;
constexpr int  FFT_SIZE      = 2048;          // ~42.6ms 分析窗口
constexpr int  MEL_BINS      = 32;           // 梅尔特征维度
constexpr int  RING_SECONDS  = 2;            // 环形缓冲区秒数
constexpr int  RING_BUFFER_SIZE = SAMPLE_RATE * RING_SECONDS;

// 匹配参数
constexpr float SIM_THRESHOLD    = 0.82f;    // 余弦相似度阈值
constexpr int   DEBOUNCE_FRAMES = 3;         // 连续命中帧数
constexpr float NOISE_GATE_DB   = -40.0f;    // 噪声门（dB）

// 带通滤波范围（Hz）—— VOG 预言者音调大致在 300~1800Hz
constexpr float FILTER_LOW_HZ  = 250.0f;
constexpr float FILTER_HIGH_HZ = 2000.0f;

// 最大模板数量
constexpr int MAX_TEMPLATES = 7;

struct TemplateInfo {
    std::string   name;       // 显示名称（如"左前"）
    COLORREF      color;      // 提示颜色
    std::vector<float> features; // 预计算的梅尔特征
};

class AudioAnalyzer {
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    // 不允许拷贝
    AudioAnalyzer(const AudioAnalyzer&) = delete;
    AudioAnalyzer& operator=(const AudioAnalyzer&) = delete;

    // 初始化：加载模板目录
    bool Initialize(const std::string& templateDir);

    // 写入音频数据（由 WASAPI 捕获线程调用）
    void WriteAudio(const float* data, size_t frameCount);

    // 启动分析线程
    void Start();

    // 停止分析线程
    void Stop();

    // 获取当前匹配结果（-1 = 无匹配，0~N-1 = 模板索引）
    int GetCurrentMatch() const { return currentMatch.load(); }

    // 获取匹配置信度（0~1）
    float GetConfidence() const { return currentConfidence.load(); }

    // 获取当前能量（用于调试/噪声门指示）
    float GetCurrentEnergy() const { return currentEnergy.load(); }

    // 是否已初始化完成
    bool IsReady() const { return ready.load(); }

    // 获取模板数量
    int GetTemplateCount() const { return (int)templates.size(); }

    // 获取模板名称
    const std::string& GetTemplateName(int idx) const;

    // 获取模板颜色
    COLORREF GetTemplateColor(int idx) const;

private:
    void AnalyzeLoop();
    std::vector<float> Preprocess(const std::vector<float>& pcm);
    std::vector<float> ExtractMelFeatures(const std::vector<float>& pcm);
    float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    void BuildMelFilterBank();

    // 环形缓冲区
    std::vector<float> ringBuffer;
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};
    std::mutex          bufferMutex;

    // 梅尔滤波器组
    std::vector<std::vector<float>> melFilters;

    // 模板
    std::vector<TemplateInfo> templates;

    // 状态
    std::atomic<int>    currentMatch{-1};
    std::atomic<float>  currentConfidence{0.0f};
    std::atomic<float>  currentEnergy{0.0f};
    std::atomic<bool>   ready{false};
    std::atomic<bool>   running{false};

    // 防抖
    int  lastMatch   = -1;
    int  matchCount  = 0;

    // 分析线程
    std::thread analyzeThread;
};
