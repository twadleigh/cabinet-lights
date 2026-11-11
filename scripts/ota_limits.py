"""Conservative bounds for the existing uncompressed LittleFS OTA format."""
from pathlib import Path

# Metadata, command file, and allocator headroom. This is not a free-space query.
STAGING_RESERVE = 64 * 1024


def staging_limit(filesystem_size):
    value = str(filesystem_size).strip().lower()
    multiplier = 1
    if value.endswith(('k', 'm')):
        multiplier = 1024 if value[-1] == 'k' else 1024 * 1024
        value = value[:-1]
    size = float(value) * multiplier
    if size != int(size) or size <= STAGING_RESERVE:
        raise ValueError('OTA filesystem size is invalid or too small')
    return int(size) - STAGING_RESERVE


def validate_image(path, filesystem_size):
    path = Path(path)
    limit = staging_limit(filesystem_size)
    size = path.stat().st_size
    if not 0 < size <= limit:
        raise ValueError(f'OTA artifact is {size} bytes; allowed range is 1..{limit} bytes')
    with path.open('rb') as image:
        if image.read(2) == b'\x1f\x8b':
            raise ValueError('Compressed OTA is not validated for this project; upload an uncompressed image')
    return size, limit
