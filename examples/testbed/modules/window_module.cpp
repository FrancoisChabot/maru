// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "window_module.h"
#include "imgui.h"
#include "maru/maru.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>

WindowModule::WindowModule(MARU_Window* primary_window, VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, uint32_t queue_family, VkQueue queue)
    : primary_window_(primary_window),
      instance_(instance),
      physical_device_(physical_device),
      device_(device),
      queue_family_(queue_family),
      queue_(queue) {
    const char* current_title = maru_getWindowTitle(primary_window_);
    if (current_title) {
        std::strncpy(primary_title_buf_, current_title, sizeof(primary_title_buf_) - 1);
        primary_title_buf_[sizeof(primary_title_buf_) - 1] = '\0';
    }
    MARU_WindowGeometry geometry = maru_getWindowGeometry(primary_window_);
    primary_size_[0] = (int)geometry.dip_size.x;
    primary_size_[1] = (int)geometry.dip_size.y;
}

WindowModule::~WindowModule() {
    handling_window_teardown_ = true;
    for (auto& sw : secondary_windows_) {
        cleanupSecondaryWindow(*sw);
    }
    handling_window_teardown_ = false;

    if (test_icon_) {
        maru_destroyImage(test_icon_);
    }
}

static void check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) return;
    std::fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) {
        std::abort();
    }
}

void WindowModule::onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) {
    (void)event;
    if (handling_window_teardown_) {
        return;
    }
    if (type == MARU_EVENT_WINDOW_READY) {
        for (auto& sw : secondary_windows_) {
            if (sw->window == window) {
                sw->ready = true;
                return;
            }
        }
    }
    if (type != MARU_EVENT_CLOSE_REQUESTED) {
        return;
    }

    for (auto& sw : secondary_windows_) {
        if (sw->window == window) {
            sw->should_close = true;
            return;
        }
    }
}

void WindowModule::onContextRecreated(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    primary_window_ = window;
    const char* current_title = maru_getWindowTitle(primary_window_);
    if (current_title) {
        std::strncpy(primary_title_buf_, current_title, sizeof(primary_title_buf_) - 1);
        primary_title_buf_[sizeof(primary_title_buf_) - 1] = '\0';
    }
    MARU_WindowGeometry geometry = maru_getWindowGeometry(primary_window_);
    primary_size_[0] = (int)geometry.dip_size.x;
    primary_size_[1] = (int)geometry.dip_size.y;

    handling_window_teardown_ = true;
    for (auto& sw : secondary_windows_) {
        cleanupSecondaryWindow(*sw);
    }
    secondary_windows_.clear();
    handling_window_teardown_ = false;
}

void WindowModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    std::vector<std::unique_ptr<SecondaryWindow>> closing_windows;
    for (size_t i = 0; i < secondary_windows_.size();) {
        if (!secondary_windows_[i]->should_close) {
            ++i;
            continue;
        }
        closing_windows.push_back(std::move(secondary_windows_[i]));
        secondary_windows_.erase(secondary_windows_.begin() + static_cast<std::vector<std::unique_ptr<SecondaryWindow>>::difference_type>(i));
    }

    if (!closing_windows.empty()) {
        handling_window_teardown_ = true;
        for (auto& sw : closing_windows) {
            cleanupSecondaryWindow(*sw);
        }
        handling_window_teardown_ = false;
    }

    for (auto& sw : secondary_windows_) {
        if (!sw->window) {
            continue;
        }
        if (!sw->ready && maru_isWindowReady(sw->window)) {
            sw->ready = true;
        }
        if (!sw->ready) {
            continue;
        }

        MARU_WindowGeometry geom = maru_getWindowGeometry(sw->window);
        if (geom.px_size.x <= 0 || geom.px_size.y <= 0) {
            continue;
        }

        if (sw->surface != VK_NULL_HANDLE) {
            if (geom.px_size.x > 0 && geom.px_size.y > 0 &&
                (sw->swapchain_rebuild || sw->wd.Width != (int)geom.px_size.x || sw->wd.Height != (int)geom.px_size.y)) {
                ImGui_ImplVulkanH_CreateOrResizeWindow(
                    instance_, physical_device_, device_, &sw->wd, queue_family_, nullptr, (int)geom.px_size.x, (int)geom.px_size.y, 2, 0);
                sw->wd.FrameIndex = 0;
                sw->swapchain_rebuild = false;
            }
        }

        renderSecondaryWindow(*sw);
    }

    (void)ctx;
}

