"""Assemble the 26 local preview frames; does not touch firmware files."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
paths = [root / 'frames' / f'{i:02d}.png' for i in range(26)]
frames = []
for path in paths:
    with Image.open(path) as frame:
        frames.append(frame.convert('RGB').resize((516, 960), Image.Resampling.NEAREST))
palette = frames[-1].quantize(colors=128)
indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
indexed[0].save(root / 'eject-up.gif', save_all=True, append_images=indexed[1:],
                duration=100, loop=0, optimize=False, disposal=2)
for path in paths:
    if path.resolve().parent != (root / 'frames').resolve():
        raise ValueError('Frame path outside preview frame directory')
    path.unlink()
(root / 'frames').rmdir()
print('Saved eject-up.gif (26 frames, 2.6 s loop).')
