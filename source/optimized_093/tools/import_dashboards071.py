#!/usr/bin/env python3
"""Package reviewed, generated dashboard atlases as complete DXT1 mip chains.

Only format conversion/resampling occurs here; detail redraws are the PNG inputs.
Requires Pillow for verification and ImageMagick for DDS encoding.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
from PIL import Image

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    encoder = shutil.which('magick') or shutil.which('convert')
    if not encoder:
        raise RuntimeError('ImageMagick is required')
    rows = []
    for car in range(1, 17):
        source = root / 'dashboard_source071' / f'car_{car}.png'
        if not source.is_file():
            raise RuntimeError(f'Missing reviewed atlas: {source}')
        with Image.open(source) as im:
            generated_size = im.size
            if im.width != im.height or im.width < 1024:
                raise RuntimeError(f'Unexpected generated atlas size: {source} {im.size}')
            # Native dashboards are fully opaque; alpha is not a glass mask.
            if im.convert('RGBA').getextrema()[3] != (255, 255):
                raise RuntimeError(f'Unexpected transparency in {source}')
        output = root / 'data' / 'cars' / f'car_{car}' / 'dashboard.dds'
        output.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([encoder, str(source), '-resize', '2048x2048!',
                        '-alpha', 'off', '-define', 'dds:compression=dxt1',
                        str(output)], check=True)
        raw = output.read_bytes()
        height, width = struct.unpack_from('<II', raw, 12)
        mips = struct.unpack_from('<I', raw, 28)[0]
        assert raw[:4] == b'DDS ' and raw[84:88] == b'DXT1'
        assert (width, height, mips) == (2048, 2048, 12)
        expected = 128 + sum(max(1, (2048 >> i) // 4) ** 2 * 8 for i in range(12))
        assert len(raw) == expected, (output, len(raw), expected)
        with Image.open(output) as decoded:
            decoded.load()
            assert decoded.size == (2048, 2048)
        rows.append({'car': car,
                     'source': source.relative_to(root).as_posix(),
                     'generated_resolution': list(generated_size),
                     'source_sha256': sha(source),
                     'output': output.relative_to(root).as_posix(),
                     'dds_resolution': [width, height], 'mip_levels': mips,
                     'compression': 'DXT1', 'bytes': len(raw),
                     'sha256': sha(output), 'decoded': True})
    report = root / 'reports' / 'fixes_071' / 'dashboards.json'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({'method': 'Built-in image edit per original atlas; DDS conversion only',
                                  'assets': rows,
                                  'limits': 'Generated microdetail; exact per-texel UV registration and native Windows cockpit appearance unverified'}, indent=2) + '\n')
    print(f'{len(rows)}/16 dashboard DDS decoded; all 2048 square, DXT1, 12 mip levels')

if __name__ == '__main__':
    main()
