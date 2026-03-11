package main

import (
	"fmt"
	"image"
	"image/png"
	"os"
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
)

// Windows API
var (
	user32   = windows.NewLazyDLL("user32.dll")
	gdi32    = windows.NewLazyDLL("gdi32.dll")
	kernel32 = windows.NewLazyDLL("kernel32.dll")
	winmm    = windows.NewLazyDLL("winmm.dll")

	procGetDC              = user32.NewProc("GetDC")
	procReleaseDC          = user32.NewProc("ReleaseDC")
	procCreateCompatibleDC = gdi32.NewProc("CreateCompatibleDC")
	procCreateCompatibleBitmap = gdi32.NewProc("CreateCompatibleBitmap")
	procSelectObject       = gdi32.NewProc("SelectObject")
	procBitBlt             = gdi32.NewProc("BitBlt")
	procDeleteDC           = gdi32.NewProc("DeleteDC")
	procDeleteObject       = gdi32.NewProc("DeleteObject")
	procGetDIBits          = gdi32.NewProc("GetDIBits")
	procGetSystemMetrics   = user32.NewProc("GetSystemMetrics")
	procOpenClipboard      = user32.NewProc("OpenClipboard")
	procCloseClipboard     = user32.NewProc("CloseClipboard")
	procEmptyClipboard     = user32.NewProc("EmptyClipboard")
	procSetClipboardData   = user32.NewProc("SetClipboardData")
	procGlobalAlloc        = kernel32.NewProc("GlobalAlloc")
	procGlobalLock         = kernel32.NewProc("GlobalLock")
	procGlobalUnlock       = kernel32.NewProc("GlobalUnlock")
	procPlaySound          = winmm.NewProc("PlaySoundW")

	// 选区覆盖窗口
	procCreateWindowEx       = user32.NewProc("CreateWindowExW")
	procDefWindowProc        = user32.NewProc("DefWindowProcW")
	procDispatchMessage      = user32.NewProc("DispatchMessageW")
	procGetMessage           = user32.NewProc("GetMessageW")
	procPostQuitMessage      = user32.NewProc("PostQuitMessage")
	procRegisterClassEx      = user32.NewProc("RegisterClassExW")
	procShowWindow           = user32.NewProc("ShowWindow")
	procUpdateWindow         = user32.NewProc("UpdateWindow")
	procSetCursor            = user32.NewProc("SetCursor")
	procLoadCursor           = user32.NewProc("LoadCursorW")
	procInvalidateRect       = user32.NewProc("InvalidateRect")
	procBeginPaint           = user32.NewProc("BeginPaint")
	procEndPaint             = user32.NewProc("EndPaint")
	procGetClientRect        = user32.NewProc("GetClientRect")
	procFillRect             = user32.NewProc("FillRect")
	procCreateSolidBrush     = gdi32.NewProc("CreateSolidBrush")
	procFrameRect            = gdi32.NewProc("FrameRect")
	procSetBkMode            = gdi32.NewProc("SetBkMode")
	procGetCursorPos         = user32.NewProc("GetCursorPos")
	procSetCapture           = user32.NewProc("SetCapture")
	procReleaseCapture       = user32.NewProc("ReleaseCapture")
	procDrawFocusRect        = user32.NewProc("DrawFocusRect")
	procSetWindowLong        = user32.NewProc("SetWindowLongW")
	procGetWindowLong        = user32.NewProc("GetWindowLongW")
	procSetLayeredWindowAttributes = user32.NewProc("SetLayeredWindowAttributes")
	procScreenToClient       = user32.NewProc("ScreenToClient")
	procClientToScreen       = user32.NewProc("ClientToScreen")
	procDrawRect             = gdi32.NewProc("Rectangle")
	procMoveToEx             = gdi32.NewProc("MoveToEx")
	procLineTo               = gdi32.NewProc("LineTo")
	procCreatePen            = gdi32.NewProc("CreatePen")
	procSetROP2              = gdi32.NewProc("SetROP2")
	procGetStockObject       = gdi32.NewProc("GetStockObject")
	procDestroyWindow        = user32.NewProc("DestroyWindow")
	procMessageBox           = user32.NewProc("MessageBoxW")
)

