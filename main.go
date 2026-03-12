package main

import (
	"fmt"
	"image"
	"image/png"
	"os"
	"syscall"
	"time"
	"unsafe"

	"golang.org/x/sys/windows"
)

var (
	user32   = windows.NewLazyDLL("user32.dll")
	gdi32    = windows.NewLazyDLL("gdi32.dll")
	kernel32 = windows.NewLazyDLL("kernel32.dll")

	procGetDC                      = user32.NewProc("GetDC")
	procReleaseDC                  = user32.NewProc("ReleaseDC")
	procCreateCompatibleDC         = gdi32.NewProc("CreateCompatibleDC")
	procCreateCompatibleBitmap     = gdi32.NewProc("CreateCompatibleBitmap")
	procSelectObject               = gdi32.NewProc("SelectObject")
	procBitBlt                     = gdi32.NewProc("BitBlt")
	procDeleteDC                   = gdi32.NewProc("DeleteDC")
	procDeleteObject               = gdi32.NewProc("DeleteObject")
	procGetDIBits                  = gdi32.NewProc("GetDIBits")
	procGetSystemMetrics           = user32.NewProc("GetSystemMetrics")
	procOpenClipboard              = user32.NewProc("OpenClipboard")
	procCloseClipboard             = user32.NewProc("CloseClipboard")
	procEmptyClipboard             = user32.NewProc("EmptyClipboard")
	procSetClipboardData           = user32.NewProc("SetClipboardData")
	procGlobalAlloc                = kernel32.NewProc("GlobalAlloc")
	procGlobalLock                 = kernel32.NewProc("GlobalLock")
	procGlobalUnlock               = kernel32.NewProc("GlobalUnlock")
	procBeep                       = kernel32.NewProc("Beep")
	procCreateWindowEx             = user32.NewProc("CreateWindowExW")
	procDefWindowProc              = user32.NewProc("DefWindowProcW")
	procDispatchMessage            = user32.NewProc("DispatchMessageW")
	procGetMessage                 = user32.NewProc("GetMessageW")
	procPostQuitMessage            = user32.NewProc("PostQuitMessage")
	procRegisterClassEx            = user32.NewProc("RegisterClassExW")
	procShowWindow                 = user32.NewProc("ShowWindow")
	procUpdateWindow               = user32.NewProc("UpdateWindow")
	procLoadCursor                 = user32.NewProc("LoadCursorW")
	procInvalidateRect             = user32.NewProc("InvalidateRect")
	procBeginPaint                 = user32.NewProc("BeginPaint")
	procEndPaint                   = user32.NewProc("EndPaint")
	procSetCapture                 = user32.NewProc("SetCapture")
	procReleaseCapture             = user32.NewProc("ReleaseCapture")
	procSetLayeredWindowAttributes = user32.NewProc("SetLayeredWindowAttributes")
	procDrawRect                   = gdi32.NewProc("Rectangle")
	procCreatePen                  = gdi32.NewProc("CreatePen")
	procGetStockObject             = gdi32.NewProc("GetStockObject")
	procDestroyWindow              = user32.NewProc("DestroyWindow")
	procSetROP2                    = gdi32.NewProc("SetROP2")
)

const (
	SRCCOPY        = 0x00CC0020
	CAPTUREBLT     = 0x40000000
	BI_RGB         = 0
	DIB_RGB_COLORS = 0
	CF_DIB         = 8
	GMEM_MOVEABLE  = 0x0002
	WS_POPUP       = 0x80000000
	WS_EX_LAYERED  = 0x00080000
	WS_EX_TOPMOST  = 0x00000008
	LWA_ALPHA      = 0x00000002
	SW_SHOW        = 5
	WM_DESTROY     = 0x0002
	WM_PAINT       = 0x000F
	WM_LBUTTONDOWN = 0x0201
	WM_LBUTTONUP   = 0x0202
	WM_MOUSEMOVE   = 0x0200
	WM_KEYDOWN     = 0x0100
	VK_ESCAPE      = 0x1B
	PS_SOLID       = 0
	NULL_BRUSH     = 5
	IDC_CROSS      = 32515
	R2_NOT         = 6
)

type POINT struct{ X, Y int32 }
type RECT struct{ Left, Top, Right, Bottom int32 }

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

var (
	screenW, screenH int
	startPt, endPt   POINT
	isDragging       bool
	captureResult    *image.RGBA
	prevRect         RECT
	hasPrevRect      bool
)

