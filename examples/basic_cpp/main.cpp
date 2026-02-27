#include "../shared_renderer/vulkan_renderer.h"
#include "maru/cpp/events.hpp"
#include "maru/maru.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

using namespace std::chrono_literals;

class Renderer {
public:
  Renderer(std::vector<const char *> ext) {
    vulkan_renderer_init(&renderer, (uint32_t)ext.size(), ext.data());
  }

  ~Renderer() {
    vulkan_renderer_cleanup(&renderer);
  }

  void on_resized(const MARU_WindowGeometry& geometry) {
    vulkan_renderer_on_resized(&renderer,
                                   (uint32_t)geometry.pixel_size.x,
                                   (uint32_t)geometry.pixel_size.y);
  }

  void setup_surface(VkSurfaceKHR surface, const MARU_WindowGeometry& geometry) {
    vulkan_renderer_setup_surface(&renderer, surface,
                                      (uint32_t)geometry.pixel_size.x,
                                      (uint32_t)geometry.pixel_size.y);
  }

  VkInstance instance() {
    return renderer.instance;
  }

  bool ready() {
    return renderer.ready;
  }

  void draw_frame() {
    vulkan_renderer_draw_frame(&renderer);
  }
private:
  VulkanRenderer renderer;
};

int main() {
  bool keep_running = true;

  // Create a maru context
  maru::Context context;

  // Initialize the vulkan renderer (we'll need a vkInstance at least)
  Renderer renderer(context.getVkExtensions());
  
  // Create a window.
  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Basic C++ Example";
  window_info.attributes.logical_size = {800, 600};
  maru::Window window = context.createWindow(window_info);

  // setup our event handler
  auto event_handler = maru::makeDispatcher(
      [&](maru::WindowCloseEvent) { 
        keep_running = false; 
      },
      [&](maru::WindowResizedEvent e) {
        renderer.on_resized(e->geometry);
      },
      [&](maru::MouseMotionEvent e) {
        std::cout << "mouse movement: (" << e->position.x << ", " << e->position.y << ")\n";
      },
      [&](maru::KeyboardEvent e) {
        std::cout << "key: (" << e->raw_key << ", " << e->state << ")\n";
      },
      [&](maru::ControllerButtonStateChangedEvent e) {
        std::cout << "Controller Button: (" << e->button_id << ", " << e->state << ")\n";
      },
      [&](maru::WindowReadyEvent e) {
        renderer.setup_surface(
          window.createVkSurface(renderer.instance(), vkGetInstanceProcAddr), 
          e->geometry
        );
      });

  while (keep_running) {
    // Avoid busy-looping before the window is ready.
    auto pump_timeout = renderer.ready() ? 0ms : 16ms;
    context.pumpEvents(event_handler, pump_timeout);

    if (renderer.ready()) {
      renderer.draw_frame();
    }
  }

  return 0;
}
