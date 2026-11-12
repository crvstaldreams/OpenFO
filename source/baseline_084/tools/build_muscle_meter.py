#!/usr/bin/env python3
"""Render the shared OpenFO dual-dial HUD atlas and its two mode previews."""
from math import cos, pi, sin
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "fonts" / "Play-Bold.ttf"
SS = 3
FACE = (416, 244)
IVORY = "#f3ddba"
DIM = "#a2927d"
TICK = "#b7a58b"
RUBY = "#e35336"


def font(size):
    return ImageFont.truetype(str(FONT), round(size * SS))


def box(x0, y0, x1, y1):
    return tuple(round(v * SS) for v in (x0, y0, x1, y1))


def points(coords):
    return [(round(x * SS), round(y * SS)) for x, y in coords]


def polar(cx, cy, radius, angle):
    angle *= pi / 180
    return cx + radius * cos(angle), cy + radius * sin(angle)


def ellipse(draw, x, y, r, fill):
    draw.ellipse(box(x-r, y-r, x+r, y+r), fill)


def bezel(draw, x, y, r):
    ellipse(draw, x, y+3, r+3, "#08090a")
    ellipse(draw, x, y, r, "#2b2b2b")
    ellipse(draw, x, y, r-1, "#85817b")
    ellipse(draw, x, y, r-2.6, "#3b3b3c")
    for start, end, color in ((185, 308, "#b1a69a"), (206, 282, "#cbc0b1"),
                              (13, 162, "#504b47"), (310, 347, "#5d564d")):
        draw.arc(box(x-r+3, y-r+3, x+r-3, y+r-3), start, end, color, width=SS)
    ellipse(draw, x, y, r-5, "#090a0b")
    ellipse(draw, x, y, r-7, "#66605a")
    ellipse(draw, x, y, r-8.5, "#17191b")
    ellipse(draw, x, y, r-10, "#252529")

    # Fine concentric bands give the dial a dark spun-metal finish.
    for band in range(int(r-11), 0, -1):
        t = 1 - band/(r-10)
        base = round(25 + t*14 + (sin(band*1.9)+1)*.8)
        ellipse(draw, x, y, band, (base, base, base+2, 255))
    draw.arc(box(x-r+10, y-r+10, x+r-10, y+r-10), 205, 335, "#51453a", width=SS)
    draw.arc(box(x-r+10, y-r+10, x+r-10, y+r-10), 20, 154, "#121315", width=SS)


def center_text(draw, x, y, value, size, color):
    draw.text((round(x*SS), round(y*SS)), str(value), font=font(size), fill=color, anchor="mm")


