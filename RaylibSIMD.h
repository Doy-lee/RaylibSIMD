#ifndef RAYLIB_SIMD_H
#define RAYLIB_SIMD_H

#if defined(__cplusplus)
extern "C" {
#endif

RLAPI void  RaylibSIMD_ImageDraw            (Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, Color tint);
RLAPI Image RaylibSIMD_GenImageColor        (int width, int height, Color color);
RLAPI void  RaylibSIMD_ImageDrawRectangleRec(Image *dst, Rectangle rec, Color color);
RLAPI void  RaylibSIMD_ImageDrawRectangle   (Image *dst, int posX, int posY, int width, int height, Color color);
RLAPI void  RaylibSIMD_ImageClearBackground (Image *dst, Color color);

#if defined(__cplusplus)
}
#endif // extern "C"
#endif // RAYLIB_SIMD_H

#ifdef RAYLIB_SIMD_IMPLEMENTATION
#include <stdint.h>

#if defined(_MSC_VER)
    #define RS_COMPILER_MSVC
#elif defined(__clang__)
    #define RS_COMPILER_CLANG
#elif defined(__GNUC__)
    #define RS_COMPILER_GCC
#endif

#ifdef RS_COMPILER_MSVC
    #include <intrin.h>
#elif defined(RS_COMPILER_CLANG) || defined(RS_COMPILER_GCC)
    #include <x86intrin.h>
#else
    #error "SIMD Implementation Required"
#endif

#define RS_FILE_SCOPE static
#define RS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define RS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define RS_CAST(x) (x)

RS_FILE_SCOPE uint32_t RaylibSIMD__ColorToU32(Color color)
{
    uint32_t result = (RS_CAST(uint32_t) color.r << 0) |
                      (RS_CAST(uint32_t) color.g << 8) |
                      (RS_CAST(uint32_t) color.b << 16) |
                      (RS_CAST(uint32_t) color.a << 24);
    return result;
}

RS_FILE_SCOPE void RaylibSIMD__SoftwareBlendPixel(unsigned char const *src_ptr, unsigned char *dest_ptr, Color tint, float src_alpha_min)
{
    float const INV_255 = 1.f / 255.f;
    uint32_t src_pixel  = *RS_CAST(uint32_t *)src_ptr;
    uint32_t dest_pixel = *RS_CAST(uint32_t *)dest_ptr;
    float src_r = RS_CAST(float)((src_pixel >> 0) & 0xFF);
    float src_g = RS_CAST(float)((src_pixel >> 8) & 0xFF);
    float src_b = RS_CAST(float)((src_pixel >> 16) & 0xFF);
    float src_a = RS_CAST(float)((src_pixel >> 24) & 0xFF);
    src_a       = RS_MAX(src_a, src_alpha_min);

    float dest_r = RS_CAST(float)((dest_pixel >> 0) & 0xFF);
    float dest_g = RS_CAST(float)((dest_pixel >> 8) & 0xFF);
    float dest_b = RS_CAST(float)((dest_pixel >> 16) & 0xFF);
    float dest_a = RS_CAST(float)((dest_pixel >> 24) & 0xFF);

    float src_r01 = src_r * INV_255;
    float src_g01 = src_g * INV_255;
    float src_b01 = src_b * INV_255;
    float src_a01 = src_a * INV_255;

    float tint_r01  = tint.r * INV_255;
    float tint_g01  = tint.g * INV_255;
    float tint_b01  = tint.b * INV_255;
    float tint_a01  = tint.a * INV_255;

    float src_tint_r01 = tint_r01 * src_r01;
    float src_tint_g01 = tint_g01 * src_g01;
    float src_tint_b01 = tint_b01 * src_b01;
    float src_tint_a01 = tint_a01 * src_a01;

    float dest_r01 = dest_r * INV_255;
    float dest_g01 = dest_g * INV_255;
    float dest_b01 = dest_b * INV_255;
    float dest_a01 = dest_a * INV_255;

    float blend_a01     = src_tint_a01 + dest_a01 * (1.0f - src_tint_a01);
    float inv_blend_a01 = 1.f / blend_a01;

    float blend_r01 = ((src_tint_r01 * src_tint_a01) + (dest_r01 * dest_a01 * (1.f - src_tint_a01))) * inv_blend_a01;
    float blend_g01 = ((src_tint_g01 * src_tint_a01) + (dest_g01 * dest_a01 * (1.f - src_tint_a01))) * inv_blend_a01;
    float blend_b01 = ((src_tint_b01 * src_tint_a01) + (dest_b01 * dest_a01 * (1.f - src_tint_a01))) * inv_blend_a01;

    float blend_a = blend_a01 * 255.f;
    float blend_r = blend_r01 * 255.f;
    float blend_g = blend_g01 * 255.f;
    float blend_b = blend_b01 * 255.f;

    unsigned char blend_r255 = RS_CAST(unsigned char)blend_r;
    unsigned char blend_g255 = RS_CAST(unsigned char)blend_g;
    unsigned char blend_b255 = RS_CAST(unsigned char)blend_b;
    unsigned char blend_a255 = RS_CAST(unsigned char)blend_a;

    uint32_t blend_pixel = (RS_CAST(uint32_t)blend_r255 & 0xFF) << 0 |
                            (RS_CAST(uint32_t)blend_g255 & 0xFF) << 8 |
                            (RS_CAST(uint32_t)blend_b255 & 0xFF) << 16 |
                            (RS_CAST(uint32_t)blend_a255 & 0xFF) << 24;
    *(RS_CAST(uint32_t *)dest_ptr) = blend_pixel;
}

