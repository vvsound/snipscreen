// snip.cpp
// g++ -O2 -o snip.exe snip.cpp -lgdi32 -luser32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HDC     g_memDC;
static HBITMAP g_memBmp;
static int     g_W, g_H;
static bool    g_drag;
static int     g_x0, g_y0;

static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hw, &ps);
        BitBlt(dc, 0, 0, g_W, g_H, g_memDC, 0, 0, SRCCOPY);
        EndPaint(hw, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
        g_x0 = (short)LOWORD(lp);
        g_y0 = (short)HIWORD(lp);
        g_drag = true;
        SetCapture(hw);
        return 0;

    case WM_MOUSEMOVE:
        if (!g_drag) return 0;
        {
            int x1 = g_x0, y1 = g_y0;
            int x2 = (short)LOWORD(lp), y2 = (short)HIWORD(lp);
            if (x1 > x2) { int t=x1; x1=x2; x2=t; }
            if (y1 > y2) { int t=y1; y1=y2; y2=t; }

            HDC dc = GetDC(hw);
            BitBlt(dc, 0, 0, g_W, g_H, g_memDC, 0, 0, SRCCOPY);
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            HGDIOBJ op = SelectObject(dc, pen);
            SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, x1, y1, x2, y2);
            SelectObject(dc, op);
            DeleteObject(pen);
            ReleaseDC(hw, dc);
        }
        return 0;

    case WM_LBUTTONUP:
        if (!g_drag) return 0;
        g_drag = false;
        ReleaseCapture();
        {
            int x1 = g_x0, y1 = g_y0;
            int x2 = (short)LOWORD(lp), y2 = (short)HIWORD(lp);
            if (x1 > x2) { int t=x1; x1=x2; x2=t; }
            if (y1 > y2) { int t=y1; y1=y2; y2=t; }
            int w = x2-x1, h = y2-y1;

            if (w > 2 && h > 2) {
                HDC hScr = GetDC(nullptr);
                HDC hTmp = CreateCompatibleDC(hScr);
                HBITMAP hBmp = CreateCompatibleBitmap(hScr, w, h);
                SelectObject(hTmp, hBmp);
                BitBlt(hTmp, 0, 0, w, h, g_memDC, x1, y1, SRCCOPY);
                DeleteDC(hTmp);
                ReleaseDC(nullptr, hScr);

                OpenClipboard(nullptr);
                EmptyClipboard();
                SetClipboardData(CF_BITMAP, hBmp);
                CloseClipboard();
            }
        }
        DestroyWindow(hw);
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hw);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int) {
    // 截屏
    g_W = GetSystemMetrics(SM_CXSCREEN);
    g_H = GetSystemMetrics(SM_CYSCREEN);
    HDC hScr = GetDC(nullptr);
    g_memDC  = CreateCompatibleDC(hScr);
    g_memBmp = CreateCompatibleBitmap(hScr, g_W, g_H);
    SelectObject(g_memDC, g_memBmp);
    BitBlt(g_memDC, 0, 0, g_W, g_H, hScr, 0, 0, SRCCOPY | CAPTUREBLT);
    ReleaseDC(nullptr, hScr);

    // 窗口
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = L"snip";
    RegisterClassExW(&wc);

    HWND hw = CreateWindowExW(
        WS_EX_TOPMOST, L"snip", L"", WS_POPUP,
        0, 0, g_W, g_H, nullptr, nullptr, hi, nullptr);

    ShowWindow(hw, SW_SHOW);
    UpdateWindow(hw);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_memBmp);
    DeleteDC(g_memDC);
    return 0;
}
