# RaylibSIMD
Re-implementations of some of the software image routines in Raylib using the SSE instruction set for educational purposes.

To test it out, copy `RaylibSIMD.h` into Raylib's folder and in `textures.c` after all the file includes, include and enable the single header file.

```
#define RAYLIB_SIMD_IMPLEMENTATION
#include "RaylibSIMD.h"
```

RaylibSIMD offers accelerated versions available with the `RaylibSIMD_*` prefix.

```cpp
void  RaylibSIMD_ImageDraw            (Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, Color tint);
Image RaylibSIMD_GenImageColor        (int width, int height, Color color);
void  RaylibSIMD_ImageDrawRectangleRec(Image *dst, Rectangle rec, Color color);
void  RaylibSIMD_ImageDrawRectangle   (Image *dst, int posX, int posY, int width, int height, Color color);
void  RaylibSIMD_ImageClearBackground (Image *dst, Color color);
```