typedef struct
{
    __m128i shuffle;
    uint8_t r_bit_mask;
    uint8_t g_bit_mask;
    uint8_t b_bit_mask;
    uint8_t a_bit_mask;
    uint8_t r_bit_shift;
    uint8_t g_bit_shift;
    uint8_t b_bit_shift;
    uint8_t a_bit_shift;
    float r_to_01_coefficient;
    float b_to_01_coefficient;
    float g_to_01_coefficient;
    float a_to_01_coefficient;
} RaylibSIMD_PixelPerLaneShuffle;

static RaylibSIMD_PixelPerLaneShuffle RaylibSIMD__FormatToPixelPerLaneShuffle128Bit(int format)
{
    RaylibSIMD_PixelPerLaneShuffle result = {0};
    result.shuffle                        = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    result.r_bit_mask                     = 0xFF;
    result.g_bit_mask                     = 0xFF;
    result.b_bit_mask                     = 0xFF;
    result.a_bit_mask                     = 0xFF;
    result.r_bit_shift                    = 0;
    result.g_bit_shift                    = 8;
    result.b_bit_shift                    = 16;
    result.a_bit_shift                    = 24;
    result.r_to_01_coefficient            = 1.f / 255.f;
    result.g_to_01_coefficient            = 1.f / 255.f;
    result.b_to_01_coefficient            = 1.f / 255.f;
    result.a_to_01_coefficient            = 1.f / 255.f;

    switch(format)
    {
        default: break;

        // NOTE: We load 4 pixels x 4 colors at a time. But if the
        // source image is RGB, then the 4th color loaded in each
        // pixel is going to be the RED component of the next pixel.
        //
        // Pixels[] = {RGB, RGB, RGB, RGB, ...}
        //
        // For example, naively loading the next pixels in a 3BPP
        // byte stream, produces in a 128 bit SIMD register
        //
        // Pixel    |   1  2     3     4     5  6*
        // Register | {[RGBR] [GBRG] [BRGB] [RGBR]}
        //              ^
        //              |
        //              +---- This is the start of the 2nd pixel.
        //
        // * Note that only the red channel of the 6th pixel gets loaded.
        //
        // The 2nd pixel needs to be moved into the SIMD lane.
        // and so forth for subsequent pixels. We shift the color
        // channels to correctly set up the SIMD lane, like so.
        //
        // Pixel    |   1      2      3      4
        // Register | {[RGB.] [RGB.] [RGB.] [RGB.]]}
        //
        // We do this by shuffling the loaded bits into place
        // duplicating the red channel and copying onwards. In the
        // RGBA case, we do a no-op shuffle that preserves positions
        // of all color components to avoid branches in the blitting
        // hot path.

        // NOTE: R8G8B8 24bit Pixel
        // Bits       | 23 22 21 20   19 18 17 16 | 15 14 13 12   11 10 9 8 | 7654 3210
        // Color Bits |  R  R  R  R    R  R  R  R | G   G  G  G    G  G G G | BBBB BBBB
        //
        // A 128bit SIMD register with 4x32bit lanes can store
        // 1 pixel per register and the red channel of the next
        // pixel.
        //
        // Pixel    |   1  2     3     4     5  6
        // Register | {[RGBR] [GBRG] [BRGB] [RGBR]}
        //
        // Desired layout 1 pixel per 32 bit lane
        //
        // Pixel           |   1      2      3      4
        // Register        | {[RGB.] [RGB.] [RGB.] [RGB.]]}
        // Bits            |  [0:23] [24:47] [48:71] [72:95]
        // Bytes (Shuffle) |  [0:2]  [3:5]   [6:8]   [9:11]
        case UNCOMPRESSED_R8G8B8:
        {
            result.shuffle = _mm_setr_epi8(0, 1, 2, 0, // Lane 1
                                           3, 4, 5, 0,
                                           6, 7, 8, 0,
                                           9, 10, 11, 0);
        }
        break;

        // NOTE: RGBA4444 16bit Pixel
        // Bits       | 15 14 13 12 | 11 10 98 | 7654 | 3210
        // Color Bits |  R  R  R  R | G   G GG | BBBB | AAAA
        //
        // A 128bit SIMD register with 4x32bit lanes can store
        // 2 pixels per register.
        //
        // Register   | {[P1, P2] [P3, P4] [P5, P6] [P7, P8]}
        //
        // See UNCOMPRESSED_R8G8B8 for reason for shuffle.
        // Desired layout 1 pixel per 32 bit lane
        //
        // Register        | {[P1]   [P2]    [P3]    [P4]}
        // Bits            |  [0:15] [16:31] [32:46] [46:61]
        // Bytes (Shuffle) |  [0:1]  [2:3]   [4:5]   [6:7]
        case UNCOMPRESSED_R4G4B4A4:
        {
            result.shuffle = _mm_setr_epi8(0, 1, 0, 1, // Lane 1
                                           2, 3, 2, 3,
                                           4, 5, 4, 5,
                                           6, 7, 6, 7);
            result.r_bit_mask = 0b1111;
            result.g_bit_mask = 0b1111;
            result.b_bit_mask = 0b1111;
            result.a_bit_mask = 0b1111;

            result.r_bit_shift = 12;
            result.g_bit_shift = 8;
            result.b_bit_shift = 4;
            result.a_bit_shift = 0;

            result.r_to_01_coefficient = 1.f/15.f;
            result.g_to_01_coefficient = 1.f/15.f;
            result.b_to_01_coefficient = 1.f/15.f;
            result.a_to_01_coefficient = 1.f/15.f;
        }
        break;

        // NOTE: RGB565 16bit Pixel
        // Bits       | 15 14 13 12 11 | 10 98765 | 43210
        // Color Bits |  R  R  R  R  R | G  GGGGG | BBBBB
        //
        // A 128bit SIMD register with 4x32bit lanes can store
        // 2 pixels per register.
        //
        // Register   | {[P1, P2] [P3, P4] [P5, P6] [P7, P8]}
        //
        // See UNCOMPRESSED_R8G8B8 for reason for shuffle.
        // Desired layout 1 pixel per 32 bit lane
        //
        // Register        | {[P1]   [P2]    [P3]    [P4]}
        // Bits            |  [0:15] [16:31] [32:46] [46:61]
        // Bytes (Shuffle) |  [0:1]  [2:3]   [4:5]   [6:7]
        case UNCOMPRESSED_R5G6B5:
        {
            result.shuffle = _mm_setr_epi8(0, 1, 0, 1, // Lane 1
                                           2, 3, 2, 3,
                                           4, 5, 4, 5,
                                           6, 7, 6, 7);
            result.r_bit_mask = 0b011111;
            result.g_bit_mask = 0b111111;
            result.b_bit_mask = 0b011111;

            result.r_bit_shift = 11;
            result.g_bit_shift = 5;
            result.b_bit_shift = 0;

            result.r_to_01_coefficient = 1.f/31.f;
            result.g_to_01_coefficient = 1.f/63.f;
            result.b_to_01_coefficient = 1.f/31.f;
        }
        break;

        // NOTE: RGBA5551 16bit Pixel
        // Bits       | 15 14 13 12 11 | 10 9876 | 54321 | 0
        // Color Bits |  R  R  R  R  R | G  GGGG | BBBBB | A
        //
        // A 128bit SIMD register with 4x32bit lanes can store
        // 2 pixels per register.
        //
        // Register   | {[P1, P2] [P3, P4] [P5, P6] [P7, P8]}
        //
        // See UNCOMPRESSED_R8G8B8 for reason for shuffle.
        // Desired layout 1 pixel per 32 bit lane
        //
        // Register        | {[P1]   [P2]    [P3]    [P4]}
        // Bits            |  [0:15] [16:31] [32:46] [46:61]
        // Bytes (Shuffle) |  [0:1]  [2:3]   [4:5]   [6:7]
        case UNCOMPRESSED_R5G5B5A1:
        {
            result.shuffle = _mm_setr_epi8(0, 1, 0, 1, // Lane 1
                                           2, 3, 2, 3,
                                           4, 5, 4, 5,
                                           6, 7, 6, 7);
            result.r_bit_mask = 0b11111;
            result.g_bit_mask = 0b11111;
            result.b_bit_mask = 0b11111;
            result.a_bit_mask = 0b00001;

            result.r_bit_shift = 11;
            result.g_bit_shift = 6;
            result.b_bit_shift = 1;
            result.a_bit_shift = 0;

            result.r_to_01_coefficient = 1.f/31.f;
            result.g_to_01_coefficient = 1.f/31.f;
            result.b_to_01_coefficient = 1.f/31.f;
            result.a_to_01_coefficient = 1.f;
        }
        break;
    }

    return result;
}

