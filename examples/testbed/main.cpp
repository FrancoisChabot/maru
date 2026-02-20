// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru/maru.h"
#include "maru/c/ext/vulkan.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

#include "imgui.h"
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include "backends/imgui_impl_vulkan.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include "imgui_impl_maru.h"

#include "event_viewer.h"
#include "monitor_explorer.h"
#include "cursor_tester.h"
#include "window_manager.h"

#define MAX_FRAMES_IN_FLIGHT 2

static bool keep_running = true;
static bool window_ready = false;
static bool framebuffer_resized = false;
static uint64_t frame_id = 0;

static MARU_Window* main_window = nullptr;

static bool show_event_viewer = true;
static bool show_monitor_explorer = false;
static bool show_cursor_tester = false;
static bool show_window_manager = false;

struct VulkanState {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
    uint32_t currentFrame = 0;
    VkDescriptorPool imguiDescriptorPool;
} vk;

static void handle_event(MARU_EventType type, MARU_Window *window,
                         const MARU_Event *event, void *userdata) {
  (void)userdata;
  EventViewer_HandleEvent(type, event, frame_id);
  WindowManager_HandleEvent(type, window, event);

  if (ImGui::GetCurrentContext() != nullptr)
    ImGui_ImplMaru_HandleEvent(type, event);

  if (type == MARU_CLOSE_REQUESTED) {
    if (window == main_window) {
        keep_running = false;
    }
  } else if (type == MARU_WINDOW_RESIZED) {
    framebuffer_resized = true;
  } else if (type == MARU_WINDOW_READY) {
    window_ready = true;
  } else if (type == MARU_KEY_STATE_CHANGED) {
    if (event->key.raw_key == MARU_KEY_ESCAPE && event->key.state == MARU_BUTTON_STATE_PRESSED) {
      maru_setWindowCursorMode(window, MARU_CURSOR_NORMAL);
    }
  }
}

// Minimal Vulkan helpers
static void create_vulkan_instance(uint32_t extension_count, const char** extensions) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Maru ImGui Testbed";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extension_count;
    createInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&createInfo, nullptr, &vk.instance) != VK_SUCCESS) {
        fprintf(stderr, "failed to create instance!\n");
        exit(1);
    }
}

static void pick_physical_device() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices.data());
    vk.physicalDevice = devices[0]; // Just pick the first one for simplicity
}

static void create_logical_device() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0; // Assuming family 0 has graphics and present
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(vk.physicalDevice, &createInfo, nullptr, &vk.device) != VK_SUCCESS) {
        fprintf(stderr, "failed to create logical device!\n");
        exit(1);
    }
    vkGetDeviceQueue(vk.device, 0, 0, &vk.graphicsQueue);
    vkGetDeviceQueue(vk.device, 0, 0, &vk.presentQueue);
}

static void create_render_pass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = vk.swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(vk.device, &renderPassInfo, nullptr, &vk.renderPass) != VK_SUCCESS) {
        fprintf(stderr, "failed to create render pass!\n");
        exit(1);
    }
}

static void create_swap_chain(MARU_Window* window) {
    MARU_WindowGeometry geometry;
    maru_getWindowGeometry(window, &geometry);
    vk.swapChainExtent = { (uint32_t)geometry.pixel_size.x, (uint32_t)geometry.pixel_size.y };
    vk.swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vk.surface;
    createInfo.minImageCount = 2;
    createInfo.imageFormat = vk.swapChainImageFormat;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = vk.swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(vk.device, &createInfo, nullptr, &vk.swapChain) != VK_SUCCESS) {
        fprintf(stderr, "failed to create swap chain!\n");
        exit(1);
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, nullptr);
    vk.swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, vk.swapChainImages.data());

    vk.swapChainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vk.swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vk.swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(vk.device, &viewInfo, nullptr, &vk.swapChainImageViews[i]);
    }

    vk.swapChainFramebuffers.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = vk.renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &vk.swapChainImageViews[i];
        framebufferInfo.width = vk.swapChainExtent.width;
        framebufferInfo.height = vk.swapChainExtent.height;
        framebufferInfo.layers = 1;
        vkCreateFramebuffer(vk.device, &framebufferInfo, nullptr, &vk.swapChainFramebuffers[i]);
    }
}

static void cleanup_swap_chain() {
    for (auto framebuffer : vk.swapChainFramebuffers) vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
    for (auto imageView : vk.swapChainImageViews) vkDestroyImageView(vk.device, imageView, nullptr);
    vkDestroySwapchainKHR(vk.device, vk.swapChain, nullptr);
}

