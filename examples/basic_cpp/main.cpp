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
                                   (uint32_t)geometry.px_size.x,
                                   (uint32_t)geometry.px_size.y);
  }

  void setup_surface(VkSurfaceKHR surface, const MARU_WindowGeometry& geometry) {
    vulkan_renderer_setup_surface(&renderer, surface,
                                      (uint32_t)geometry.px_size.x,
                                      (uint32_t)geometry.px_size.y);
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
  auto context_result = maru::Context::create();
  if (!context_result) {
    std::cerr << "Failed to create context\n";
    return -1;
  }
  maru::Context& context = *context_result;

  // Initialize the vulkan renderer (we'll need a vkInstance at least)
  Renderer renderer(context.getVkExtensions());
  
  // Create a window.
  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Basic C++ Example";
  window_info.attributes.dip_size = {800, 600};
  
  auto window_result = context.createWindow(window_info);
  if (!window_result) {
    std::cerr << "Failed to create window\n";
    return -1;
  }
  maru::Window& window = *window_result;

  // setup our event handler
  auto event_handler = maru::makeDispatcher(
      [&](maru::CloseRequestedEvent) { 
        keep_running = false; 
      },
      [&](maru::WindowResizedEvent e) {
        renderer.on_resized(e->geometry);
      },
      [&](maru::MouseMovedEvent e) {
        std::cout << "mouse movement: (" << e->dip_position.x << ", " << e->dip_position.y << ")\n";
      },
      [&](maru::KeyChangedEvent e) {
        std::cout << "key: (" << e->raw_key << ", " << e->state << ")\n";
      },
      [&](maru::ControllerButtonChangedEvent e) {
        std::cout << "Controller Button: (" << e->button_id << ", " << e->state << ")\n";
      },
      [&](maru::WindowReadyEvent e) {
        auto surface_result = window.createVkSurface(renderer.instance(), vkGetInstanceProcAddr);
        if (surface_result) {
          renderer.setup_surface(*surface_result, e->geometry);
        }
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
