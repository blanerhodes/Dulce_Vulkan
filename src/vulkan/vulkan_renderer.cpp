#include "vulkan_renderer.h"

/* NOTE: the following variables I believe can be wiped in the transient arena
    - available extensions (only used for querying)
    - required extensions if i make it dynamic later
    - required validation layers if i make it dynamic later
    - queue_properties (only used for querying)
*/

/* NOTE: the following variables I believe will need to stick around (maybe not the whole alloc though, just one thats picked)
    - swapchain formats
    - swapchain present_modes

*/

VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*  userData) {
        char* message = (char*)(callbackData->pMessage);
        switch(messageSeverity){
            default:
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                DERROR(message);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                DWARN(message);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                DINFO(message);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                DTRACE(message);
                break;
        }
    return VK_FALSE;
}

static void VulkanCreateInstance(VulkanContext* context);
static void VulkanCreateDebugger(VulkanContext* context);
static b8 VulkanCreateSurface(VulkanContext* context, Win32StateHandles state_handles);
static VulkanQueueFamilyInfo VulkanFindQueueFamilies(VulkanContext* context, VkPhysicalDevice device);
static b8 VulkanIsDeviceSuitable(VulkanContext* context, VkPhysicalDevice device);
static void VulkanSelectDevice(VulkanContext* context);
static void VulkanCreateLogicalDevice(VulkanContext* context);
static b8 VulkanQuerySwapchainSupport(VulkanContext* context);
static VkSurfaceFormatKHR VulkanChooseSwapSurfaceFormat(VulkanContext* context) {

static void VulkanRendererInitialize(Win32StateHandles state_handles, VulkanContext* context) {
    VulkanCreateInstance(context);
    VulkanCreateDebugger(context);
    VulkanCreateSurface(context, state_handles);
    VulkanSelectDevice(context);
    VulkanCreateLogicalDevice(context);
}

static void VulkanRendererShutdown(VulkanContext* context) {
    vkDestroyDevice(context->logical_device, NULL);

    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT");
    func(context->instance, context->debug_messenger, NULL);
    vkDestroySurfaceKHR(context->instance, context->surface, NULL);
    vkDestroyInstance(context->instance, NULL);
    DDEBUG("Vulkan shutdown complete.");
}

static void VulkanCreateInstance(VulkanContext* context) {
    DINFO("Creating Vulkan instance...");
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = "Hello triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

    //TODO: make extension gathering more flexible and disable debug ones on non-debug builds
    const u32 required_extension_count = 3;
    char* required_extensions[required_extension_count] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        "VK_KHR_win32_surface",
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    create_info.enabledExtensionCount = required_extension_count;
    create_info.ppEnabledExtensionNames = required_extensions;

    //TODO: do more robust required layer checking by enumerating what is available vs what is required (vkEnumerateInstanceLayerProperties)
    const u32 required_validation_layers_count = 1;
    char* required_validation_layers[required_validation_layers_count] = {
        "VK_LAYER_KHRONOS_validation"
    };
    create_info.enabledLayerCount = required_validation_layers_count;
    create_info.ppEnabledLayerNames = required_validation_layers;
    
    VK_CHECK(vkCreateInstance(&create_info, NULL, &context->instance));
    DINFO("Vulkan instance created.");
}

static void VulkanCreateDebugger(VulkanContext* context) {
    DDEBUG("Creating Vulkan debugger...");
    u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = vkDebugCallback;

    //gotta do this for access to extension functions
    PFN_vkCreateDebugUtilsMessengerEXT create_vk_debugger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");
    DASSERT_MSG(create_vk_debugger, "Failed to create debug messenger!");
    VK_CHECK(create_vk_debugger(context->instance, &debug_create_info, NULL, &context->debug_messenger));
    DDEBUG("Vulkan debugger created.");
}

static b8 VulkanCreateSurface(VulkanContext* context, Win32StateHandles state_handles) {
    VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    create_info.hinstance = state_handles.hInstance;
    create_info.hwnd = state_handles.hwnd;

    //TODO: maybe have temp surface variable to i dont assign before checking a good result
    VkResult result = vkCreateWin32SurfaceKHR(context->instance, &create_info, NULL, &context->surface);
    if (result != VK_SUCCESS) {
        DFATAL("Vulkan surface creation failed.");
        return false;
    }
    return true;
}

static VulkanQueueFamilyInfo VulkanFindQueueFamilies(VulkanContext* context, VkPhysicalDevice device) {
    VulkanQueueFamilyInfo result = {};
    result.graphics_family_index = -1;
    result.present_family_index = -1;

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_properties =
        (VkQueueFamilyProperties*)ArenaReserveBytes(&context->transient_memory, sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_properties);

    for (u32 i = 0; i < queue_family_count; i++) {
        if (queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            result.graphics_family_index = i;
        }
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, context->surface, &present_support);
        if (present_support) {
            result.present_family_index = i;
        }
    }
    return result;
}
//TODO: bundle device requirements because they're all over the place
static b8 VulkanIsDeviceSuitable(VulkanContext* context, VkPhysicalDevice device) {
    char* required_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; 
    VulkanQueueFamilyInfo queue_info = VulkanFindQueueFamilies(context, device);

    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    VkExtensionProperties* extensions = 
        (VkExtensionProperties*)ArenaReserveBytes(&context->transient_memory, sizeof(VkExtensionProperties) * extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions);

    for (u32 i = 0; i < ArrayCount(required_extensions); i++) {
        b8 required_extension_present = false;
        for (u32 j = 0; j < extension_count && !required_extension_present; j++) {
            required_extension_present = StringsEqual(required_extensions[i], extensions[j].extensionName);
        }
        if (!required_extension_present) {
            DFATAL("VulkanIsDeviceSuitable - Required extension %s not available.", required_extensions[i]);
            return false;
        }
    }

    //TODO: need to rework this stuff to not be setting values in the context before the values are confirmed to be valid
    b8 swapchain_sufficient = VulkanQuerySwapchainSupport(context);
    //NOTE: have to query swapchain after querying extensions
    if (context->queue_family_info.graphics_family_index != -1    && 
            context->queue_family_info.present_family_index != -1 &&
            swapchain_sufficient) {
        context->queue_family_info = queue_info;
        return true;
    }
    return false;
}

