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

struct VulkanSwapchain {

};

struct VulkanContext {
    u32 frame_buffer_width;
    u32 frame_buffer_height;
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VulkanSwapchainSupportInfo swapchain_support;
    VulkanQueueFamilyInfo queue_family_info;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSurfaceFormatKHR swapchain_image_format;
    VkExtent2D swapchain_extent;
    VkSwapchainKHR swapchain;
    u32 swapchain_image_count;
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;

    VkDebugUtilsMessengerEXT debug_messenger;

    //TODO: possibly revisit this and remove it from the context out to a standalone memory buffer
    MemoryArena transient_memory;
    //MemoryArena permanent_memory;
};