void WindowModule::createSecondaryWindow(MARU_Context* ctx) {
    const int id = ++secondary_counter_;
    std::string title = secondary_create_.title[0] != '\0'
        ? secondary_create_.title
        : ("Secondary Window " + std::to_string(id));

    MARU_WindowCreateInfo win_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
    win_info.attributes.title = title.c_str();
    win_info.attributes.dip_size = {
        (MARU_Scalar)secondary_create_.size[0],
        (MARU_Scalar)secondary_create_.size[1]
    };
    win_info.attributes.fullscreen = secondary_create_.fullscreen;
    win_info.attributes.maximized = secondary_create_.maximized;
    win_info.attributes.resizable = secondary_create_.resizable;
    win_info.app_id = secondary_create_.app_id[0] != '\0' ? secondary_create_.app_id : nullptr;
    win_info.content_type = (MARU_ContentType)secondary_create_.content_type;
    win_info.has_decorations = secondary_create_.has_decorations;

    MARU_Window* created_window = nullptr;
    if (maru_createWindow(ctx, &win_info, &created_window) != MARU_SUCCESS) {
        std::fprintf(stderr, "Failed to create secondary window\n");
        return;
    }

    auto sw = std::make_unique<SecondaryWindow>();
    sw->window = created_window;
    sw->ready = false;
    sw->title = std::move(title);
    sw->is_resizable = secondary_create_.resizable;
    secondary_windows_.push_back(std::move(sw));
}

void WindowModule::renderSecondaryWindow(SecondaryWindow& sw) {
    if (sw.surface == VK_NULL_HANDLE) {
        if (!maru_isWindowReady(sw.window)) {
            return;
        }
        if (maru_createVkSurface(sw.window, instance_, vkGetInstanceProcAddr, &sw.surface) != MARU_SUCCESS) {
            sw.should_close = true;
            return;
        }

        sw.wd.Surface = sw.surface;
        const VkFormat request_surface_image_format[] = {
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
        };
        const VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        sw.wd.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
            physical_device_, sw.wd.Surface, request_surface_image_format, (int)IM_ARRAYSIZE(request_surface_image_format), request_surface_color_space);

        VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
        sw.wd.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(physical_device_, sw.wd.Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));

        MARU_WindowGeometry geom = maru_getWindowGeometry(sw.window);
        if (geom.px_size.x <= 0 || geom.px_size.y <= 0) {
            return;
        }
        ImGui_ImplVulkanH_CreateOrResizeWindow(
            instance_, physical_device_, device_, &sw.wd, queue_family_, nullptr, (int)geom.px_size.x, (int)geom.px_size.y, 2, 0);
        sw.vk_window_initialized = true;
    }

    ImGui_ImplVulkanH_Window* wd = &sw.wd;
    VkResult err;
    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[(int)wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[(int)wd->SemaphoreIndex].RenderCompleteSemaphore;

    err = vkAcquireNextImageKHR(device_, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        sw.swapchain_rebuild = true;
        return;
    }
    check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[(int)wd->FrameIndex];
    err = vkWaitForFences(device_, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    check_vk_result(err);

    err = vkResetFences(device_, 1, &fd->Fence);
    check_vk_result(err);

    err = vkResetCommandPool(device_, fd->CommandPool, 0);
    check_vk_result(err);

    VkCommandBufferBeginInfo cmd_info = {};
    cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &cmd_info);
    check_vk_result(err);

    wd->ClearValue.color.float32[0] = sw.clear_color.x * sw.clear_color.w;
    wd->ClearValue.color.float32[1] = sw.clear_color.y * sw.clear_color.w;
    wd->ClearValue.color.float32[2] = sw.clear_color.z * sw.clear_color.w;
    wd->ClearValue.color.float32[3] = sw.clear_color.w;

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = wd->RenderPass;
    render_pass_info.framebuffer = fd->Framebuffer;
    render_pass_info.renderArea.extent.width = (uint32_t)wd->Width;
    render_pass_info.renderArea.extent.height = (uint32_t)wd->Height;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(fd->CommandBuffer);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_acquired_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &fd->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);
    err = vkQueueSubmit(queue_, 1, &submit_info, fd->Fence);
    check_vk_result(err);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &wd->Swapchain;
    present_info.pImageIndices = &wd->FrameIndex;
    err = vkQueuePresentKHR(queue_, &present_info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        sw.swapchain_rebuild = true;
        return;
    }
    check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

