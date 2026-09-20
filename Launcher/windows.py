"""Single-screen normal-window layout; deliberately no display enumeration."""
import ctypes
from ctypes import wintypes

def work_area():
    rect = wintypes.RECT()
    if not ctypes.windll.user32.SystemParametersInfoW(48, 0, ctypes.byref(rect), 0):
        raise OSError('Cannot read desktop working area')
    return rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top

def rectangles(area):
    x,y,w,h = area
    gap = 8
    half = (w-gap)//2
    height = (h-gap)//2
    return {'Command': (x,y,half,h), 'Map': (x+half+gap,y,w-half-gap,height),
            'Video': (x+half+gap,y+height+gap,w-half-gap,h-height-gap)}

def windows_for_pid(pid):
    found = []
    user = ctypes.windll.user32
    callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    @callback
    def each(hwnd, _):
        owner = wintypes.DWORD(); user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user.IsWindowVisible(hwnd) and user.GetWindowTextLengthW(hwnd) > 0:
            found.append(hwnd)
        return True
    user.EnumWindows(each, 0)
    return found

def place(pid, rect, title):
    for hwnd in windows_for_pid(pid):
        user = ctypes.windll.user32
        user.SetWindowTextW(wintypes.HWND(hwnd), title)
        user.ShowWindow(wintypes.HWND(hwnd), 9)
        user.SetWindowPos(wintypes.HWND(hwnd), None, *rect, 0x0014)
        return True
    return False

def focus(pid):
    windows = windows_for_pid(pid)
    if windows:
        ctypes.windll.user32.ShowWindow(wintypes.HWND(windows[0]), 3)
        ctypes.windll.user32.SetForegroundWindow(wintypes.HWND(windows[0]))
