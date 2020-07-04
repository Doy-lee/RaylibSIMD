# RaylibSIMD
Toy re-implementations of some of the software image routines in Raylib using the SSE instruction set for my learning purposes.

To test it out, copy `RaylibSIMD.h` into Raylib's folder and in `textures.c` after all the file includes, include and enable the single header file.

```
#define RAYLIB_SIMD_IMPLEMENTATION
#include "RaylibSIMD.h"
```
