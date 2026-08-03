#include "AudioAnalyzer.h"
#include "WavLoader.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <cstring>
#include "kiss_fft.h"

// ============================================================
// 构造函数 / 析构函数
// ============================================================
AudioAnalyzer::AudioAnalyzer()
    : ringBuffer(RING_BUFFER_SIZE) {}

AudioAnalyzer::~AudioAnalyzer() {
    Stop();
}

// ============================================================
// 初始化：加载模板目录
// ============================================================
bool AudioAnalyzer::Initialize(const std::string& templateDir) {
    // 尝试加载 1~7 号模板
    const char* defaultNames[MAX_TEMPLATES] = {
        "左前", "右前", "左后", "右后", "中左", "中右", "正中间"
    };
    const COLORREF defaultColors[MAX_TEMPLATES] = {
        RGB(255,  50,  50),   // 红
        RGB( 50,  50, 255),   // 蓝
        RGB( 50, 220,  50),   // 绿
        RGB(255, 230,  50),   // 黄
        RGB(220,  80, 255),   // 紫
        RGB( 50, 230, 230),   // 青
        RGB(255, 140,  30)    // 橙
    };

    int loaded = 0;
    for (int i = 1; i <= MAX_TEMPLATES; ++i) {
        std::string path = templateDir + "/oracle_" + std::to_string(i) + ".wav";
        std::vector<float> samples;
        int sr = 0;
        if (!LoadWavFile(path, samples, sr)) {
            // 尝试 .WAV 大写
            path = templateDir + "/oracle_" + std::to_string(i) + ".WAV";
            if (!LoadWavFile(path, samples, sr)) {
                break; // 没有更多模板了
            }
        }

        // 重采样到 SAMPLE_RATE（如果不同）
        std::vector<float> resampled;
        if (sr == SAMPLE_RATE) {
            resampled = samples;
        } else {
            // 简单线性重采样
            resampled.resize((size_t)((double)samples.size() * SAMPLE_RATE / sr));
            for (size_t j = 0; j < resampled.size(); ++j) {
                double srcIdx = (double)j * sr / SAMPLE_RATE;
                size_t i0 = (size_t)srcIdx;
                size_t i1 = i0 + 1 < samples.size() ? i0 + 1 : i0;
                double frac = srcIdx - i0;
                resampled[j] = (float)(samples[i0] * (1.0 - frac) + samples[i1] * frac);
            }
        }

        // 预处理 + 提取特征
        auto preprocessed = Preprocess(resampled);
        auto feats = ExtractMelFeatures(preprocessed);

        TemplateInfo tpl;
        tpl.name = defaultNames[i - 1];
        tpl.color = defaultColors[i - 1];
        tpl.features = feats;

        templates.push_back(tpl);
        ++loaded;
    }

    if (loaded == 0) return false;

    // 构建梅尔滤波器组
    BuildMelFilterBank();

    ready.store(true);
    return true;
}

// ============================================================
// 写入音频数据（线程安全）
// ============================================================
void AudioAnalyzer::WriteAudio(const float* data, size_t frameCount) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    for (size_t i = 0; i < frameCount; ++i) {
        size_t pos = writePos.fetch_add(1) % RING_BUFFER_SIZE;
        ringBuffer[pos] = data[i];
    }
}

// ============================================================
// 启动 / 停止分析线程
// ============================================================
void AudioAnalyzer::Start() {
    if (running.load()) return;
    running.store(true);
    analyzeThread = std::thread([this]() { AnalyzeLoop(); });
    analyzeThread.detach();
}

void AudioAnalyzer::Stop() {
    running.store(false);
    if (analyzeThread.joinable()) {
        analyzeThread.join();
    }
}

// ============================================================
// 分析主循环
// ============================================================
void AudioAnalyzer::AnalyzeLoop() {
    std::vector<float> pcmChunk(FFT_SIZE);

    while (running.load()) {
        // 计算可用数据量
        size_t w = writePos.load();
        size_t r = readPos.load();
        size_t available = (w >= r) ? (w - r) : (w + RING_BUFFER_SIZE - r);

        if (available < FFT_SIZE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // 读取 FFT_SIZE 个采样
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            for (int i = 0; i < FFT_SIZE; ++i) {
                pcmChunk[i] = ringBuffer[(readPos.fetch_add(1)) % RING_BUFFER_SIZE];
            }
        }

        // 计算能量（用于噪声门指示）
        float energy = 0.0f;
        for (int i = 0; i < FFT_SIZE; ++i) {
            energy += pcmChunk[i] * pcmChunk[i];
        }
        energy = sqrtf(energy / FFT_SIZE);
        currentEnergy.store(energy);

        // 噪声门
        float gateLinear = powf(10.0f, NOISE_GATE_DB / 20.0f);
        if (energy < gateLinear) {
            // 静音期，重置
            currentMatch.store(-1);
            currentConfidence.store(0.0f);
            matchCount = 0;
            lastMatch = -1;
            continue;
        }

        // 预处理 + 特征提取
        auto processed = Preprocess(pcmChunk);
        auto features = ExtractMelFeatures(processed);

        // 与所有模板计算相似度
        float maxSim = 0.0f;
        int   bestMatch = -1;
        for (size_t t = 0; t < templates.size(); ++t) {
            float sim = CosineSimilarity(features, templates[t].features);
            if (sim > maxSim) {
                maxSim = sim;
                bestMatch = (int)t;
            }
        }

        // 防抖逻辑
        if (bestMatch == lastMatch && maxSim >= SIM_THRESHOLD) {
            matchCount++;
            if (matchCount >= DEBOUNCE_FRAMES) {
                currentMatch.store(bestMatch);
                currentConfidence.store(maxSim);
            }
        } else {
            lastMatch = bestMatch;
            matchCount = 1;
        }

        // 低于阈值 → 清除
        if (maxSim < SIM_THRESHOLD) {
            currentMatch.store(-1);
            currentConfidence.store(0.0f);
            matchCount = 0;
            lastMatch = -1;
        }
    }
}