typedef enum
{
    RaylibSIMD_ImageDrawMode_Original,
    RaylibSIMD_ImageDrawMode_Flattened,
    RaylibSIMD_ImageDrawMode_SIMD,
} RaylibSIMD_ImageDrawMode;

void RaylibSIMD_ImageDraw(Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, Color tint)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0) ||
        (src.data == NULL) || (src.width == 0) || (src.height == 0)) return;

    if (dst->mipmaps > 1)
    {
        TRACELOG(LOG_WARNING, "Image drawing only applied to base mipmap level");
    }
    if (dst->format >= COMPRESSED_DXT1_RGB)
    {
        TRACELOG(LOG_WARNING, "Image drawing not supported for compressed formats");
    }
    else
    {
        Image srcMod = { 0 };       // Source copy (in case it was required)
        Image *srcPtr = &src;       // Pointer to source image
        bool useSrcMod = false;     // Track source copy required

        // Source rectangle out-of-bounds security checks
        if (srcRec.x < 0) { srcRec.width += srcRec.x; srcRec.x = 0; }
        if (srcRec.y < 0) { srcRec.height += srcRec.y; srcRec.y = 0; }
        if ((srcRec.x + srcRec.width) > src.width) srcRec.width = src.width - srcRec.x;
        if ((srcRec.y + srcRec.height) > src.height) srcRec.height = src.height - srcRec.y;

        // Check if source rectangle needs to be resized to destination rectangle
        // In that case, we make a copy of source and we apply all required transform
        if (((int)srcRec.width != (int)dstRec.width) || ((int)srcRec.height != (int)dstRec.height))
        {
            srcMod = ImageFromImage(src, srcRec);   // Create image from another image
            ImageResize(&srcMod, (int)dstRec.width, (int)dstRec.height);   // Resize to destination rectangle
            srcRec = (Rectangle){ 0.f, 0.f, (float)srcMod.width, (float)srcMod.height };

            srcPtr = &srcMod;
            useSrcMod = true;
        }

        // Destination rectangle out-of-bounds security checks
        if (dstRec.x < 0)
        {
            srcRec.x = -dstRec.x;
            srcRec.width += dstRec.x;
            dstRec.x = 0;
        }
        else if ((dstRec.x + srcRec.width) > dst->width) srcRec.width = dst->width - dstRec.x;

        if (dstRec.y < 0)
        {
            srcRec.y = -dstRec.y;
            srcRec.height += dstRec.y;
            dstRec.y = 0;
        }
        else if ((dstRec.y + srcRec.height) > dst->height) srcRec.height = dst->height - dstRec.y;

        if (dst->width < srcRec.width) srcRec.width = (float)dst->width;
        if (dst->height < srcRec.height) srcRec.height = (float)dst->height;

        // This blitting method is quite fast! The process followed is:
        // for every pixel -> [get_src_format/get_dst_format -> blend -> format_to_dst]
        // Some optimization ideas:
        //    [x] Avoid creating source copy if not required (no resize required)
        //    [x] Optimize ImageResize() for pixel format (alternative: ImageResizeNN())
        //    [x] Optimize ColorAlphaBlend() to avoid processing (alpha = 0) and (alpha = 1)
        //    [x] Optimize ColorAlphaBlend() for faster operations (maybe avoiding divs?)
        //    [x] Consider fast path: no alpha blending required cases (src has no alpha)
        //    [x] Consider fast path: same src/dst format with no alpha -> direct line copy
        //    [-] GetPixelColor(): Return Vector4 instead of Color, easier for ColorAlphaBlend()

        bool blendRequired = true;
        
        // Fast path: Avoid blend if source has no alpha to blend
        if ((tint.a == 255) && ((srcPtr->format == UNCOMPRESSED_GRAYSCALE) || (srcPtr->format == UNCOMPRESSED_R8G8B8) || (srcPtr->format == UNCOMPRESSED_R5G6B5))) blendRequired = false;       

        int strideDst = GetPixelDataSize(dst->width, 1, dst->format);
        int bytesPerPixelDst = strideDst/(dst->width);

        int strideSrc = GetPixelDataSize(srcPtr->width, 1, srcPtr->format);
        int bytesPerPixelSrc = strideSrc/(srcPtr->width);

        unsigned char *pSrcBase = (unsigned char *)srcPtr->data + ((int)srcRec.y*srcPtr->width + (int)srcRec.x)*bytesPerPixelSrc;
        unsigned char *pDstBase = (unsigned char *)dst->data + ((int)dstRec.y*dst->width + (int)dstRec.x)*bytesPerPixelDst;

        float const INV_255 = 1.f / 255.f;
        RaylibSIMD_ImageDrawMode draw_mode = RaylibSIMD_ImageDrawMode_Original;

        if (dst->format == UNCOMPRESSED_R8G8B8A8 &&
            (srcPtr->format == UNCOMPRESSED_R8G8B8A8 ||
             srcPtr->format == UNCOMPRESSED_R8G8B8 ||
             srcPtr->format == UNCOMPRESSED_R5G6B5 ||
             srcPtr->format == UNCOMPRESSED_R5G5B5A1 ||
             srcPtr->format == UNCOMPRESSED_R4G4B4A4
             ))
        {
            draw_mode = RaylibSIMD_ImageDrawMode_SIMD;
        }

        switch(draw_mode)
        {
            case RaylibSIMD_ImageDrawMode_Original:
            {
                Color colSrc, colDst, blend;
                for (int y = 0; y < (int)srcRec.height; y++)
                {
                    unsigned char *pSrc = pSrcBase;
                    unsigned char *pDst = pDstBase;

                    // Fast path: Avoid moving pixel by pixel if no blend required and same format
                    if (!blendRequired && (srcPtr->format == dst->format)) memcpy(pDst, pSrc, (size_t)srcRec.width*bytesPerPixelSrc);
                    else
                    {
                        for (int x = 0; x < (int)srcRec.width; x++)
                        {
                            colSrc = GetPixelColor(pSrc, srcPtr->format);
                            colDst = GetPixelColor(pDst, dst->format);

                            // Fast path: Avoid blend if source has no alpha to blend
                            if (blendRequired) blend = ColorAlphaBlend(colDst, colSrc, tint);
                            else blend = colSrc;

                            SetPixelColor(pDst, blend, dst->format);

                            pDst += bytesPerPixelDst;
                            pSrc += bytesPerPixelSrc;
                        }
                    }

                    pSrcBase += strideSrc;
                    pDstBase += strideDst;
                }
            }
            break;

            case RaylibSIMD_ImageDrawMode_Flattened:
            {
                unsigned char *src_row    = RS_CAST(unsigned char *)pSrcBase;
                unsigned char *dest_row   = RS_CAST(unsigned char *)pDstBase;
                float const src_alpha_min = (srcPtr->format == UNCOMPRESSED_R8G8B8) ? 255.f : 0.f;

                for (int y = 0; y < (int)srcRec.height; y++)
                {
                    unsigned char *src_ptr  = src_row;
                    unsigned char *dest_ptr = dest_row;
                    for (int x = 0; x < (int)srcRec.width; x++)
                    {
                        RaylibSIMD__SoftwareBlendPixel(src_ptr, dest_ptr, tint, src_alpha_min);
                        src_ptr += bytesPerPixelSrc;
                        dest_ptr += bytesPerPixelDst;
                    }

                    src_row += strideSrc;
                    dest_row += strideDst;
                }

            }
            break;

            case RaylibSIMD_ImageDrawMode_SIMD:
            {
                // NOTE: The general approach to SIMD the drawing loop is to
                // pull out each pixel into each available f32 SIMD lane to
                // do color blends in a [0, 1] 32 bit float space.
                // For example a __m128 consists of 4x32 bit lanes.
                //
                // SIMD Register
                // {[Pixel1] [Pixel2] [Pixel3] [Pixel4]}
                //
                // Followed by pulling each color component from pixels 1, 2,
                // 3 and 4 into a SIMD lane to perform the color blend.
                //
                // {[R1] [R2] [R3] [R4]} Register 1
                // {[G1] [G2] [G3] [G4]}    ..
                // {[B1] [B2] [B3] [B4]}    ..
                // {[A1] [A2] [A3] [A4]}    ..
                //
                // We collate the same colors of each pixel into the lanes
                // because the required blend equation is the same across the
                // same color components.

                __m128 const tint_r01_4x      = _mm_set1_ps(tint.r * INV_255);
                __m128 const tint_g01_4x      = _mm_set1_ps(tint.g * INV_255);
                __m128 const tint_b01_4x      = _mm_set1_ps(tint.b * INV_255);
                __m128 const tint_a01_4x      = _mm_set1_ps(tint.a * INV_255);
                __m128 const one_4x           = _mm_set1_ps(1.f);
                __m128 const u255_4x          = _mm_set1_ps(255.f);
                __m128i const hex_0xFF_4x     = _mm_set1_epi32(0xFF);

                float src_alpha_min = 0.f;
                if (srcPtr->format == UNCOMPRESSED_R8G8B8)      src_alpha_min = 255.f;
                else if (srcPtr->format == UNCOMPRESSED_R5G6B5) src_alpha_min = 255.f;

                RaylibSIMD_PixelPerLaneShuffle src_lanes  = RaylibSIMD__FormatToPixelPerLaneShuffle128Bit(srcPtr->format);
                RaylibSIMD_PixelPerLaneShuffle dest_lanes = RaylibSIMD__FormatToPixelPerLaneShuffle128Bit(dst->format);

                __m128 const src_alpha_min_4x   = _mm_set1_ps(src_alpha_min);

                __m128i src_r_bit_mask          = _mm_set1_epi32(src_lanes.r_bit_mask);
                __m128i src_g_bit_mask          = _mm_set1_epi32(src_lanes.g_bit_mask);
                __m128i src_b_bit_mask          = _mm_set1_epi32(src_lanes.b_bit_mask);
                __m128i src_a_bit_mask          = _mm_set1_epi32(src_lanes.a_bit_mask);

                __m128 src_r_to_01_coefficient  = _mm_set1_ps(src_lanes.r_to_01_coefficient);
                __m128 src_g_to_01_coefficient  = _mm_set1_ps(src_lanes.g_to_01_coefficient);
                __m128 src_b_to_01_coefficient  = _mm_set1_ps(src_lanes.b_to_01_coefficient);
                __m128 src_a_to_01_coefficient  = _mm_set1_ps(src_lanes.a_to_01_coefficient);

                __m128i dest_r_bit_mask         = _mm_set1_epi32(dest_lanes.r_bit_mask);
                __m128i dest_g_bit_mask         = _mm_set1_epi32(dest_lanes.g_bit_mask);
                __m128i dest_b_bit_mask         = _mm_set1_epi32(dest_lanes.b_bit_mask);
                __m128i dest_a_bit_mask         = _mm_set1_epi32(dest_lanes.a_bit_mask);

                __m128 dest_r_to_01_coefficient = _mm_set1_ps(dest_lanes.r_to_01_coefficient);
                __m128 dest_g_to_01_coefficient = _mm_set1_ps(dest_lanes.g_to_01_coefficient);
                __m128 dest_b_to_01_coefficient = _mm_set1_ps(dest_lanes.b_to_01_coefficient);
                __m128 dest_a_to_01_coefficient = _mm_set1_ps(dest_lanes.a_to_01_coefficient);

                // NOTE: Divide by float because we blend in [0,1] 32 bit float space
                // Each color component requires 1 SIMD lane to perform such blend.
                int const PIXELS_PER_SIMD_WRITE = sizeof(__m128) / sizeof(float);
                int const src_bits_per_pixel    = RaylibSIMD__FormatToBitsPerPixel(srcPtr->format);
                int const src_bytes_per_pixel   = src_bits_per_pixel / 8;

                int const src_bytes_per_simd_write  = PIXELS_PER_SIMD_WRITE * src_bytes_per_pixel;
                int const dest_bytes_per_simd_write = PIXELS_PER_SIMD_WRITE * bytesPerPixelDst;

                int const simd_iterations       = RS_CAST(int) srcRec.width / PIXELS_PER_SIMD_WRITE; // NOTE: Divison here rounds down fractional pixels
                int const total_simd_pixels     = simd_iterations * PIXELS_PER_SIMD_WRITE;
                int const remaining_iterations  = srcRec.width - total_simd_pixels;                  // NOTE: Ensure pixels fractionally written to are dealt with

                unsigned char const *src_row = RS_CAST(unsigned char const *)pSrcBase;
                unsigned char *dest_row      = RS_CAST(unsigned char *)pDstBase;
                for (int y = 0; y < (int)srcRec.height; y++)
                {
                    unsigned char *src_ptr  = src_row;
                    unsigned char *dest_ptr = dest_row;
                    for (int x = 0; x < simd_iterations; x++)
                    {
                        unsigned char *dest = dest_ptr;

                        // NOTE: Extract Pixels From Buffer
                        __m128i src_pixels_4x           = _mm_loadu_si128((__m128i *)src_ptr);
                        __m128i dest_pixels_4x          = _mm_loadu_si128((__m128i *)dest_ptr);

                        // NOTE: Arrange loaded pixels to 1 pixel per lane.
                        __m128i src_pixels_4x_shuffled  = _mm_shuffle_epi8(src_pixels_4x, src_lanes.shuffle);
                        __m128i dest_pixels_4x_shuffled = _mm_shuffle_epi8(dest_pixels_4x, dest_lanes.shuffle);

                        // NOTE: Advance Pixel Buffer
                        src_ptr += src_bytes_per_simd_write;
                        dest_ptr += dest_bytes_per_simd_write;

                        // NOTE: Unpack Source & Dest Pixel Layout for SIMD
                        // From {ABGR1, ABGR2, ABGR3, ABGR3} to {RRRR} {GGGG} {BBBB} {AAAA} where each
                        // new {...} is one SIMD register with u32x4 lanes of the same color component.
                        //
                        //    1. Shift colour component to lowest 8 bits
                        //    2. Isolate the color component
                        //
                        __m128i src0123_r_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, src_lanes.r_bit_shift), src_r_bit_mask);
                        __m128i src0123_g_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, src_lanes.g_bit_shift), src_g_bit_mask);
                        __m128i src0123_b_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, src_lanes.b_bit_shift), src_b_bit_mask);
                        __m128i src0123_a_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, src_lanes.a_bit_shift), src_a_bit_mask);

                        __m128i dest0123_r_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x_shuffled, dest_lanes.r_bit_shift), dest_r_bit_mask);
                        __m128i dest0123_g_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x_shuffled, dest_lanes.g_bit_shift), dest_g_bit_mask);
                        __m128i dest0123_b_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x_shuffled, dest_lanes.b_bit_shift), dest_b_bit_mask);
                        __m128i dest0123_a_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x_shuffled, dest_lanes.a_bit_shift), dest_a_bit_mask);

                        // NOTE: Convert to SIMD f32x4
                        __m128 src0123_r  = _mm_cvtepi32_ps(src0123_r_int);
                        __m128 src0123_g  = _mm_cvtepi32_ps(src0123_g_int);
                        __m128 src0123_b  = _mm_cvtepi32_ps(src0123_b_int);
                        __m128 src0123_a  = _mm_cvtepi32_ps(src0123_a_int);

                        // NOTE: For images without an alpha component the src_alpha_min_4x is set to 255 to completely overwrite dest.
                        //       For images with an alpha component the src_alpha_min_4x is set to 0 (i.e. no-op)
                        src0123_a = _mm_max_ps(src0123_a, src_alpha_min_4x);

                        __m128 dest0123_r = _mm_cvtepi32_ps(dest0123_r_int);
                        __m128 dest0123_g = _mm_cvtepi32_ps(dest0123_g_int);
                        __m128 dest0123_b = _mm_cvtepi32_ps(dest0123_b_int);
                        __m128 dest0123_a = _mm_cvtepi32_ps(dest0123_a_int);

                        // NOTE: Source Pixels to Normalized [0, 1] Float Space
                        __m128 src0123_r01 = _mm_mul_ps(src0123_r, src_r_to_01_coefficient);
                        __m128 src0123_g01 = _mm_mul_ps(src0123_g, src_g_to_01_coefficient);
                        __m128 src0123_b01 = _mm_mul_ps(src0123_b, src_b_to_01_coefficient);
                        __m128 src0123_a01 = _mm_mul_ps(src0123_a, src_a_to_01_coefficient);

                        // NOTE: Tint Source Pixels
                        __m128 src0123_tinted_r01 = _mm_mul_ps(src0123_r01, tint_r01_4x);
                        __m128 src0123_tinted_g01 = _mm_mul_ps(src0123_g01, tint_g01_4x);
                        __m128 src0123_tinted_b01 = _mm_mul_ps(src0123_b01, tint_b01_4x);
                        __m128 src0123_tinted_a01 = _mm_mul_ps(src0123_a01, tint_a01_4x);

                        // NOTE: Dest Pixels to Normalized [0, 1] Float Space
                        __m128 dest0123_r01 = _mm_mul_ps(dest0123_r, dest_r_to_01_coefficient);
                        __m128 dest0123_g01 = _mm_mul_ps(dest0123_g, dest_g_to_01_coefficient);
                        __m128 dest0123_b01 = _mm_mul_ps(dest0123_b, dest_b_to_01_coefficient);
                        __m128 dest0123_a01 = _mm_mul_ps(dest0123_a, dest_a_to_01_coefficient);

                        // NOTE: Porter Duff Blend
                        // NOTE: Blend Alpha
                        // i.e. blend_a = src_a + (dest_a * (1 - src_a))
                        __m128 blend0123_a01     = _mm_add_ps(src0123_tinted_a01, _mm_mul_ps(dest0123_a01, _mm_sub_ps(one_4x, src0123_tinted_a01)));
                        __m128 inv_blend0123_a01 = _mm_div_ps(one_4x, blend0123_a01);

                        // NOTE: Blend Colors
                        // i.e. blend_r = ((src_r * a) + (dest_r * dest_a * (1.f - src_a))) / blend_a;
                        __m128 blend0123_r01 = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(src0123_tinted_r01, src0123_tinted_a01), _mm_mul_ps(dest0123_r01, _mm_mul_ps(dest0123_a01, _mm_sub_ps(one_4x, src0123_tinted_a01)))), inv_blend0123_a01);
                        __m128 blend0123_g01 = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(src0123_tinted_g01, src0123_tinted_a01), _mm_mul_ps(dest0123_g01, _mm_mul_ps(dest0123_a01, _mm_sub_ps(one_4x, src0123_tinted_a01)))), inv_blend0123_a01);
                        __m128 blend0123_b01 = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(src0123_tinted_b01, src0123_tinted_a01), _mm_mul_ps(dest0123_b01, _mm_mul_ps(dest0123_a01, _mm_sub_ps(one_4x, src0123_tinted_a01)))), inv_blend0123_a01);

                        // NOTE: Convert Blend to [0, 255] F32 Space
                        __m128 blend0123_a = _mm_mul_ps(blend0123_a01, u255_4x);
                        __m128 blend0123_r = _mm_mul_ps(blend0123_r01, u255_4x);
                        __m128 blend0123_g = _mm_mul_ps(blend0123_g01, u255_4x);
                        __m128 blend0123_b = _mm_mul_ps(blend0123_b01, u255_4x);

                        // NOTE: Convert Blend to [0, 255] Integer Space
                        __m128i blended0123_a_int = _mm_cvtps_epi32(blend0123_a);
                        __m128i blended0123_r_int = _mm_cvtps_epi32(blend0123_r);
                        __m128i blended0123_g_int = _mm_cvtps_epi32(blend0123_g);
                        __m128i blended0123_b_int = _mm_cvtps_epi32(blend0123_b);

                        // NOTE: Repack The Pixel
                        // From {RRRR} {GGGG} {BBBB} {AAAA} to {ABGR ABGR ABGR ABGR}
                        // Each blend has the color component converted to 8 bits sitting in the low bits of the SIMD lane.
                        // Shift the colors into place and or them together to get the final output
                        //
                        //      blended0123_r_int = {[0,0,0,R], [0,0,0,R], [0,0,0,R], [0,0,0,R]}
                        //      blended0123_g_int = {[0,0,0,G], [0,0,0,G], [0,0,0,G], [0,0,0,G]}
                        //      blended0123_b_int = {[0,0,0,B], [0,0,0,B], [0,0,0,B], [0,0,0,B]}
                        //      blended0123_b_int = {[0,0,0,A], [0,0,0,A], [0,0,0,A], [0,0,0,A]}
                        //      pixel0123         = {[A,B,G,R], [A,B,G,R], [A,B,G,R], [A,B,G,R]}
                        //
                        __m128i pixel0123 =
                            _mm_or_si128(blended0123_r_int,
                                         _mm_or_si128(_mm_or_si128(_mm_slli_epi32(blended0123_g_int, 8),
                                                                   _mm_slli_epi32(blended0123_b_int, 16)),
                                                      _mm_slli_epi32(blended0123_a_int, 24)));
                        _mm_storeu_si128((__m128i *)dest, pixel0123);
                    }

                    // NOTE: Remaining iterations are done serially.
                    for (int x = 0; x < remaining_iterations; x++)
                    {
                        RaylibSIMD__SoftwareBlendPixel(src_ptr, dest_ptr, tint, src_alpha_min);
                        src_ptr += bytesPerPixelSrc;
                        dest_ptr += bytesPerPixelDst;
                    }

                    src_row += strideSrc;
                    dest_row += strideDst;
                }

            }
        }

        if (useSrcMod) UnloadImage(srcMod);     // Unload source modified image
    }
}