const (
	SRCCOPY      = 0x00CC0020
	CAPTUREBLT   = 0x40000000
	BI_RGB       = 0
	DIB_RGB_COLORS = 0
	CF_DIB       = 8
	GMEM_MOVEABLE = 0x0002
	WS_POPUP     = 0x80000000
	WS_EX_LAYERED    = 0x00080000
	WS_EX_TOPMOST    = 0x00000008
	WS_EX_TRANSPARENT = 0x00000020
	GWL_EXSTYLE  = -20
	LWA_ALPHA    = 0x00000002
	SW_SHOW      = 5
	WM_DESTROY   = 0x0002
	WM_PAINT     = 0x000F
	WM_LBUTTONDOWN = 0x0201
	WM_LBUTTONUP   = 0x0202
	WM_MOUSEMOVE   = 0x0200
	WM_KEYDOWN     = 0x0100
	VK_ESCAPE      = 0x1B
	R2_NOT        = 6
	PS_SOLID      = 0
	NULL_BRUSH    = 5
	IDC_CROSS     = 32515
	SND_ALIAS     = 0x00010000
	SND_ASYNC     = 0x0001
	SND_NODEFAULT = 0x0002
)

type POINT struct {
	X, Y int32
}

type RECT struct {
	Left, Top, Right, Bottom int32
}

type BITMAPINFOHEADER struct {
	BiSize          uint32
	BiWidth         int32
	BiHeight        int32
	BiPlanes        uint16
	BiBitCount      uint16
	BiCompression   uint32
	BiSizeImage     uint32
	BiXPelsPerMeter int32
	BiYPelsPerMeter int32
	BiClrUsed       uint32
	BiClrImportant  uint32
}

type BITMAPINFO struct {
	BmiHeader BITMAPINFOHEADER
	BmiColors [1]uint32
}

type PAINTSTRUCT struct {
	Hdc         uintptr
	FErase      int32
	RcPaint     RECT
	FRestore    int32
	FIncUpdate  int32
	RgbReserved [32]byte
}

type WNDCLASSEX struct {
	CbSize        uint32
	Style         uint32
	LpfnWndProc   uintptr
	CbClsExtra    int32
	CbWndExtra    int32
	HInstance     uintptr
	HIcon         uintptr
	HCursor       uintptr
	HbrBackground uintptr
	LpszMenuName  *uint16
	LpszClassName *uint16
	HIconSm       uintptr
}

type MSG struct {
	Hwnd    uintptr
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Pt      POINT
}

// 全局状态
var (
	screenW, screenH int
	startPt, endPt   POINT
	isDragging       bool
	hwndOverlay      uintptr
	captureResult    *image.RGBA
)

func getScreenSize() (int, int) {
	w, _, _ := procGetSystemMetrics.Call(0) // SM_CXSCREEN
	h, _, _ := procGetSystemMetrics.Call(1) // SM_CYSCREEN
	return int(w), int(h)
}

// 截取屏幕指定区域
func captureScreen(x, y, w, h int) *image.RGBA {
	hdcScreen, _, _ := procGetDC.Call(0)
	hdcMem, _, _ := procCreateCompatibleDC.Call(hdcScreen)
	hBmp, _, _ := procCreateCompatibleBitmap.Call(hdcScreen, uintptr(w), uintptr(h))
	procSelectObject.Call(hdcMem, hBmp)
	procBitBlt.Call(hdcMem, 0, 0, uintptr(w), uintptr(h), hdcScreen, uintptr(x), uintptr(y), SRCCOPY|CAPTUREBLT)

	bi := BITMAPINFO{}
	bi.BmiHeader.BiSize = uint32(unsafe.Sizeof(bi.BmiHeader))
	bi.BmiHeader.BiWidth = int32(w)
	bi.BmiHeader.BiHeight = -int32(h) // top-down
	bi.BmiHeader.BiPlanes = 1
	bi.BmiHeader.BiBitCount = 32
	bi.BmiHeader.BiCompression = BI_RGB

	pixels := make([]byte, w*h*4)
	procGetDIBits.Call(
		hdcMem, hBmp, 0, uintptr(h),
		uintptr(unsafe.Pointer(&pixels[0])),
		uintptr(unsafe.Pointer(&bi)),
		DIB_RGB_COLORS,
	)

	img := image.NewRGBA(image.Rect(0, 0, w, h))
	for i := 0; i < w*h; i++ {
		b := pixels[i*4]
		g := pixels[i*4+1]
		r := pixels[i*4+2]
		img.Pix[i*4] = r
		img.Pix[i*4+1] = g
		img.Pix[i*4+2] = b
		img.Pix[i*4+3] = 255
	}

	procDeleteObject.Call(hBmp)
	procDeleteDC.Call(hdcMem)
	procReleaseDC.Call(0, hdcScreen)
	return img
}