int main() {
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
#ifdef _WIN32
  create_info.backend = MARU_BACKEND_WINDOWS;
#else
  create_info.backend = MARU_BACKEND_WAYLAND;
#endif
  create_info.attributes.inhibit_idle = false;
  create_info.attributes.diagnostic_cb = NULL;
  create_info.attributes.diagnostic_userdata = NULL;
  create_info.attributes.event_mask = MARU_ALL_EVENTS;
  create_info.allocator.alloc_cb = NULL;
  create_info.allocator.realloc_cb = NULL;
  create_info.allocator.free_cb = NULL;
  create_info.allocator.userdata = NULL;
  create_info.userdata = NULL;
  
  MARU_ContextTuning tuning = MARU_CONTEXT_TUNING_DEFAULT;
  create_info.tuning = &tuning;

  MARU_Context *context = NULL;
  MARU_Status status = maru_createContext(&create_info, &context);
  if (status != MARU_SUCCESS || !context) {
      fprintf(stderr, "Failed to create context: status=%d\n", status);
      return 1;
  }

  maru_vulkan_enable(context, (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr);

  uint32_t vk_extension_count = 0;
  const char **vk_extensions = maru_vulkan_getVkExtensions(context, &vk_extension_count);

  create_vulkan_instance(vk_extension_count, vk_extensions);
  pick_physical_device();

  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru ImGui Testbed";
  window_info.attributes.logical_size = {1280, 720};
  window_info.attributes.position = {0, 0};
  window_info.userdata = NULL;
  window_info.app_id = "maru.testbed";

  MARU_Status win_status = maru_createWindow(context, &window_info, &main_window);
  if (win_status != MARU_SUCCESS) {
      fprintf(stderr, "Failed to create window: status=%d\n", win_status);
      return 1;
  }

  while (keep_running && !window_ready) {
      maru_pumpEvents(context, 16, handle_event, NULL);
  }
  if (!window_ready) {
      fprintf(stderr, "Window failed to become ready\n");
      return 1;
  }

  maru_vulkan_createVkSurface(main_window, vk.instance, &vk.surface);
  create_logical_device();
  vk.swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  create_render_pass();
  create_swap_chain(main_window);

  WindowManager_VulkanContext wm_vk_ctx = {};
  wm_vk_ctx.instance = vk.instance;
  wm_vk_ctx.physicalDevice = vk.physicalDevice;
  wm_vk_ctx.device = vk.device;
  wm_vk_ctx.graphicsQueue = vk.graphicsQueue;
  wm_vk_ctx.queueFamilyIndex = 0; // Assuming 0 for simplicity as in main.cpp
  WindowManager_Init(context, main_window, wm_vk_ctx);

  // Sync objects
  VkSemaphoreCreateInfo semaphoreInfo;
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = VK_NULL_HANDLE;
  VkFenceCreateInfo fenceInfo;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = VK_NULL_HANDLE;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.imageAvailableSemaphores[i]);
    vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.renderFinishedSemaphores[i]);
    vkCreateFence(vk.device, &fenceInfo, nullptr, &vk.inFlightFences[i]);
  }

  // Command pool & buffers
  VkCommandPoolCreateInfo poolInfo;
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.pNext = VK_NULL_HANDLE;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = 0;
  vkCreateCommandPool(vk.device, &poolInfo, nullptr, &vk.commandPool);

  VkCommandBufferAllocateInfo allocInfo;
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.pNext = VK_NULL_HANDLE;
  allocInfo.commandPool = vk.commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
  vkAllocateCommandBuffers(vk.device, &allocInfo, vk.commandBuffers);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();
  ImGui_ImplMaru_Init(main_window);
  
  // ImGui Descriptor Pool
  VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
  VkDescriptorPoolCreateInfo descPoolInfo;
  descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descPoolInfo.pNext = VK_NULL_HANDLE;
  descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descPoolInfo.maxSets = 1;
  descPoolInfo.poolSizeCount = 1;
  descPoolInfo.pPoolSizes = pool_sizes;
  vkCreateDescriptorPool(vk.device, &descPoolInfo, nullptr, &vk.imguiDescriptorPool);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = vk.instance;
  init_info.PhysicalDevice = vk.physicalDevice;
  init_info.Device = vk.device;
  init_info.QueueFamily = 0;
  init_info.Queue = vk.graphicsQueue;
  init_info.DescriptorPool = vk.imguiDescriptorPool;
  init_info.RenderPass = vk.renderPass;
  init_info.MinImageCount = 2;
  init_info.ImageCount = (uint32_t)vk.swapChainImages.size();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  ImGui_ImplVulkan_Init(&init_info);

  while (keep_running) {
    frame_id++;
    maru_pumpEvents(context, 0, handle_event, NULL);

    if (framebuffer_resized) {
        cleanup_swap_chain();
        create_swap_chain(main_window);
        framebuffer_resized = false;
    }

    vkWaitForFences(vk.device, 1, &vk.inFlightFences[vk.currentFrame], VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(vk.device, vk.swapChain, UINT64_MAX, vk.imageAvailableSemaphores[vk.currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) { framebuffer_resized = true; continue; }
    vkResetFences(vk.device, 1, &vk.inFlightFences[vk.currentFrame]);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplMaru_NewFrame();
    ImGui::NewFrame();
    
    // TOC Window
    ImGui::SetNextWindowSize(ImVec2(200, 150), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Table of Contents")) {
        ImGui::Checkbox("Event Viewer", &show_event_viewer);
        ImGui::Checkbox("Monitor Explorer", &show_monitor_explorer);
        ImGui::Checkbox("Cursor Tester", &show_cursor_tester);
        ImGui::Checkbox("Window Manager", &show_window_manager);
    }
    ImGui::End();

    if (show_event_viewer) EventViewer_Render(main_window, &show_event_viewer);
    if (show_monitor_explorer) MonitorExplorer_Render(context, &show_monitor_explorer);
    if (show_cursor_tester) CursorTester_Render(context, main_window, &show_cursor_tester);
    if (show_window_manager) WindowManager_Render(context, &show_window_manager);

    WindowManager_Update();

    ImGui::Render();

    vkResetCommandBuffer(vk.commandBuffers[vk.currentFrame], 0);
    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = VK_NULL_HANDLE;
    vkBeginCommandBuffer(vk.commandBuffers[vk.currentFrame], &beginInfo);

    VkRenderPassBeginInfo rpBegin;
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.pNext = VK_NULL_HANDLE;
    rpBegin.renderPass = vk.renderPass;
    rpBegin.framebuffer = vk.swapChainFramebuffers[imageIndex];
    rpBegin.renderArea.extent = vk.swapChainExtent;
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearColor;
    vkCmdBeginRenderPass(vk.commandBuffers[vk.currentFrame], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.commandBuffers[vk.currentFrame]);

    vkCmdEndRenderPass(vk.commandBuffers[vk.currentFrame]);
    vkEndCommandBuffer(vk.commandBuffers[vk.currentFrame]);

    VkSubmitInfo submit;
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = VK_NULL_HANDLE;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &vk.imageAvailableSemaphores[vk.currentFrame];
    submit.pWaitDstStageMask = waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &vk.commandBuffers[vk.currentFrame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &vk.renderFinishedSemaphores[vk.currentFrame];
    vkQueueSubmit(vk.graphicsQueue, 1, &submit, vk.inFlightFences[vk.currentFrame]);

    VkPresentInfoKHR present;
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = VK_NULL_HANDLE;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &vk.renderFinishedSemaphores[vk.currentFrame];
    present.swapchainCount = 1;
    present.pSwapchains = &vk.swapChain;
    present.pImageIndices = &imageIndex;
    vkQueuePresentKHR(vk.presentQueue, &present);

    vk.currentFrame = (vk.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  vkDeviceWaitIdle(vk.device);
  WindowManager_Cleanup();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplMaru_Shutdown();
  ImGui::DestroyContext();
  
  cleanup_swap_chain();
  vkDestroyDescriptorPool(vk.device, vk.imguiDescriptorPool, nullptr);
  vkDestroyCommandPool(vk.device, vk.commandPool, nullptr);
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(vk.device, vk.imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(vk.device, vk.renderFinishedSemaphores[i], nullptr);
    vkDestroyFence(vk.device, vk.inFlightFences[i], nullptr);
  }
  vkDestroyRenderPass(vk.device, vk.renderPass, nullptr);
  vkDestroyDevice(vk.device, nullptr);
  vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
  vkDestroyInstance(vk.instance, nullptr);
  maru_destroyWindow(main_window);
  maru_destroyContext(context);
  return 0;
}
