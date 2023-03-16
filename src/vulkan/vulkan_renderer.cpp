#include "vulkan/vulkan_renderer.h"
#include "platform_services.h"
/* NOTE: the following variables I believe can be wiped in the transient arena
    - available extensions (only used for querying)
    - required extensions if i make it dynamic later
    - required validation layers if i make it dynamic later
    - queue_properties (only used for querying)
*/

/* NOTE: the following variables I believe will need to stick around (maybe not the whole alloc though, just one thats picked)
    - swapchain formats
    - swapchain present_modes
    - swapchain images
    - swapchain image views
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
static void VulkanCreateSwapchain(VulkanContext* context);
static b8 VulkanQuerySwapchainSupport(VulkanContext* context, VkPhysicalDevice device);
static VkSurfaceFormatKHR VulkanChooseSwapSurfaceFormat(VulkanContext* context);
static VkPresentModeKHR VulkanChooseSwapPresentMode(VulkanContext* context);
static VkExtent2D VulkanChooseSwapExtent(VulkanContext* context);
static void VulkanCreateImageViews(VulkanContext* context);
static void VulkanCreateGraphicsPipeline(VulkanContext* context);

static void VulkanRendererInitialize(Win32StateHandles state_handles, VulkanContext* context) {
    VulkanCreateInstance(context);
    VulkanCreateDebugger(context);
    VulkanCreateSurface(context, state_handles);
    VulkanSelectDevice(context);
    VulkanCreateLogicalDevice(context);
    VulkanCreateSwapchain(context);
    VulkanCreateImageViews(context);
}

static void VulkanRendererShutdown(VulkanContext* context) {
    for (u32 i = 0; i < context->swapchain_image_count; i++) {
        vkDestroyImageView(context->logical_device, context->swapchain_image_views[i], context->allocator);
    }
    vkDestroySwapchainKHR(context->logical_device, context->swapchain, 0);
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
    DINFO("Vulkan surface created.")
    return true;
}

static VulkanQueueFamilyInfo VulkanFindQueueFamilies(VulkanContext* context, VkPhysicalDevice device) {
    VulkanQueueFamilyInfo result = {};
    result.graphics_family_index = -1;
    result.present_family_index = -1;

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_properties = 
        PushArray(&context->transient_memory, queue_family_count, VkQueueFamilyProperties);
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
        PushArray(&context->transient_memory, extension_count, VkExtensionProperties);
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
    //NOTE: have to query swapchain after querying extensions
    b8 swapchain_sufficient = VulkanQuerySwapchainSupport(context, device);
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
    DINFO("Vulkan device selected.");
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

static void VulkanCreateSwapchain(VulkanContext* context) {
    VulkanQuerySwapchainSupport(context, context->physical_device);
    /*NOTE: need the next 3 functions for swapchain settings
        - Surface format (color depth)
        - Presentation mode (conditions for swapping the images to the screen)
        - Swap extent (resolution of images in the swapchain)
    */
    VkSurfaceFormatKHR surface_format = VulkanChooseSwapSurfaceFormat(context);
    VkPresentModeKHR present_mode = VulkanChooseSwapPresentMode(context);
    VkExtent2D extent = VulkanChooseSwapExtent(context);

    context->swapchain_image_format = surface_format;
    context->swapchain_extent = extent;

    VulkanSwapchainSupportInfo* swapchain_info = &context->swapchain_support;
    u32 image_count = swapchain_info->capabilities.minImageCount + 1;
    if (swapchain_info->capabilities.maxImageCount > 0 && image_count > swapchain_info->capabilities.maxImageCount) {
        image_count = swapchain_info->capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create_info.surface = context->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1; //NOTE: always 1 unless doing stereoscopic 3D, so never
    /*NOTE: imageUsage specs what ops we'll use the images in the swapchain for
            since right now we render directly to them, we use color attachment.
            Can render to a separate image first to perform ops like post processing with VK_IMAGE_USAGE_TRANSFER_DST_BIT
            and use a memory op to transfer the rendered image to a swapchain image
    */
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; 

    /*NOTE: drawing from the graphics queue then submitting on the presentation queue
            - VK_SHARING_MODE_EXCLUSIVE: image is owned by one queue family at a time and ownership must be
                explicitly transferred, best for performance
            - VK_SHARING_MODE_CONCURRENT: iamges can be used across queue families without explicity transfers
    */
    if (context->queue_family_info.graphics_family_index != context->queue_family_info.present_family_index) {
        u32 queue_family_indices[] {context->queue_family_info.graphics_family_index, context->queue_family_info.present_family_index};
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = 0;
    }
    //NOTE: specify a transform that should be applied to all images in a swapchain, currentTransform == no transform
    create_info.preTransform = swapchain_info->capabilities.currentTransform;
    //NOTE: almost always ignore since this is for blending with other windows in the window system
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    /*NOTE: This is for if the swapchain becomes invalid or unoptimized like with a window resize.
            If this happens then the swapchain needs to be recreated and needs a ref to the old swapchain
    */
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(context->logical_device, &create_info, NULL, &context->swapchain));

    u32 swapchain_image_count;
    vkGetSwapchainImagesKHR(context->logical_device, context->swapchain, &swapchain_image_count, NULL);
    context->swapchain_images = PushArray(&context->transient_memory, swapchain_image_count, VkImage);
    vkGetSwapchainImagesKHR(context->logical_device, context->swapchain, &swapchain_image_count, context->swapchain_images);
    context->swapchain_image_count = swapchain_image_count;
    DINFO("Swapchain created.");
}