// ============================================================
// 预处理：噪声门 + 汉宁窗
// ============================================================
std::vector<float> AudioAnalyzer::Preprocess(const std::vector<float>& pcm) {
    std::vector<float> result(pcm.size());
    float gateLinear = powf(10.0f, NOISE_GATE_DB / 20.0f);

    for (size_t i = 0; i < pcm.size(); ++i) {
        float s = pcm[i];
        if (fabsf(s) < gateLinear) s = 0.0f;

        // 汉宁窗减少频谱泄漏
        float window = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (float)i / (float)(pcm.size() - 1));
        result[i] = s * window;
    }
    return result;
}

// ============================================================
// 构建梅尔滤波器组
// ============================================================
void AudioAnalyzer::BuildMelFilterBank() {
    melFilters.clear();
    melFilters.resize(MEL_BINS, std::vector<float>(FFT_SIZE / 2 + 1, 0.0f));

    // 频率 → 梅尔
    auto hz2mel = [](float hz) { return 2595.0f * log10f(1.0f + hz / 700.0f); };
    // 梅尔 → 频率
    auto mel2hz = [](float mel) { return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f); };

    float melLow  = hz2mel(FILTER_LOW_HZ);
    float melHigh = hz2mel(FILTER_HIGH_HZ);
    float melStep = (melHigh - melLow) / (float)(MEL_BINS + 1);

    // 每个 bin 对应的频率
    auto bin2hz = [](int bin, int fftSize, int sampleRate) {
        return (float)bin * (float)sampleRate / (float)fftSize;
    };

    for (int m = 1; m <= MEL_BINS; ++m) {
        float f_low  = mel2hz(melLow  + (m - 1) * melStep);
        float f_ctr  = mel2hz(melLow  + m * melStep);
        float f_high = mel2hz(melLow  + (m + 1) * melStep);

        for (int k = 0; k <= FFT_SIZE / 2; ++k) {
            float freq = bin2hz(k, FFT_SIZE, SAMPLE_RATE);
            if (freq >= f_low && freq <= f_high) {
                if (freq <= f_ctr) {
                    melFilters[m - 1][k] = (freq - f_low) / (f_ctr - f_low + 1e-6f);
                } else {
                    melFilters[m - 1][k] = (f_high - freq) / (f_high - f_ctr + 1e-6f);
                }
            }
        }
    }
}

// ============================================================
// 提取梅尔频谱特征
// ============================================================
std::vector<float> AudioAnalyzer::ExtractMelFeatures(const std::vector<float>& pcm) {
    // FFT
    kiss_fftr_cfg cfg = kiss_fftr_alloc(FFT_SIZE, 0, nullptr, nullptr);
    std::vector<kiss_fft_cpx> fftOut(FFT_SIZE / 2 + 1);
    kiss_fftr(cfg, pcm.data(), fftOut.data());

    // 计算幅度谱
    std::vector<float> magnitude(FFT_SIZE / 2 + 1);
    for (int i = 0; i <= FFT_SIZE / 2; ++i) {
        magnitude[i] = sqrtf(fftOut[i].r * fftOut[i].r + fftOut[i].i * fftOut[i].i);
    }

    // 应用梅尔滤波器组
    std::vector<float> melFeats(MEL_BINS, 0.0f);
    for (int m = 0; m < MEL_BINS; ++m) {
        float sum = 0.0f;
        for (int k = 0; k <= FFT_SIZE / 2; ++k) {
            sum += magnitude[k] * melFilters[m][k];
        }
        // 对数压缩
        melFeats[m] = logf(sum + 1e-6f);
    }

    kiss_fftr_free(cfg);

    // L2 归一化
    float norm = 0.0f;
    for (int i = 0; i < MEL_BINS; ++i) norm += melFeats[i] * melFeats[i];
    norm = sqrtf(norm) + 1e-6f;
    for (int i = 0; i < MEL_BINS; ++i) melFeats[i] /= norm;

    return melFeats;
}

// ============================================================
// 余弦相似度
// ============================================================
float AudioAnalyzer::CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return 0.0f;
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    return dot / (sqrtf(na) * sqrtf(nb) + 1e-6f);
}

// ============================================================
// 获取模板名称 / 颜色
// ============================================================
const std::string& AudioAnalyzer::GetTemplateName(int idx) const {
    static std::string empty = "";
    if (idx < 0 || idx >= (int)templates.size()) return empty;
    return templates[idx].name;
}

COLORREF AudioAnalyzer::GetTemplateColor(int idx) const {
    if (idx < 0 || idx >= (int)templates.size()) return RGB(128, 128, 128);
    return templates[idx].color;
}
