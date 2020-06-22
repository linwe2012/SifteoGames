from PIL import Image
import os

base = 'w'
prefix = 'resized_'
for d in os.listdir(base):
    if base.startswith(prefix):
        continue
    else:
        im = Image.open(os.path.join(base, d))
        im = im.resize((64, 128))
        im.save(os.path.join(base, prefix + d))