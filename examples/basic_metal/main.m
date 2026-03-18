// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru/maru.h"
#include "maru/native/cocoa.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <stdio.h>

static bool keep_running = true;

static void handle_event(MARU_EventId type, MARU_Window *window,
                         const MARU_Event *event, void *userdata) {
  (void)window;
  (void)userdata;
  (void)event;
  if (type == MARU_EVENT_CLOSE_REQUESTED) {
    keep_running = false;
  }
}

int main(int argc, const char * argv[]) {
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  MARU_Context *context = NULL;
  if (maru_createContext(&create_info, &context) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create context.\n");
    return 1;
  }

  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Metal Example";
  window_info.attributes.dip_size = (MARU_Vec2Dip){800, 600};

  MARU_Window *window = NULL;
  if (maru_createWindow(context, &window_info, &window) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create window.\n");
    maru_destroyContext(context);
    return 1;
  }

  printf("Waiting for window to be ready...\n");
  while (keep_running && !maru_isWindowReady(window)) {
    maru_pumpEvents(context, 16, MARU_ALL_EVENTS, handle_event, NULL);
  }

  if (!keep_running) {
    maru_destroyWindow(window);
    maru_destroyContext(context);
    return 0;
  }

  // Setup Metal
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  id<MTLCommandQueue> commandQueue = [device newCommandQueue];

  MARU_CocoaWindowHandle native_handle = maru_getCocoaWindowHandle(window);
  CAMetalLayer *layer = (__bridge CAMetalLayer *)native_handle.ns_layer;
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

  printf("Entering main loop...\n");
  while (keep_running) {
    maru_pumpEvents(context, 0, MARU_ALL_EVENTS, handle_event, NULL);

    @autoreleasepool {
      id<CAMetalDrawable> drawable = [layer nextDrawable];
      if (drawable) {
        MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.5, 1.0, 1.0); // Light blue
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder endEncoding];
        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];
      }
    }
  }

  maru_destroyWindow(window);
  maru_destroyContext(context);

  return 0;
}
