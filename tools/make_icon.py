#!/usr/bin/env python3
"""Builds src/mac/asr10.icns (and asr10_icon.png) from a crop of the emulator's VFD showing
"ASR-10". Usage: make_icon.py vfd_crop.png   (crop of the rendered display, see the README)."""
import sys, struct, io, os
from PIL import Image, ImageDraw, ImageFilter

src = Image.open(sys.argv[1]).convert('RGB')
glyphs = src.crop((505, 125, 760, 192))          # the "ASR-10" characters
S = 1024
icon = Image.new('RGBA', (S, S), (0, 0, 0, 0))
# body: rounded square with a vertical gradient
body = Image.new('RGBA', (S, S))
d = ImageDraw.Draw(body)
for y in range(S):
    c = int(58 - 26 * y / S)
    d.line([(0, y), (S, y)], fill=(c, c + 1, c + 4, 255))
mask = Image.new('L', (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle((100, 100, S - 100, S - 100), radius=180, fill=255)
icon.paste(body, (0, 0), mask)
# glass window
g = ImageDraw.Draw(icon)
g.rounded_rectangle((170, 330, S - 170, 694), radius=36, fill=(6, 9, 10, 255), outline=(70, 72, 76, 255), width=6)
gl = glyphs.resize((int(glyphs.width * 2.6), int(glyphs.height * 2.6)), Image.LANCZOS)
glow = gl.filter(ImageFilter.GaussianBlur(10))
x0 = (S - gl.width) // 2; y0 = 512 - gl.height // 2
icon.alpha_composite(Image.merge('RGBA', (*glow.split(), Image.new('L', glow.size, 140))), (x0, y0))
icon.paste(gl, (x0, y0))
# a row of small buttons under the display
for i in range(5):
    bx = 250 + i * 110
    g.rounded_rectangle((bx, 760, bx + 84, 812), radius=10, fill=(24, 25, 27, 255), outline=(80, 82, 86, 255), width=3)
out_dir = os.path.join(os.path.dirname(__file__), '..', 'src', 'mac')
icon.save(os.path.join(out_dir, 'asr10_icon.png'))
# .icns with PNG payloads
entries = [(b'ic07', 128), (b'ic08', 256), (b'ic09', 512), (b'ic10', 1024), (b'ic11', 32), (b'ic12', 64), (b'ic13', 256), (b'ic14', 512)]
chunks = b''
for tag, size in entries:
    buf = io.BytesIO(); icon.resize((size, size), Image.LANCZOS).save(buf, 'PNG')
    data = buf.getvalue()
    chunks += tag + struct.pack('>I', len(data) + 8) + data
with open(os.path.join(out_dir, 'asr10.icns'), 'wb') as f:
    f.write(b'icns' + struct.pack('>I', len(chunks) + 8) + chunks)
print('ok')
