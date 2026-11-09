#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
创建桌面快捷方式：直接调用 Shell 的 IShellLink / IPersistFile COM 接口。
不依赖 PowerShell 的 WScript.Shell（本机被安全策略拦截），也不依赖 pywin32。
用法: python make_shortcut.py <target.exe> <workdir> <icon.ico> <desktop_name>
"""
import ctypes
import os
import sys
from ctypes import POINTER, byref, c_int, c_long, c_void_p, c_ubyte
from ctypes import wintypes

ole32 = ctypes.windll.ole32
shell32 = ctypes.windll.shell32

CLSCTX_INPROC_SERVER = 1
COINIT_APARTMENTTHREADED = 2


class GUID(ctypes.Structure):
    _fields_ = [("Data1", wintypes.DWORD),
                ("Data2", wintypes.WORD),
                ("Data3", wintypes.WORD),
                ("Data4", c_ubyte * 8)]


def guid(text):
    g = GUID()
    hr = ole32.CLSIDFromString(ctypes.c_wchar_p(text), byref(g))
    if hr < 0:
        raise OSError("CLSIDFromString failed: %#x" % (hr & 0xFFFFFFFF))
    return g


def vcall(obj, index, argtypes, *args):
    """通过 vtable 索引调用 COM 方法（obj 为接口指针）"""
    vtbl = ctypes.cast(obj, POINTER(POINTER(c_void_p)))[0]
    fn = ctypes.WINFUNCTYPE(c_long, c_void_p, *argtypes)(vtbl[index])
    return fn(obj, *args)


def main():
    target, workdir, icon, name = sys.argv[1:5]
    desktop = os.path.join(os.environ["USERPROFILE"], "Desktop")
    lnk = os.path.join(desktop, "Sudoku.lnk")      # 先用 ASCII 名，之后再改名
    if os.path.exists(lnk):
        os.remove(lnk)

    ole32.CoInitializeEx(None, COINIT_APARTMENTTHREADED)

    clsid = guid("{00021401-0000-0000-C000-000000000046}")   # ShellLink
    iid_link = guid("{000214F9-0000-0000-C000-000000000046}")  # IShellLinkW
    iid_pf = guid("{0000010B-0000-0000-C000-000000000046}")    # IPersistFile

    p = c_void_p()
    hr = ole32.CoCreateInstance(byref(clsid), None, CLSCTX_INPROC_SERVER,
                                byref(iid_link), byref(p))
    if hr < 0:
        raise OSError("CoCreateInstance failed: %#x" % (hr & 0xFFFFFFFF))

    # 20 SetPath / 9 SetWorkingDirectory / 7 SetDescription / 17 SetIconLocation
    vcall(p, 20, [wintypes.LPCWSTR], target)
    vcall(p, 9, [wintypes.LPCWSTR], workdir)
    vcall(p, 7, [wintypes.LPCWSTR], "Sudoku")
    vcall(p, 17, [wintypes.LPCWSTR, c_int], icon, 0)

    pf = c_void_p()
    hr = vcall(p, 0, [c_void_p, POINTER(c_void_p)], byref(iid_pf), byref(pf))  # QueryInterface
    if hr < 0:
        raise OSError("QueryInterface(IPersistFile) failed: %#x" % (hr & 0xFFFFFFFF))

    hr = vcall(pf, 6, [wintypes.LPCWSTR, c_int], lnk, 1)     # IPersistFile::Save
    if hr < 0:
        raise OSError("Save failed: %#x" % (hr & 0xFFFFFFFF))

    # 校验：.lnk 里应含目标的 UTF-16 路径
    with open(lnk, "rb") as f:
        blob = f.read()
    ok = target.encode("utf-16-le") in blob
    print("LNK_WRITTEN %d bytes, target_embedded=%s" % (len(blob), ok))

    final = os.path.join(desktop, name + ".lnk")
    if os.path.exists(final):
        os.remove(final)
    os.replace(lnk, final)
    print("SHORTCUT=" + final)


if __name__ == "__main__":
    main()
