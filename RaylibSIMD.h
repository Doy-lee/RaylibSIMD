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

#define RS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define RS_CAST(x) (x)

void RaylibSIMD__SoftwareBlendPixel(unsigned char const *src_ptr, unsigned char *dest_ptr, Color tint, float src_alpha_min)
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
            (srcPtr->format == UNCOMPRESSED_R8G8B8A8 || srcPtr->format == UNCOMPRESSED_R8G8B8))
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
                    }

                    src_row += strideSrc;
                    dest_row += strideDst;
                }

            }
            break;

            case RaylibSIMD_ImageDrawMode_SIMD:
            {
                __m128 const inv_255_4x       = _mm_set1_ps(INV_255);
                __m128 const tint_r01_4x      = _mm_set1_ps(tint.r * INV_255);
                __m128 const tint_g01_4x      = _mm_set1_ps(tint.g * INV_255);
                __m128 const tint_b01_4x      = _mm_set1_ps(tint.b * INV_255);
                __m128 const tint_a01_4x      = _mm_set1_ps(tint.a * INV_255);
                __m128 const one_4x           = _mm_set1_ps(1.f);
                __m128 const u255_4x          = _mm_set1_ps(255.f);
                __m128i const hex_0xFF_4x     = _mm_set1_epi32(0xFF);

                float src_alpha_min        = 0.f;
                __m128i src_pixels_shuffle = _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0); // No-op shuffle.
                if (srcPtr->format == UNCOMPRESSED_R8G8B8)
                {
                    // NOTE: We load 4 pixels x 4 colors at a time. But if the
                    // source image is 3BPP, then the 4th color loaded in each
                    // pixel is going to be the RED component of the next pixel.
                    //
                    // Pixels[] = {RGB, RGB, RGB, RGB, ...}
                    //
                    // For example, naively loading the next 4 color components
                    // in a 3BPP byte stream, produces
                    //
                    // RGBR GBRG BRGB
                    //
                    // The subsequent pixel needs to re-load the red channel
                    // to correctly pull the RGB components out and so forth for
                    // subsequent pixels.
                    //
                    // RGBR RGBR RGBR
                    //
                    // We do this by shuffling the loaded pixels into place. In
                    // the 4BPP case, we do a no-op shuffle that preserves
                    // positions of all color components to avoid branches in
                    // the blitting hot path.
                    src_alpha_min      = 255.f;
                    src_pixels_shuffle = _mm_set_epi8(12, 11, 10, 9, 9, 8, 7, 6, 6, 5, 4, 3, 3, 2, 1, 0);
                }

                int const SIMD_WIDTH           = 4;
                __m128 const src_alpha_min_4x  = _mm_set1_ps(src_alpha_min);
                int simd_x_iterations          = RS_CAST(int)srcRec.width / SIMD_WIDTH;
                int remaining_x_iterations     = RS_CAST(int)srcRec.width % SIMD_WIDTH;

                unsigned char const *src_row = RS_CAST(unsigned char const *)pSrcBase;
                unsigned char *dest_row      = RS_CAST(unsigned char *)pDstBase;
                for (int y = 0; y < (int)srcRec.height; y++)
                {
                    unsigned char *src_ptr  = src_row;
                    unsigned char *dest_ptr = dest_row;
                    for (int x = 0; x < simd_x_iterations; x++)
                    {
                        unsigned char *dest = dest_ptr;

                        // NOTE: Extract Pixels From Buffer
                        __m128i src_pixels_4x          = _mm_loadu_si128((__m128i *)src_ptr);
                        __m128i dest_pixels_4x         = _mm_loadu_si128((__m128i *)dest_ptr);
                        __m128i src_pixels_4x_shuffled = _mm_shuffle_epi8(src_pixels_4x, src_pixels_shuffle);

                        // NOTE: Advance Pixel Buffer
                        src_ptr += (SIMD_WIDTH * bytesPerPixelSrc);
                        dest_ptr += (SIMD_WIDTH * bytesPerPixelDst);

                        // NOTE: Unpack Source & Dest Pixel Layout for SIMD
                        // From {RGBA0, RGBA1, RGBA2, RGBA3} to {RRRR} {GGGG} {BBBB} {AAAA} where each
                        // new {...} is one SIMD register with u32x4 lanes of the same color component.
                        //
                        //    1. Shift colour component to lowest 8 bits
                        //    2. Isolate the color component
                        //
                        __m128i src0123_r_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, 0), hex_0xFF_4x);
                        __m128i src0123_g_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, 8), hex_0xFF_4x);
                        __m128i src0123_b_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, 16), hex_0xFF_4x);
                        __m128i src0123_a_int = _mm_and_si128(_mm_srli_epi32(src_pixels_4x_shuffled, 24), hex_0xFF_4x);

                        __m128i dest0123_r_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x, 0), hex_0xFF_4x);
                        __m128i dest0123_g_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x, 8), hex_0xFF_4x);
                        __m128i dest0123_b_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x, 16), hex_0xFF_4x);
                        __m128i dest0123_a_int = _mm_and_si128(_mm_srli_epi32(dest_pixels_4x, 24), hex_0xFF_4x);

                        // NOTE: Convert to SIMD f32x4
                        __m128 src0123_r  = _mm_cvtepi32_ps(src0123_r_int);
                        __m128 src0123_g  = _mm_cvtepi32_ps(src0123_g_int);
                        __m128 src0123_b  = _mm_cvtepi32_ps(src0123_b_int);
                        __m128 src0123_a  = _mm_cvtepi32_ps(src0123_a_int);

                        // NOTE: For 3BPP Images the src_alpha_min_4x is set to 255 to completely overwrite dest.
                        //       For 4BPP Images the src_alpha_min_4x is set to 0 (i.e. no-op)
                        src0123_a = _mm_max_ps(src0123_a, src_alpha_min_4x);

                        __m128 dest0123_r = _mm_cvtepi32_ps(dest0123_r_int);
                        __m128 dest0123_g = _mm_cvtepi32_ps(dest0123_g_int);
                        __m128 dest0123_b = _mm_cvtepi32_ps(dest0123_b_int);
                        __m128 dest0123_a = _mm_cvtepi32_ps(dest0123_a_int);

                        // NOTE: Source Pixels to Normalized [0, 1] Float Space
                        __m128 src0123_r01 = _mm_mul_ps(src0123_r, inv_255_4x);
                        __m128 src0123_g01 = _mm_mul_ps(src0123_g, inv_255_4x);
                        __m128 src0123_b01 = _mm_mul_ps(src0123_b, inv_255_4x);
                        __m128 src0123_a01 = _mm_mul_ps(src0123_a, inv_255_4x);

                        // NOTE: Tint Source Pixels
                        __m128 src0123_tinted_r01 = _mm_mul_ps(src0123_r01, tint_r01_4x);
                        __m128 src0123_tinted_g01 = _mm_mul_ps(src0123_g01, tint_g01_4x);
                        __m128 src0123_tinted_b01 = _mm_mul_ps(src0123_b01, tint_b01_4x);
                        __m128 src0123_tinted_a01 = _mm_mul_ps(src0123_a01, tint_a01_4x);

                        // NOTE: Dest Pixels to Normalized [0, 1] Float Space
                        __m128 dest0123_r01 = _mm_mul_ps(dest0123_r, inv_255_4x);
                        __m128 dest0123_g01 = _mm_mul_ps(dest0123_g, inv_255_4x);
                        __m128 dest0123_b01 = _mm_mul_ps(dest0123_b, inv_255_4x);
                        __m128 dest0123_a01 = _mm_mul_ps(dest0123_a, inv_255_4x);

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
                        // From {RRRR} {GGGG} {BBBB} {AAAA} to {RGBA RGBA RGBA RGBA}
                        // i.e. blended0123_r_int = {[0,0,0,R], [0,0,0,R], [0,0,0,R], [0,0,0,R]}
                        //      blended0123_g_int = {[0,0,0,G], [0,0,0,G], [0,0,0,G], [0,0,0,G]}
                        //      ....
                        //
                        // Each blend has the color component converted to 8 bits sitting in the low bits of the register.
                        // Shift the colors into place and or them together to get the final output
                        //      pixel0123 = {[R,G,B,A], [R,G,B,A], [R,G,B,A], [R,G,B,A]}
                        //
                        __m128i pixel0123 =
                            _mm_or_si128(blended0123_r_int,
                                         _mm_or_si128(_mm_or_si128(_mm_slli_epi32(blended0123_g_int, 8),
                                                                   _mm_slli_epi32(blended0123_b_int, 16)),
                                                      _mm_slli_epi32(blended0123_a_int, 24)));
                        _mm_storeu_si128((__m128i *)dest, pixel0123);
                    }

                    // NOTE: Remaining iterations are done serially.
                    for (int x = 0; x < remaining_x_iterations; x++)
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
    Color *pixels            = (Color *)RL_CALLOC(width*height, sizeof(Color));
    int num_pixels           = width * height;
    int simd_iterations      = num_pixels / 4;
    int remaining_iterations = num_pixels % 4;

    __m128i *dest_4x     = RS_CAST(__m128i *) pixels;
    uint32_t color_u32   = (RS_CAST(uint32_t)color.r << 0) | (RS_CAST(uint32_t)color.g << 8) | (RS_CAST(uint32_t)color.b << 16) | (RS_CAST(uint32_t)color.a << 24);
    __m128i color_u32_4x = _mm_set1_epi32(color_u32);
    for (int i = 0; i < simd_iterations; i++)
    {
        _mm_store_si128(dest_4x, color_u32_4x);
        dest_4x++;
    }

    uint32_t *dest = RS_CAST(uint32_t *)dest_4x;
    for (int i = 0; i < remaining_iterations; i++)
        *dest++ = color_u32;

    Image image   = {0};
    image.data    = pixels;
    image.width   = width;
    image.height  = height;
    image.format  = UNCOMPRESSED_R8G8B8A8;
    image.mipmaps = 1;
    return image;
}

// Draw rectangle within an image
void RaylibSIMD_ImageDrawRectangleRec(Image *dst, Rectangle rec, Color color)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0)) return;

    Image imRec = RaylibSIMD_GenImageColor((int)rec.width, (int)rec.height, color);
    RaylibSIMD_ImageDraw(dst, imRec, (Rectangle){0.f, 0.f, RS_CAST(float)rec.width, RS_CAST(float)rec.height}, rec, WHITE);
    UnloadImage(imRec);
}

void RaylibSIMD_ImageDrawRectangle(Image *dst, int posX, int posY, int width, int height, Color color)
{
    RaylibSIMD_ImageDrawRectangleRec(dst, (Rectangle){RS_CAST(float)posX, RS_CAST(float)posY, RS_CAST(float)width, RS_CAST(float)height}, color);
}

void RaylibSIMD_ImageClearBackground(Image *dst, Color color)
{
    RaylibSIMD_ImageDrawRectangle(dst, 0, 0, dst->width, dst->height, color);
}

#endif // RAYLIB_SIMD_IMPLEMENTATION
