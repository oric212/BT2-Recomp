"""Press to screen: one black-box tester for PCSX2 and the port (docs/plans/validation/PCSX2_COMPARISON_PLAN.md,
step 9, second part).

The tester starts a program in the same played fight (the title's demo battle as a versus battle, side 0 on the
keyboard: PCSX2 through a realtime comparison session, the port through the probe's live play), brings its window
to the front, and presses a key with SendInput. It reads the window's middle from the screen (GDI, the picture the
desktop composes) as fast as it can and times the first picture that differs from the one before it by much more
than the fight's own motion does. The press is Start: the pause menu covers the fight at once, so each pause and
each unpause is a sample. The time includes everything after the key: the program's input path, the game's frames,
the drawing and the presentation through the desktop compositor (windowed).

Usage: python tools/pcsx2_compare/press_to_screen.py --program pcsx2|port --disc <iso> [--presses 40]
       [--port-env BT2_SIXTY_FPS=1 ...] [--label NAME]
Writes work/latency/<label>.json (each sample's milliseconds and the sampling interval) and prints the median and
spread. The PC's keyboard focus goes to the program's window for the run.
"""
import argparse
import ctypes
import json
import os
import random
import statistics
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / 'tools/ps2recomp'))
import session as ses  # noqa: E402
from run_startup_probe import REPORT, command, gpu_build  # noqa: E402

user32 = ctypes.WinDLL('user32', use_last_error=True)
# Window and screen coordinates in the same pixels: without this a scaled display gives the window's rectangle in
# scaled units while the screen copy is in pixels, and the tester reads another part of the screen.
user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))              # Per monitor, version 2.
gdi32 = ctypes.WinDLL('gdi32', use_last_error=True)
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
user32.GetDC.restype = wintypes.HDC
user32.GetDC.argtypes = [wintypes.HWND]
user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
gdi32.CreateCompatibleDC.restype = wintypes.HDC
gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi32.CreateDIBSection.restype = wintypes.HBITMAP
gdi32.CreateDIBSection.argtypes = [wintypes.HDC, ctypes.c_void_p, wintypes.UINT, ctypes.POINTER(ctypes.c_void_p), wintypes.HANDLE, wintypes.DWORD]
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi32.BitBlt.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.HDC, ctypes.c_int, ctypes.c_int, wintypes.DWORD]
gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
gdi32.DeleteDC.argtypes = [wintypes.HDC]

VK = {'start': 0x0D, 'up': 0x26, 'cross': 0x5A}
EXTENDED = {0x26, 0x28, 0x25, 0x27}
DEMO_BATTLE = '8,120,9,23'          # The title's demo fight in PCSX2's session (characters 8 and 120, stage index 9, music 23).
SETS = ['%101A60:1:a0=0,a2=0,a3=1', '%101AB0:1:a2=0', '%101AF8:1:a3=0']


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [('wVk', wintypes.WORD), ('wScan', wintypes.WORD), ('dwFlags', wintypes.DWORD),
                ('time', wintypes.DWORD), ('dwExtraInfo', ctypes.c_size_t)]


class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [('ki', KEYBDINPUT), ('pad', ctypes.c_byte * 32)]
    _anonymous_ = ('u',)
    _fields_ = [('type', wintypes.DWORD), ('u', _U)]


def key(vk, down):
    flags = (0 if down else 2) | (1 if vk in EXTENDED else 0)
    item = INPUT(type=1)
    item.ki = KEYBDINPUT(vk, user32.MapVirtualKeyW(vk, 0), flags, 0, 0)
    if user32.SendInput(1, ctypes.byref(item), ctypes.sizeof(INPUT)) != 1:
        raise ctypes.WinError(ctypes.get_last_error())


