# OracleHelper — Destiny2 VOG Oracle Tone Helper

> ⚠️ **合规声明**：本工具仅面向**听障玩家无障碍辅助**场景。使用本工具在 Raid 竞速、PvP、或普通玩家降低游戏难度，违反 Bungie《外部辅助政策》，可能导致账号处罚。使用即表示你已知悉并自行承担风险。

## 功能说明

纯用户态 Windows 音频捕获 → 梅尔频谱特征提取 → 模板余弦相似度匹配 → 第二屏幕视觉提示。

- **零注入**：不读写游戏内存、不注入 DLL、不安装驱动
- **零覆盖**：提示窗口独立存在，不叠加在游戏画面上
- **反作弊安全**：仅通过 WASAPI Loopback 捕获系统音频流

## 目录结构

```
OracleHelper/
├── CMakeLists.txt
├── README.md
├── templates/              ← 放入 oracle_1.wav ~ oracle_7.wav
│   ├── oracle_1.wav
│   ├── oracle_2.wav
│   └── ...
├── thirdparty/kissfft/
│   └── kiss_fft.h
└── src/
    ├── main.cpp            ← 入口 + WASAPI 捕获
    ├── AudioAnalyzer.h
    ├── AudioAnalyzer.cpp   ← 核心：预处理 / 梅尔特征 / 模板匹配
    ├── OverlayWindow.h
    ├── OverlayWindow.cpp   ← 第二屏幕提示窗口
    ├── WavLoader.h
    ├── WavLoader.cpp       ← WAV 文件加载
    └── kissfft.c           ← 轻量 FFT 实现
```

## 编译步骤（Visual Studio 2022）

### 1. 安装依赖
- **Visual Studio 2022**：勾选「使用 C++ 的桌面开发」
- 确保安装 **Windows 10/11 SDK**（≥ 10.0.19041）

### 2. 用 CMake 生成项目
```cmd
cd OracleHelper
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```
生成 `OracleHelper.sln`，用 VS2022 打开。

### 3. 编译
- 选择 **Release | x64** 配置
- 菜单 → 生成 → 生成解决方案
- 输出：`build/Release/OracleHelper.exe`

### 4. 打包分发
将以下文件一起复制到目标机器：
```
OracleHelper.exe
templates/oracle_1.wav ~ oracle_7.wav
```

## 录制模板（关键步骤）

### 准备工作
1. 进 VOG 预言者房间
2. 游戏内设置：**音乐音量 = 0，音效音量 = 100**
3. 关闭所有无关声音（语音、Discord 通知等）

### 录制每个预言者的音调
1. 打开 Windows 自带「录音机」（开始菜单搜索「录音机」）
2. 触发预言者（让队友或自己引出）
3. 每个预言者**单独录制 3~5 秒纯音调**
4. 导出为 WAV 格式（16-bit PCM，单声道或立体声均可）
5. 命名为 `oracle_1.wav` ~ `oracle_7.wav`

> 💡 命名顺序对应你在 `AudioAnalyzer.cpp` 中定义的 `defaultNames` 数组顺序。你可以修改数组内容来匹配自己的命名习惯。

### 模板放置位置
```
OracleHelper.exe
templates/
  ├── oracle_1.wav   ← 第1个预言者位置的音调
  ├── oracle_2.wav
  ├── ...
  └── oracle_7.wav
```

## 运行

1. 双击 `OracleHelper.exe`
2. 程序会自动：
   - 加载 `templates/` 目录下的模板
   - 初始化 WASAPI 音频捕获
   - 创建提示窗口并移动到**第二屏幕**
3. 打开《命运2》进入 VOG 预言者房间
4. 当预言者发出音调时，提示窗口会显示：
   - **大色块**（对应位置的标识颜色）
   - **位置名称**（如"左前"、"右后"）
   - **置信度进度条**（0~100%）

## 调参指南

在 `AudioAnalyzer.h` 中调整以下参数：

