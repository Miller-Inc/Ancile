#include "App.h"
#include <functional>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include <utility>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <thread>
#include "NetParser.h"

namespace Ancile {
    void Application::Main(const int argc, char **argv, const std::function<void(Application*, int, char**)>& runtimeProcess ) {
        if (runtimeProcess != nullptr) {
            runtimeProcess(this, argc, argv);
        }
    }

    Application::Application(std::string  windowTitle, const int width, const int height)
        : m_Title(std::move(windowTitle)), m_Width(width), m_Height(height) {

        if (!InitSDL() || !InitVulkan() || !InitImGui()) {
            throw std::runtime_error("Ancile Framework failed to initialize.");
        }
    }

    Application::~Application() {
        Cleanup();
    }

    bool Application::InitSDL() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
            return false;
        }

        m_Window = SDL_CreateWindow(
            m_Title.c_str(),
            m_Width, m_Height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
        );

        if (!m_Window) {
            std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << "\n";
            return false;
        }
        return true;
    }

    bool Application::InitVulkan() {
        // 1. Get SDL Vulkan extensions
        uint32_t extensionsCount = 0;
        const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionsCount);
        std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionsCount);

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Ancile";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Ancile Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3; // Request Vulkan 1.3

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&createInfo, m_Allocator, &m_Instance) != VK_SUCCESS) return false;

        // 2. Select Physical Device
        uint32_t gpuCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &gpuCount, nullptr);
        if (gpuCount == 0) return false;
        std::vector<VkPhysicalDevice> gpus(gpuCount);
        vkEnumeratePhysicalDevices(m_Instance, &gpuCount, gpus.data());
        m_PhysicalDevice = gpus[0];

        // 3. Find Queue Family index
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_QueueFamily = i;
                break;
            }
        }
        if (m_QueueFamily == 0xFFFFFFFF) return false;

        // 4. Create Logical Device with Dynamic Rendering features
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = m_QueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &features13;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, m_Allocator, &m_Device) != VK_SUCCESS) return false;
        vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);

        // 5. Load ImGui Vulkan functions with API version 1.3
        ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
            const auto instance = static_cast<VkInstance>(user_data);
            return vkGetInstanceProcAddr(instance, function_name);
        }, m_Instance);

        // 6. Create Surface
        if (!SDL_Vulkan_CreateSurface(m_Window, m_Instance, m_Allocator, &m_Surface)) return false;

        // 7. Setup Descriptor Pool
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }
        };
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
        poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
        poolInfo.pPoolSizes = poolSizes;
        if (vkCreateDescriptorPool(m_Device, &poolInfo, m_Allocator, &m_DescriptorPool) != VK_SUCCESS) return false;

        // 8. Create Command Pool
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = m_QueueFamily;
        if (vkCreateCommandPool(m_Device, &cmdPoolInfo, m_Allocator, &m_CommandPool) != VK_SUCCESS) return false;

        // 9. Initial Swapchain Creation
        RecreateSwapChain();

        return true;
    }

    bool Application::InitImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_Window);

        static VkFormat color_format = VK_FORMAT_B8G8R8A8_UNORM;

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = m_Instance;
        initInfo.PhysicalDevice = m_PhysicalDevice;
        initInfo.Device = m_Device;
        initInfo.QueueFamily = m_QueueFamily;
        initInfo.Queue = m_Queue;
        initInfo.DescriptorPool = m_DescriptorPool;
        initInfo.MinImageCount = m_MinImageCount;
        initInfo.ImageCount = m_MinImageCount;

        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_format;

        return ImGui_ImplVulkan_Init(&initInfo);
    }

    void Application::HandleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                m_Running = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window)) {
                m_Running = false;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                m_SwapChainRebuild = true;
            }
        }
    }

    void Application::AddProcess(const std::shared_ptr<Process> &process) {
        if (!m_Running) Processes.emplace(process);
        else {
            ProcController.lock();
            newProcesses.emplace(process);
            ProcController.unlock();
        }
    }

    void Application::RemoveProcess(const std::shared_ptr<Process> &process) {
        if (!m_Running) Processes.erase(process);
        else {
            ProcController.lock();
            removeProcList.emplace(process);
            ProcController.unlock();
        }
    }

    void Application::SetControlThread(const std::function<void(Application *, int, char **)> &Function) {
        ControlThreadFunction = Function;
    }

    void Application::ClearControlThread() {
        ControlThreadFunction = nullptr;
    }

    void Application::Run(const int argc, char** argv) {
        MainThread = std::thread([&] {
            Main(argc, argv, ControlThreadFunction);
        });

        RenderThread();

        MainThread.join();
    }

    void Application::Wait() {
        if (MainThread.joinable()) {
            MainThread.join();
        }
    }

    void Application::RenderThread() {
        for (const auto& customProcess : Processes) {
            if (customProcess) {
                customProcess->BeginPlay();
            }
        }

        uint64_t lastTime = SDL_GetTicks();

        while (m_Running) {
            HandleEvents();

            if (m_SwapChainRebuild) {
                RecreateSwapChain();
            }

            uint64_t currentTime = SDL_GetTicks();
            float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;

            if (deltaTime > 0.25f) deltaTime = 0.25f;

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            for (const auto& customProcess : Processes) {
                if (customProcess) {
                    customProcess->Tick(deltaTime);
                }
            }

            ImGui::Render();
            RenderFrame();

            ProcController.lock();
            for (const auto& process : newProcesses) {
                if (process) {
                    Processes.emplace(process);
                    process->BeginPlay();
                }
            }
            for (const auto& process : removeProcList) {
                if (process) {
                    Processes.erase(process);
                }
            }
            newProcesses.clear();
            removeProcList.clear();
            ProcController.unlock();
        }
    }

    void Application::RecreateSwapChain() {
        vkDeviceWaitIdle(m_Device);
        CleanupSwapChain();

        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(m_Window, &width, &height);
        if (width <= 0 || height <= 0) return;

        m_Width = width;
        m_Height = height;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;
        createInfo.minImageCount = m_MinImageCount;
        createInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        createInfo.imageExtent = { static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height) };
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(m_Device, &createInfo, m_Allocator, &m_SwapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan Swapchain!");
        }

        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
        m_SwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

        m_SwapChainImageViews.resize(imageCount);
        for (size_t i = 0; i < imageCount; i++) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_SwapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            vkCreateImageView(m_Device, &viewInfo, m_Allocator, &m_SwapChainImageViews[i]);
        }

        m_SwapChainRebuild = false;
    }

    void Application::CleanupSwapChain() {
        for (auto imageView : m_SwapChainImageViews) {
            vkDestroyImageView(m_Device, imageView, m_Allocator);
        }
        m_SwapChainImageViews.clear();

        if (m_SwapChain) {
            vkDestroySwapchainKHR(m_Device, m_SwapChain, m_Allocator);
            m_SwapChain = VK_NULL_HANDLE;
        }
    }

    void Application::RenderFrame() {
        if (!m_SwapChain) return;

        // Acquire image
        VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &m_CurrentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            m_SwapChainRebuild = true;
            return;
        }

        ImDrawData* draw_data = ImGui::GetDrawData();

        // Allocate transient command buffer
        VkCommandBuffer command_buffer;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(m_Device, &allocInfo, &command_buffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(command_buffer, &beginInfo);

        // Transition image layout to COLOR_ATTACHMENT_OPTIMAL
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_SwapChainImages[m_CurrentImageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Dynamic rendering attachment
        VkClearValue clearValue = { {{ 0.1f, 0.1f, 0.1f, 1.0f }} };
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = m_SwapChainImageViews[m_CurrentImageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearValue;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { {0, 0}, { (uint32_t)m_Width, (uint32_t)m_Height } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(command_buffer, &renderingInfo);
        ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
        vkCmdEndRendering(command_buffer);

        // Transition image layout to PRESENT_SRC_KHR
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(command_buffer);

        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &command_buffer;
        vkQueueSubmit(m_Queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_Queue);

        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &command_buffer);

        // Present frame
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_SwapChain;
        presentInfo.pImageIndices = &m_CurrentImageIndex;
        vkQueuePresentKHR(m_Queue, &presentInfo);
    }

    void Application::Cleanup() {
        if (m_Device) vkDeviceWaitIdle(m_Device);

        CleanupSwapChain();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        if (m_CommandPool) vkDestroyCommandPool(m_Device, m_CommandPool, m_Allocator);
        if (m_DescriptorPool) vkDestroyDescriptorPool(m_Device, m_DescriptorPool, m_Allocator);
        if (m_Surface) vkDestroySurfaceKHR(m_Instance, m_Surface, m_Allocator);
        if (m_Device) vkDestroyDevice(m_Device, m_Allocator);
        if (m_Instance) vkDestroyInstance(m_Instance, m_Allocator);

        if (m_Window) SDL_DestroyWindow(m_Window);
        SDL_Quit();
    }
}