//TODO: maybe picking the device should be lumped into creating the logical device
static void VulkanSelectDevice(VulkanContext* context) {
    context->physical_device = VK_NULL_HANDLE;

    u32 device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &device_count, NULL));
    if (device_count == 0) {
        DERROR("VulkanCreateDevice - No Vulkan supported device found.");
        return;
    }

    const u32 max_device_count = 32;
    VkPhysicalDevice devices[max_device_count] = {};
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &device_count, devices));

    for (u32 i = 0; i < device_count; i++) {
        if (VulkanIsDeviceSuitable(context, devices[i])) {
            context->physical_device = devices[i];
            break;
        }
    }
    if (context->physical_device == VK_NULL_HANDLE) {
        DFATAL("VulkanSelectDevice - Could not find suitable physical device.");
        return;
    }
}

static void VulkanCreateLogicalDevice(VulkanContext* context) {
    //TODO: need to handle queues being on the same index instead of always considering them separate
    //Graphics queue
    VkDeviceQueueCreateInfo graphics_queue_create_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    graphics_queue_create_info.queueFamilyIndex = context->queue_family_info.graphics_family_index;
    graphics_queue_create_info.queueCount = 1;
    f32 queue_priority = 1.0f;
    graphics_queue_create_info.pQueuePriorities = &queue_priority;

    //Present queue
    VkDeviceQueueCreateInfo present_queue_create_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    present_queue_create_info.queueFamilyIndex = context->queue_family_info.present_family_index;
    present_queue_create_info.queueCount = 1;
    present_queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {};

    VkDeviceQueueCreateInfo queue_create_infos[2] = {graphics_queue_create_info, present_queue_create_info};

    VkDeviceCreateInfo create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.queueCreateInfoCount = ArrayCount(queue_create_infos);
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = 1;
    char* extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    create_info.ppEnabledExtensionNames = &extension_names;

    //TODO: maybe worry about setting layer count and validation layers later for old vulkan implementations

    VK_CHECK(vkCreateDevice(context->physical_device, &create_info, NULL, &context->logical_device));

    DINFO("Logical device created.");

    vkGetDeviceQueue(context->logical_device, context->queue_family_info.graphics_family_index, 0, &context->graphics_queue);
    vkGetDeviceQueue(context->logical_device, context->queue_family_info.present_family_index, 0, &context->present_queue);
}

static b8 VulkanQuerySwapchainSupport(VulkanContext* context) {
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->surface,
                                              &context->swapchain_support.capabilities));
    //TODO: do the format and present mode allocations need to be part of permanent memory?
    u32 format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, NULL));

    u32 present_mode_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->surface, &present_mode_count, NULL));

    //TODO: does this need to be fatal
    if (!format_count || !present_mode_count) {
        DFATAL("VulkanQuerySwapchainSupport - Swapchain support not sufficient. Aborting.");
        return false;
    }

    context->swapchain_support.formats = 
        (VkSurfaceFormatKHR*)ArenaReserveBytes(&context->transient_memory,
                                                sizeof(VkSurfaceFormatKHR) * format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, context->swapchain_support.formats));
    context->swapchain_support.format_count = format_count;
    context->swapchain_support.present_modes =
        (VkPresentModeKHR*)ArenaReserveBytes(&context->transient_memory,
                                             sizeof(VkPresentModeKHR) * present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device,
                                                       context->surface,
                                                       &present_mode_count,
                                                       context->swapchain_support.present_modes));
    context->swapchain_support.present_mode_count = present_mode_count;
    return true;;
}

static VkSurfaceFormatKHR VulkanChooseSwapSurfaceFormat(VulkanContext* context) {
    VkSurfaceFormatKHR* formats = context->swapchain_support.formats;
    for (u32 i = 0; i < context->swapchain_support.format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[i];
        }
    }
    //TODO: work in picking based on how "good" the format is but in most cases the first one is fine
    return formats[0];
}

/* NOTE: 4 possible modes
    - VK_PRESENT_MODE_IMMEDIATE_KHR: images submitted by app are transferred to the screen right away, may cause tearing
    - VK_PRESENT_MODE_FIFO_KHR: images pulled off the swapchain queue and new images are pushed in the back 
                                if the queue is full then the program has to wait
                                similar to vertical sync, "vertical blank" is the moment the display is refreshed
                                NOTE: this is the only one guaranteed to exist
    - VK_PRESENT_MODE_FIFO_RELAXED_KHR: only different from the previous in that if the app transfer is late
                                        and the queue is emtpy it transfers to the screen right away instead
                                        of waiting for the next vertical blank, can cause tearing
    - VK_PRESENT_MODE_MAILBOX_KHR: variation of 2nd mode but instead of blocking on a full queue the images
                                   in the queue are replaced with the newer ones
                                   can be used to render as fast as possible without tearing (aka triple buffering)
*/
static VkPresentModeKHR VulkanChooseSwapPresentMode(VulkanContext* context) {
    VkPresentModeKHR* present_modes = context->swapchain_
}