// 将图像写入剪贴板（DIB格式）
func copyToClipboard(img *image.RGBA) error {
	bounds := img.Bounds()
	w := bounds.Max.X
	h := bounds.Max.Y

	bi := BITMAPINFOHEADER{
		BiSize:     uint32(unsafe.Sizeof(BITMAPINFOHEADER{})),
		BiWidth:    int32(w),
		BiHeight:   int32(h), // bottom-up for clipboard
		BiPlanes:   1,
		BiBitCount: 32,
		BiCompression: BI_RGB,
	}
	pixelSize := w * h * 4
	totalSize := int(unsafe.Sizeof(bi)) + pixelSize

	hMem, _, _ := procGlobalAlloc.Call(GMEM_MOVEABLE, uintptr(totalSize))
	if hMem == 0 {
		return fmt.Errorf("GlobalAlloc failed")
	}
	ptr, _, _ := procGlobalLock.Call(hMem)
	if ptr == 0 {
		return fmt.Errorf("GlobalLock failed")
	}

	// 写入 BITMAPINFOHEADER
	headerSlice := (*[1 << 20]byte)(unsafe.Pointer(ptr))
	headerBytes := (*[40]byte)(unsafe.Pointer(&bi))
	copy(headerSlice[:40], headerBytes[:])

	// 写入像素（bottom-up）
	offset := 40
	for row := h - 1; row >= 0; row-- {
		for col := 0; col < w; col++ {
			r := img.Pix[(row*w+col)*4]
			g := img.Pix[(row*w+col)*4+1]
			b := img.Pix[(row*w+col)*4+2]
			headerSlice[offset] = b
			headerSlice[offset+1] = g
			headerSlice[offset+2] = r
			headerSlice[offset+3] = 0
			offset += 4
		}
	}

	procGlobalUnlock.Call(hMem)

	procOpenClipboard.Call(0)
	procEmptyClipboard.Call()
	procSetClipboardData.Call(CF_DIB, hMem)
	procCloseClipboard.Call()
	return nil
}

// 播放提示音
func playBeep() {
	// 播放系统"截图"提示音，用内置 SystemAsterisk
	soundName, _ := syscall.UTF16PtrFromString("SystemAsterisk")
	procPlaySound.Call(
		uintptr(unsafe.Pointer(soundName)),
		0,
		SND_ALIAS|SND_ASYNC,
	)
}

