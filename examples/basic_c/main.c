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

static void handle_event(MARU_EventId type, MARU_Window *window,
                         const MARU_Event *event, void *userdata) {
  (void)window;
  (void)userdata;
  if (type == MARU_EVENT_CLOSE_REQUESTED) {
    keep_running = false;
  } else if (type == MARU_EVENT_WINDOW_RESIZED) {
    vulkan_renderer_on_resized(&renderer,
                               (uint32_t)event->resized.geometry.pixel_size.x,
                               (uint32_t)event->resized.geometry.pixel_size.y);
  } else if (type == MARU_EVENT_WINDOW_READY) {
    window_ready = true;
  } else if (type == MARU_EVENT_KEY_STATE_CHANGED) {
    if (event->key.raw_key == MARU_KEY_F && event->key.state == MARU_BUTTON_STATE_PRESSED) {
      maru_setWindowFullscreen(window, !maru_isWindowFullscreen(window));
    }
  }
}

static void handle_diagnostic(const struct MARU_DiagnosticInfo *info, void *userdata) {
  (void)userdata;
  const char* severity = "INFO";
  if (info->diagnostic >= MARU_DIAGNOSTIC_INVALID_ARGUMENT) {
    severity = "ERROR";
  } else if (info->diagnostic > MARU_DIAGNOSTIC_INFO) {
    severity = "WARNING";
  }
  
  fprintf(stderr, "[MARU %s] %d: %s\n", severity, (int)info->diagnostic, info->message);
}

int main() {
  MARU_Version version = maru_getVersion();
  printf("Maru version: %u.%u.%u\n", version.major, version.minor,
         version.patch);

  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  create_info.tuning.wayland.decoration_mode = MARU_WAYLAND_DECORATION_MODE_AUTO;

  create_info.attributes.diagnostic_cb = handle_diagnostic;
  create_info.attributes.event_mask = MARU_ALL_EVENTS;
  
  const void *extensions[] = { (void *)maru_vulkan_init };
  create_info.extensions = extensions;
  create_info.extension_count = 1;

  MARU_Context *context = NULL;
  if (maru_createContext(&create_info, &context) != MARU_SUCCESS) {
    fprintf(stderr, "Failed to create context.\n");
    return 1;
  }

  uint32_t vk_extension_count = 0;
  const char **vk_extensions = maru_vulkan_getVkExtensions(context, &vk_extension_count);
  if (!vk_extensions) {
    fprintf(stderr, "Failed to get Vulkan extensions.\n");
    maru_destroyContext(context);
    return 1;
  }

  // Cast const char* const* to const char**
  vulkan_renderer_init(&renderer, vk_extension_count,
                       (const char **)vk_extensions);

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
    maru_pumpEvents(context, 16, handle_event, NULL);
  }

  if (!keep_running) {
    vulkan_renderer_cleanup(&renderer);
    maru_destroyWindow(window);
    maru_destroyContext(context);
    return 0;
  }

  VkSurfaceKHR surface;
  if (maru_vulkan_createVkSurface(window, renderer.instance, (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr, &surface) !=
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
    maru_pumpEvents(context, 0, handle_event, NULL);

    vulkan_renderer_draw_frame(&renderer);
  }

  vkDeviceWaitIdle(renderer.device);

  vulkan_renderer_cleanup(&renderer);
  maru_destroyWindow(window);
  maru_destroyContext(context);

  return 0;
}