def scale(draw, x, y, r, maximum, interval, subdivisions, digit_size, red_start=None):
    majors = maximum // interval
    tick_count = majors * subdivisions
    for tick in range(tick_count + 1):
        angle = 135 + 270 * tick/tick_count
        major = tick % subdivisions == 0
        color = RUBY if red_start is not None and tick / tick_count >= red_start else IVORY if major else TICK
        draw.line(points((polar(x,y,r-15,angle),polar(x,y,r-(27 if major else 21),angle))),
                  fill=color, width=round((2.5 if major else 1.15)*SS))
        if major:
            tx,ty = polar(x,y,r-38,angle)
            center_text(draw,tx,ty,tick//subdivisions*interval,digit_size,color)
    if red_start is not None:
        draw.arc(box(x-r+12,y-r+12,x+r-12,y+r-12), 135+270*red_start, 405, RUBY, width=SS)


def inset(draw, xy, radius=5):
    draw.rounded_rectangle(box(*xy), radius*SS, "#625c54")
    x0,y0,x1,y1=xy
    draw.rounded_rectangle(box(x0+1,y0+1,x1-1,y1-1), (radius-1)*SS, "#111214")
    draw.rounded_rectangle(box(x0+3,y0+3,x1-3,y1-3), (radius-2)*SS, "#08090b")


def make_face(imperial):
    image = Image.new("RGBA", (FACE[0]*SS, FACE[1]*SS))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(box(138,119,402,234), 25*SS, "#08090a")
    draw.rounded_rectangle(box(139,116,402,230), 24*SS, "#77706a")
    draw.rounded_rectangle(box(141,118,400,228), 22*SS, "#393737")
    draw.rounded_rectangle(box(144,120,397,225), 19*SS, "#1c1d20")
    for y in range(122, 221, 2):
        draw.line(points(((169,y),(387,y))), "#202124" if y%4 else "#1c1d1f", width=SS)
    bezel(draw,113,108,101)
    bezel(draw,305,93,82)
    scale(draw,113,108,101,150 if imperial else 240,15 if imperial else 20,4,12.5)
    scale(draw,305,93,82,10,1,4,13,.8)

    center_text(draw,113,137,"MPH" if imperial else "KM/H",12,IVORY)
    inset(draw,(78,161,148,190))
    center_text(draw,305,114,"RPM",13,IVORY)
    center_text(draw,305,129,"×1000",10,DIM)
    inset(draw,(220,176,341,222))
    inset(draw,(344,183,386,220))
    center_text(draw,365,177,"GEAR",10,DIM)
    ellipse(draw,332,187,4.5,"#292220")

    for i in range(10):
        left=230+i*11
        draw.rounded_rectangle(box(left,200,left+8,214),SS,"#252325")
        draw.line(points(((left+1,201),(left+7,201))),"#3d3732",width=SS)

    for x,y in ((390,136),(389,219),(204,218)):
        ellipse(draw,x,y,3.4,"#111214")
        ellipse(draw,x,y,2.6,"#665f57")
        draw.line(points(((x-1.5,y+.9),(x+1.5,y-.9))),"#1d1b1a",width=SS)

    return image.resize(FACE, Image.Resampling.LANCZOS)


def make_sprites():
    # Local sheet begins at atlas x=424; no neighboring sprite shares its border.
    image=Image.new("RGBA",(88*SS,264*SS))
    draw=ImageDraw.Draw(image)
    draw.polygon(points(((1,11),(15,3),(79,7.5),(15,13))),"#7d1f15")
    draw.polygon(points(((2,9),(15,4),(80,7),(15,10))),"#e45b39")
    draw.polygon(points(((14,5),(73,7),(14,7.7))),"#ffad6b")
    ellipse(draw,8,37,7.5,"#0e0e10")
    ellipse(draw,8,37,6.1,"#a59a86")
    ellipse(draw,8,37,5.1,"#4a453e")
    ellipse(draw,8,37,3.7,"#2b2a29")
    draw.arc(box(4.3,33.3,11.7,40.7),180,295,"#bba88e",width=SS)
    ellipse(draw,30,36,4.7,"#6c4129")
    ellipse(draw,30,36,3.5,"#ee9452")
    ellipse(draw,29.4,35.3,2,"#ffda95")

    for i in range(10):
        x=i*8
        draw.rounded_rectangle(box(x,52,x+6,62),SS,"#d5d5d5")
        draw.line(points(((x+1,53),(x+5,53))),"#ffffff",width=SS)
        draw.line(points(((x+1,61),(x+5,61))),"#999999",width=SS)
    draw.text((0,round(76.5*SS)),"NITRO",font=font(12),fill="#f6c68b",anchor="lm")
    draw.text((0,round(95.5*SS)),"DAMAGE",font=font(12),fill="#f19a83",anchor="lm")

    for i,digit in enumerate("0123456789RN"):
        x=i%3*27+11.5
        y=i//3*36+113+15.5
        center_text(draw,x,y,digit,24,"#ffffff")
    return image.resize((88,264),Image.Resampling.LANCZOS)


def preview(atlas, imperial, speed, rpm, gear, segments, derby):
    top=248 if imperial else 0
    img=atlas.crop((0,top,416,top+244))

    def sprite(source, target, tint=None):
        tile=atlas.crop(source).resize((target[2],target[3]),Image.Resampling.LANCZOS)
        if tint:
            tr,tg,tb=tint
            rr,gg,bb,aa=tile.split()
            tile=Image.merge("RGBA",(rr.point(lambda v:v*tr/255),gg.point(lambda v:v*tg/255),
                                      bb.point(lambda v:v*tb/255),aa))
        img.alpha_composite(tile,target[:2])

    for x,y,w,h,val,limit in ((113,108,85,15,speed,150 if imperial else 240),(305,93,70,13,rpm,10000)):
        tile=atlas.crop((424,0,506,16)).resize((w,h),Image.Resampling.LANCZOS)
        field=Image.new("RGBA",(174,174))
        field.alpha_composite(tile,(87-round(w*15/82),87-round(h*7.5/16)))
        field=field.rotate(-(135+270*min(val/limit,1)),Image.Resampling.BICUBIC)
        img.alpha_composite(field,(x-87,y-87))
        sprite((424,29,440,45),(x-8,y-8,16,16))

    def digit(d,x,y,w,h):
        sx,sy=424+(d%3)*27,113+(d//3)*36
        sprite((sx,sy,sx+23,sy+32),(x,y,w,h),(255,214,151))
    digits=str(speed)
    for i,d in enumerate(digits):
        digit(int(d),113-10*len(digits)+20*i,160,20,29)
    digit(gear,353,187,23,32)
    y=88 if derby else 69
    sprite((424,y,504,y+15),(230,181,80,15))
    if not derby and segments:
        sprite((448,30,460,42),(326,181,12,12))
    if segments:
        w=8*segments
        sprite((424,52,424+w,63),(230,200,11*segments,15),(255,119,101) if derby else (255,196,113))
    return img


def main():
    atlas=Image.new("RGBA",(512,512))
    atlas.alpha_composite(make_face(False),(0,0))
    atlas.alpha_composite(make_face(True),(0,248))
    atlas.alpha_composite(make_sprites(),(424,0))
    atlas.save(ROOT/"hud_assets"/"muscle_meter.dds")

    image=Image.new("RGB",(904,292),"#25262a")
    draw=ImageDraw.Draw(image)
    heading=ImageFont.truetype(str(FONT),15)
    for x, label, mode in ((24,"RACE / KM/H", (False,124,5840,4,8,False)),
                           (464,"DERBY / MPH",(True,68,4150,3,4,True))):
        draw.text((x+10,16),label,font=heading,fill="#ddc6a9")
        panel=preview(atlas,*mode)
        image.paste(panel,(x,37),panel.getchannel("A"))
    image.save(ROOT/"reports"/"muscle_meter_076_preview.png")


if __name__=="__main__":
    main()
