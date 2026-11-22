"""Recompone assets/atlas.rgba a partir de los PNG incluidos. Requiere Pillow."""

from pathlib import Path
from PIL import Image, ImageDraw

base = Path(__file__).resolve().parent.parent / "assets"
names = """
 Tiles/grass_top Tiles/dirt_grass Tiles/dirt Tiles/stone Tiles/sand
 Tiles/redsand Tiles/gravel_stone Tiles/greysand Tiles/trunk_side Tiles/trunk_top
 Tiles/trunk_white_side Tiles/trunk_white_top Tiles/leaves Tiles/leaves_orange
 Tiles/wood Tiles/wood_red Tiles/brick_red Tiles/brick_grey Tiles/stone_grass
 Tiles/glass_frame Tiles/water Tiles/snow Tiles/ice Tiles/stone_coal
 Tiles/stone_iron Tiles/stone_gold Tiles/greystone_ruby Tiles/cotton_blue
 Tiles/table _ Tiles/greystone Items/pick_iron Items/axe_iron
 Items/shovel_iron Items/sword_iron Items/apple Other/sun Other/moon
 Tiles/grass1 Tiles/grass2 Tiles/grass3 Tiles/mushroom_red
""".split()
atlas = Image.new("RGBA", (512,512))
for i, name in enumerate(names):
    if name != "_":
        path = base / "kenney" / (name+".png")
        with Image.open(path) as image:
            tile = image.convert("RGBA").resize((60,60), Image.Resampling.LANCZOS)
    if i == 19:
        tint = Image.new("RGBA",(60,60),(139,209,218,65))
        tile = Image.alpha_composite(tint,tile)
    if i == 20:
        tint = Image.new("RGBA",(60,60),(52,146,174,255))
        tile = Image.alpha_composite(tint,tile).convert("RGB").convert("RGBA")
    if i == 29:
        tile = Image.new("RGBA",(60,60),(248,181,79,255))
        d = ImageDraw.Draw(tile)
        d.rectangle((0,0,59,59), outline=(90,57,44), width=5)
        d.rectangle((7,7,52,52), outline=(255,225,132), width=5)
        d.rectangle((25,0,32,59), fill=(148,85,57), width=2)
    x,y = (i%8)*64 + 2, (i//8)*64 + 2
    atlas.paste(tile,(x,y))
    atlas.paste(tile.crop((0,0,1,60)).resize((2,60)),(x-2,y))
    atlas.paste(tile.crop((59,0,60,60)).resize((2,60)),(x+60,y))
    atlas.paste(atlas.crop((x-2,y,x+62,y+1)).resize((64,2)),(x-2,y-2))
    atlas.paste(atlas.crop((x-2,y+59,x+62,y+60)).resize((64,2)),(x-2,y+60))
atlas.paste((255,255,255,255),(448,448,512,512))
(base/"atlas.rgba").write_bytes(atlas.tobytes())
