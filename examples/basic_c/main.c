#include "../shared_renderer/vulkan_renderer.h"
#include "maru/c/events.h"
#include "maru/c/ext/vulkan.h"
#include "maru/c/windows.h"
#include "maru/maru.h"
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

static bool keep_running = true;
static bool window_ready = false;
static VulkanRenderer renderer;

static void handle_event(MARU_EventType type, MARU_Window *window,
                         const MARU_Event *event) {
  (void)window;
  if (type == MARU_CLOSE_REQUESTED) {
    keep_running = false;
  } else if (type == MARU_WINDOW_RESIZED) {
    vulkan_renderer_on_resized(&renderer,
                               (uint32_t)event->resized.geometry.pixel_size.x,
                               (uint32_t)event->resized.geometry.pixel_size.y);
  } else if (type == MARU_WINDOW_READY) {
    window_ready = true;
  } else if (type == MARU_KEY_STATE_CHANGED) {
    if (event->key.raw_key == MARU_KEY_F && event->key.state == MARU_BUTTON_STATE_PRESSED) {
      maru_setWindowFullscreen(window, !maru_isWindowFullscreen(window));
    }
  }
}

int main() {
  MARU_Version version = maru_getVersion();
  printf("Maru version: %u.%u.%u\n", version.major, version.minor,
         version.patch);

  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
#ifdef _WIN32
  create_info.backend = MARU_BACKEND_WINDOWS;
#else
  create_info.backend = MARU_BACKEND_WAYLAND;
#endif
  
  MARU_ContextTuning tuning = MARU_CONTEXT_TUNING_DEFAULT;
  tuning.vk_loader = (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr;
  create_info.tuning = &tuning;

  create_info.attributes.event_callback = (MARU_EventCallback)handle_event;
  create_info.attributes.event_mask = MARU_ALL_EVENTS;
  MARU_Context *context = NULL;
  if (maru_createContext(&create_info, &context) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create context.\n");
    return 1;
  }

  MARU_ExtensionList vk_extensions = {0};
  if (maru_getVkExtensions(context, &vk_extensions) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to get Vulkan extensions.\n");
    maru_destroyContext(context);
    return 1;
  }

  // Cast const char* const* to const char**
  vulkan_renderer_init(&renderer, vk_extensions.count,
                       (const char **)vk_extensions.extensions);

  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Basic C Example";
  window_info.attributes.logical_size = (MARU_Vec2Dip){800, 600};

  MARU_Window *window = NULL;
  if (maru_createWindow(context, &window_info, &window) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create window.\n");
    vulkan_renderer_cleanup(&renderer);
    maru_destroyContext(context);
    return 1;
  }

  printf("Waiting for window to be ready...\n");
  while (keep_running && !window_ready) {
    maru_pumpEvents(context, 16);
  }

  if (!keep_running) {
    vulkan_renderer_cleanup(&renderer);
    maru_destroyWindow(window);
    maru_destroyContext(context);
    return 0;
  }

  VkSurfaceKHR surface;
  if (maru_createVkSurface(window, renderer.instance, &surface) !=
      MARU_SUCCESS) {
    fprintf(stderr, "Failed to create Vulkan surface.\n");
    maru_destroyWindow(window);
    vulkan_renderer_cleanup(&renderer);
    maru_destroyContext(context);
    return 1;
  }

  // We need pixel size for swapchain
  MARU_WindowGeometry geometry;
  maru_getWindowGeometry(window, &geometry);
  vulkan_renderer_setup_surface(&renderer, surface,
                                (uint32_t)geometry.pixel_size.x,
                                (uint32_t)geometry.pixel_size.y);

  printf("Entering main loop...\n");
  while (keep_running) {
    maru_pumpEvents(context, 0);

    vulkan_renderer_draw_frame(&renderer);
  }

  vkDeviceWaitIdle(renderer.device);

  vulkan_renderer_cleanup(&renderer);
  maru_destroyWindow(window);
  maru_destroyContext(context);

  return 0;
}