Image RaylibSIMD_GenImageColor(int width, int height, Color color)
{
    Image image   = {0};
    image.data    = RS_CAST(Color *) RL_MALLOC(width * height * sizeof(Color));
    image.width   = width;
    image.height  = height;
    image.format  = UNCOMPRESSED_R8G8B8A8;
    image.mipmaps = 1;
    RaylibSIMD_ImageDrawRectangleRec(&image, (Rectangle){0, 0, width, height}, color);
    return image;
}

RS_FILE_SCOPE int RaylibSIMD__FormatToBitsPerPixel(int format)
{
    int result = 4;
    switch (format)
    {
        case UNCOMPRESSED_GRAYSCALE: result = 8; break;
        case UNCOMPRESSED_GRAY_ALPHA:
        case UNCOMPRESSED_R5G6B5:
        case UNCOMPRESSED_R5G5B5A1:
        case UNCOMPRESSED_R4G4B4A4: result = 16; break;
        case UNCOMPRESSED_R8G8B8A8: result = 32; break;
        case UNCOMPRESSED_R8G8B8: result = 24; break;
        case UNCOMPRESSED_R32: result = 32; break;
        case UNCOMPRESSED_R32G32B32: result = 32*3; break;
        case UNCOMPRESSED_R32G32B32A32: result = 32*4; break;
        case COMPRESSED_DXT1_RGB:
        case COMPRESSED_DXT1_RGBA:
        case COMPRESSED_ETC1_RGB:
        case COMPRESSED_ETC2_RGB:
        case COMPRESSED_PVRT_RGB:
        case COMPRESSED_PVRT_RGBA: result = 4; break;
        case COMPRESSED_DXT3_RGBA:
        case COMPRESSED_DXT5_RGBA:
        case COMPRESSED_ETC2_EAC_RGBA:
        case COMPRESSED_ASTC_4x4_RGBA: result = 8; break;
        case COMPRESSED_ASTC_8x8_RGBA: result = 2; break;
        default: break;
    }

    return result;
}

