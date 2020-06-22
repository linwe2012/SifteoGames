from PIL import Image
import os

base = './'
prefix = 'resized_'
for d in os.listdir(base):
    if os.path.isdir(os.path.join(base, d)):
        continue
    if d.startswith(prefix):
        continue
    elif d.endswith('py'):
        continue
    else:
        im = Image.open(os.path.join(base, d))
        im = im.resize((128, 128))
        im.save(os.path.join(base, prefix + d))