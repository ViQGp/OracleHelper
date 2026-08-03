#pragma once

#include <string>
#include <vector>

// 简易 WAV 文件加载器（支持 16bit PCM / 32bit float PCM）
bool LoadWavFile(const std::string& path, std::vector<float>& outSamples, int& outSampleRate);