func getScreenSize() (int, int) {
	w, _, _ := procGetSystemMetrics.Call(0)
	h, _, _ := procGetSystemMetrics.Call(1)
	return int(w), int(h)
}

func drawXorRect(x1, y1, x2, y2 int32) {
	if x1 > x2 { x1, x2 = x2, x1 }
	if y1 > y2 { y1, y2 = y2, y1 }
	if x2-x1 < 2 || y2-y1 < 2 { return }

	hdc, _, _ := procGetDC.Call(0)
	defer procReleaseDC.Call(0, hdc)

	pen, _, _ := procCreatePen.Call(PS_SOLID, 4, 0x0000FF)
	nullBrush, _, _ := procGetStockObject.Call(NULL_BRUSH)
	oldPen, _, _ := procSelectObject.Call(hdc, pen)
	procSelectObject.Call(hdc, nullBrush)
	procSetROP2.Call(hdc, R2_NOT)
	procDrawRect.Call(hdc, uintptr(x1), uintptr(y1), uintptr(x2), uintptr(y2))
	procSelectObject.Call(hdc, oldPen)
	procDeleteObject.Call(pen)
}

func captureScreen(x, y, w, h int) *image.RGBA {
	hdcScreen, _, _ := procGetDC.Call(0)
	hdcMem, _, _ := procCreateCompatibleDC.Call(hdcScreen)
	hBmp, _, _ := procCreateCompatibleBitmap.Call(hdcScreen, uintptr(w), uintptr(h))
	procSelectObject.Call(hdcMem, hBmp)
	procBitBlt.Call(hdcMem, 0, 0, uintptr(w), uintptr(h),
		hdcScreen, uintptr(x), uintptr(y), SRCCOPY|CAPTUREBLT)

	bi := BITMAPINFO{}
	bi.BmiHeader.BiSize = uint32(unsafe.Sizeof(bi.BmiHeader))
	bi.BmiHeader.BiWidth = int32(w)
	bi.BmiHeader.BiHeight = -int32(h)
	bi.BmiHeader.BiPlanes = 1
	bi.BmiHeader.BiBitCount = 32
	bi.BmiHeader.BiCompression = BI_RGB

	pixels := make([]byte, w*h*4)
	procGetDIBits.Call(hdcMem, hBmp, 0, uintptr(h),
		uintptr(unsafe.Pointer(&pixels[0])),
		uintptr(unsafe.Pointer(&bi)), DIB_RGB_COLORS)

	img := image.NewRGBA(image.Rect(0, 0, w, h))
	for i := 0; i < w*h; i++ {
		img.Pix[i*4] = pixels[i*4+2]
		img.Pix[i*4+1] = pixels[i*4+1]
		img.Pix[i*4+2] = pixels[i*4]
		img.Pix[i*4+3] = 255
	}
	procDeleteObject.Call(hBmp)
	procDeleteDC.Call(hdcMem)
	procReleaseDC.Call(0, hdcScreen)
	return img
}

func copyToClipboard(img *image.RGBA) {
	bounds := img.Bounds()
	w, h := bounds.Max.X, bounds.Max.Y
	bi := BITMAPINFOHEADER{
		BiSize:        uint32(unsafe.Sizeof(BITMAPINFOHEADER{})),
		BiWidth:       int32(w),
		BiHeight:      int32(h),
		BiPlanes:      1,
		BiBitCount:    32,
		BiCompression: BI_RGB,
	}
	totalSize := int(unsafe.Sizeof(bi)) + w*h*4
	hMem, _, _ := procGlobalAlloc.Call(GMEM_MOVEABLE, uintptr(totalSize))
	ptr, _, _ := procGlobalLock.Call(hMem)
	sl := (*[1 << 28]byte)(unsafe.Pointer(ptr))
	hb := (*[40]byte)(unsafe.Pointer(&bi))
	copy(sl[:40], hb[:])
	off := 40
	for row := h - 1; row >= 0; row-- {
		for col := 0; col < w; col++ {
			sl[off] = img.Pix[(row*w+col)*4+2]
			sl[off+1] = img.Pix[(row*w+col)*4+1]
			sl[off+2] = img.Pix[(row*w+col)*4]
			sl[off+3] = 0
			off += 4
		}
	}
	procGlobalUnlock.Call(hMem)
	procOpenClipboard.Call(0)
	procEmptyClipboard.Call()
	procSetClipboardData.Call(CF_DIB, hMem)
	procCloseClipboard.Call()
}

