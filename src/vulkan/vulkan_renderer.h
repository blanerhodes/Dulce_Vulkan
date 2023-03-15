#pragma once
#include <vulkan/vulkan_win32.h>

struct Win32StateHandles {
    HINSTANCE hInstance;
    HWND hwnd;
};

struct VulkanQueueFamilyInfo {
    u32 graphics_family_index;
    u32 present_family_index;
};

struct VulkanSwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    VkSurfaceFormatKHR* formats;
    u32 present_mode_count;
    VkPresentModeKHR* present_modes;
};

struct VulkanContext {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VulkanSwapchainSupportInfo swapchain_support;
    VulkanQueueFamilyInfo queue_family_info;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkDebugUtilsMessengerEXT debug_messenger;

    //TODO: possibly revisit this and remove it from the context out to a standalone memory buffer
    MemoryArena transient_memory;
};