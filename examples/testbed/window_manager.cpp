// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "window_manager.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <algorithm>
#include "imgui.h"
#include "maru/c/ext/vulkan.h"

struct SecondaryWindow {
    MARU_Window* maru_window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkExtent2D swapChainExtent = {0, 0};
    VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFence> inFlightFences;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    
    uint32_t currentFrame = 0;
    float clearColor[4] = {0.1f, 0.2f, 0.3f, 1.0f};
    bool needs_resize = false;
    bool ready = false;
    bool should_close = false;
    char title[64];
};

static std::vector<SecondaryWindow*> g_secondary_windows;
static WindowManager_VulkanContext g_vk_ctx;
static MARU_Context* g_maru_context;
static MARU_Window* g_main_window;
static VkRenderPass g_renderPass = VK_NULL_HANDLE;

static void cleanup_swapchain(SecondaryWindow* sw) {
    if (sw->swapChain == VK_NULL_HANDLE) return;
    
    vkDeviceWaitIdle(g_vk_ctx.device);
    
    for (auto fb : sw->swapChainFramebuffers) vkDestroyFramebuffer(g_vk_ctx.device, fb, nullptr);
    for (auto iv : sw->swapChainImageViews) vkDestroyImageView(g_vk_ctx.device, iv, nullptr);
    
    sw->swapChainFramebuffers.clear();
    sw->swapChainImageViews.clear();
    sw->swapChainImages.clear();
    
    vkDestroySwapchainKHR(g_vk_ctx.device, sw->swapChain, nullptr);
    sw->swapChain = VK_NULL_HANDLE;
}

static void create_swapchain(SecondaryWindow* sw, MARU_Window* window) {
    MARU_WindowGeometry geometry;
    maru_getWindowGeometry(window, &geometry);
    
    if (geometry.pixel_size.x == 0 || geometry.pixel_size.y == 0) return;
    
    sw->swapChainExtent = { (uint32_t)geometry.pixel_size.x, (uint32_t)geometry.pixel_size.y };
    sw->swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = sw->surface;
    createInfo.minImageCount = 2;
    createInfo.imageFormat = sw->swapChainImageFormat;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = sw->swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(g_vk_ctx.device, &createInfo, nullptr, &sw->swapChain) != VK_SUCCESS) {
        fprintf(stderr, "failed to create swap chain!\n");
        return;
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(g_vk_ctx.device, sw->swapChain, &imageCount, nullptr);
    sw->swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(g_vk_ctx.device, sw->swapChain, &imageCount, sw->swapChainImages.data());

    sw->swapChainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = sw->swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = sw->swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(g_vk_ctx.device, &viewInfo, nullptr, &sw->swapChainImageViews[i]);
    }

    sw->swapChainFramebuffers.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = g_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &sw->swapChainImageViews[i];
        framebufferInfo.width = sw->swapChainExtent.width;
        framebufferInfo.height = sw->swapChainExtent.height;
        framebufferInfo.layers = 1;
        vkCreateFramebuffer(g_vk_ctx.device, &framebufferInfo, nullptr, &sw->swapChainFramebuffers[i]);
    }
}

static void cleanup_window(SecondaryWindow* sw) {
    vkDeviceWaitIdle(g_vk_ctx.device);
    cleanup_swapchain(sw);
    
    for (auto f : sw->inFlightFences) vkDestroyFence(g_vk_ctx.device, f, nullptr);
    for (auto s : sw->imageAvailableSemaphores) vkDestroySemaphore(g_vk_ctx.device, s, nullptr);
    for (auto s : sw->renderFinishedSemaphores) vkDestroySemaphore(g_vk_ctx.device, s, nullptr);
    
    if (sw->commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_vk_ctx.device, sw->commandPool, nullptr);
    }
    
    if (sw->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_vk_ctx.instance, sw->surface, nullptr);
    }
    
    if (sw->maru_window) {
        maru_destroyWindow(sw->maru_window);
    }
    
    delete sw;
}

void WindowManager_Init(MARU_Context* context, MARU_Window* main_window, const WindowManager_VulkanContext& vk_ctx) {
    g_maru_context = context;
    g_main_window = main_window;
    g_vk_ctx = vk_ctx;

    // Create a shared render pass for all secondary windows
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM; // Should match swapchain format
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

    if (vkCreateRenderPass(g_vk_ctx.device, &renderPassInfo, nullptr, &g_renderPass) != VK_SUCCESS) {
        fprintf(stderr, "failed to create shared render pass!\n");
    }
}