func playBeep() {
	procBeep.Call(1318, 80)
	procBeep.Call(1760, 120)
}

func wndProc(hwnd, msg, wParam, lParam uintptr) uintptr {
	switch msg {
	case WM_LBUTTONDOWN:
		x := int32(lParam & 0xFFFF)
		y := int32((lParam >> 16) & 0xFFFF)
		startPt = POINT{x, y}
		endPt = startPt
		isDragging = true
		hasPrevRect = false
		procSetCapture.Call(hwnd)

	case WM_MOUSEMOVE:
		if isDragging {
			x := int32(lParam & 0xFFFF)
			y := int32((lParam >> 16) & 0xFFFF)
			if hasPrevRect {
				drawXorRect(prevRect.Left, prevRect.Top, prevRect.Right, prevRect.Bottom)
			}
			endPt = POINT{x, y}
			drawXorRect(startPt.X, startPt.Y, endPt.X, endPt.Y)
			prevRect = RECT{startPt.X, startPt.Y, endPt.X, endPt.Y}
			hasPrevRect = true
		}

	case WM_LBUTTONUP:
		if isDragging {
			isDragging = false
			procReleaseCapture.Call()
			if hasPrevRect {
				drawXorRect(prevRect.Left, prevRect.Top, prevRect.Right, prevRect.Bottom)
				hasPrevRect = false
			}
			x1, y1 := startPt.X, startPt.Y
			x2 := int32(lParam & 0xFFFF)
			y2 := int32((lParam >> 16) & 0xFFFF)
			if x1 > x2 { x1, x2 = x2, x1 }
			if y1 > y2 { y1, y2 = y2, y1 }
			w, h := int(x2-x1), int(y2-y1)
			procDestroyWindow.Call(hwnd)
			if w > 5 && h > 5 {
				captureResult = captureScreen(int(x1), int(y1), w, h)
				copyToClipboard(captureResult)
				playBeep()
			}
		}

	case WM_KEYDOWN:
		if wParam == VK_ESCAPE {
			if hasPrevRect {
				drawXorRect(prevRect.Left, prevRect.Top, prevRect.Right, prevRect.Bottom)
			}
			procDestroyWindow.Call(hwnd)
		}

	case WM_PAINT:
		var ps PAINTSTRUCT
		procBeginPaint.Call(hwnd, uintptr(unsafe.Pointer(&ps)))
		procEndPaint.Call(hwnd, uintptr(unsafe.Pointer(&ps)))

	case WM_DESTROY:
		procPostQuitMessage.Call(0)
	}

	ret, _, _ := procDefWindowProc.Call(hwnd, msg, wParam, lParam)
	return ret
}

func main() {
	screenW, screenH = getScreenSize()

	className, _ := syscall.UTF16PtrFromString("SnipOverlay")
	cursor, _, _ := procLoadCursor.Call(0, IDC_CROSS)

	wc := WNDCLASSEX{
		CbSize:        uint32(unsafe.Sizeof(WNDCLASSEX{})),
		LpfnWndProc:   syscall.NewCallback(wndProc),
		HCursor:       cursor,
		LpszClassName: className,
	}
	procRegisterClassEx.Call(uintptr(unsafe.Pointer(&wc)))

	winName, _ := syscall.UTF16PtrFromString("")
	hwnd, _, _ := procCreateWindowEx.Call(
		WS_EX_LAYERED|WS_EX_TOPMOST,
		uintptr(unsafe.Pointer(className)),
		uintptr(unsafe.Pointer(winName)),
		WS_POPUP,
		0, 0, uintptr(screenW), uintptr(screenH),
		0, 0, 0, 0,
	)

	procSetLayeredWindowAttributes.Call(hwnd, 0, 1, LWA_ALPHA)
	procShowWindow.Call(hwnd, SW_SHOW)
	procUpdateWindow.Call(hwnd)

	time.Sleep(150 * time.Millisecond)

	var msg MSG
	for {
		r, _, _ := procGetMessage.Call(uintptr(unsafe.Pointer(&msg)), 0, 0, 0)
		if r == 0 {
			break
		}
		procDispatchMessage.Call(uintptr(unsafe.Pointer(&msg)))
	}

	if captureResult != nil && len(os.Args) > 1 && os.Args[1] == "--save" {
		f, _ := os.Create("screenshot.png")
		png.Encode(f, captureResult)
		f.Close()
		fmt.Println("已保存到 screenshot.png")
	}
}
