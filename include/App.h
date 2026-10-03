#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vulkan/vulkan.h>
#include <unordered_set>
#include <vector>
#include "imgui.h"

// Forward declarations to avoid exposing massive dependency headers everywhere
struct SDL_Window;

namespace Ancile {

    // Your requested Process interface
    class Process {
    public:
        virtual ~Process() = default;
        virtual void BeginPlay() {}
        virtual void Tick(float deltaTime) {}
    };

    class Application {
    public:
        Application(std::string  windowTitle, int width, int height);
        ~Application();

        // Disallow copying to protect unique Vulkan/SDL handles
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void AddProcess(const std::shared_ptr<Process>& process);
        void RemoveProcess(const std::shared_ptr<Process>& process);
        void SetControlThread(const std::function<void(Application*, int, char **)> &Function);
        void ClearControlThread();
        void Run(int argc, char** argv);
        void Wait();

    private:
        void Main(int argc, char **argv, const std::function<void(Application*, int, char **)> &runtimeProcess);
        void RenderThread();

        bool InitSDL();
        bool InitVulkan();
        bool InitImGui();
        void Cleanup();

        void HandleEvents();
        void RenderFrame();
        void RecreateSwapChain();

        void CleanupSwapChain();

        std::unordered_set<std::shared_ptr<Process>> Processes;
        std::thread MainThread;
        std::function<void(Application*, int, char **)> ControlThreadFunction{};

        // Configuration Window Options
        std::string m_Title;
        int m_Width;
        int m_Height;
        bool m_Running = true;
        bool m_SwapChainRebuild = false;

        // Core Platform Pointers
        SDL_Window* m_Window = nullptr;

        // Core Vulkan Handles
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        uint32_t m_QueueFamily = 0xFFFFFFFF;
        VkQueue m_Queue = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        // Simplified Swapchain State
        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
        VkAllocationCallbacks* m_Allocator = nullptr;
        int m_MinImageCount = 2;
        std::vector<VkImage> m_SwapChainImages;
        std::vector<VkImageView> m_SwapChainImageViews;
        uint32_t m_CurrentImageIndex = 0;

        std::mutex ProcController{};
        std::unordered_set<std::shared_ptr<Process>> newProcesses;
        std::unordered_set<std::shared_ptr<Process>> removeProcList;
    };
}
