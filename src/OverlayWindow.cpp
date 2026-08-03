#include "OverlayWindow.h"
#include <windowsx.h>
#include <stdexcept>

// ============================================================
// 静态窗口类注册
// ============================================================
const wchar_t* ORACLE_HELPER_CLASS = L"OracleHelperWindowClass";

bool OverlayWindow::RegisterClassOnce() {
    static bool registered = false;
    if (registered) return true;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = OverlayWindow::WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = ORACLE_HELPER_CLASS;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    if (!RegisterClassExW(&wc)) return false;
    registered = true;
    return true;
}

// ============================================================
// 构造函数 / 析构函数
// ============================================================
OverlayWindow::OverlayWindow() = default;
OverlayWindow::~OverlayWindow() = default;

// ============================================================
// 创建窗口
// ============================================================
bool OverlayWindow::Create(int width, int height) {
    if (!RegisterClassOnce()) return false;

    winWidth  = width;
    winHeight = height;

    hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        ORACLE_HELPER_CLASS,
        L"Oracle Helper",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr,
        GetModuleHandleW(nullptr),
        this
    );

    if (!hwnd) return false;

    // 设置透明色键（黑色透明）
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    // 设置窗口标题栏图标（可选）
    // SendMessage(hwnd, WM_SETICON, ICON_SMALL, ...);

    ShowWindow(hwnd, SW_SHOW);
    Redraw();

    return true;
}

// ============================================================
// 设置 / 获取提示
// ============================================================
void OverlayWindow::SetPrompt(int idx) {
    promptIdx = idx;
    Redraw();
}

void OverlayWindow::SetStatusText(const std::string& text) {
    statusText = text;
    Redraw();
}

void OverlayWindow::SetStyles(const std::vector<OracleStyle>& s) {
    styles = s;
}

void OverlayWindow::Redraw() {
    if (hwnd) {
        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
    }
}

// ============================================================
// 自动移动到第二屏幕
// ============================================================
void OverlayWindow::MoveToSecondMonitor() {
    if (!hwnd) return;

    DISPLAY_DEVICEW dd;
    dd.cb = sizeof(DISPLAY_DEVICEW);
    int monitorIdx = 0;

    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            monitorIdx++;
            if (monitorIdx == 2) { // 第二个显示器
                DEVMODEW dm;
                dm.dmSize = sizeof(DEVMODEW);
                EnumDisplaySettingsW(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm);
                SetWindowPos(hwnd, nullptr,
                             dm.dmPosition.x, dm.dmPosition.y,
                             winWidth, winHeight,
                             SWP_NOZORDER);
                return;
            }
        }
    }
    // 没找到第二屏幕 → 居中主屏
    SetWindowPos(hwnd, nullptr,
                 (GetSystemMetrics(SM_CXSCREEN) - winWidth) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - winHeight) / 2,
                 winWidth, winHeight,
                 SWP_NOZORDER);
}

// ============================================================
// 消息循环
// ============================================================
void OverlayWindow::MessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// ============================================================
// 窗口过程
// ============================================================
LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        self = (OverlayWindow*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (OverlayWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 黑色背景（会被透明色键过滤掉）
            FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));

            int idx = self->promptIdx;

            if (idx >= 0 && idx < (int)self->styles.size()) {
                // ---- 画大色块（圆角矩形） ----
                COLORREF col = self->styles[idx].color;
                HBRUSH brush = CreateSolidBrush(col);
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);

                // 外圈光晕
                for (int g = 20; g > 0; g--) {
                    int alpha = 10 + g * 5;
                    HBRUSH glow = CreateSolidBrush(
                        RGB(GetRValue(col) * alpha / 255,
                            GetGValue(col) * alpha / 255,
                            GetBValue(col) * alpha / 255)
                    );
                    SelectObject(hdc, glow);
                    RoundRect(hdc,
                        40 - g * 2, 40 - g * 2,
                        self->winWidth - 40 + g * 2, self->winHeight - 120 + g * 2,
                        30 + g, 30 + g
                    );
                    DeleteObject(glow);
                }

                // 主色块
                SelectObject(hdc, brush);
                RoundRect(hdc, 40, 40, self->winWidth - 40, self->winHeight - 120, 30, 30);

                // 边框
                HPEN pen = CreatePen(PS_SOLID, 4, RGB(255, 255, 255));
                HPEN oldPen = (HPEN)SelectObject(hdc, pen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                RoundRect(hdc, 40, 40, self->winWidth - 40, self->winHeight - 120, 30, 30);
                SelectObject(hdc, oldPen);
                DeleteObject(pen);

                SelectObject(hdc, oldBrush);
                DeleteObject(brush);

                // ---- 画文字 ----
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);

                HFONT bigFont = CreateFontW(
                    -72, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI"
                );
                SelectObject(hdc, bigFont);

                // UTF-8 → UTF-16
                std::wstring wname;
                if (!self->styles[idx].name.empty()) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0,
                                self->styles[idx].name.c_str(), -1, nullptr, 0);
                    wname.resize(wlen);
                    MultiByteToWideChar(CP_UTF8, 0,
                        self->styles[idx].name.c_str(), -1, &wname[0], wlen);
                    // 去掉末尾 null
                    if (!wname.empty() && wname.back() == L'\0') wname.pop_back();
                }

                RECT textRect = { 40, 40, self->winWidth - 40, self->winHeight - 120 };
                DrawTextW(hdc, wname.c_str(), (int)wname.length(), &textRect,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                DeleteObject(bigFont);

                // ---- 画置信度条 ----
                int barY = self->winHeight - 70;
                int barX1 = 60, barX2 = self->winWidth - 60;
                int barH = 24;
                // 背景
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, barX1, barY, barX2, barY + barH);
                // 填充
                int fillW = (int)((barX2 - barX1) * self->confidence);
                HBRUSH barBrush = CreateSolidBrush(col);
                SelectObject(hdc, barBrush);
                Rectangle(hdc, barX1, barY, barX1 + fillW, barY + barH);
                DeleteObject(barBrush);

                // 置信度文字
                HFONT smallFont = CreateFontW(
                    -20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI"
                );
                SelectObject(hdc, smallFont);
                wchar_t confStr[64];
                swprintf(confStr, 64, L"Confidence: %.0f%%", self->confidence * 100.0f);
                TextOutW(hdc, barX1, barY - 28, confStr, (int)wcslen(confStr));
                DeleteObject(smallFont);
            } else {
                // 无匹配 → 显示状态文字
                SetTextColor(hdc, RGB(180, 180, 180));
                SetBkMode(hdc, TRANSPARENT);

                HFONT font = CreateFontW(
                    -36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI"
                );
                SelectObject(hdc, font);

                std::wstring wstatus;
                int wlen = MultiByteToWideChar(CP_UTF8, 0, self->statusText.c_str(), -1, nullptr, 0);
                wstatus.resize(wlen);
                MultiByteToWideChar(CP_UTF8, 0, self->statusText.c_str(), -1, &wstatus[0], wlen);
                if (!wstatus.empty() && wstatus.back() == L'\0') wstatus.pop_back();

                RECT r = { 20, 20, self->winWidth - 20, self->winHeight - 20 };
                DrawTextW(hdc, wstatus.c_str(), (int)wstatus.length(), &r,
                          DT_CENTER | DT_VCENTER | DT_WORDBREAK);
                DeleteObject(font);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
