#include "WavLoader.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4];        // "RIFF"
    uint32_t fileSize;       // 文件大小 - 8
    char     wave[4];        // "WAVE"
    char     fmt_[4];        // "fmt "
    uint32_t fmtSize;        // fmt chunk 大小
    uint16_t audioFormat;    // 1=PCM, 3=IEEE float
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

bool LoadWavFile(const std::string& path, std::vector<float>& outSamples, int& outSampleRate) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    WavHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return false; }

    // 验证 RIFF/WAVE
    if (memcmp(hdr.riff, "RIFF", 4) != 0 || memcmp(hdr.wave, "WAVE", 4) != 0) {
        fclose(fp); return false;
    }

    outSampleRate = (int)hdr.sampleRate;
    int channels = (int)hdr.numChannels;
    int bits = (int)hdr.bitsPerSample;
    int format = (int)hdr.audioFormat;

    // 跳过 fmt chunk 的额外字节（如果有）
    if (hdr.fmtSize > 16) {
        fseek(fp, hdr.fmtSize - 16, SEEK_CUR);
    }

    // 查找 data chunk
    char chunkId[4];
    uint32_t chunkSize = 0;
    while (fread(chunkId, 1, 4, fp) == 4) {
        fread(&chunkSize, 4, 1, fp);
        if (memcmp(chunkId, "data", 4) == 0) break;
        fseek(fp, chunkSize, SEEK_CUR);
    }

    if (memcmp(chunkId, "data", 4) != 0) { fclose(fp); return false; }

    int totalSamples = chunkSize / (bits / 8);
    int monoSamples  = totalSamples / channels;
    outSamples.resize(monoSamples);

    if (format == 1 && bits == 16) {
        std::vector<int16_t> raw(totalSamples);
        fread(raw.data(), sizeof(int16_t), totalSamples, fp);
        for (int i = 0; i < monoSamples; ++i) {
            // 取第一个通道
            outSamples[i] = (float)raw[i * channels] / 32768.0f;
        }
    } else if (format == 3 && bits == 32) {
        std::vector<float> raw(totalSamples);
        fread(raw.data(), sizeof(float), totalSamples, fp);
        for (int i = 0; i < monoSamples; ++i) {
            outSamples[i] = raw[i * channels];
        }
    } else {
        // 不支持的格式
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}