static b8 VulkanQuerySwapchainSupport(VulkanContext* context, VkPhysicalDevice device) {
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, context->surface,
                                              &context->swapchain_support.capabilities));
    //TODO: do the format and present mode allocations need to be part of permanent memory?
    u32 format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, context->surface, &format_count, NULL));

    u32 present_mode_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, context->surface, &present_mode_count, NULL));

    //TODO: does this need to be fatal
    if (!format_count || !present_mode_count) {
        DFATAL("VulkanQuerySwapchainSupport - Swapchain support not sufficient. Aborting.");
        return false;
    }

    context->swapchain_support.formats = PushArray(&context->transient_memory, format_count, VkSurfaceFormatKHR);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, context->surface, &format_count, context->swapchain_support.formats));
    context->swapchain_support.format_count = format_count;
    context->swapchain_support.present_modes =
        PushArray(&context->transient_memory, present_mode_count, VkPresentModeKHR);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device,
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
    VkPresentModeKHR* present_modes = context->swapchain_support.present_modes;
    for (u32 i = 0; i < context->swapchain_support.present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_modes[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

/* NOTE: swap extent is the resolution of the swap chain images (almost always exactly the resolution of the window we draw into in pixels)
         range of possible resolutions is defined in VkSurfaceCapabilitiesKHR

*/
static VkExtent2D VulkanChooseSwapExtent(VulkanContext* context) {
    VkSurfaceCapabilitiesKHR* capabilities = &context->swapchain_support.capabilities;
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    } else {
        VkExtent2D min = capabilities->minImageExtent;
        VkExtent2D max = capabilities->maxImageExtent;
        VkExtent2D actual_extent = {context->frame_buffer_width, context->frame_buffer_height};
        actual_extent.width = DCLAMP(actual_extent.width, min.width, max.width);
        actual_extent.height = DCLAMP(actual_extent.height, min.height, max.height);
        return actual_extent;
    }
}

/*NOTE: need a VkImageView to actually use any VkImage, even in the swapchain
        Describes how to access the image and which part of it to access
*/
static void VulkanCreateImageViews(VulkanContext* context) {
    context->swapchain_image_views = PushArray(&context->transient_memory,
                                               context->swapchain_image_count,
                                               VkImageView);
    for (u32 i = 0; i < context->swapchain_image_count; i++) {
        VkImageViewCreateInfo create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        create_info.image = context->swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = context->swapchain_image_format.format;
        //NOTE: the components are how the image values should be mapped (like you can map all channels to red for a monochrome texture)
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        //NOTE: subresourceRange describes the image's purpose and which part should be accessed
        //currently the below means the images are color targets w/o any mipmapping levels or multiple layers
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        //NOTE: if you do stereographic 3D then you would make a swapchain with multiple layers for multiple image views
        //for each image for the left and right eyes
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(context->logical_device, &create_info, NULL, &context->swapchain_image_views[i]));

    }
}

static VkShaderModule VulkanCreateShaderModule(VulkanContext* context, DebugReadFileResult shader_code) {
    VkShaderModuleCreateInfo create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = shader_code.contents_size;
    create_info.pCode = (u32*)shader_code.contents;

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(context->logical_device, &create_info, context->allocator, &shader_module));
    return shader_module;
}

static void VulkanCreateGraphicsPipeline(VulkanContext* context) {
    DebugReadFileResult vert_shader_code = DebugPlatformReadEntireFile("assets/shaders/starter.vert.glsl");
    DebugReadFileResult frag_shader_code = DebugPlatformReadEntireFile("assets/shaders/starter.frag.glsl");

    VkShaderModule vert_shader_module = VulkanCreateShaderModule(context, vert_shader_code);
    VkShaderModule frag_shader_module = VulkanCreateShaderModule(context, frag_shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";
    /*NOTE: SpecializationInfo is for specifying values for shader constants so allow the shader's behavior
            to be configured at pipeline creation by giving different values to those constants.
            This is more efficient than configuring the shader with variables at render time
    */
    vert_shader_stage_info.pSpecializationInfo = NULL;

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    frag_shader_stage_info.pSpecializationInfo = NULL;

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    /*NOTE: Specifying values in dynamic state means they will be excluded from the preset configuration
      and must be specified at drawing time. Viewport and scissor state are common to put here. 
    */    
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = ArrayCount(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    /*NOTE: Describes the format of the vertex data that is passed to the vertex shader in 2 ways
        - Bindings: Spacing between data and if the data is per-vertex or per-instance
        - Attribute descriptions: Type of attrs passed to vertex shader, which binding to load them from
                                  and at which offset
    */
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = NULL;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = NULL;

    /*NOTE: Describes what kind of geometry will be drawn and enable primitiveRestart to break up
            lines and triangles into a _STRIP topology
    */
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    /*NOTE: Describes the region of the framebuffer that the output will be rendered to.
            Defines the transformation from the image to the framebuffer.
            Want to use the swapchain extent since the swapchain image size can differ from the window
            width/height. Stick to swapchain size since the swapchain will be used as framebuffers
    */
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32) context->swapchain_extent.width;
    viewport.height = (f32) context->swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    /*NOTE: Defines in which regions pixels will actually be stored. Any pixels outside the scissor
            will be discarded by the rasterizer. Basically filters out what's outside of it.
    */
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = context->swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    /*NOTE: set these for non-dynamic state
    viewport_state.pViewports = &viewports;
    viewport_state.pScissors = &scissors
    */

    vkDestroyShaderModule(context->logical_device, vert_shader_module, context->allocator);
    vkDestroyShaderModule(context->logical_device, frag_shader_module, context->allocator);
}