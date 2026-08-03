#pragma once

#include <windows.h>
#include <string>
#include <vector>

// 预言者位置提示样式
struct OracleStyle {
    COLORREF  color;
    std::string name;  // UTF-8 名称
};

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    // 创建窗口（width/height 为窗口尺寸）
    bool Create(int width = 500, int height = 560);

    // 设置提示内容（idx: -1=清空, 0~N-1=对应模板）
    void SetPrompt(int idx);

    // 设置置信度（0~1），用于绘制能量条
    void SetConfidence(float c) { confidence = c; }

    // 设置状态文本（如"等待音频..."）
    void SetStatusText(const std::string& text);

    // 设置模板样式列表
    void SetStyles(const std::vector<OracleStyle>& styles);

    // 重绘
    void Redraw();

    // 自动移动到第二屏幕（找不到则居中主屏）
    void MoveToSecondMonitor();

    // 窗口消息循环（阻塞，应在独立线程中调用）
    void MessageLoop();

    // 获取 HWND
    HWND GetHwnd() const { return hwnd; }

    // 静态注册窗口类（确保只注册一次）
    static bool RegisterClassOnce();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd = nullptr;
    int  winWidth  = 500;
    int  winHeight = 560;

    int    promptIdx  = -1;
    float  confidence = 0.0f;
    std::string statusText = "Initializing...";

    std::vector<OracleStyle> styles;
};
