# Joker60 dog image

Extracted from the Joker60_V3.5 keyboard project for reference/reuse in the
PM_Controller repository. This directory is standalone; the firmware does not
reference the PNG yet.

- File: `dog.png`
- Size: 240 x 91 px
- Format: PNG, 24-bit RGB, black silhouette on white
- Original data: `E:\Develop\Joker60_V3.5\Joker60_V3_5_Code\Composite\HARDWARE\LCD\pic.h`
  - `const unsigned char gImage_DOG[43680]`
  - RGB565, big-endian, 240 x 91 (43,680 bytes)
  - The 8-byte metadata comment in `pic.h` was excluded from the pixel data.
- Display use in the source project: `Joker60_V3_5_Code/Composite/Core/Src/freertos.c`
  - `LCD_ShowPicture(0, 10, 240, 91, gImage_DOG);`

The image is kept as an asset only; no source code or build files were changed.
