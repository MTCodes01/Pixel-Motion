import os
import subprocess
from PIL import Image

chrome = r'C:\Program Files\Google\Chrome\Application\chrome.exe'
logos_dir = r'd:\Github\Pixel-Motion\logos'
res_dir = r'd:\Github\Pixel-Motion\src\ui'

# SVG sources
color_svg = os.path.join(logos_dir, 'Pixel-Motion Logo - color.svg')
depth_svg = os.path.join(logos_dir, 'Pixel-Motion Logo depth.svg')
dark_svg = os.path.join(logos_dir, 'Pixel-Motion Logo - dark.svg')
light_svg = os.path.join(logos_dir, 'Pixel-Motion Logo - light.svg')

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

# Render depth and color
depth_png = os.path.join(logos_dir, 'logo_depth_512.png')
color_png = os.path.join(logos_dir, 'logo_color_512.png')

render_svg(depth_svg, depth_png, 512)
render_svg(color_svg, color_png, 512)

# Create high quality .ico containing all standard Windows icon sizes
for name, png_path in [('color', color_png), ('depth', depth_png)]:
    img = Image.open(png_path)
    # Autocrop transparent borders while keeping square
    bbox = img.getbbox()
    if bbox:
        cropped = img.crop(bbox)
        # Create square canvas
        max_dim = max(cropped.width, cropped.height)
        square_img = Image.new('RGBA', (max_dim, max_dim), (0, 0, 0, 0))
        offset = ((max_dim - cropped.width) // 2, (max_dim - cropped.height) // 2)
        square_img.paste(cropped, offset)
        
        # Save ICO
        ico_path = os.path.join(res_dir, f'PixelMotion_{name}.ico')
        icon_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
        square_img.save(ico_path, format='ICO', sizes=icon_sizes)
        print(f"Saved icon: {ico_path}")
        
        # Also copy main ico to PixelMotion.ico
        if name == 'color' or name == 'depth':
            main_ico = os.path.join(res_dir, 'PixelMotion.ico')
            square_img.save(main_ico, format='ICO', sizes=icon_sizes)
            print(f"Saved primary app icon: {main_ico}")
            
            # Save logo.png for UI header
            header_logo_png = os.path.join(res_dir, 'logo.png')
            square_img.resize((128, 128), Image.Resampling.LANCZOS).save(header_logo_png, format='PNG')
            print(f"Saved UI logo: {header_logo_png}")