// 窗口过程
func wndProc(hwnd, msg, wParam, lParam uintptr) uintptr {
	switch msg {
	case WM_LBUTTONDOWN:
		x := int32(lParam & 0xFFFF)
		y := int32((lParam >> 16) & 0xFFFF)
		startPt = POINT{x, y}
		endPt = startPt
		isDragging = true
		procSetCapture.Call(hwnd)

	case WM_MOUSEMOVE:
		if isDragging {
			x := int32(lParam & 0xFFFF)
			y := int32((lParam >> 16) & 0xFFFF)
			endPt = POINT{x, y}
			procInvalidateRect.Call(hwnd, 0, 1)
		}

	case WM_LBUTTONUP:
		if isDragging {
			isDragging = false
			procReleaseCapture.Call()

			x := int32(lParam & 0xFFFF)
			y := int32((lParam >> 16) & 0xFFFF)
			endPt = POINT{x, y}

			// 计算截图区域
			x1 := startPt.X
			y1 := startPt.Y
			x2 := endPt.X
			y2 := endPt.Y
			if x1 > x2 { x1, x2 = x2, x1 }
			if y1 > y2 { y1, y2 = y2, y1 }
			w := int(x2 - x1)
			h := int(y2 - y1)

			if w > 5 && h > 5 {
				// 先销毁覆盖窗口，再截图（避免捕获到选框）
				procDestroyWindow.Call(hwnd)
				// 截图
				captureResult = captureScreen(int(x1), int(y1), w, h)
				copyToClipboard(captureResult)
				playBeep()
			} else {
				procDestroyWindow.Call(hwnd)
			}
		}

	case WM_KEYDOWN:
		if wParam == VK_ESCAPE {
			procDestroyWindow.Call(hwnd)
		}

	case WM_PAINT:
		var ps PAINTSTRUCT
		hdc, _, _ := procBeginPaint.Call(hwnd, uintptr(unsafe.Pointer(&ps)))
		if isDragging {
			// 用 R2_NOT 异或模式画选区框（不遮挡背景）
			procSetROP2.Call(hdc, R2_NOT)
			nullBrush, _, _ := procGetStockObject.Call(NULL_BRUSH)
			procSelectObject.Call(hdc, nullBrush)
			pen, _, _ := procCreatePen.Call(PS_SOLID, 2, 0xFFFFFF)
			procSelectObject.Call(hdc, pen)
			procDrawRect.Call(
				hdc,
				uintptr(startPt.X), uintptr(startPt.Y),
				uintptr(endPt.X), uintptr(endPt.Y),
			)
			procDeleteObject.Call(pen)
		}
		procEndPaint.Call(hwnd, uintptr(unsafe.Pointer(&ps)))

	case WM_DESTROY:
		procPostQuitMessage.Call(0)
	}

	ret, _, _ := procDefWindowProc.Call(hwnd, msg, wParam, lParam)
	return ret
}

func main() {
	screenW, screenH = getScreenSize()

	// 注册窗口类
	className, _ := syscall.UTF16PtrFromString("ScreenshotOverlay")
	cursor, _, _ := procLoadCursor.Call(0, IDC_CROSS)

	wc := WNDCLASSEX{
		CbSize:        uint32(unsafe.Sizeof(WNDCLASSEX{})),
		LpfnWndProc:   syscall.NewCallback(wndProc),
		HCursor:       cursor,
		LpszClassName: className,
	}
	procRegisterClassEx.Call(uintptr(unsafe.Pointer(&wc)))

	// 创建全屏透明覆盖窗口
	winName, _ := syscall.UTF16PtrFromString("")
	hwnd, _, _ := procCreateWindowEx.Call(
		WS_EX_LAYERED|WS_EX_TOPMOST,
		uintptr(unsafe.Pointer(className)),
		uintptr(unsafe.Pointer(winName)),
		WS_POPUP,
		0, 0,
		uintptr(screenW), uintptr(screenH),
		0, 0, 0, 0,
	)
	hwndOverlay = hwnd

	// 设置窗口半透明（alpha=1，几乎透明但可接收鼠标）
	procSetLayeredWindowAttributes.Call(hwnd, 0, 1, LWA_ALPHA)
	procShowWindow.Call(hwnd, SW_SHOW)
	procUpdateWindow.Call(hwnd)

	// 消息循环
	var msg MSG
	for {
		r, _, _ := procGetMessage.Call(uintptr(unsafe.Pointer(&msg)), 0, 0, 0)
		if r == 0 {
			break
		}
		procDispatchMessage.Call(uintptr(unsafe.Pointer(&msg)))
	}

	// 可选：保存到文件
	if captureResult != nil && len(os.Args) > 1 && os.Args[1] == "--save" {
		f, _ := os.Create("screenshot.png")
		png.Encode(f, captureResult)
		f.Close()
		fmt.Println("已保存到 screenshot.png")
	}
}