static void RenderWindowControls(SecondaryWindow* sw, MARU_Window* window, const char* default_title) {
    char title_buf[256];
    const char* current_title = maru_getWindowTitle(window);
    if (!current_title) current_title = default_title;
    
    snprintf(title_buf, sizeof(title_buf), "%s", current_title);
    if (ImGui::InputText("Title", title_buf, sizeof(title_buf))) {
        maru_setWindowTitle(window, title_buf);
    }

    bool fullscreen = maru_isWindowFullscreen(window);
    if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
        maru_setWindowFullscreen(window, fullscreen);
    }

    bool passthrough = maru_isWindowMousePassthrough(window);
    if (ImGui::Checkbox("Mouse Passthrough", &passthrough)) {
        maru_setWindowMousePassthrough(window, passthrough);
    }

    bool resizable = maru_isWindowResizable(window);
    if (ImGui::Checkbox("Resizable", &resizable)) {
        maru_setWindowResizable(window, resizable);
    }

    bool decorated = maru_isWindowDecorated(window);
    if (ImGui::Checkbox("Decorated", &decorated)) {
        maru_setWindowDecorated(window, decorated);
    }

    bool maximized = maru_isWindowMaximized(window);
    if (ImGui::Checkbox("Maximized", &maximized)) {
        maru_setWindowMaximized(window, maximized);
    }

    MARU_WindowGeometry geometry;
    maru_getWindowGeometry(window, &geometry);
    int size[2] = { (int)geometry.logical_size.x, (int)geometry.logical_size.y };
    if (ImGui::InputInt2("Logical Size", size)) {
        maru_setWindowSize(window, {(MARU_Scalar)size[0], (MARU_Scalar)size[1]});
    }

    int pos[2] = { (int)geometry.origin.x, (int)geometry.origin.y };
    if (ImGui::InputInt2("Position", pos)) {
        MARU_WindowAttributes attr = {};
        attr.position = {(MARU_Scalar)pos[0], (MARU_Scalar)pos[1]};
        maru_updateWindow(window, MARU_WINDOW_ATTR_POSITION, &attr);
    }

    if (sw) {
        static float min_size[2] = { 200, 200 };
        static float max_size[2] = { 1000, 1000 };
        static int aspect[2] = { 0, 0 };

        ImGui::InputFloat2("Min Size", min_size);
        ImGui::InputFloat2("Max Size", max_size);
        ImGui::InputInt2("Aspect Ratio (Num/Denom)", aspect);

        if (ImGui::Button("Commit Constraints")) {
            maru_setWindowMinSize(window, {min_size[0], min_size[1]});
            maru_setWindowMaxSize(window, {max_size[0], max_size[1]});
            maru_setWindowAspectRatio(window, {(int32_t)aspect[0], (int32_t)aspect[1]});
        }
    }
}

