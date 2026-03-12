// snip.cpp
// g++ -O2 -o snip.exe snip.cpp -lgdi32 -luser32 -ldwmapi
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
using std::min; using std::max;

static HDC     g_hMemDC;
static HBITMAP g_hBmp;
static int     g_W, g_H;
static bool    g_drag;
static int     g_sx, g_sy;

static void toClipboard(int x1, int y1, int x2, int y2) {
    int w = x2-x1, h = y2-y1;
    HDC hScr = GetDC(nullptr);
    HDC hTmp = CreateCompatibleDC(hScr);
    HBITMAP hBmp = CreateCompatibleBitmap(hScr, w, h);
    SelectObject(hTmp, hBmp);
    BitBlt(hTmp, 0, 0, w, h, g_hMemDC, x1, y1, SRCCOPY);
    DeleteDC(hTmp);
    ReleaseDC(nullptr, hScr);
    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, hBmp);
    CloseClipboard();
}

static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BitBlt(BeginPaint(hw,&ps), 0, 0, g_W, g_H, g_hMemDC, 0, 0, SRCCOPY);
        EndPaint(hw, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        g_sx=(int)(short)LOWORD(lp); g_sy=(int)(short)HIWORD(lp);
        g_drag = true; SetCapture(hw);
        return 0;
    case WM_MOUSEMOVE:
        if (!g_drag) break; {
        int ex=(int)(short)LOWORD(lp), ey=(int)(short)HIWORD(lp);
        HDC hdc = GetDC(hw);
        BitBlt(hdc, 0, 0, g_W, g_H, g_hMemDC, 0, 0, SRCCOPY);
        HPEN pen = CreatePen(PS_SOLID,2,RGB(255,0,0));
        SelectObject(hdc, (HPEN)SelectObject(hdc,pen));
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, min(g_sx,ex), min(g_sy,ey), max(g_sx,ex), max(g_sy,ey));
        DeleteObject(pen);
        ReleaseDC(hw, hdc);
        } return 0;
    case WM_LBUTTONUP:
        if (!g_drag) break;
        g_drag = false; ReleaseCapture(); {
        int x1=min(g_sx,(int)(short)LOWORD(lp)), y1=min(g_sy,(int)(short)HIWORD(lp));
        int x2=max(g_sx,(int)(short)LOWORD(lp)), y2=max(g_sy,(int)(short)HIWORD(lp));
        if (x2-x1>2 && y2-y1>2) toClipboard(x1,y1,x2,y2);
        } DestroyWindow(hw); return 0;
    case WM_KEYDOWN:
        if (wp==VK_ESCAPE) DestroyWindow(hw); return 0;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int) {
    Sleep(150); DwmFlush();
    g_W = GetSystemMetrics(SM_CXSCREEN);
    g_H = GetSystemMetrics(SM_CYSCREEN);
    HDC hScr = GetDC(nullptr);
    g_hMemDC = CreateCompatibleDC(hScr);
    g_hBmp   = CreateCompatibleBitmap(hScr, g_W, g_H);
    SelectObject(g_hMemDC, g_hBmp);
    BitBlt(g_hMemDC, 0, 0, g_W, g_H, hScr, 0, 0, SRCCOPY|CAPTUREBLT);
    ReleaseDC(nullptr, hScr);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc; wc.hInstance = hi;
    wc.hCursor     = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515));
    wc.lpszClassName = L"S";
    RegisterClassExW(&wc);

    HWND hw = CreateWindowExW(WS_EX_TOPMOST, L"S", L"",
        WS_POPUP, 0, 0, g_W, g_H, nullptr, nullptr, hi, nullptr);
    ShowWindow(hw, SW_SHOW); UpdateWindow(hw); SetForegroundWindow(hw);

    MSG msg{};
    while (GetMessageW(&msg,nullptr,0,0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    DeleteObject(g_hBmp); DeleteDC(g_hMemDC);
    return 0;
}
