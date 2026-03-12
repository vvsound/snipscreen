// snip.cpp  ── 编译: g++ -O2 -o snip.exe snip.cpp -lgdi32 -luser32 -ldwmapi
// 或 MSVC:  cl snip.cpp /link gdi32.lib user32.lib dwmapi.lib
//
// 用法: 直接运行 → 截全屏冻结 → 拖框 → 自动复制到剪贴板并退出

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <cmath>

// ─── 全局状态 ────────────────────────────────────────────────────────────────
static HBITMAP  g_hFullBmp  = nullptr;   // 全屏截图
static HDC      g_hMemDC    = nullptr;   // 内存DC（持有 g_hFullBmp）
static int      g_W = 0, g_H = 0;        // 屏幕尺寸

static bool     g_dragging  = false;
static POINT    g_start{}, g_end{};
static bool     g_hasPrev   = false;
static RECT     g_prevRect{};

// ─── 声音提示 ────────────────────────────────────────────────────────────────
// 就绪音: 低→高（叮~）
static void beepReady() {
    Beep(880,  80);
    Beep(1318, 120);
}
// 截取音: 高→更高（清脆叮！）
static void beepDone() {
    Beep(1318, 80);
    Beep(1760, 120);
}

// ─── 全屏截图存入内存DC ───────────────────────────────────────────────────────
static void captureFullScreen() {
    g_W = GetSystemMetrics(SM_CXSCREEN);
    g_H = GetSystemMetrics(SM_CYSCREEN);

    HDC hdcScreen = GetDC(nullptr);
    g_hMemDC  = CreateCompatibleDC(hdcScreen);
    g_hFullBmp = CreateCompatibleBitmap(hdcScreen, g_W, g_H);
    SelectObject(g_hMemDC, g_hFullBmp);
    BitBlt(g_hMemDC, 0, 0, g_W, g_H, hdcScreen, 0, 0, SRCCOPY | CAPTUREBLT);
    ReleaseDC(nullptr, hdcScreen);
}

// ─── 把选区复制到剪贴板（DIB 格式）─────────────────────────────────────────
static void copyRegionToClipboard(int x1, int y1, int w, int h) {
    // 创建临时 DC + bitmap 存裁剪区域
    HDC hdcTmp = CreateCompatibleDC(g_hMemDC);
    HBITMAP hCrop = CreateCompatibleBitmap(g_hMemDC, w, h);
    SelectObject(hdcTmp, hCrop);
    BitBlt(hdcTmp, 0, 0, w, h, g_hMemDC, x1, y1, SRCCOPY);

    // 转成 DIB（32bpp）
    BITMAPINFOHEADER bi{};
    bi.biSize        = sizeof(bi);
    bi.biWidth       = w;
    bi.biHeight      = h;          // 正值 → bottom-up（剪贴板标准）
    bi.biPlanes      = 1;
    bi.biBitCount    = 32;
    bi.biCompression = BI_RGB;

    int pixBytes = w * h * 4;
    DWORD totalSize = sizeof(BITMAPINFOHEADER) + pixBytes;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    BYTE* ptr = (BYTE*)GlobalLock(hMem);

    memcpy(ptr, &bi, sizeof(bi));
    // GetDIBits 直接写到 ptr + 40
    BITMAPINFO bmi{};
    bmi.bmiHeader = bi;
    GetDIBits(hdcTmp, hCrop, 0, h, ptr + sizeof(bi), &bmi, DIB_RGB_COLORS);

    GlobalUnlock(hMem);
    DeleteObject(hCrop);
    DeleteDC(hdcTmp);

    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_DIB, hMem);
    CloseClipboard();
}

// ─── 用 XOR 在窗口DC上画矩形选框 ────────────────────────────────────────────
static void drawXorRect(HDC hdc, RECT r) {
    if (r.right  < r.left) { std::swap(r.left, r.right); }
    if (r.bottom < r.top)  { std::swap(r.top,  r.bottom); }
    if (r.right - r.left < 2 || r.bottom - r.top < 2) return;

    int oldRop = SetROP2(hdc, R2_NOT);
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    Rectangle(hdc, r.left, r.top, r.right, r.bottom);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(hPen);
    SetROP2(hdc, oldRop);
}

// ─── 窗口过程 ────────────────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    // 绘制背景：把全屏截图贴满窗口
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        BitBlt(hdc, 0, 0, g_W, g_H, g_hMemDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        g_start = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        g_end   = g_start;
        g_dragging = true;
        g_hasPrev  = false;
        SetCapture(hwnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_dragging) break;
        HDC hdc = GetDC(hwnd);
        // 擦除上一帧
        if (g_hasPrev) drawXorRect(hdc, g_prevRect);
        // 画新框
        g_end = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        RECT r{ g_start.x, g_start.y, g_end.x, g_end.y };
        drawXorRect(hdc, r);
        g_prevRect = r;
        g_hasPrev  = true;
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    case WM_LBUTTONUP: {
        if (!g_dragging) break;
        g_dragging = false;
        ReleaseCapture();

        // 擦除最后一帧框
        if (g_hasPrev) {
            HDC hdc = GetDC(hwnd);
            drawXorRect(hdc, g_prevRect);
            ReleaseDC(hwnd, hdc);
            g_hasPrev = false;
        }

        int x1 = g_start.x, y1 = g_start.y;
        int x2 = (int)(short)LOWORD(lParam);
        int y2 = (int)(short)HIWORD(lParam);
        if (x1 > x2) std::swap(x1, x2);
        if (y1 > y2) std::swap(y1, y2);
        int w = x2 - x1, h = y2 - y1;

        if (w > 5 && h > 5) {
            copyRegionToClipboard(x1, y1, w, h);
            beepDone();
        }
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            // 按 ESC 取消，不保存
            if (g_hasPrev) {
                HDC hdc = GetDC(hwnd);
                drawXorRect(hdc, g_prevRect);
                ReleaseDC(hwnd, hdc);
            }
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ─── 入口 ────────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // 1. 立刻截全屏（菜单/弹窗此时仍可见）
    captureFullScreen();

    // 2. 就绪提示音
    beepReady();

    // 3. 注册窗口类
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = L"SnipWnd";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    // 4. 创建全屏置顶窗口（无边框）
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"SnipWnd", L"",
        WS_POPUP | WS_VISIBLE,
        0, 0, g_W, g_H,
        nullptr, nullptr, hInst, nullptr
    );

    // 5. 等待 DWM 合成一帧，避免闪烁
    DwmFlush();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // 6. 消息循环
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 7. 释放资源
    if (g_hMemDC)  DeleteDC(g_hMemDC);
    if (g_hFullBmp) DeleteObject(g_hFullBmp);

    return 0;
}