Rectangle RaylibSIMD__RectangleIntersection(Rectangle a, Rectangle b)
{
    float a_max_x = a.x + a.width;
    float a_max_y = a.y + a.height;

    float b_max_x = b.x + b.width;
    float b_max_y = b.y + b.height;

    Rectangle result = {0};
    int intersects   = (a.x <= b_max_x && a_max_x >= b.x) && (a.y <= b_max_y && a_max_y >= b.y);
    if (intersects)
    {
        result.x      = RS_MAX(a.x, b.x);
        result.y      = RS_MAX(a.y, b.y);
        result.width  = RS_MIN(a_max_x, b_max_x) - result.x;
        result.height = RS_MIN(a_max_y, b_max_y) - result.y;
    }

    return result;
}

// Draw rectangle within an image
void RaylibSIMD_ImageDrawRectangleRec(Image *dst, Rectangle rec, Color color)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0)) return;

    // TODO(doyle): Grayscale, Gray Alpha, R5G6B5, R5G5B5A1, R4G4B4A4 haven't
    // been tested yet but, I wrote this function to technically be agnostic of
    // the storage format. It probably works but should be checked.

    __m128i color_4x = {0};
    switch(dst->format)
    {
        default: break;
        case UNCOMPRESSED_GRAYSCALE:
        {
            float r01 = RS_CAST(float) color.r / 255.0f;
            float g01 = RS_CAST(float) color.g / 255.0f;
            float b01 = RS_CAST(float) color.b / 255.0f;

            unsigned char gray = RS_CAST(unsigned char)((r01 * 0.299f + g01 * 0.587f + b01 * 0.114f) * 255.0f);
            color_4x           = _mm_set1_epi8(gray);
        }
        break;

        case UNCOMPRESSED_GRAY_ALPHA:
        {
            float r01 = RS_CAST(float) color.r / 255.0f;
            float g01 = RS_CAST(float) color.g / 255.0f;
            float b01 = RS_CAST(float) color.b / 255.0f;
            unsigned char gray           = RS_CAST(unsigned char)((r01 * 0.299f + g01 * 0.587f + b01 * 0.114f) * 255.0f);
            color_4x = _mm_setr_epi8(gray, color.a,
                                     gray, color.a,
                                     gray, color.a,
                                     gray, color.a,
                                     gray, color.a,
                                     gray, color.a,
                                     gray, color.a,
                                     gray, color.a);
        }
        break;

        case UNCOMPRESSED_R8G8B8A8:
        {
            uint32_t color_u32 = RaylibSIMD__ColorToU32(color);
            color_4x           = _mm_set1_epi32(color_u32);
        }
        break;

        case UNCOMPRESSED_R8G8B8:
        {
            char r = RS_CAST(char)color.r;
            char g = RS_CAST(char)color.g;
            char b = RS_CAST(char)color.b;
            color_4x = _mm_setr_epi8(r, g, b,
                                     r, g, b,
                                     r, g, b,
                                     r, g, b,
                                     r, g, b,
                                     r);
        }
        break;

        case UNCOMPRESSED_R5G6B5:
        case UNCOMPRESSED_R5G5B5A1:
        case UNCOMPRESSED_R4G4B4A4:
        {
            float r01 = RS_CAST(float) color.r / 255.0f;
            float g01 = RS_CAST(float) color.g / 255.0f;
            float b01 = RS_CAST(float) color.b / 255.0f;
            float a01 = RS_CAST(float) color.a / 255.0f;

            uint16_t rgba = 0;
            if (dst->format == UNCOMPRESSED_R5G6B5)
            {
                char r = RS_CAST(char)(r01 * 31.f);
                char g = RS_CAST(char)(g01 * 63.f);
                char b = RS_CAST(char)(b01 * 31.f);
                rgba   = r << 11 | g << 5 | b << 0;
            }
            else if (dst->format == UNCOMPRESSED_R5G5B5A1)
            {
                char r = RS_CAST(char)(r01 * 31.f);
                char g = RS_CAST(char)(g01 * 31.f);
                char b = RS_CAST(char)(b01 * 31.f);
                char a = RS_CAST(char)(a01 * 31.f);
                rgba   = r << 11 | g << 6 | b << 1 | a << 0;
            }
            else if (dst->format == UNCOMPRESSED_R4G4B4A4)
            {
                char r = RS_CAST(char)(r01 * 15.f);
                char g = RS_CAST(char)(g01 * 15.f);
                char b = RS_CAST(char)(b01 * 15.f);
                char a = RS_CAST(char)(a01 * 15.f);
                rgba   = r << 12 | g << 8 | b << 4 | a << 0;
            }

            color_4x = _mm_set1_epi32((rgba << 0) | (rgba << 16));
        }
        break;
    }

    Rectangle dst_rect = (Rectangle){0, 0, dst->width, dst->height};
    rec                = RaylibSIMD__RectangleIntersection(dst_rect, rec);

    int const bits_per_pixel        = RaylibSIMD__FormatToBitsPerPixel(dst->format);
    int const bytes_per_pixel       = bits_per_pixel / 8;

    int const pixels_per_simd_write = sizeof(__m128i) / bytes_per_pixel;
    int const bytes_per_simd_write  = pixels_per_simd_write * bytes_per_pixel;

    int const simd_iterations       = RS_CAST(int)(rec.width * bytes_per_pixel) / sizeof(__m128i);
    int const remaining_iterations  = rec.width - (pixels_per_simd_write * simd_iterations);

    int const stride                = dst->width * bytes_per_pixel;
    int const row_offset            = (rec.y * stride) + rec.x * bytes_per_pixel;

    for (int y = 0; y < RS_CAST(int)rec.height; y++)
    {
        unsigned char *dest = RS_CAST(unsigned char *)dst->data + (row_offset + (stride * y));
        for (int iteration = 0; iteration < simd_iterations; iteration++)
        {
            _mm_storeu_si128(RS_CAST(__m128i *)dest, color_4x);
            dest += bytes_per_simd_write;
        }

        for (int iteration = 0; iteration < remaining_iterations; iteration++)
        {
            SetPixelColor(dest, color, dst->format);
            dest += bytes_per_pixel;
        }
    }
}

void RaylibSIMD_ImageDrawRectangle(Image *dst, int posX, int posY, int width, int height, Color color)
{
    RaylibSIMD_ImageDrawRectangleRec(dst, (Rectangle){RS_CAST(float)posX, RS_CAST(float)posY, RS_CAST(float)width, RS_CAST(float)height}, color);
}

void RaylibSIMD_ImageClearBackground(Image *dst, Color color)
{
    RaylibSIMD_ImageDrawRectangleRec(dst, (Rectangle){0, 0, dst->width, dst->height}, color);
}
#endif // RAYLIB_SIMD_IMPLEMENTATION