void WindowManager_Render(MARU_Context* context, bool* p_open) {
    if (!ImGui::Begin("Window Manager", p_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Create New Window")) {
        SecondaryWindow* sw = new SecondaryWindow();
        static int window_count = 0;
        snprintf(sw->title, sizeof(sw->title), "Secondary Window %d", ++window_count);
        
        MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
        window_info.attributes.title = sw->title;
        window_info.attributes.logical_size = {640, 480};
        window_info.userdata = sw;

        if (maru_createWindow(context, &window_info, &sw->maru_window) == MARU_SUCCESS) {
            g_secondary_windows.push_back(sw);
        } else {
            delete sw;
        }
    }

    ImGui::Separator();
    
    if (ImGui::TreeNode("Main Window")) {
        RenderWindowControls(nullptr, g_main_window, "Maru ImGui Testbed");
        ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::Text("Secondary Windows: %zu", g_secondary_windows.size());

    for (size_t i = 0; i < g_secondary_windows.size(); ++i) {
        SecondaryWindow* sw = g_secondary_windows[i];
        ImGui::PushID((int)i);
        if (ImGui::TreeNode(sw->title)) {
            RenderWindowControls(sw, sw->maru_window, sw->title);
            ImGui::ColorEdit4("Clear Color", sw->clearColor);
            if (ImGui::Button("Close")) {
                sw->should_close = true;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::End();
}

void WindowManager_HandleEvent(MARU_EventType type, MARU_Window* window, const MARU_Event* event) {
    SecondaryWindow* sw = (SecondaryWindow*)maru_getWindowUserdata(window);
    if (!sw) return;

    if (type == MARU_CLOSE_REQUESTED) {
        sw->should_close = true;
    } else if (type == MARU_WINDOW_RESIZED) {
        sw->needs_resize = true;
    } else if (type == MARU_WINDOW_READY) {
        sw->ready = true;
        sw->maru_window = window;
        
        maru_createVkSurface(window, g_vk_ctx.instance, &sw->surface);
        
        // Setup sync objects
        VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        const int MAX_FRAMES_IN_FLIGHT = 2;
        sw->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        sw->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        sw->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkCreateSemaphore(g_vk_ctx.device, &semaphoreInfo, nullptr, &sw->imageAvailableSemaphores[i]);
            vkCreateSemaphore(g_vk_ctx.device, &semaphoreInfo, nullptr, &sw->renderFinishedSemaphores[i]);
            vkCreateFence(g_vk_ctx.device, &fenceInfo, nullptr, &sw->inFlightFences[i]);
        }

        // Command pool & buffers
        VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = g_vk_ctx.queueFamilyIndex;
        vkCreateCommandPool(g_vk_ctx.device, &poolInfo, nullptr, &sw->commandPool);

        sw->commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = sw->commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        vkAllocateCommandBuffers(g_vk_ctx.device, &allocInfo, sw->commandBuffers.data());
        
        create_swapchain(sw, window);
    }
}

void WindowManager_Update() {
    // Remove closed windows
    for (auto it = g_secondary_windows.begin(); it != g_secondary_windows.end(); ) {
        if ((*it)->should_close) {
            cleanup_window(*it);
            it = g_secondary_windows.erase(it);
        } else {
            ++it;
        }
    }

    // Render active windows
    for (auto sw : g_secondary_windows) {
        if (!sw->ready || sw->swapChain == VK_NULL_HANDLE) continue;

        if (sw->needs_resize) {
            cleanup_swapchain(sw);
            create_swapchain(sw, sw->maru_window);
            sw->needs_resize = false;
        }

        vkWaitForFences(g_vk_ctx.device, 1, &sw->inFlightFences[sw->currentFrame], VK_TRUE, UINT64_MAX);
        
        uint32_t imageIndex;
        VkResult res = vkAcquireNextImageKHR(g_vk_ctx.device, sw->swapChain, UINT64_MAX, sw->imageAvailableSemaphores[sw->currentFrame], VK_NULL_HANDLE, &imageIndex);
        
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            sw->needs_resize = true;
            continue;
        } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            continue;
        }

        vkResetFences(g_vk_ctx.device, 1, &sw->inFlightFences[sw->currentFrame]);

        vkResetCommandBuffer(sw->commandBuffers[sw->currentFrame], 0);
        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(sw->commandBuffers[sw->currentFrame], &beginInfo);

        VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpBegin.renderPass = g_renderPass;
        rpBegin.framebuffer = sw->swapChainFramebuffers[imageIndex];
        rpBegin.renderArea.extent = sw->swapChainExtent;
        VkClearValue clearColor = {{{sw->clearColor[0], sw->clearColor[1], sw->clearColor[2], sw->clearColor[3]}}};
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues = &clearColor;
        
        vkCmdBeginRenderPass(sw->commandBuffers[sw->currentFrame], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(sw->commandBuffers[sw->currentFrame]);
        vkEndCommandBuffer(sw->commandBuffers[sw->currentFrame]);

        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &sw->imageAvailableSemaphores[sw->currentFrame];
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &sw->commandBuffers[sw->currentFrame];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &sw->renderFinishedSemaphores[sw->currentFrame];
        
        vkQueueSubmit(g_vk_ctx.graphicsQueue, 1, &submit, sw->inFlightFences[sw->currentFrame]);

        VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &sw->renderFinishedSemaphores[sw->currentFrame];
        present.swapchainCount = 1;
        present.pSwapchains = &sw->swapChain;
        present.pImageIndices = &imageIndex;
        
        vkQueuePresentKHR(g_vk_ctx.graphicsQueue, &present);

        sw->currentFrame = (sw->currentFrame + 1) % 2;
    }
}

void WindowManager_Cleanup() {
    for (auto sw : g_secondary_windows) {
        cleanup_window(sw);
    }
    g_secondary_windows.clear();
    
    if (g_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_vk_ctx.device, g_renderPass, nullptr);
    }
}