def window_of(pid):
    """The process's largest visible top-level window, or None."""
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(window, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(window):
            rect = wintypes.RECT()
            user32.GetClientRect(window, ctypes.byref(rect))
            found.append((rect.right * rect.bottom, window))
        return True

    user32.EnumWindows(visit, 0)
    return max(found)[1] if found and max(found)[0] > 0 else None


def bring_to_front(window):
    user32.ShowWindow(window, 9)                                       # SW_RESTORE
    foreground = user32.GetForegroundWindow()
    here = kernel32.GetCurrentThreadId()
    there = user32.GetWindowThreadProcessId(foreground, None)
    user32.AttachThreadInput(here, there, True)
    user32.BringWindowToTop(window)
    user32.SetForegroundWindow(window)
    user32.AttachThreadInput(here, there, False)
    time.sleep(0.3)
    if user32.GetForegroundWindow() != window:
        raise RuntimeError('The program window did not get the keyboard focus.')


class Screen:
    """Reads a rectangle of the screen (the window's client area, its middle) into a numpy array."""

    def __init__(self, window, fraction=0.5):
        rect, origin = wintypes.RECT(), wintypes.POINT(0, 0)
        user32.GetClientRect(window, ctypes.byref(rect))
        user32.ClientToScreen(window, ctypes.byref(origin))
        w, h = int(rect.right * fraction) & ~3, int(rect.bottom * fraction) & ~3
        self.x, self.y = origin.x + (rect.right - w) // 2, origin.y + (rect.bottom - h) // 2
        self.w, self.h = w, h
        self.screen = user32.GetDC(None)
        self.dc = gdi32.CreateCompatibleDC(self.screen)
        header = (ctypes.c_uint32 * 10)(40, w, ctypes.c_uint32(-h).value, 1 | (32 << 16), 0, 0, 0, 0, 0, 0)
        bits = ctypes.c_void_p()
        self.bitmap = gdi32.CreateDIBSection(self.dc, header, 0, ctypes.byref(bits), None, 0)
        gdi32.SelectObject(self.dc, self.bitmap)
        self.pixels = np.ctypeslib.as_array(ctypes.cast(bits, ctypes.POINTER(ctypes.c_uint8)), shape=(h, w, 4))

    def grab(self):
        gdi32.BitBlt(self.dc, 0, 0, self.w, self.h, self.screen, self.x, self.y, 0x00CC0020)  # SRCCOPY
        return self.pixels[::2, ::2, :3].astype(np.int16)

    def close(self):
        gdi32.DeleteObject(self.bitmap)
        gdi32.DeleteDC(self.dc)
        user32.ReleaseDC(None, self.screen)


def press(screen, vk, hold, timeout=1.0):
    """Presses the key on a still picture and returns (milliseconds to the first changed picture or None, the
    threshold, the grabs' mean interval in seconds, the trace of changes). The picture's own noise over half a
    second sets the threshold."""
    previous, noise, stamp = screen.grab(), [], time.perf_counter()
    while time.perf_counter() - stamp < 0.5:
        now = screen.grab()
        noise.append(float(np.abs(now - previous).mean()))
        previous = now
    threshold = max(2.0, 3 * max(noise, default=0))
    previous = screen.grab()
    start = time.perf_counter()
    key(vk, True)
    released, seen, grabs, trace = False, None, 0, []
    while time.perf_counter() - start < timeout:
        now = screen.grab()
        grabs += 1
        t = time.perf_counter() - start
        if not released and t >= hold:
            key(vk, False)
            released = True
        difference = float(np.abs(now - previous).mean())
        if difference:
            trace.append([round(t * 1000, 1), round(difference, 1)])
        if seen is None and difference > threshold:
            seen = t
            if released:
                break
        previous = now
    if not released:
        key(vk, False)
    return (round(seen * 1000, 1) if seen is not None else None, threshold,
            (time.perf_counter() - start) / max(grabs, 1), trace[:40])


def check_picture(screen, path):
    """The captured region as a picture, to confirm it shows the game (Pillow)."""
    from PIL import Image
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(screen.grab()[:, :, ::-1].astype(np.uint8)).save(path)


def measure(window, vk, presses, hold, check=None):
    """Pauses the fight with the key, lets the pause menu settle, and times the unpause: a still picture that
    changes when the game takes the press. Returns the samples and the sampling interval in milliseconds."""
    bring_to_front(window)
    screen = Screen(window)
    samples, intervals = [], []
    try:
        if check:
            check_picture(screen, check)
        for n in range(presses):
            if user32.GetForegroundWindow() != window:
                bring_to_front(window)
            time.sleep(random.uniform(0.3, 0.8))
            key(vk, True)                                              # Pause.
            time.sleep(hold)
            key(vk, False)
            time.sleep(random.uniform(1.4, 1.9))                       # The menu's fade-in done.
            ms, threshold, interval, trace = press(screen, vk, hold)   # Unpause, timed.
            intervals.append(interval)
            samples.append({'press': n + 1, 'ms': ms, 'threshold': round(threshold, 2), 'trace': trace})
            print(f'unpause {n + 1}: {ms} ms (threshold {threshold:.1f})', flush=True)
    finally:
        screen.close()
    return samples, statistics.median(intervals) * 1000


def summary(samples):
    values = sorted(s['ms'] for s in samples if s['ms'] is not None)
    if not values:
        return {'samples': 0}
    q = lambda f: values[min(len(values) - 1, int(f * len(values)))]
    return {'samples': len(values), 'missed': sum(s['ms'] is None for s in samples), 'median_ms': statistics.median(values),
            'p10_ms': q(0.1), 'p90_ms': q(0.9), 'min_ms': values[0], 'max_ms': values[-1]}


def gamepads():
    """XInput controllers connected (the port gives the keyboard to port 2 when one is)."""
    try:
        xinput = ctypes.WinDLL('xinput1_4')
    except OSError:
        return 0
    state = (ctypes.c_byte * 16)()
    return sum(xinput.XInputGetState(k, state) == 0 for k in range(4))


def run_pcsx2(args):
    capture = ROOT / 'work/pcsx2-capture/ai-think-before-200'
    name = f'latency-{args.label}'
    folder = ROOT / 'work/pcsx2-compare' / name
    cmd = [sys.executable, str(ROOT / 'tools/pcsx2_compare/session.py'), '--realtime',
           '--pcsx2', str(capture / 'package/pcsx2-qt.exe'), '--bios', str(capture / 'profile/PCSX2/bios/scph39001.bin'),
           '--disc', str(args.disc), '--card', str(capture / 'profile/PCSX2/memcards/Mcd001.ps2'), '--session', name,
           *[a for s in SETS for a in ('--set', s)], '--log', f'@102F08:{args.ready_frame}', '--until', '@102F08',
           '--timeout', '1200']
    process = subprocess.Popen(cmd, cwd=ROOT)
    try:
        while not (folder / ses.READY).exists():
            if process.poll() is not None:
                raise RuntimeError('The PCSX2 session ended before the fight.')
            time.sleep(0.5)
        pid = int((folder / ses.READY).read_text(encoding='utf-8'))
        window = None
        while window is None:
            window = window_of(pid)
            time.sleep(0.2)
        return measure(window, VK[args.key], args.presses, args.hold, args.check)
    finally:
        (folder / ses.STOP).write_text('', encoding='utf-8')
        process.wait(timeout=60)


def run_port(args):
    if gamepads():
        raise RuntimeError('A gamepad is connected: the port gives the keyboard to port 2 then; disconnect it for the run.')
    cmd = command(ROOT, args.disc, 'Release')
    out = ROOT / 'work/latency' / f'{args.label}.report.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd[REPORT] = str(out)
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT2_')}
    env.update(BT2_START_BATTLE=DEMO_BATTLE, BT2_START_BATTLE_PLAYER='1', BT2_HOST_INPUT='1', BT2_VIEWER='1',
               BT2_VIEWER_SCALE='integer', BT2_AUDIO_DEVICE='1')
    if gpu_build('Release'):
        env['BT2_GPU_GS'] = 'on'
    for item in args.port_env:
        name, _, value = item.partition('=')
        env[name] = value
    process = subprocess.Popen(cmd, cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        window = None
        while window is None:
            if process.poll() is not None:
                raise RuntimeError('The port ended before its window opened.')
            window = window_of(process.pid)
            time.sleep(0.2)
        time.sleep(args.port_wait)                                     # The fight's intro (471 fight frames).
        return measure(window, VK[args.key], args.presses, args.hold, args.check)
    finally:
        process.kill()
        process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--program', choices=('pcsx2', 'port'), required=True)
    parser.add_argument('--disc', type=Path, required=True)
    parser.add_argument('--presses', type=int, default=40)
    parser.add_argument('--key', choices=sorted(VK), default='start')
    parser.add_argument('--hold', type=float, default=0.1, help='Seconds the key is held.')
    parser.add_argument('--label', required=True, help='The result file work/latency/<label>.json.')
    parser.add_argument('--port-env', action='append', default=[], help='NAME=VALUE for the port (BT2_SIXTY_FPS=1, ...).')
    parser.add_argument('--port-wait', type=float, default=30, help='Seconds from the port window to the fight.')
    parser.add_argument('--ready-frame', type=int, default=540, help='PCSX2: the fight frame after which to press.')
    args = parser.parse_args()
    args.disc = args.disc.resolve()
    args.check = ROOT / 'work/frames/latency' / f'{args.label}-region.png'
    samples, interval = run_pcsx2(args) if args.program == 'pcsx2' else run_port(args)
    result = {'program': args.program, 'port_env': args.port_env, 'key': args.key, 'sampling_ms': round(interval, 2),
              'summary': summary(samples), 'samples': samples}
    out = ROOT / 'work/latency' / f'{args.label}.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('program', 'port_env', 'sampling_ms', 'summary')}, indent=2))


if __name__ == '__main__':
    main()
