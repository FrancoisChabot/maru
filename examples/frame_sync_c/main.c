#include "../shared_renderer/vulkan_renderer.h"
#include "maru/c/events.h"
#include "maru/c/vulkan.h"
#include "maru/c/windows.h"
#include "maru/maru.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vulkan/vulkan.h>

static bool keep_running = true;
static bool window_ready = false;
static VulkanRenderer renderer;
static int frame_count = 0;
static double last_stat_time = 0.0;

#ifdef _WIN32
#include <windows.h>
static double get_time_sec() {
  static LARGE_INTEGER frequency;
  static BOOL frequency_initialized = FALSE;
  if (!frequency_initialized) {
    QueryPerformanceFrequency(&frequency);
    frequency_initialized = TRUE;
  }
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart / (double)frequency.QuadPart;
}
#else
static double get_time_sec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}
#endif

static void handle_event(MARU_EventId type, MARU_Window *window,
                         const MARU_Event *event, void *userdata) {
  (void)userdata;
  if (type == MARU_EVENT_CLOSE_REQUESTED) {
    keep_running = false;
  } else if (type == MARU_EVENT_WINDOW_RESIZED) {
    vulkan_renderer_on_resized(&renderer,
                               (uint32_t)event->resized.geometry.pixel_size.x,
                               (uint32_t)event->resized.geometry.pixel_size.y);
  } else if (type == MARU_EVENT_WINDOW_READY) {
    window_ready = true;
  } else if (type == MARU_EVENT_WINDOW_FRAME) {
    frame_count++;
    double now = get_time_sec();
    if (now - last_stat_time >= 1.0) {
      printf("FPS: %d\n", frame_count);
      frame_count = 0;
      last_stat_time = now;
    }

    // Draw the frame
    vulkan_renderer_draw_frame(&renderer);

    // Request the NEXT frame.
    // If _NET_WM_FRAME_DRAWN is working, this will be throttled.
    maru_requestWindowFrame(window);
  }
}

int main() {
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  create_info.attributes.event_mask = MARU_ALL_EVENTS;

  MARU_Context *context = NULL;
  MARU_Window *window = NULL;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (maru_createContext(&create_info, &context) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create context (X11 might not be available).\n");
    return 1;
  }

  uint32_t vk_extension_count = 0;
  const char **vk_extensions =
      maru_getVkExtensions(context, &vk_extension_count);
  vulkan_renderer_init(&renderer, vk_extension_count,
                       (const char **)vk_extensions);

  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Frame Sync Test";
  window_info.attributes.logical_size = (MARU_Vec2Dip){800, 600};
  window_info.attributes.visible = true;

  if (maru_createWindow(context, &window_info, &window) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create window.\n");
    vulkan_renderer_cleanup(&renderer);
    maru_destroyContext(context);
    return 1;
  }

  while (keep_running && !window_ready) {
    maru_pumpEvents(context, 16, handle_event, NULL);
  }

  if (maru_createVkSurface(window, renderer.instance, vkGetInstanceProcAddr,
                           &surface) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create Vulkan surface.\n");
    maru_destroyWindow(window);
    vulkan_renderer_cleanup(&renderer);
    maru_destroyContext(context);
    return 1;
  }

  MARU_WindowGeometry geometry;
  maru_getWindowGeometry(window, &geometry);
  vulkan_renderer_setup_surface(&renderer, surface,
                                (uint32_t)geometry.pixel_size.x,
                                (uint32_t)geometry.pixel_size.y);

  last_stat_time = get_time_sec();

  // Kick off the frame loop
  maru_requestWindowFrame(window);

  printf("Frame loop started. If composited, FPS should be locked to refresh "
         "rate.\n");
  while (keep_running) {
    // We use a small timeout to avoid busy-waiting, but 0 is also fine
    // if we are driven by frame events.
    maru_pumpEvents(context, 10, handle_event, NULL);

    vulkan_renderer_draw_frame(&renderer);
  }

  vkDeviceWaitIdle(renderer.device);
  vulkan_renderer_cleanup(&renderer);
  maru_destroyWindow(window);
  maru_destroyContext(context);

  return 0;
}
