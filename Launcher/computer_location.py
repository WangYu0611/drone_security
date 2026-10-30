"""Windows system-location adapter; never uses IP lookup or changes permissions.

Only the fixed companion script queries the Windows location service. Its
permission prompt is suppressed. Callers must still obtain a usable fix from
Windows; this module does not fabricate a city or a physical ground elevation.
"""
from __future__ import annotations

import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time


_ERRORS = {
    'unsupported': 'Windows system location is unsupported on this computer. / 此电脑不支持 Windows 系统定位。',
    'denied': 'Windows location permission is denied. Check the system location settings. / Windows 定位权限未授权，请检查系统定位设置。',
    'disabled': 'Windows location services are disabled or unavailable. / Windows 定位服务已关闭或不可用。',
    'timeout': 'Windows system location timed out. / 获取 Windows 电脑位置超时。',
    'no_position': 'Windows has no usable system location fix. / Windows 尚未提供有效的电脑位置。',
    'unavailable': 'Windows system location could not be read. / 无法读取 Windows 电脑位置。',
    'cancelled': 'Computer location was cancelled. / 已取消获取电脑位置。',
    'invalid_position': 'Windows returned invalid location information. / Windows 返回的电脑位置信息无效。',
    'stale': 'Windows system location is older than five minutes. / Windows 电脑位置已超过五分钟，请更新系统定位后重试。',
}


def _finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def _is_cancelled(cancelled):
    if cancelled is None:
        return False
    return bool(cancelled() if callable(cancelled) else cancelled.is_set())


def _location(payload, geoid):
    if not isinstance(payload, dict) or payload.get('ok') is not True:
        code = payload.get('error') if isinstance(payload, dict) else 'invalid_position'
        if not isinstance(code, str):
            code = 'invalid_position'
        raise RuntimeError(_ERRORS.get(code, _ERRORS['unavailable']))
    longitude, latitude = payload.get('longitude'), payload.get('latitude')
    accuracy, timestamp = payload.get('accuracy_m'), payload.get('timestamp')
    if (not _finite(longitude) or not -180 <= longitude <= 180
            or not _finite(latitude) or not -85.05112878 <= latitude <= 85.05112878
            or (longitude == 0 and latitude == 0)
            or not _finite(accuracy) or accuracy <= 0
            or not _finite(timestamp)):
        raise RuntimeError(_ERRORS['invalid_position'])
    age = time.time() - timestamp
    if age < 0:
        raise RuntimeError(_ERRORS['invalid_position'])
    if age > 300:
        raise RuntimeError(_ERRORS['stale'])
    altitude = payload.get('altitude')
    estimated = (payload.get('altitude_reference') != 'ellipsoid' or altitude is None
                 or (type(altitude) is float and math.isnan(altitude)))
    if estimated:
        # A camera anchor only: 0 m ellipsoid is not a measured ground height.
        height = 0.0
    elif _finite(altitude):
        # The Windows Location bridge reports reference-ellipsoid altitude.
        # The separate UAV AMSL geoid correction must not be added a second time.
        height = altitude
    else:
        raise RuntimeError(_ERRORS['invalid_position'])
    if not _finite(height) or not -1000 <= height <= 100000:
        raise RuntimeError(_ERRORS['invalid_position'])
    return {'longitude': float(longitude), 'latitude': float(latitude),
            'altitude': float(height), 'source': 'computer',
            'accuracy_m': float(accuracy), 'height_estimated': estimated}


def _reap(process):
    """Stop only the helper retained by this invocation, then drain its pipes."""
    if process.poll() is None:
        process.kill()
    try:
        process.communicate(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate(timeout=2)


def computer_origin(timeout=15, *, geoid=0.0, cancelled=None):
    """Read a fresh Windows fix without opening location-permission UI.

    Longitude/latitude are WGS84 within the application's map latitude limits.
    A measured system altitude explicitly tagged by this bridge is already
    ellipsoid height and is used unchanged. Missing/NaN or unknown-datum altitude
    yields an explicitly estimated 0 m ellipsoid camera anchor. ``geoid`` remains
    a validated interface-compatibility argument; the UAV AMSL correction never
    changes computer height. No position or diagnostics are printed or saved.
    ``cancelled`` accepts a zero-argument callback or threading.Event.
    """
    if not _finite(timeout) or not 0 < timeout <= 600 or not _finite(geoid):
        raise RuntimeError('Invalid computer-location timeout or height correction. / 电脑定位超时或高程修正配置无效。')
    # Include PowerShell process startup in the caller's total timeout budget.
    deadline = time.monotonic() + timeout
    deadline_unix_ms = math.floor((time.time() + timeout) * 1000)
    if _is_cancelled(cancelled):
        raise RuntimeError(_ERRORS['cancelled'])
    if sys.platform != 'win32':
        raise RuntimeError(_ERRORS['unsupported'])
    script = Path(__file__).with_suffix('.ps1')
    if not script.is_file():
        raise RuntimeError(_ERRORS['unsupported'])
    powershell = Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    arguments = [str(powershell), '-NoLogo', '-NoProfile', '-NonInteractive',
                 # This applies only to this process; no machine/user policy changes.
                 '-ExecutionPolicy', 'Bypass', '-File', str(script),
                 '-TimeoutSeconds', str(math.ceil(timeout)),
                 '-DeadlineUnixMilliseconds', str(deadline_unix_ms)]
    startup = subprocess.STARTUPINFO() if hasattr(subprocess, 'STARTUPINFO') else None
    if startup is not None:
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
    process = None
    try:
        process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, encoding='utf-8-sig', errors='strict',
                                   creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0),
                                   startupinfo=startup)
        while True:
            if _is_cancelled(cancelled):
                raise RuntimeError(_ERRORS['cancelled'])
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError(_ERRORS['timeout'])
            try:
                output, _ = process.communicate(timeout=min(.2, remaining))
                break
            except subprocess.TimeoutExpired:
                continue
        if process.returncode != 0:
            raise RuntimeError(_ERRORS['unavailable'])
        try:
            payload = json.loads(output)
        except (TypeError, ValueError):
            raise RuntimeError(_ERRORS['invalid_position']) from None
        return _location(payload, geoid)
    except (OSError, UnicodeError):
        raise RuntimeError(_ERRORS['unavailable']) from None
    finally:
        if process is not None and process.poll() is None:
            _reap(process)
