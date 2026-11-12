"""Render semantic menu commands with bundled Play, for layout review without a GPU.
This is a command-level audit: it does not render the 3D showroom or emulate GDI.
Usage: python tools/menu_preview062.py snapshots.json original-data output.png
"""
import json, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops, ImageFont
snapshots=json.loads(Path(sys.argv[1]).read_text())
root=Path(sys.argv[2]);fonts=Path(__file__).resolve().parent.parent/'fonts';cache={}
def texture(path):
    path=path.replace('\\','/').lower().removeprefix('data/')
    if path not in cache:
        try:cache[path]=Image.open(root/path).convert('RGBA')
        except Exception:cache[path]=None
    return cache[path]
selected=[s for s in snapshots if s['width']==1280 and s['height']==720]
canvas=Image.new('RGB',(1920,((len(selected)+2)//3)*390),(20,22,27))
for index,snap in enumerate(selected):
    im=Image.new('RGBA',(1280,720),(40,47,55,255))
    for c in snap['commands']:
        x,y,w,h=[round(c[n]) for n in ('x','y','w','h')];color=c['color'];rgba=((color>>16)&255,(color>>8)&255,color&255,(color>>24)&255)
        if c['kind']==2:
            px=max(6,min(256,round((24 if c['heading'] else 18)*c['scale'])))
            font=ImageFont.truetype(str(fonts/('Play-Bold.ttf' if c['heading'] else 'Play-Regular.ttf')),px)
            layer=Image.new('RGBA',im.size);ImageDraw.Draw(layer).text((x,y),c['value'],font=font,fill=rgba,anchor='lt');im.alpha_composite(layer);continue
        if w<=0 or h<=0:continue
        if c['kind']==0:tile=Image.new('RGBA',(w,h),rgba)
        else:
            tex=texture(c['value'])
            if tex is None:continue
            box=(round(c['u']*tex.width),round(c['v']*tex.height),round((c['u']+c['uw'])*tex.width),round((c['v']+c['vh'])*tex.height))
            if box[2]<=box[0] or box[3]<=box[1]:continue
            tile=tex.crop(box).resize((w,h),Image.Resampling.BILINEAR);tile=ImageChops.multiply(tile,Image.new('RGBA',tile.size,rgba))
        im.alpha_composite(tile,(x,y))
    x=(index%3)*640;y=(index//3)*390
    canvas.paste(im.convert('RGB').resize((640,360),Image.Resampling.LANCZOS),(x,y));ImageDraw.Draw(canvas).text((x+8,y+367),snap['title'] or 'START',fill='white')
canvas.save(sys.argv[3])
