// snip.cpp
// g++ -O2 -o snip.exe snip.cpp -lgdi32 -luser32 -ldwmapi
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
using std::swap;

static HDC     g_hMemDC  = nullptr;
static HBITMAP g_hBmp    = nullptr;
static int     g_W = 0, g_H = 0;
static bool    g_drag    = false;
static POINT   g_s{}, g_e{};
static bool    g_hasPrev = false;
static RECT    g_prev{};

static void beepReady() { Beep(880,80);  Beep(1318,120); }
static void beepDone()  { Beep(1318,80); Beep(1760,120); }

static void captureFullScreen() {
    g_W = GetSystemMetrics(SM_CXSCREEN);
    g_H = GetSystemMetrics(SM_CYSCREEN);
    HDC hScr  = GetDC(nullptr);
    g_hMemDC  = CreateCompatibleDC(hScr);
    g_hBmp    = CreateCompatibleBitmap(hScr, g_W, g_H);
    SelectObject(g_hMemDC, g_hBmp);
    // 使用 PrintWindow 替代 BitBlt，解决 DWM 硬件加速下黑屏问题
    PrintWindow(GetDesktopWindow(), g_hMemDC, PW_RENDERFULLCONTENT);
    ReleaseDC(nullptr, hScr);
}

static void toClipboard(int x, int y, int w, int h) {
    HDC hScr = GetDC(nullptr);
    HDC hTmp = CreateCompatibleDC(hScr);
    HBITMAP hBmp = CreateCompatibleBitmap(hScr, w, h);
    HGDIOBJ hOld = SelectObject(hTmp, hBmp);
    BitBlt(hTmp, 0, 0, w, h, g_hMemDC, x, y, SRCCOPY);
    SelectObject(hTmp, hOld);
    DeleteDC(hTmp);

    int stride = (w*3+3)&~3, pixSz = stride*h;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, sizeof(BITMAPINFOHEADER)+pixSz);
    BYTE* p = (BYTE*)GlobalLock(hMem);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage   = pixSz;
    memcpy(p, &bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    GetDIBits(hScr, hBmp, 0, h, p+sizeof(BITMAPINFOHEADER), &bi, DIB_RGB_COLORS);
    GlobalUnlock(hMem);
    DeleteObject(hBmp);
    ReleaseDC(nullptr, hScr);

    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_DIB, hMem);
    CloseClipboard();
}

static void xorRect(HDC hdc, RECT r) {
    if (r.right<r.left) swap(r.left,r.right);
    if (r.bottom<r.top) swap(r.top,r.bottom);
    if (r.right-r.left<2||r.bottom-r.top<2) return;
    int old = SetROP2(hdc, R2_NOT);
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(255,0,0));
    HPEN op  = (HPEN)SelectObject(hdc, pen);
    HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, op); SelectObject(hdc, ob);
    DeleteObject(pen); SetROP2(hdc, old);
}

static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hw, &ps);
        BitBlt(hdc, 0, 0, g_W, g_H, g_hMemDC, 0, 0, SRCCOPY);
        if (g_hasPrev) xorRect(hdc, g_prev);
        EndPaint(hw, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        g_s = {(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};
        g_e = g_s; g_drag = true; g_hasPrev = false;
        SetCapture(hw); return 0;
    case WM_MOUSEMOVE:
        if (!g_drag) break; {
        HDC hdc = GetDC(hw);
        if (g_hasPrev) xorRect(hdc, g_prev);
        g_e = {(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};
        RECT r{g_s.x,g_s.y,g_e.x,g_e.y};
        xorRect(hdc, r); g_prev = r; g_hasPrev = true;
        ReleaseDC(hw, hdc); } return 0;
    case WM_LBUTTONUP:
        if (!g_drag) break;
        g_drag = false; ReleaseCapture();
        if (g_hasPrev) { HDC hdc=GetDC(hw); xorRect(hdc,g_prev); ReleaseDC(hw,hdc); g_hasPrev=false; }
        { int x1=g_s.x,y1=g_s.y,x2=(int)(short)LOWORD(lp),y2=(int)(short)HIWORD(lp);
          if(x1>x2)swap(x1,x2); if(y1>y2)swap(y1,y2);
          if(x2-x1>5&&y2-y1>5){toClipboard(x1,y1,x2-x1,y2-y1);beepDone();}
          DestroyWindow(hw); } return 0;
    case WM_KEYDOWN:
        if (wp==VK_ESCAPE) {
            if(g_hasPrev){HDC hdc=GetDC(hw);xorRect(hdc,g_prev);ReleaseDC(hw,hdc);}
            toClipboard(0,0,g_W,g_H); beepDone(); DestroyWindow(hw);
        } return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int) {
    DwmFlush();
    captureFullScreen();
    beepReady();

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515));
    wc.lpszClassName = L"Snip";
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    RegisterClassExW(&wc);

    HWND hw = CreateWindowExW(WS_EX_TOPMOST, L"Snip", L"",
        WS_POPUP, 0, 0, g_W, g_H, nullptr, nullptr, hi, nullptr);

    ShowWindow(hw, SW_SHOW);
    UpdateWindow(hw);
    SetForegroundWindow(hw);
    SetFocus(hw);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_hBmp);
    DeleteDC(g_hMemDC);
    return 0;
}
