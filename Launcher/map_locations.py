"""Workstation-local map locations for the launcher."""
from __future__ import annotations

import json
import math
from pathlib import Path
import re
from tempfile import NamedTemporaryFile

from launch_modes import valid_origin


MAP_LOCATIONS_PATH = Path('Saved/Launcher/map-locations.json')
DECIMAL = re.compile(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?')
LOCATION_FIELDS = {'name', 'latitude', 'longitude'}


def _coordinate(value, label):
    if isinstance(value, str):
        value = value.strip()
        if not DECIMAL.fullmatch(value):
            raise ValueError(f'{label} must be a finite decimal number / 请输入有效经纬度数字。')
    elif type(value) not in (int, float):
        raise ValueError(f'{label} must be a finite decimal number / 请输入有效经纬度数字。')

    try:
        result = float(value)
    except OverflowError:
        raise ValueError(f'{label} is out of range / 经纬度超出范围。') from None
    if not math.isfinite(result):
        raise ValueError(f'{label} must be a finite decimal number / 请输入有限经纬度数字。')
    return result


def map_origin(latitude, longitude):
    """Build a camera origin from WGS84 coordinates, with estimated height 0 m."""
    origin = {
        'latitude': _coordinate(latitude, 'Latitude'),
        'longitude': _coordinate(longitude, 'Longitude'),
        'altitude': 0.0,
    }
    if not valid_origin(origin):
        raise ValueError('Latitude must be within ±85.05112878 and longitude within ±180 / '
                         '纬度须在 ±85.05112878 内，经度须在 ±180 内。')
    return origin


def named_location(name, latitude, longitude):
    """Normalize a location, using its coordinates when its name is blank."""
    origin = map_origin(latitude, longitude)
    if name is not None and not isinstance(name, str):
        raise ValueError('Location name must be text / 地点名称必须为文本。')
    latitude, longitude = origin['latitude'], origin['longitude']
    return {
        'name': (name or '').strip() or f'{latitude}, {longitude}',
        'latitude': latitude,
        'longitude': longitude,
    }


def _validated_locations(locations):
    if not isinstance(locations, list):
        raise ValueError('Saved locations must be a list / 常用地点必须为列表。')
    result = []
    for location in locations:
        if not isinstance(location, dict) or location.keys() != LOCATION_FIELDS:
            raise ValueError('Saved location requires a name, latitude and longitude / '
                             '常用地点记录须包含名称、纬度和经度。')
        result.append(named_location(location['name'], location['latitude'], location['longitude']))
    return result


def load_map_locations(root):
    """Read saved locations without creating or overwriting local files."""
    target = Path(root) / MAP_LOCATIONS_PATH
    try:
        return _validated_locations(json.loads(target.read_text(encoding='utf-8-sig')))
    except FileNotFoundError:
        return []
    except (OSError, ValueError) as error:
        raise ValueError(f'Cannot load saved locations / 无法读取常用地点 {target}\n{error}') from error


def save_map_locations(root, locations):
    """Validate all records, then atomically replace the local list."""
    serialized = json.dumps(_validated_locations(locations), ensure_ascii=False, indent=2) + '\n'
    target = Path(root) / MAP_LOCATIONS_PATH
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with NamedTemporaryFile(mode='w', encoding='utf-8', dir=target.parent,
                                prefix=f'.{target.stem}-', suffix='.tmp', delete=False) as output:
            temporary = Path(output.name)
            output.write(serialized)
        temporary.replace(target)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
