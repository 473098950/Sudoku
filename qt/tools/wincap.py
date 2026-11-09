"""窗口自身表面抓取：用 PrintWindow(PW_RENDERFULLCONTENT) 抓指定 hwnd 的内容，
不受其它窗口遮挡影响（SetForegroundWindow 对非前台进程会被系统拒绝）。"""
import ctypes
import ctypes.wintypes as wt

from PIL import Image

USER32 = ctypes.windll.user32
GDI32 = ctypes.windll.gdi32

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    pass

USER32.GetWindowDC.restype = wt.HDC
USER32.GetWindowDC.argtypes = [wt.HWND]
USER32.GetDC.restype = wt.HDC
USER32.GetDC.argtypes = [wt.HWND]
USER32.ReleaseDC.argtypes = [wt.HWND, wt.HDC]
USER32.PrintWindow.argtypes = [wt.HWND, wt.HDC, wt.UINT]
USER32.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
GDI32.CreateCompatibleDC.restype = wt.HDC
GDI32.CreateCompatibleDC.argtypes = [wt.HDC]
GDI32.CreateCompatibleBitmap.restype = wt.HBITMAP
GDI32.CreateCompatibleBitmap.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int]
GDI32.SelectObject.restype = wt.HGDIOBJ
GDI32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
GDI32.DeleteObject.argtypes = [wt.HGDIOBJ]
GDI32.DeleteDC.argtypes = [wt.HDC]
GDI32.GetDIBits.argtypes = [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT,
                            ctypes.c_void_p, ctypes.POINTER(None), wt.UINT]

PW_RENDERFULLCONTENT = 0x00000002
BI_RGB = 0
DIB_RGB_COLORS = 0


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                ("biPlanes", wt.WORD), ("biBitCount", wt.WORD),
                ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG),
                ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]


class RGBQUAD(ctypes.Structure):
    _fields_ = [("rgbBlue", ctypes.c_ubyte), ("rgbGreen", ctypes.c_ubyte),
                ("rgbRed", ctypes.c_ubyte), ("rgbReserved", ctypes.c_ubyte)]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", RGBQUAD * 1)]


def window_rect(hwnd):
    r = wt.RECT()
    USER32.GetWindowRect(hwnd, ctypes.byref(r))
    return r.left, r.top, r.right - r.left, r.bottom - r.top


def capture(hwnd):
    """返回窗口矩形内的 RGB Image（含标题栏/边框区域，Qt 无边框窗口即整卡）。"""
    x, y, w, h = window_rect(hwnd)
    if w <= 0 or h <= 0:
        raise RuntimeError("invalid window rect")
    screen = USER32.GetDC(0)
    mem = GDI32.CreateCompatibleDC(screen)
    bmp = GDI32.CreateCompatibleBitmap(screen, w, h)
    old = GDI32.SelectObject(mem, bmp)
    USER32.PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT)

    bi = BITMAPINFO()
    bi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.bmiHeader.biWidth = w
    bi.bmiHeader.biHeight = -h            # 负值 = 自上而下
    bi.bmiHeader.biPlanes = 1
    bi.bmiHeader.biBitCount = 32
    bi.bmiHeader.biCompression = BI_RGB

    buf = ctypes.create_string_buffer(w * h * 4)
    GDI32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), DIB_RGB_COLORS)
    img = Image.frombuffer("RGB", (w, h), buf, "raw", "BGRX", 0, 1).copy()

    GDI32.SelectObject(mem, old)
    GDI32.DeleteObject(bmp)
    GDI32.DeleteDC(mem)
    USER32.ReleaseDC(0, screen)
    return img
