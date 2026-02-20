// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru/maru.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui_impl_maru.h"

#define MAX_FRAMES_IN_FLIGHT 2

static bool keep_running = true;
static bool window_ready = false;
static bool framebuffer_resized = false;
static uint64_t frame_id = 0;

struct LoggedEvent {
    uint64_t frame;
    MARU_EventType type;
    std::string description;
};
static std::vector<LoggedEvent> event_log;

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

static const char* event_type_to_string(MARU_EventType type) {
    if (type == MARU_CLOSE_REQUESTED) return "CLOSE_REQUESTED";
    if (type == MARU_WINDOW_RESIZED) return "WINDOW_RESIZED";
    if (type == MARU_KEY_STATE_CHANGED) return "KEY_STATE_CHANGED";
    if (type == MARU_WINDOW_READY) return "WINDOW_READY";
    if (type == MARU_MOUSE_MOVED) return "MOUSE_MOVED";
    if (type == MARU_MOUSE_BUTTON_STATE_CHANGED) return "MOUSE_BUTTON_STATE_CHANGED";
    if (type == MARU_MOUSE_SCROLLED) return "MOUSE_SCROLLED";
    if (type == MARU_IDLE_STATE_CHANGED) return "IDLE_STATE_CHANGED";
    if (type == MARU_MONITOR_CONNECTION_CHANGED) return "MONITOR_CONNECTION_CHANGED";
    if (type == MARU_MONITOR_MODE_CHANGED) return "MONITOR_MODE_CHANGED";
    if (type == MARU_SYNC_POINT_REACHED) return "SYNC_POINT_REACHED";
    if (type == MARU_TEXT_INPUT_RECEIVED) return "TEXT_INPUT_RECEIVED";
    if (type == MARU_FOCUS_CHANGED) return "FOCUS_CHANGED";
    if (type == MARU_WINDOW_MAXIMIZED) return "WINDOW_MAXIMIZED";
    return "UNKNOWN";
}

static void handle_event(MARU_EventType type, MARU_Window *window,
                         const MARU_Event *event) {
  (void)window;
  
  char buf[256];
  buf[0] = '\0';
  if (type == MARU_WINDOW_RESIZED) {
      snprintf(buf, sizeof(buf), "Size: %dx%d", event->resized.geometry.pixel_size.x, event->resized.geometry.pixel_size.y);
  } else if (type == MARU_KEY_STATE_CHANGED) {
      snprintf(buf, sizeof(buf), "Key: %d, State: %d", (int)event->key.raw_key, (int)event->key.state);
  } else if (type == MARU_MOUSE_MOVED) {
      snprintf(buf, sizeof(buf), "Pos: %.1f, %.1f", event->mouse_motion.position.x, event->mouse_motion.position.y);
  } else if (type == MARU_TEXT_INPUT_RECEIVED) {
      snprintf(buf, sizeof(buf), "Text: %s", event->text_input.text);
  }

  event_log.push_back({frame_id, type, buf});
  if (event_log.size() > 500) event_log.erase(event_log.begin());

  if (ImGui::GetCurrentContext() != nullptr)
    ImGui_ImplMaru_HandleEvent(type, event);

  if (type == MARU_CLOSE_REQUESTED) {
    keep_running = false;
  } else if (type == MARU_WINDOW_RESIZED) {
    framebuffer_resized = true;
  } else if (type == MARU_WINDOW_READY) {
    window_ready = true;
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
  
  MARU_ContextTuning tuning = MARU_CONTEXT_TUNING_DEFAULT;
  tuning.vk_loader = (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr;
  create_info.tuning = &tuning;

  create_info.attributes.event_callback = (MARU_EventCallback)handle_event;
  create_info.attributes.event_mask = MARU_ALL_EVENTS;
  MARU_Context *context = NULL;
  maru_createContext(&create_info, &context);

  MARU_ExtensionList vk_extensions = {0};
  maru_getVkExtensions(context, &vk_extensions);

  create_vulkan_instance(vk_extensions.count, (const char **)vk_extensions.extensions);
  pick_physical_device();

  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru ImGui Testbed";
  window_info.attributes.logical_size = {1280, 720};

  MARU_Window *window = NULL;
  maru_createWindow(context, &window_info, &window);

  while (keep_running && !window_ready) maru_pumpEvents(context, 16);

  maru_createVkSurface(window, vk.instance, &vk.surface);
  create_logical_device();
  vk.swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  create_render_pass();
  create_swap_chain(window);

  // Sync objects
  VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.imageAvailableSemaphores[i]);
    vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.renderFinishedSemaphores[i]);
    vkCreateFence(vk.device, &fenceInfo, nullptr, &vk.inFlightFences[i]);
  }

  // Command pool & buffers
  VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = 0;
  vkCreateCommandPool(vk.device, &poolInfo, nullptr, &vk.commandPool);

  VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
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
  ImGui_ImplMaru_Init(window);
  
  // ImGui Descriptor Pool
  VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
  VkDescriptorPoolCreateInfo descPoolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
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
    maru_pumpEvents(context, 0);

    if (framebuffer_resized) {
        cleanup_swap_chain();
        create_swap_chain(window);
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
    
    // Event Log Window
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Event Log")) {
        bool is_fs = maru_isWindowFullscreen(window);
        if (ImGui::Checkbox("Fullscreen", &is_fs)) {
            maru_setWindowFullscreen(window, is_fs);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) event_log.clear();
        ImGui::BeginChild("LogScroll");
        for (const auto& entry : event_log) {
            float alpha = (entry.frame % 2) ? 0.1f : 0.2f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, alpha));
            ImGui::BeginChild(std::to_string((uintptr_t)&entry).c_str(), ImVec2(0, ImGui::GetTextLineHeightWithSpacing()), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::Text("[%llu] %s %s", entry.frame, event_type_to_string(entry.type), entry.description.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::ShowDemoWindow();
    ImGui::Render();

    vkResetCommandBuffer(vk.commandBuffers[vk.currentFrame], 0);
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(vk.commandBuffers[vk.currentFrame], &beginInfo);

    VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
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

    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &vk.imageAvailableSemaphores[vk.currentFrame];
    submit.pWaitDstStageMask = waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &vk.commandBuffers[vk.currentFrame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &vk.renderFinishedSemaphores[vk.currentFrame];
    vkQueueSubmit(vk.graphicsQueue, 1, &submit, vk.inFlightFences[vk.currentFrame]);

    VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &vk.renderFinishedSemaphores[vk.currentFrame];
    present.swapchainCount = 1;
    present.pSwapchains = &vk.swapChain;
    present.pImageIndices = &imageIndex;
    vkQueuePresentKHR(vk.presentQueue, &present);

    vk.currentFrame = (vk.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  vkDeviceWaitIdle(vk.device);
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
  maru_destroyWindow(window);
  maru_destroyContext(context);
  return 0;
}
