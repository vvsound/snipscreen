// snip.cpp
// g++ -O2 -o snip.exe snip.cpp -lgdi32 -luser32 -lmsimg32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HDC  g_memDC;
static HDC  g_dimDC;
static int  g_W, g_H;
static bool g_drag;
static int  g_x0, g_y0;

static void swap(int &a, int &b) { int t=a; a=b; b=t; }

static void makeDim(HDC src, HDC dst) {
    BitBlt(dst, 0, 0, g_W, g_H, src, 0, 0, SRCCOPY);
    HDC hBlk = CreateCompatibleDC(src);
    HBITMAP hBmp = CreateCompatibleBitmap(src, g_W, g_H);
    SelectObject(hBlk, hBmp);
    RECT rc = {0, 0, g_W, g_H};
    FillRect(hBlk, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 100, 0};
    AlphaBlend(dst, 0, 0, g_W, g_H, hBlk, 0, 0, g_W, g_H, bf);
    DeleteObject(hBmp);
    DeleteDC(hBlk);
}

static void redraw(HWND hw, int x1, int y1, int x2, int y2) {
    HDC dc = GetDC(hw);
    BitBlt(dc, 0, 0, g_W, g_H, g_dimDC, 0, 0, SRCCOPY);
    BitBlt(dc, x1, y1, x2-x1, y2-y1, g_memDC, x1, y1, SRCCOPY);
    HPEN pen = CreatePen(PS_SOLID, 6, RGB(0, 0, 0));
    HGDIOBJ op = SelectObject(dc, pen);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, x1, y1, x2, y2);
    SelectObject(dc, op);
    DeleteObject(pen);
    ReleaseDC(hw, dc);
}

static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BitBlt(BeginPaint(hw, &ps), 0, 0, g_W, g_H, g_dimDC, 0, 0, SRCCOPY);
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
            int x1=g_x0, y1=g_y0, x2=(short)LOWORD(lp), y2=(short)HIWORD(lp);
            if (x1>x2) swap(x1,x2);
            if (y1>y2) swap(y1,y2);
            redraw(hw, x1, y1, x2, y2);
        }
        return 0;

    case WM_LBUTTONUP:
        if (!g_drag) return 0;
        g_drag = false;
        ReleaseCapture();
        {
            int x1=g_x0, y1=g_y0, x2=(short)LOWORD(lp), y2=(short)HIWORD(lp);
            if (x1>x2) swap(x1,x2);
            if (y1>y2) swap(y1,y2);
            int w=x2-x1, h=y2-y1;
            if (w>2 && h>2) {
                HDC hTmp = CreateCompatibleDC(g_memDC);
                HBITMAP hBmp = CreateCompatibleBitmap(g_memDC, w, h);
                SelectObject(hTmp, hBmp);
                BitBlt(hTmp, 0, 0, w, h, g_memDC, x1, y1, SRCCOPY);
                DeleteDC(hTmp);
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
    g_W = GetSystemMetrics(SM_CXSCREEN);
    g_H = GetSystemMetrics(SM_CYSCREEN);

    HDC hScr = GetDC(nullptr);
    g_memDC = CreateCompatibleDC(hScr);
    SelectObject(g_memDC, CreateCompatibleBitmap(hScr, g_W, g_H));
    BitBlt(g_memDC, 0, 0, g_W, g_H, hScr, 0, 0, SRCCOPY | CAPTUREBLT);

    g_dimDC = CreateCompatibleDC(hScr);
    SelectObject(g_dimDC, CreateCompatibleBitmap(hScr, g_W, g_H));
    ReleaseDC(nullptr, hScr);
    makeDim(g_memDC, g_dimDC);

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    // 尝试加载 Windows 内置的"特大高亮"箭头指针（需开启"轻松访问→鼠标指针"大号/特大）
    // 文件路径：%SystemRoot%\Cursors\aero_arrow_xl.cur
    {
        wchar_t curPath[MAX_PATH];
        GetSystemWindowsDirectoryW(curPath, MAX_PATH);
        lstrcatW(curPath, L"\\Cursors\\aero_arrow_xl.cur");
        HCURSOR hCur = (HCURSOR)LoadImageW(nullptr, curPath,
            IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        if (!hCur) {
            // 回退：用系统默认箭头
            hCur = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
        }
        wc.hCursor = hCur;
    }
    wc.lpszClassName = L"snip";
    RegisterClassExW(&wc);

    HWND hw = CreateWindowExW(WS_EX_TOPMOST, L"snip", L"", WS_POPUP,
        0, 0, g_W, g_H, nullptr, nullptr, hi, nullptr);
    ShowWindow(hw, SW_SHOW);
    SetForegroundWindow(hw);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteDC(g_memDC);
    DeleteDC(g_dimDC);
    return 0;
}
