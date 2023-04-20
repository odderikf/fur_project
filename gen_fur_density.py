import png
import random


def gen_texture(filename, width, height, count):
    img = [
        [
            0
        for _ in range(width*4)]
    for __ in range(height)]


    for _ in range(count):
        x = 4*random.randint(0, width-1)
        y = random.randint(0, height-1)
        img[y][x:x+4] = (0,0,0,255)

    with open(filename, 'wb') as f:
        w = png.Writer(width, height, alpha=True, greyscale=False)
        w.write(f, img)


gen_texture('./res/textures/dog_fur_tur.png', 1900, 1900, 1000000)
gen_texture('./res/textures/terrain_fur_tur.png', 2000, 2000, 1500000)

# then blur with gauss blur, stack the blur layer a bit so it's more of a local weighting than a true gauss.