void WindowModule::cleanupSecondaryWindow(SecondaryWindow& sw) {
    if (sw.surface != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        if (sw.vk_window_initialized) {
            ImGui_ImplVulkanH_DestroyWindow(instance_, device_, &sw.wd, nullptr);
            sw.vk_window_initialized = false;
        } else {
            // If ImGui Vulkan window state was never initialized, we still own the raw surface.
            vkDestroySurfaceKHR(instance_, sw.surface, nullptr);
        }
        sw.surface = VK_NULL_HANDLE;
    }
    if (sw.icon) {
        maru_destroyImage(sw.icon);
        sw.icon = nullptr;
    }
    if (sw.window) {
        maru_destroyWindow(sw.window);
        sw.window = nullptr;
    }
}

void WindowModule::render(MARU_Context* ctx, MARU_Window* window) {
    if (!enabled_) return;

    ImGui::SetNextWindowSize(ImVec2(460, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Windowing", &enabled_)) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Secondary Create Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Create Title", secondary_create_.title, sizeof(secondary_create_.title));
        ImGui::InputText("Create App ID", secondary_create_.app_id, sizeof(secondary_create_.app_id));
        ImGui::InputInt2("Create Dip Size", secondary_create_.size);
        ImGui::Checkbox("Create Decorations", &secondary_create_.has_decorations);
        ImGui::Checkbox("Create Maximized", &secondary_create_.maximized);
        ImGui::SameLine();
        ImGui::Checkbox("Create Fullscreen", &secondary_create_.fullscreen);
        ImGui::Checkbox("Create Resizable", &secondary_create_.resizable);

        const char* content_items[] = {"None", "Photo", "Video", "Game"};
        ImGui::Combo("Create Content Type", &secondary_create_.content_type, content_items, IM_ARRAYSIZE(content_items));

        if (secondary_create_.content_type < 0) secondary_create_.content_type = 0;
        if (secondary_create_.content_type > 3) secondary_create_.content_type = 3;
        if (secondary_create_.size[0] < 1) secondary_create_.size[0] = 1;
        if (secondary_create_.size[1] < 1) secondary_create_.size[1] = 1;
    }

    if (ImGui::Button("Create Secondary Window")) {
        createSecondaryWindow(ctx);
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Primary Window", ImGuiTreeNodeFlags_DefaultOpen)) {
        MARU_Window* target = primary_window_ ? primary_window_ : window;
        if (target) {
            MARU_WindowGeometry geometry = maru_getWindowGeometry(target);

            ImGui::Text("Handle: %p", (void*)target);
            ImGui::Text("Flags: %s%s%s%s",
                        maru_isWindowReady(target) ? "[Ready] " : "",
                        maru_isWindowFocused(target) ? "[Focused] " : "",
                        maru_isWindowMaximized(target) ? "[Maximized] " : "",
                        maru_isWindowFullscreen(target) ? "[Fullscreen] " : "");
            ImGui::Text("Origin:  %.1f, %.1f", geometry.dip_position.x, geometry.dip_position.y);
            ImGui::Text("Dip: %.1f x %.1f", geometry.dip_size.x, geometry.dip_size.y);
            ImGui::Text("Px:   %d x %d", geometry.px_size.x, geometry.px_size.y);

            if (ImGui::InputText("Title", primary_title_buf_, sizeof(primary_title_buf_))) {
            }
            if (ImGui::Button("Commit Title")) {
                MARU_WindowAttributes attrs = {};
                attrs.title = primary_title_buf_;
                maru_updateWindow(target, MARU_WINDOW_ATTR_TITLE, &attrs);
            }

            ImGui::InputInt2("Dip Size", primary_size_);
            if (ImGui::Button("Set Size")) {
                MARU_WindowAttributes attrs = {};
                attrs.dip_size = {(MARU_Scalar)primary_size_[0], (MARU_Scalar)primary_size_[1]};
                maru_updateWindow(target, MARU_WINDOW_ATTR_DIP_SIZE, &attrs);
            }

            bool is_max = maru_isWindowMaximized(target);
            bool is_fs = maru_isWindowFullscreen(target);
            if (ImGui::Checkbox("Maximized", &is_max)) {
                MARU_WindowAttributes attrs = {};
                attrs.maximized = is_max;
                maru_updateWindow(target, MARU_WINDOW_ATTR_MAXIMIZED, &attrs);
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Fullscreen", &is_fs)) {
                MARU_WindowAttributes attrs = {};
                attrs.fullscreen = is_fs;
                maru_updateWindow(target, MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
            }

            bool is_resizable = maru_isWindowResizable(target);
            ImGui::Text("Decorated: %s", maru_isWindowDecorated(target) ? "Yes" : "No");
            if (ImGui::Checkbox("Resizable", &is_resizable)) {
                MARU_WindowAttributes attrs = {};
                attrs.resizable = is_resizable;
                maru_updateWindow(target, MARU_WINDOW_ATTR_RESIZABLE, &attrs);
            }

#ifdef MARU_USE_FLOAT
            const ImGuiDataType scalar_type = ImGuiDataType_Float;
#else
            const ImGuiDataType scalar_type = ImGuiDataType_Double;
#endif
            ImGui::InputScalarN("Min Size", scalar_type, &primary_min_size_.x, 2, nullptr, nullptr, "%.0f");
            ImGui::InputScalarN("Max Size", scalar_type, &primary_max_size_.x, 2, nullptr, nullptr, "%.0f");
            ImGui::InputInt2("Aspect Fraction (num/den)", &primary_aspect_ratio_.num);
            if (ImGui::Button("Commit Constraints")) {
                MARU_WindowAttributes attrs = {};
                attrs.dip_min_size = primary_min_size_;
                attrs.dip_max_size = primary_max_size_;
                attrs.aspect_ratio = primary_aspect_ratio_;
                maru_updateWindow(target, MARU_WINDOW_ATTR_DIP_MIN_SIZE | MARU_WINDOW_ATTR_DIP_MAX_SIZE | MARU_WINDOW_ATTR_ASPECT_RATIO, &attrs);
            }

            if (ImGui::Button("Request Focus")) {
                maru_requestWindowFocus(target);
            }
            ImGui::SameLine();
            if (ImGui::Button("Request Frame Callback")) {
                maru_requestWindowFrame(target);
            }

            if (ImGui::Button("Toggle Test Icon")) {
                if (!test_icon_) {
                    uint32_t pixels[32 * 32];
                    for (int i = 0; i < 32 * 32; ++i) pixels[i] = 0xFFFF0000; // Red
                    
                    MARU_ImageCreateInfo img_info = {};
                    img_info.px_size = {32, 32};
                    img_info.pixels = pixels;
                    maru_createImage(const_cast<MARU_Context*>(maru_getWindowContext(target)), &img_info, &test_icon_);
                }

                MARU_WindowAttributes attrs = {};
                if (maru_getWindowIcon(target)) {
                    attrs.icon = nullptr;
                } else {
                    attrs.icon = test_icon_;
                }
                maru_updateWindow(target, MARU_WINDOW_ATTR_ICON, &attrs);
            }

        }
    }

    ImGui::Separator();
    ImGui::Text("Secondary Windows: %zu", secondary_windows_.size());
    for (size_t i = 0; i < secondary_windows_.size(); ++i) {
        auto& sw = *secondary_windows_[i];
        const bool window_ready = sw.window && (sw.ready || maru_isWindowReady(sw.window));
        if (window_ready) {
            sw.ready = true;
        }
        ImGui::PushID((int)i);
        if (ImGui::CollapsingHeader(sw.title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            char title_buf[256];
            std::strncpy(title_buf, sw.title.c_str(), sizeof(title_buf) - 1);
            title_buf[sizeof(title_buf) - 1] = '\0';
            if (ImGui::InputText("Title", title_buf, sizeof(title_buf))) {
                sw.title = title_buf;
            }

            if (sw.window && window_ready) {
                MARU_WindowGeometry geometry = maru_getWindowGeometry(sw.window);
                ImGui::Text("Handle: %p", (void*)sw.window);
                ImGui::Text("Flags: %s%s%s%s",
                            maru_isWindowReady(sw.window) ? "[Ready] " : "",
                            maru_isWindowFocused(sw.window) ? "[Focused] " : "",
                            maru_isWindowMaximized(sw.window) ? "[Maximized] " : "",
                            maru_isWindowFullscreen(sw.window) ? "[Fullscreen] " : "");
                ImGui::Text("Origin:  %.1f, %.1f", geometry.dip_position.x, geometry.dip_position.y);
                ImGui::Text("Dip: %.1f x %.1f", geometry.dip_size.x, geometry.dip_size.y);
                ImGui::Text("Px:   %d x %d", geometry.px_size.x, geometry.px_size.y);
            } else if (sw.window) {
                ImGui::Text("Handle: %p", (void*)sw.window);
                ImGui::TextUnformatted("State: Waiting for WINDOW_READY");
            }

            bool is_max = sw.window ? maru_isWindowMaximized(sw.window) : false;
            bool is_fs = sw.window ? maru_isWindowFullscreen(sw.window) : false;
            if (ImGui::Checkbox("Maximized", &is_max) && sw.window && window_ready) {
                MARU_WindowAttributes attrs = {};
                attrs.maximized = is_max;
                maru_updateWindow(sw.window, MARU_WINDOW_ATTR_MAXIMIZED, &attrs);
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Fullscreen", &is_fs) && sw.window && window_ready) {
                MARU_WindowAttributes attrs = {};
                attrs.fullscreen = is_fs;
                maru_updateWindow(sw.window, MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
            }

            if (sw.window) {
                sw.is_resizable = maru_isWindowResizable(sw.window);
            }

            ImGui::Text("Decorated: %s", (sw.window && maru_isWindowDecorated(sw.window)) ? "Yes" : "No");
            if (ImGui::Checkbox("Resizable", &sw.is_resizable) && sw.window && window_ready) {
                MARU_WindowAttributes attrs = {};
                attrs.resizable = sw.is_resizable;
                maru_updateWindow(sw.window, MARU_WINDOW_ATTR_RESIZABLE, &attrs);
            }

#ifdef MARU_USE_FLOAT
            const ImGuiDataType scalar_type = ImGuiDataType_Float;
#else
            const ImGuiDataType scalar_type = ImGuiDataType_Double;
#endif
            ImGui::InputScalarN("Min Size", scalar_type, &sw.dip_min_size.x, 2, nullptr, nullptr, "%.0f");
            ImGui::InputScalarN("Max Size", scalar_type, &sw.dip_max_size.x, 2, nullptr, nullptr, "%.0f");
            ImGui::InputInt2("Aspect Fraction (num/den)", &sw.aspect_ratio.num);

            if (ImGui::Button("Commit") && sw.window && window_ready) {
                MARU_WindowAttributes attrs = {};
                attrs.title = sw.title.c_str();
                attrs.dip_min_size = sw.dip_min_size;
                attrs.dip_max_size = sw.dip_max_size;
                attrs.aspect_ratio = sw.aspect_ratio;
                maru_updateWindow(sw.window, MARU_WINDOW_ATTR_TITLE | MARU_WINDOW_ATTR_DIP_MIN_SIZE | MARU_WINDOW_ATTR_DIP_MAX_SIZE | MARU_WINDOW_ATTR_ASPECT_RATIO, &attrs);
            }

            ImGui::ColorEdit4("Clear Color", (float*)&sw.clear_color);

            if (ImGui::Button("Request Focus") && sw.window && window_ready) {
                maru_requestWindowFocus(sw.window);
            }
            ImGui::SameLine();
            if (ImGui::Button("Request Frame Callback") && sw.window && window_ready) {
                maru_requestWindowFrame(sw.window);
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                sw.should_close = true;
            }

        }
        ImGui::PopID();
    }

    ImGui::End();
}
