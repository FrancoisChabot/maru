// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>

static void _maru_cocoa_release_provider_pixels(void *info,
                                                const void *data,
                                                size_t size) {
    (void)size;
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)info;
    if (ctx_base && data) {
        maru_context_free(ctx_base, (void *)data);
    }
}

MARU_Status maru_createImage_Cocoa(MARU_Context *context,
                                     const MARU_ImageCreateInfo *create_info,
                                     MARU_Image **out_image) {
    if (!context || !create_info || !out_image || !create_info->pixels) {
        return MARU_FAILURE;
    }

    if (create_info->px_size.x <= 0 || create_info->px_size.y <= 0) {
        return MARU_FAILURE;
    }

    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    MARU_Image_Cocoa *img = maru_context_alloc(ctx_base, sizeof(MARU_Image_Cocoa));
    if (!img) return MARU_FAILURE;

    memset(img, 0, sizeof(MARU_Image_Cocoa));
    img->base.ctx_base = ctx_base;
    img->base.width = (uint32_t)create_info->px_size.x;
    img->base.height = (uint32_t)create_info->px_size.y;
    const uint32_t min_stride = img->base.width * 4u;
    const uint32_t stride = create_info->stride_bytes ? create_info->stride_bytes : min_stride;
    if (stride < min_stride) {
        maru_context_free(ctx_base, img);
        return MARU_FAILURE;
    }
    img->base.stride_bytes = min_stride;
    
#ifdef MARU_INDIRECT_BACKEND
    img->base.backend = &maru_backend_Cocoa;
#endif

    size_t width = (size_t)img->base.width;
    size_t height = (size_t)img->base.height;
    size_t packed_stride = (size_t)min_stride;
    size_t copy_size = packed_stride * height;

    img->base.pixels = maru_context_alloc(ctx_base, copy_size);
    if (!img->base.pixels) {
        maru_context_free(ctx_base, img);
        return MARU_FAILURE;
    }

    const uint8_t *src = (const uint8_t *)create_info->pixels;
    for (uint32_t y = 0; y < img->base.height; ++y) {
        memcpy(img->base.pixels + ((size_t)y * packed_stride),
               src + ((size_t)y * (size_t)stride),
               packed_stride);
    }
    
    // Create CGImage from a stable ARGB8888 copy.
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dataProvider = CGDataProviderCreateWithData(
        (void *)ctx_base, img->base.pixels, copy_size, _maru_cocoa_release_provider_pixels);
    if (!colorSpace || !dataProvider) {
        if (dataProvider) {
            CGDataProviderRelease(dataProvider);
        } else {
            maru_context_free(ctx_base, img->base.pixels);
        }
        if (colorSpace) {
            CGColorSpaceRelease(colorSpace);
        }
        maru_context_free(ctx_base, img);
        return MARU_FAILURE;
    }
    
    CGImageRef cgImage = CGImageCreate(
        width, height,
        8, 32, packed_stride,
        colorSpace,
        kCGBitmapByteOrder32Little | kCGImageAlphaFirst, // ARGB8888 input is alpha-first on little-endian machines
        dataProvider, NULL, false, kCGRenderingIntentDefault
    );

    CGDataProviderRelease(dataProvider);
    CGColorSpaceRelease(colorSpace);

    if (!cgImage) {
        maru_context_free(ctx_base, img);
        return MARU_FAILURE;
    }

    img->ns_image = [[NSImage alloc] initWithCGImage:cgImage size:NSMakeSize((CGFloat)width, (CGFloat)height)];
    CGImageRelease(cgImage);

    if (!img->ns_image) {
        maru_context_free(ctx_base, img);
        return MARU_FAILURE;
    }

    img->base.pub.userdata = create_info->userdata;
    *out_image = (MARU_Image *)img;
    return MARU_SUCCESS;
}

MARU_Status maru_destroyImage_Cocoa(MARU_Image *image) {
    if (!image) return MARU_FAILURE;
    MARU_Image_Cocoa *img = (MARU_Image_Cocoa *)image;
    
    if (img->ns_image) {
        [img->ns_image release];
    }
    
    maru_context_free(img->base.ctx_base, img);
    return MARU_SUCCESS;
}
