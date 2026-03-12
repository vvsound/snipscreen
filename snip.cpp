// snip.cpp
// 编译: g++ -O2 -o snip.exe snip.cpp -lgdi32 -luser32 -ldwmapi
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
using std::swap;

static HBITMAP g_hFullBmp  = nullptr;
static HDC     g_hMemDC    = nullptr;
static int     g_W = 0, g_H = 0;

static bool  g_dragging = false;
static POINT g_start{}, g_end{};
static bool  g_hasPrev  = false;
static RECT  g_prevRect{};

static void beepReady() { Beep(880, 80);  Beep(1318, 120); }
static void beepDone()  { Beep(1318, 80); Beep(1760, 120); }

static void captureFullScreen() {
    g_W = GetSystemMetrics(SM_CXSCREEN);
    g_H = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = GetDC(nullptr);
    g_hMemDC      = CreateCompatibleDC(hdcScreen);
    g_hFullBmp    = CreateCompatibleBitmap(hdcScreen, g_W, g_H);
    SelectObject(g_hMemDC, g_hFullBmp);
    BitBlt(g_hMemDC, 0, 0, g_W, g_H, hdcScreen, 0, 0, SRCCOPY | CAPTUREBLT);
    ReleaseDC(nullptr, hdcScreen);
}

static void copyRegionToClipboard(int x1, int y1, int w, int h) {
    HDC     hdcTmp = CreateCompatibleDC(g_hMemDC);
    HBITMAP hCrop  = CreateCompatibleBitmap(g_hMemDC, w, h);
    SelectObject(hdcTmp, hCrop);
    BitBlt(hdcTmp, 0, 0, w, h, g_hMemDC, x1, y1, SRCCOPY);

    BITMAPINFOHEADER bi{};
    bi.biSize        = sizeof(bi);
    bi.biWidth       = w;
    bi.biHeight      = h;
    bi.biPlanes      = 1;
    bi.biBitCount    = 32;
    bi.biCompression = BI_RGB;

    DWORD totalSize = sizeof(bi) + w * h * 4;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
    BYTE*   ptr  = (BYTE*)GlobalLock(hMem);
    memcpy(ptr, &bi, sizeof(bi));
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

static void drawXorRect(HDC hdc, RECT r) {
    if (r.right  < r.left) swap(r.left,  r.right);
    if (r.bottom < r.top)  swap(r.top,   r.bottom);
    if (r.right - r.left < 2 || r.bottom - r.top < 2) return;
    int     oldRop = SetROP2(hdc, R2_NOT);
    HPEN    hPen   = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN    oldPen = (HPEN)SelectObject(hdc, hPen);
    HGDIOBJ oldBr  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(hPen);
    SetROP2(hdc, oldRop);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        BitBlt(hdc, 0, 0, g_W, g_H, g_hMemDC, 0, 0, SRCCOPY);
        if (g_hasPrev) drawXorRect(hdc, g_prevRect);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        g_start    = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        g_end      = g_start;
        g_dragging = true;
        g_hasPrev  = false;
        SetCapture(hwnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_dragging) break;
        HDC hdc = GetDC(hwnd);
        if (g_hasPrev) drawXorRect(hdc, g_prevRect);
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
        if (g_hasPrev) {
            HDC hdc = GetDC(hwnd);
            drawXorRect(hdc, g_prevRect);
            ReleaseDC(hwnd, hdc);
            g_hasPrev = false;
        }
        int x1 = g_start.x, y1 = g_start.y;
        int x2 = (int)(short)LOWORD(lParam);
        int y2 = (int)(short)HIWORD(lParam);
        if (x1 > x2) swap(x1, x2);
        if (y1 > y2) swap(y1, y2);
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
            // ESC = 截全屏
            if (g_hasPrev) {
                HDC hdc = GetDC(hwnd);
                drawXorRect(hdc, g_prevRect);
                ReleaseDC(hwnd, hdc);
                g_hasPrev = false;
            }
            copyRegionToClipboard(0, 0, g_W, g_H);
            beepDone();
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

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    captureFullScreen();
    beepReady();

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515));
    wc.lpszClassName = L"SnipWnd";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"SnipWnd", L"",
        WS_POPUP | WS_VISIBLE,
        0, 0, g_W, g_H,
        nullptr, nullptr, hInst, nullptr
    );

    DwmFlush();
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hMemDC)   DeleteDC(g_hMemDC);
    if (g_hFullBmp) DeleteObject(g_hFullBmp);
    return 0;
}
