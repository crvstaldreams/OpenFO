#!/usr/bin/env python3
"""Inventory authored W32 topology/materials; never infer pole connections."""
import argparse, collections, json, pathlib, struct, zipfile

class Reader:
    def __init__(self, data): self.data, self.pos = data, 0
    def u(self):
        value = struct.unpack_from('<I', self.data, self.pos)[0]; self.pos += 4; return value
    def skip(self, count):
        self.pos += count
        if self.pos > len(self.data): raise ValueError('Truncated W32')
    def string(self):
        end = self.data.index(b'\0', self.pos)
        value = self.data[self.pos:end].decode('latin1'); self.pos = end + 1; return value

def audit(data, path):
    r = Reader(data)
    if r.u() not in (0x10004, 0x10005): raise ValueError('Unsupported W32')
    materials = []
    for _ in range(r.u()):
        if r.u() != 0x4354414d: raise ValueError('Missing MATC')
        name = r.string(); alpha = r.u(); r.skip(8); shader = r.u(); r.skip(100)
        materials.append(dict(name=name, alpha=alpha, shader=shader, textures=[r.string() for _ in range(3)]))
    for _ in range(r.u()):
        kind = r.u()
        if r.u(): raise ValueError('Compressed stream')
        count = r.u(); stride = 2 if kind == 2 else r.u()
        if kind == 1: r.u()
        r.skip(count * stride)
    modes, candidates = collections.Counter(), []
    for index in range(r.u()):
        r.u(); material = r.u(); vertices = r.u(); r.u(); primitives = r.u(); mode = r.u(); indices = r.u(); r.skip(24)
        streams = r.u(); r.skip(streams * 8); modes[mode] += 1
        mat = materials[material]
        if any(word in json.dumps(mat).lower() for word in ('wire', 'cable', 'electric')):
            candidates.append(dict(surface=index, vertices=vertices, primitives=primitives, indices=indices, mode=mode, material=mat))
    return dict(source=path, surface_modes=dict(sorted(modes.items())), line_surfaces=modes[2]+modes[3], material_candidates=candidates)

def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('source', type=pathlib.Path); args=parser.parse_args()
    source=args.source
    if source.is_dir():
        rows=[audit(p.read_bytes(),str(p.relative_to(source))) for p in sorted(source.rglob('track_geom.w32'))]
    else:
        with zipfile.ZipFile(source) as archive:
            rows=[audit(archive.read(name),name) for name in sorted(archive.namelist()) if name.endswith('/track_geom.w32')]
    print(json.dumps(dict(method='Raw original W32 record inventory; D3D modes 2/3 are line list/strip. Mode 0 uses native tree sprite stream. Material names alone do not establish overhead cable routing.', tracks=rows),indent=2))
if __name__ == '__main__': main()