| 参数 | 默认值 | 说明 |
|---|---|---|
| `SIM_THRESHOLD` | 0.82 | 余弦相似度阈值，越高越严格 |
| `DEBOUNCE_FRAMES` | 3 | 连续命中帧数，防止抖动 |
| `NOISE_GATE_DB` | -40.0 | 噪声门（dB），低于此值视为静音 |
| `FILTER_LOW_HZ` | 250 | 带通滤波下限（Hz） |
| `FILTER_HIGH_HZ` | 2000 | 带通滤波上限（Hz） |
| `MEL_BINS` | 32 | 梅尔特征维度 |

### 调参建议
- **误报太多**（没有预言者时也提示）→ 提高 `SIM_THRESHOLD` 到 0.85~0.90
- **漏报**（有预言者但不提示）→ 降低 `SIM_THRESHOLD` 到 0.75~0.80
- **背景音乐干扰** → 缩小 `FILTER_LOW_HZ` / `FILTER_HIGH_HZ` 范围
- **响应慢** → 降低 `DEBOUNCE_FRAMES` 到 2

## 修改位置名称和颜色

在 `AudioAnalyzer.cpp` 的 `Initialize()` 函数中：

```cpp
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
```

修改为你习惯的位置名称和颜色即可。

## 常见问题

**Q: 提示窗口没有出现？**
- 检查是否有第二屏幕连接
- 如果没有第二屏幕，窗口会居中显示在主屏幕
- 确认杀毒软件没有拦截 exe

**Q: 程序提示"模板加载失败"？**
- 确认 `templates/` 目录存在且包含 `oracle_1.wav` 文件
- 确认 WAV 格式为 16-bit PCM 或 32-bit float
- 路径中不要包含中文或特殊字符

**Q: 匹配不准确？**
- 重新录制模板，确保环境安静、只有预言者音调
- 调整 `SIM_THRESHOLD` 参数
- 在游戏内把音乐音量设为 0

**Q: 会被 Bungie 封号吗？**
- 技术上不会被反作弊检测（纯用户态、零注入）
- 但 Bungie 人工审核仍可判定违规使用外部辅助
- **仅限听障玩家无障碍使用，风险自负**

## 技术架构

```
┌─────────────────────────────────────────────────────────┐
│                    Destiny 2 (游戏)                      │
└────────────────────┬────────────────────────────────────┘
                     │ 系统音频输出
                     ▼
┌─────────────────────────────────────────────────────────┐
│         Windows Audio Engine (WASAPI)                    │
└────────────────────┬────────────────────────────────────┘
                     │ Loopback 捕获 (用户态)
                     ▼
┌─────────────────────────────────────────────────────────┐
│         WasapiCapture (main.cpp)                         │
│  - IAudioCaptureClient::GetBuffer()                     │
│  - 转换为 float 单声道                                  │
└────────────────────┬────────────────────────────────────┘
                     │ 写入环形缓冲区
                     ▼
┌─────────────────────────────────────────────────────────┐
│         AudioAnalyzer (AudioAnalyzer.cpp)                │
│  1. Preprocess: 噪声门 + 汉宁窗                         │
│  2. FFT (kissfft)                                      │
│  3. 梅尔滤波器组 → 32维特征                             │
│  4. 余弦相似度匹配 → 防抖                                │
└────────────────────┬────────────────────────────────────┘
                     │ 匹配结果 (原子变量)
                     ▼
┌─────────────────────────────────────────────────────────┐
│         OverlayWindow (OverlayWindow.cpp)                │
│  - 独立 Win32 窗口                                      │
│  - 自动移动到第二屏幕                                    │
│  - 显示色块 + 位置名称 + 置信度条                        │
└─────────────────────────────────────────────────────────┘
```

## 许可证

本代码采用 MIT 许可证。kissfft 组件采用 BSD-3-Clause 许可证。

## 免责声明

本软件按"原样"提供，不附带任何明示或暗示的保证。作者不对因使用本软件导致的任何账号处罚、系统损坏或其他损失承担责任。用户应自行评估使用风险并确保遵守相关服务条款。
