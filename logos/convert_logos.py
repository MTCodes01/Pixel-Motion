import os
import subprocess
from PIL import Image, ImageFilter

chrome = r'C:\Program Files\Google\Chrome\Application\chrome.exe'
logos_dir = r'd:\Github\Pixel-Motion\logos'
res_dir = r'd:\Github\Pixel-Motion\src\ui'

# SVG sources
svg_map = {
    'light': os.path.join(logos_dir, 'Pixel-Motion Logo - light.svg'),
    'dark': os.path.join(logos_dir, 'Pixel-Motion Logo - dark.svg'),
    'color': os.path.join(logos_dir, 'Pixel-Motion Logo - color.svg'),
    'depth': os.path.join(logos_dir, 'Pixel-Motion Logo depth.svg')
}

def render_svg(svg_path, out_png, size=512):
    svg_url = 'file:///' + svg_path.replace('\\', '/')
    html_content = f'''<!DOCTYPE html>
<html>
<head>
<style>
html, body {{
    margin: 0; padding: 0;
    background: transparent;
    overflow: hidden;
    width: {size}px; height: {size}px;
    display: flex; align-items: center; justify-content: center;
}}
img {{
    max-width: {int(size * 0.92)}px;
    max-height: {int(size * 0.92)}px;
    object-fit: contain;
}}
</style>
</head>
<body>
<img src="{svg_url}">
</body>
</html>'''

    temp_html = os.path.join(logos_dir, 'temp_render.html')
    with open(temp_html, 'w', encoding='utf-8') as f:
        f.write(html_content)

    cmd = [
        chrome,
        '--headless',
        '--disable-gpu',
        '--default-background-color=00000000',
        f'--screenshot={out_png}',
        f'--window-size={size},{size}',
        'file:///' + temp_html.replace('\\', '/')
    ]
    subprocess.run(cmd, check=True)
    if os.path.exists(temp_html):
        os.remove(temp_html)
    print(f"Rendered {svg_path} -> {out_png}")

# 1. Render all SVGs to 512x512 PNGs
png_map = {}
for name, svg_path in svg_map.items():
    png_path = os.path.join(logos_dir, f'logo_{name}_512.png')
    render_svg(svg_path, png_path, 512)
    png_map[name] = png_path

icon_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]

def make_square(img, add_shadow=False):
    bbox = img.getbbox()
    if not bbox:
        return img
    cropped = img.crop(bbox)
    max_dim = int(max(cropped.width, cropped.height) * 1.08)
    
    square_img = Image.new('RGBA', (max_dim, max_dim), (0, 0, 0, 0))
    offset = ((max_dim - cropped.width) // 2, (max_dim - cropped.height) // 2)
    
    if add_shadow:
        # Create subtle dark outline/shadow for visibility on pure white backgrounds
        alpha = cropped.split()[3]
        shadow_mask = alpha.filter(ImageFilter.GaussianBlur(radius=3))
        shadow = Image.new('RGBA', cropped.size, (0, 0, 0, 160))
        shadow.putalpha(shadow_mask)
        
        # Paste shadow offset slightly
        square_img.paste(shadow, (offset[0], offset[1] + 2), shadow)
        square_img.paste(shadow, (offset[0], offset[1]), shadow)
    
    square_img.paste(cropped, offset, cropped)
    return square_img

# 2. Generate ICO files for all variants
for name, png_path in png_map.items():
    img = Image.open(png_path)
    sq = make_square(img, add_shadow=(name == 'light'))
    ico_path = os.path.join(res_dir, f'PixelMotion_{name}.ico')
    sq.save(ico_path, format='ICO', sizes=icon_sizes)
    print(f"Saved: {ico_path}")

# 3. Use LIGHT logo as the primary application icon and in-app UI logo
light_img = Image.open(png_map['light'])

# Primary application icon (with shadow so it pops on both light explorer & dark taskbar)
main_ico = os.path.join(res_dir, 'PixelMotion.ico')
sq_light_with_shadow = make_square(light_img, add_shadow=True)
sq_light_with_shadow.save(main_ico, format='ICO', sizes=icon_sizes)
print(f"Saved primary application icon (LIGHT): {main_ico}")

# UI header logo: pure crisp white light logo without shadow (since UI header is dark #18181F)
header_logo_png = os.path.join(res_dir, 'logo.png')
sq_light_clean = make_square(light_img, add_shadow=False)
sq_light_clean.resize((256, 256), Image.Resampling.LANCZOS).save(header_logo_png, format='PNG')
print(f"Saved in-app UI logo (LIGHT): {header_logo_png}")
