/*
 * Copyright (c) 2016 Kenichi Takahashi
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vector>
#include <cassert>
#include <unistd.h>

#include <android/log.h>
#include <android_native_app_glue.h>
#include "VulkanDevice.h"
#include "teapot.inl"


#include "glm/gtc/matrix_transform.hpp"

// Android log function wrappers
static const char* kTAG = "Vulkan-Tutorial04";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

#if defined(NDEBUG) && defined(__GNUC__)
#define U_ASSERT_ONLY __attribute__((unused))
#else
#define U_ASSERT_ONLY
#endif


VulkanDevice::VulkanDevice(android_app *app)
    : initialized(false), androidAppCtx(app)
{
    init();
}

VulkanDevice::~VulkanDevice() {
    vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
    vkDestroyDevice(device_, nullptr);
    vkDestroyInstance(instance_, nullptr);

    initialized = false;
}

bool VulkanDevice::isReady() {
    return initialized;
}

void VulkanDevice::setReady() {
    initialized = true;
}

void VulkanDevice::initPipeLineLayout()
{
    VkDescriptorSetLayoutBinding layout_bindings{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = NULL,
    };

    /* Next take layout bindings and use them to create a descriptor set layout
     */
    VkDescriptorSetLayoutCreateInfo descriptor_layout{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .bindingCount = 1,
            .pBindings = &layout_bindings,
    };

    CALL_VK(vkCreateDescriptorSetLayout(device_, &descriptor_layout, NULL, &descLayout));

    // Create pipeline layout (empty)
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1, // ToDo
            .pSetLayouts = &descLayout, // ToDo
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    };
    CALL_VK(vkCreatePipelineLayout(device_, &pipelineLayoutCreateInfo,
                                   nullptr, &pipelineLayout));

}

VkResult VulkanDevice::loadShaderFromFile(const char *filePath, VkShaderModule *shaderOut) {
    // Read the file
    assert(androidAppCtx);
    AAsset* file = AAssetManager_open(androidAppCtx->activity->assetManager,
                                      filePath, AASSET_MODE_BUFFER);
    size_t fileLength = AAsset_getLength(file);

    char* fileContent = new char[fileLength];

    AAsset_read(file, fileContent, fileLength);

    VkShaderModuleCreateInfo shaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .codeSize = fileLength,
            .pCode = (const uint32_t*)fileContent,
            .flags = 0,
    };
    VkResult result = vkCreateShaderModule(device_, &shaderModuleCreateInfo, nullptr, shaderOut);
    assert(result == VK_SUCCESS);

    delete[] fileContent;

    return result;
}

bool VulkanDevice::MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(gpuDevice_, &memoryProperties);
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memoryProperties.memoryTypes[i].propertyFlags &
                 requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }

    return false;
}

////////
void VulkanDevice::init()
{
    init_global_layer_properties();
    init_instance_extension_names();
    init_device_extension_names();
    init_instance("VulkanTeapot");
    init_enumerate_device(1);
    init_swapchain_extension();
    init_device();

    const bool depthPresent = true;
    init_swap_chain(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    init_command_pool();
    init_command_buffer();
    execute_begin_command_buffer();
    init_device_queue();
    initSwapChainImages();
    init_depth_buffer();
    init_uniform_buffer();
    init_descriptor_and_pipeline_layouts(depthPresent);
    init_renderpass(depthPresent, true);
    init_shaders();
    init_framebuffers(depthPresent);
    initVertexBufferWithNormal(teapotPositions, sizeof(teapotPositions), teapotNormals, sizeof(teapotNormals));
    initIndexForVertex(teapotIndices, sizeof(teapotIndices));
    init_descriptor_pool(false);
    init_descriptor_set(false);
    init_pipeline_cache();
    init_pipeline(depthPresent, true);

    preDraw();

    initialized = true;
}

VkResult init_global_extension_properties(layer_properties &layer_props) {
    VkExtensionProperties *instance_extensions;
    uint32_t instance_extension_count;
    VkResult res;
    char *layer_name = NULL;

    layer_name = layer_props.properties.layerName;

    do {
        res = vkEnumerateInstanceExtensionProperties(
                layer_name, &instance_extension_count, NULL);
        if (res)
            return res;

        if (instance_extension_count == 0) {
            return VK_SUCCESS;
        }

        layer_props.extensions.resize(instance_extension_count);
        instance_extensions = layer_props.extensions.data();
        res = vkEnumerateInstanceExtensionProperties(
                layer_name, &instance_extension_count, instance_extensions);
    } while (res == VK_INCOMPLETE);

    return res;
}

VkResult VulkanDevice::init_global_layer_properties() {
    uint32_t instance_layer_count;
    VkLayerProperties *vk_props = NULL;
    VkResult res;

    /*
     * It's possible, though very rare, that the number of
     * instance layers could change. For example, installing something
     * could include new layers that the loader would pick up
     * between the initial query for the count and the
     * request for VkLayerProperties. The loader indicates that
     * by returning a VK_INCOMPLETE status and will update the
     * the count parameter.
     * The count parameter will be updated with the number of
     * entries loaded into the data pointer - in case the number
     * of layers went down or is smaller than the size given.
     */
    do {
        res = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        if (res)
            return res;

        if (instance_layer_count == 0) {
            return VK_SUCCESS;
        }

        vk_props = (VkLayerProperties *)realloc(
                vk_props, instance_layer_count * sizeof(VkLayerProperties));

        res =
                vkEnumerateInstanceLayerProperties(&instance_layer_count, vk_props);
    } while (res == VK_INCOMPLETE);

    /*
     * Now gather the extension list for each instance layer.
     */
    for (uint32_t i = 0; i < instance_layer_count; i++) {
        layer_properties layer_props;
        layer_props.properties = vk_props[i];
        res = init_global_extension_properties(layer_props);
        if (res) {
            return res;
        }
        instance_layer_properties.push_back(layer_props);
    }
    free(vk_props);

    return res;
}

void VulkanDevice::init_instance_extension_names() {
    instance_extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef __ANDROID__
    instance_extension_names.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_WIN32)
    instance_extension_names.push_back(
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
    instance_extension_names.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
}

void VulkanDevice::init_device_extension_names() {
    device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

#ifndef VK_API_VERSION_1_0
// On Android, NDK would include slightly older version of headers that is missing the definition.
#define VK_API_VERSION_1_0 VK_API_VERSION
#endif

VkResult VulkanDevice::init_instance(char const *const app_short_name) {
    VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = NULL,
            .pApplicationName = app_short_name,
            .applicationVersion = 1,
            .pEngineName = app_short_name,
            .engineVersion = 1,
            .apiVersion = VK_API_VERSION_1_0,
    };

    uint32_t layerNameSize = instance_layer_names.size();
    uint32_t extensionNameSize = instance_extension_names.size();
    VkInstanceCreateInfo inst_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = layerNameSize,
            .ppEnabledLayerNames = layerNameSize ? instance_layer_names.data() : NULL,
            .enabledExtensionCount = extensionNameSize,
            .ppEnabledExtensionNames = instance_extension_names.data(),
    };

    VkResult res = vkCreateInstance(&inst_info, NULL, &instance_);
    assert(res == VK_SUCCESS);

    return res;
}

VkResult VulkanDevice::init_enumerate_device(uint32_t gpu_count) {
    uint32_t const U_ASSERT_ONLY req_count = gpu_count;
    VkResult res = vkEnumeratePhysicalDevices(instance_, &gpu_count, NULL);
    assert(gpu_count);
    gpus.resize(gpu_count);

    res = vkEnumeratePhysicalDevices(instance_, &gpu_count, gpus.data());
    assert(!res && gpu_count >= req_count);

    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queue_count,
                                             NULL);
    assert(queue_count >= 1);

    queue_props.resize(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queue_count,
                                             queue_props.data());
    assert(queue_count >= 1);

    /* This is as good a place as any to do this */
    vkGetPhysicalDeviceMemoryProperties(gpus[0], &memory_properties);
    vkGetPhysicalDeviceProperties(gpus[0], &gpu_props);

    return res;
}

void init_window_size(int32_t default_width,
                      int32_t default_height) {
#ifdef __ANDROID__
    //AndroidGetWindowSize(&info.width, &info.height);
#else
    info.width = default_width;
    info.height = default_height;
#endif
}

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                               \
    {                                                                          \
        fp##entrypoint =                                                  \
            (PFN_vk##entrypoint)vkGetInstanceProcAddr(inst, "vk" #entrypoint); \
        if (fp##entrypoint == NULL) {                                     \
            LOGE("vkGetDeviceProcAddr failed to find vk" #entrypoint);  \
            return false; \
        }                                                                      \
    }

bool VulkanDevice::init_swapchain_extension() {
    /* DEPENDS on init_connection() and init_window() */

    VkResult U_ASSERT_ONLY res;

// Construct the surface description:
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.hinstance = info.connection;
    createInfo.hwnd = info.window;
    res = vkCreateWin32SurfaceKHR(info.inst, &createInfo,
                                  NULL, &info.surface);
#elif defined(__ANDROID__)
    GET_INSTANCE_PROC_ADDR(instance_, CreateAndroidSurfaceKHR);

    VkAndroidSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.window = androidAppCtx->window;
    res = fpCreateAndroidSurfaceKHR(instance_, &createInfo, nullptr, &surface_);
#else  // !__ANDROID__ && !_WIN32
    VkXcbSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.connection = info.connection;
    createInfo.window = info.window;
    res = vkCreateXcbSurfaceKHR(info.inst, &createInfo,
                                NULL, &info.surface);
#endif // __ANDROID__  && _WIN32
    assert(res == VK_SUCCESS);

    // Iterate over each queue to learn whether it supports presenting:
    VkBool32 *supportsPresent =
            (VkBool32 *)malloc(queue_count * sizeof(VkBool32));
    for (uint32_t i = 0; i < queue_count; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(gpus[0], i, surface_,
                                             &supportsPresent[i]);
    }

    // Search for a graphics queue and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queue_count; i++) {
        if ((queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            if (supportsPresent[i] == VK_TRUE) {
                graphicsQueueNodeIndex = i;
                break;
            }
        }
    }
    free(supportsPresent);

    // Generate error if could not find a queue that supports both a graphics
    // and present
    if (graphicsQueueNodeIndex == UINT32_MAX) {
        LOGE("Could not find a queue that supports both graphics and present");
        return false;
    }

    graphics_queue_family_index = graphicsQueueNodeIndex;

    // Get the list of VkFormats that are supported:
    uint32_t formatCount;
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(gpus[0], surface_,
                                               &formatCount, NULL);
    assert(res == VK_SUCCESS);
    VkSurfaceFormatKHR *surfFormats =
            (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(gpus[0], surface_,
                                               &formatCount, surfFormats);
    assert(res == VK_SUCCESS);
    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
        format = VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        assert(formatCount >= 1);
        format = surfFormats[0].format;
    }

    return true;
}

VkResult VulkanDevice::init_device() {
    VkResult res;

    float queue_priorities[1] = {0.0};
    VkDeviceQueueCreateInfo queue_info{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .queueCount = 1,
            .pQueuePriorities = queue_priorities,
            .queueFamilyIndex = graphics_queue_family_index,
    };

    VkDeviceCreateInfo device_info{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = NULL,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queue_info,
            .pEnabledFeatures = NULL,
    };

    device_info.enabledLayerCount = device_layer_names.size();
    device_info.ppEnabledLayerNames =
            device_info.enabledLayerCount ? device_layer_names.data() : NULL;
    device_info.enabledExtensionCount = device_extension_names.size();
    device_info.ppEnabledExtensionNames =
            device_info.enabledExtensionCount ? device_extension_names.data()
                                              : NULL;

    res = vkCreateDevice(gpus[0], &device_info, NULL, &device_);
    assert(res == VK_SUCCESS);

    return res;
}

void VulkanDevice::init_command_pool() {
    /* DEPENDS on init_swapchain_extension() */
    VkResult U_ASSERT_ONLY res;

    VkCommandPoolCreateInfo cmd_pool_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = graphics_queue_family_index,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    res =
            vkCreateCommandPool(device_, &cmd_pool_info, NULL, &cmd_pool);
    assert(res == VK_SUCCESS);
}

void VulkanDevice::init_command_buffer() {
    /* DEPENDS on init_swapchain_extension() and init_command_pool() */
    VkResult U_ASSERT_ONLY res;

    cmdBuffer = new VkCommandBuffer[swapchainImageCount];
    VkCommandBufferAllocateInfo cmdBufInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = swapchainImageCount,
    };

    res = vkAllocateCommandBuffers(device_, &cmdBufInfo, cmdBuffer);
    assert(res == VK_SUCCESS);
}

void VulkanDevice::execute_begin_command_buffer() {
    /* DEPENDS on init_command_buffer() */
    VkResult U_ASSERT_ONLY res;

    for (int i = 0; i < swapchainImageCount; i++) {
        VkCommandBufferBeginInfo cmd_buf_info = {};
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = 0;
        cmd_buf_info.pInheritanceInfo = NULL;

        res = vkBeginCommandBuffer(cmdBuffer[i], &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }
}

void VulkanDevice::init_device_queue() {
    /* DEPENDS on init_swapchain_extension() */

    vkGetDeviceQueue(device_, graphics_queue_family_index, 0,
                     &queue_);
}

void VulkanDevice::init_swap_chain(VkImageUsageFlags usageFlags) {
    /* DEPENDS on info.cmd and info.queue initialized */

    VkResult U_ASSERT_ONLY res;
    VkSurfaceCapabilitiesKHR surfCapabilities;

    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpus[0], surface_,
                                                    &surfCapabilities);
    assert(res == VK_SUCCESS);

    uint32_t presentModeCount;
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(gpus[0], surface_,
                                                    &presentModeCount, NULL);
    assert(res == VK_SUCCESS);
    VkPresentModeKHR *presentModes =
            (VkPresentModeKHR *) malloc(presentModeCount * sizeof(VkPresentModeKHR));
    assert(presentModes);
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(
            gpus[0], surface_, &presentModeCount, presentModes);
    assert(res == VK_SUCCESS);

    VkExtent2D swapChainExtent;
    // width and height are either both -1, or both not -1.
    // If the surface size is defined, the swap chain size must match
    swapChainExtent = surfCapabilities.currentExtent;
    width = swapChainExtent.width;
    height = swapChainExtent.height;

    // If mailbox mode is available, use it, as is the lowest-latency non-
    // tearing mode.  If not, try IMMEDIATE which will usually be available,
    // and is fastest (though it tears).  If not, fall back to FIFO which is
    // always available.
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (size_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
            (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) {
            swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }
    LOGI("swapChainPresentMode = %d", swapchainPresentMode);

#ifdef __ANDROID__
    // Current driver only support VK_PRESENT_MODE_FIFO_KHR.
    //swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
#endif

    // Determine the number of VkImage's to use in the swap chain (we desire to
    // own only 1 image at a time, besides the images being displayed and
    // queued for display):
    uint32_t desiredNumberOfSwapChainImages =
            surfCapabilities.minImageCount + 1;
    if ((surfCapabilities.maxImageCount > 0) &&
        (desiredNumberOfSwapChainImages > surfCapabilities.maxImageCount)) {
        // Application must settle for fewer images than desired:
        desiredNumberOfSwapChainImages = surfCapabilities.maxImageCount;
    }

    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfCapabilities.supportedTransforms &
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfCapabilities.currentTransform;
    }

    LOGI("desiredNumberOfSwapChainImages = %u", desiredNumberOfSwapChainImages);
    VkSwapchainCreateInfoKHR swapChainInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = NULL,
            .surface = surface_,
            .minImageCount = desiredNumberOfSwapChainImages,
            .imageFormat = format,
            .imageExtent.width = swapChainExtent.width,
            .imageExtent.height = swapChainExtent.height,
            .preTransform = preTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .imageArrayLayers = 1,
            .presentMode = swapchainPresentMode,
            .oldSwapchain = VK_NULL_HANDLE,
#ifndef __ANDROID__
            .clipped = true,
#else
            .clipped = false,
#endif
            .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            .imageUsage = usageFlags,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
    };

    res =
            vkCreateSwapchainKHR(device_, &swapChainInfo, NULL, &swap_chain);
    assert(res == VK_SUCCESS);

    res = vkGetSwapchainImagesKHR(device_, swap_chain,
                                  &swapchainImageCount, NULL);
    assert(res == VK_SUCCESS);

    if (NULL != presentModes) {
        free(presentModes);
    }
}

void VulkanDevice::initSwapChainImages() {

    VkImage *swapchainImages =
            (VkImage *)malloc(swapchainImageCount * sizeof(VkImage));
    assert(swapchainImages);
    VkResult res = vkGetSwapchainImagesKHR(device_, swap_chain,
                                  &swapchainImageCount, swapchainImages);
    assert(res == VK_SUCCESS);

    LOGI("swapchainImageCount = %u", swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        LOGI("swapChainImageIndex = %u", i);
        swap_chain_buffer sc_buffer;

        VkImageViewCreateInfo color_image_view = {};
        color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        color_image_view.pNext = NULL;
        color_image_view.format = format;
        color_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
        color_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
        color_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
        color_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
        color_image_view.subresourceRange.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
        color_image_view.subresourceRange.baseMipLevel = 0;
        color_image_view.subresourceRange.levelCount = 1;
        color_image_view.subresourceRange.baseArrayLayer = 0;
        color_image_view.subresourceRange.layerCount = 1;
        color_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        color_image_view.flags = 0;

        sc_buffer.image = swapchainImages[i];
        LOGI("swapChainImage-1");

        set_image_layout(i, sc_buffer.image, VK_IMAGE_ASPECT_COLOR_BIT,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        LOGI("swapChainImage-2");

        color_image_view.image = sc_buffer.image;

        res = vkCreateImageView(device_, &color_image_view, NULL,
                                &sc_buffer.view);
        LOGI("swapChainImage-2");
        buffers.push_back(sc_buffer);
        assert(res == VK_SUCCESS);
    }
    free(swapchainImages);
    current_buffer = 0;

}

void VulkanDevice::set_image_layout(uint32_t cmdIndex, VkImage image,
                                    VkImageAspectFlags aspectMask,
                                    VkImageLayout old_image_layout,
                                    VkImageLayout new_image_layout) {
    /* DEPENDS on info.cmd and info.queue initialized */

    //assert(cmd != VK_NULL_HANDLE);
    assert(queue_ != VK_NULL_HANDLE);

    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = 0;
    image_memory_barrier.oldLayout = old_image_layout;
    image_memory_barrier.newLayout = new_image_layout;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange.aspectMask = aspectMask;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = 1;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;

    if (old_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.srcAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (old_image_layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
        image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        image_memory_barrier.srcAccessMask =
                VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    vkCmdPipelineBarrier(cmdBuffer[cmdIndex], src_stages, dest_stages, 0, 0, NULL, 0, NULL,
                         1, &image_memory_barrier);
}

bool VulkanDevice::memory_type_from_properties(uint32_t typeBits,
                                               VkFlags requirements_mask,
                                               uint32_t *typeIndex) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memory_properties.memoryTypes[i].propertyFlags &
                 requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

bool VulkanDevice::init_depth_buffer() {
    VkResult U_ASSERT_ONLY res;
    bool U_ASSERT_ONLY pass;
    VkImageCreateInfo image_info = {};

    /* allow custom depth formats */
    if (depth.format == VK_FORMAT_UNDEFINED) {
        depth.format = VK_FORMAT_D16_UNORM;
    }

#ifdef __ANDROID__
    // Depth format needs to be VK_FORMAT_D24_UNORM_S8_UINT on Android.
    const VkFormat depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
#else
    const VkFormat depth_format = info.depth.format;
#endif
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(gpus[0], depth_format, &props);
    if (props.linearTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        image_info.tiling = VK_IMAGE_TILING_LINEAR;
    } else if (props.optimalTilingFeatures &
               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    } else {
        /* Try other depth formats? */
        //std::cout << "depth_format " << depth_format << " Unsupported.\n";
        LOGE("depth_format %d Unsupported.", depth_format);
        return false;
    }

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = depth_format;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = NUM_SAMPLES;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = NULL;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.flags = 0;

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext = NULL;
    view_info.image = VK_NULL_HANDLE;
    view_info.format = depth_format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.flags = 0;

    if (depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
        depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
        depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkMemoryRequirements mem_reqs;

    /* Create image */
    res = vkCreateImage(device_, &image_info, NULL, &depth.image);
    assert(res == VK_SUCCESS);

    vkGetImageMemoryRequirements(device_, depth.image, &mem_reqs);

    mem_alloc.allocationSize = mem_reqs.size;
    /* Use the memory properties to determine the type of memory required */
    pass = memory_type_from_properties(mem_reqs.memoryTypeBits,
                                       0, /* No requirements */
                                       &mem_alloc.memoryTypeIndex);
    assert(pass);

    /* Allocate memory */
    res = vkAllocateMemory(device_, &mem_alloc, NULL, &depth.mem);
    assert(res == VK_SUCCESS);

    /* Bind memory */
    res = vkBindImageMemory(device_, depth.image, depth.mem, 0);
    assert(res == VK_SUCCESS);

    /* Set the image layout to depth stencil optimal */
    for (int i = 0; i < swapchainImageCount; i++) {
        set_image_layout(i, depth.image,
                         view_info.subresourceRange.aspectMask,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    /* Create image view */
    view_info.image = depth.image;
    res = vkCreateImageView(device_, &view_info, NULL, &depth.view);
    assert(res == VK_SUCCESS);

    return true;
}

void VulkanDevice::init_uniform_buffer() {
    VkResult U_ASSERT_ONLY res;
    bool U_ASSERT_ONLY pass;
    float fov = glm::radians(45.0f);
    if (width > height) {
        fov *= static_cast<float>(height) / static_cast<float>(width);
    }
    Projection = glm::perspective(fov,
                                       static_cast<float>(width) /
                                       static_cast<float>(height), 0.1f, 300.0f);
    View = glm::lookAt(
            glm::vec3(30, -200, 20), // Camera is at (5,3,10), in World Space
            glm::vec3(0, 0, 0),  // and looks at the origin
            glm::vec3(0, 0, 1)  // Head is up (set to 0,-1,0 to look upside-down)
    );
    Model = glm::mat4(1.0f);
    // Vulkan clip space has inverted Y and half Z.
    Clip = glm::mat4(1.0f,  0.0f, 0.0f, 0.0f,
                          0.0f, -1.0f, 0.0f, 0.0f,
                          0.0f,  0.0f, 0.5f, 0.0f,
                          0.0f,  0.0f, 0.5f, 1.0f);

    MVP = Clip * Projection * View * Model;

    /* VULKAN_KEY_START */
    VkBufferCreateInfo buf_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .size = sizeof(MVP),
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
    };
    res = vkCreateBuffer(device_, &buf_info, NULL, &uniform_data.buf);
    assert(res == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device_, uniform_data.buf,
                                  &mem_reqs);

    VkMemoryAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .memoryTypeIndex = 0,
    };

    alloc_info.allocationSize = mem_reqs.size;
    pass = memory_type_from_properties(mem_reqs.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                       &alloc_info.memoryTypeIndex);
    assert(pass);

    res = vkAllocateMemory(device_, &alloc_info, NULL,
                           &(uniform_data.mem));
    assert(res == VK_SUCCESS);

    uint8_t *pData;
    res = vkMapMemory(device_, uniform_data.mem, 0, mem_reqs.size, 0,
                      (void **)&pData);
    assert(res == VK_SUCCESS);

    memcpy(pData, &MVP, sizeof(MVP));

    vkUnmapMemory(device_, uniform_data.mem);

    res = vkBindBufferMemory(device_, uniform_data.buf, uniform_data.mem, 0);
    assert(res == VK_SUCCESS);

    uniform_data.buffer_info.buffer = uniform_data.buf;
    uniform_data.buffer_info.offset = 0;
    uniform_data.buffer_info.range = sizeof(MVP);
}

void VulkanDevice::init_descriptor_and_pipeline_layouts(bool use_texture) {
    VkDescriptorSetLayoutBinding layout_bindings[2];
    layout_bindings[0].binding = 0;
    layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layout_bindings[0].descriptorCount = 1;
    layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_bindings[0].pImmutableSamplers = NULL;

    if (use_texture) {
        layout_bindings[1].binding = 1;
        layout_bindings[1].descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[1].descriptorCount = 1;
        layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_bindings[1].pImmutableSamplers = NULL;
    }

    /* Next take layout bindings and use them to create a descriptor set layout
     */
    VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
    descriptor_layout.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.pNext = NULL;
    descriptor_layout.bindingCount = use_texture ? 2 : 1;
    descriptor_layout.pBindings = layout_bindings;

    VkResult U_ASSERT_ONLY res;

    desc_layout.resize(NUM_DESCRIPTOR_SETS);
    res = vkCreateDescriptorSetLayout(device_, &descriptor_layout, NULL,
                                      desc_layout.data());
    assert(res == VK_SUCCESS);

    /* Now use the descriptor layout to create a pipeline layout */
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = NULL;
    pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pPipelineLayoutCreateInfo.pPushConstantRanges = NULL;
    pPipelineLayoutCreateInfo.setLayoutCount = NUM_DESCRIPTOR_SETS;
    pPipelineLayoutCreateInfo.pSetLayouts = desc_layout.data();

    res = vkCreatePipelineLayout(device_, &pPipelineLayoutCreateInfo, NULL, &pipelineLayout);
    assert(res == VK_SUCCESS);
}

void VulkanDevice::init_renderpass(bool include_depth, bool clear) {
    /* DEPENDS on init_swap_chain() and init_depth_buffer() */

    VkResult U_ASSERT_ONLY res;
    /* Need attachments for render target and depth buffer */
    VkAttachmentDescription attachments[2];
    attachments[0].format = format;
    attachments[0].samples = NUM_SAMPLES;
    attachments[0].loadOp =
            clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].flags = 0;

    if (include_depth) {
        attachments[1].format = depth.format;
        attachments[1].samples = NUM_SAMPLES;
        attachments[1].loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                      : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].initialLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[1].finalLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[1].flags = 0;
    }

    VkAttachmentReference color_reference = {};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference = {};
    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = include_depth ? &depth_reference : NULL;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = include_depth ? 2 : 1;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 0;
    rp_info.pDependencies = NULL;

    res = vkCreateRenderPass(device_, &rp_info, NULL, &render_pass);
    assert(res == VK_SUCCESS);
}

void VulkanDevice::init_shaders() {
    loadShaderFromFile("shaders/shape.vert.spv", &vertexShader);
    loadShaderFromFile("shaders/shape.frag.spv", &fragmentShader);
}

void VulkanDevice::init_framebuffers(bool include_depth) {
    /* DEPENDS on init_depth_buffer(), init_renderpass() and
     * init_swapchain_extension() */

    VkResult U_ASSERT_ONLY res;
    VkImageView attachments[2];
    attachments[1] = depth.view;

    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext = NULL;
    fb_info.renderPass = render_pass;
    fb_info.attachmentCount = include_depth ? 2 : 1;
    fb_info.pAttachments = attachments;
    fb_info.width = width;
    fb_info.height = height;
    fb_info.layers = 1;

    uint32_t i;

    framebuffers = (VkFramebuffer *)malloc(swapchainImageCount *
                                                sizeof(VkFramebuffer));

    for (i = 0; i < swapchainImageCount; i++) {
        attachments[0] = buffers[i].view;
        res = vkCreateFramebuffer(device_, &fb_info, NULL,
                                  &framebuffers[i]);
        assert(res == VK_SUCCESS);
    }
}

void VulkanDevice::init_vertex_buffer(const void *vertexData,
                                      uint32_t dataSize, uint32_t dataStride,
                                      bool use_texture) {
    VkResult U_ASSERT_ONLY res;
    bool U_ASSERT_ONLY pass;

    VkBufferCreateInfo buf_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .size = dataSize,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
    };
    res = vkCreateBuffer(device_, &buf_info, NULL, &vertex_buffer.buf);
    assert(res == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device_, vertex_buffer.buf,
                                  &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.memoryTypeIndex = 0;

    alloc_info.allocationSize = mem_reqs.size;
    pass = memory_type_from_properties(mem_reqs.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       &alloc_info.memoryTypeIndex);
    assert(pass);

    res = vkAllocateMemory(device_, &alloc_info, NULL,
                           &(vertex_buffer.mem));
    assert(res == VK_SUCCESS);
    vertex_buffer.buffer_info.range = mem_reqs.size;
    vertex_buffer.buffer_info.offset = 0;

    uint8_t *pData;
    res = vkMapMemory(device_, vertex_buffer.mem, 0, mem_reqs.size, 0,
                      (void **)&pData);
    assert(res == VK_SUCCESS);

    memcpy(pData, vertexData, dataSize);

    vkUnmapMemory(device_, vertex_buffer.mem);

    res = vkBindBufferMemory(device_, vertex_buffer.buf, vertex_buffer.mem, 0);
    assert(res == VK_SUCCESS);

    vi_binding.binding = 0;
    vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vi_binding.stride = dataStride;

    vi_attribs[0].binding = 0;
    vi_attribs[0].location = 0;
    vi_attribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vi_attribs[0].offset = 0;
    vi_attribs[1].binding = 0;
    vi_attribs[1].location = 1;
    vi_attribs[1].format =
            use_texture ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
    vi_attribs[1].offset = 16;
}

void VulkanDevice::initVertexBufferWithNormal(const float *vertexData,
                                              uint32_t vertexDataSize, const float *normalData, uint32_t normalDataSize) {
    VkResult U_ASSERT_ONLY res;
    bool U_ASSERT_ONLY pass;

    uint32_t dataSize = vertexDataSize + normalDataSize;
    VkBufferCreateInfo buf_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .size = dataSize,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
    };
    res = vkCreateBuffer(device_, &buf_info, NULL, &vertex_buffer.buf);
    assert(res == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device_, vertex_buffer.buf,
                                  &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.memoryTypeIndex = 0;

    alloc_info.allocationSize = mem_reqs.size;
    pass = memory_type_from_properties(mem_reqs.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       &alloc_info.memoryTypeIndex);
    assert(pass);

    res = vkAllocateMemory(device_, &alloc_info, NULL,
                           &(vertex_buffer.mem));
    assert(res == VK_SUCCESS);
    vertex_buffer.buffer_info.range = mem_reqs.size;
    vertex_buffer.buffer_info.offset = 0;

    uint8_t *pData;
    res = vkMapMemory(device_, vertex_buffer.mem, 0, mem_reqs.size, 0,
                      (void **)&pData);
    assert(res == VK_SUCCESS);

    const float *vData = vertexData;
    const float *nData = normalData;
    float *vBuf = (float *)pData;
    uint32_t elementNum = vertexDataSize / (sizeof(float) * 3);
    for (int i = 0; i < elementNum; i++) {
        // vertex
        *vBuf++ = *vData++;
        *vBuf++ = *vData++;
        *vBuf++ = *vData++;
        // normal
        *vBuf++ = *nData++;
        *vBuf++ = *nData++;
        *vBuf++ = *nData++;
    }

    vkUnmapMemory(device_, vertex_buffer.mem);

    res = vkBindBufferMemory(device_, vertex_buffer.buf, vertex_buffer.mem, 0);
    assert(res == VK_SUCCESS);

    vi_binding.binding = 0;
    vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vi_binding.stride = sizeof(float) * 6;

    vi_attribs[0].binding = 0;
    vi_attribs[0].location = 0;
    vi_attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vi_attribs[0].offset = 0;
    vi_attribs[1].binding = 0;
    vi_attribs[1].location = 1;
    vi_attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vi_attribs[1].offset = 12;
}

void VulkanDevice::initIndexForVertex(const uint16_t *indexData, size_t indexDataSize) {
    // Create a vertex buffer
    uint32_t queueIdx = 0;
    VkBufferCreateInfo createBufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = indexDataSize,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .flags = 0,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .pQueueFamilyIndices = &queueIdx,
            .queueFamilyIndexCount = 1,
    };
    CALL_VK(vkCreateBuffer(device_, &createBufferInfo, nullptr, &indexBuf));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device_, indexBuf, &memReq);

    VkMemoryAllocateInfo allocInfo {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = indexDataSize,
            .memoryTypeIndex = 0,  // Memory type assigned in the next step
    };

    // Assign the proper memory type for that buffer
    allocInfo.allocationSize = memReq.size;
    bool U_ASSERT_ONLY pass;
    pass = memory_type_from_properties(memReq.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       &allocInfo.memoryTypeIndex);
    assert(pass);

    // Allocate memory for the buffer
    VkDeviceMemory deviceMemory;
    CALL_VK(vkAllocateMemory(device_, &allocInfo, nullptr, &deviceMemory));

    void* data;
    CALL_VK(vkMapMemory(device_, deviceMemory, 0, indexDataSize, 0, &data));
    memcpy(data, indexData, indexDataSize);
    vkUnmapMemory(device_, deviceMemory);

    CALL_VK(vkBindBufferMemory(device_, indexBuf, deviceMemory, 0));

    drawElementNum = indexDataSize / sizeof(uint16_t);
    drawInstanceNum = drawElementNum / 3;

    LOGI("drawElementNum=%zu", drawElementNum);
}

void VulkanDevice::init_descriptor_pool(bool use_texture) {
    /* DEPENDS on init_uniform_buffer() and
     * init_descriptor_and_pipeline_layouts() */

    VkResult U_ASSERT_ONLY res;
    VkDescriptorPoolSize type_count[2];
    type_count[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    type_count[0].descriptorCount = 1;
    if (use_texture) {
        type_count[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        type_count[1].descriptorCount = 1;
    }

    VkDescriptorPoolCreateInfo descriptor_pool = {};
    descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool.pNext = NULL;
    descriptor_pool.maxSets = 1;
    descriptor_pool.poolSizeCount = use_texture ? 2 : 1;
    descriptor_pool.pPoolSizes = type_count;

    res = vkCreateDescriptorPool(device_, &descriptor_pool, NULL,
                                 &desc_pool);
    assert(res == VK_SUCCESS);
}

void VulkanDevice::init_descriptor_set(bool use_texture) {
    /* DEPENDS on init_descriptor_pool() */

    VkResult U_ASSERT_ONLY res;

    VkDescriptorSetAllocateInfo alloc_info[1];
    alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info[0].pNext = NULL;
    alloc_info[0].descriptorPool = desc_pool;
    alloc_info[0].descriptorSetCount = NUM_DESCRIPTOR_SETS;
    alloc_info[0].pSetLayouts = desc_layout.data();

    desc_set.resize(NUM_DESCRIPTOR_SETS);
    res =
            vkAllocateDescriptorSets(device_, alloc_info, desc_set.data());
    assert(res == VK_SUCCESS);

    VkWriteDescriptorSet writes[2];

    writes[0] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = desc_set[0];
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &uniform_data.buffer_info;
    writes[0].dstArrayElement = 0;
    writes[0].dstBinding = 0;

    if (use_texture) {
        writes[1] = {};
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = desc_set[0];
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &texture_data.image_info;
        writes[1].dstArrayElement = 0;
    }

    vkUpdateDescriptorSets(device_, use_texture ? 2 : 1, writes, 0, NULL);
}

void VulkanDevice::init_pipeline_cache() {
    VkResult U_ASSERT_ONLY res;

    VkPipelineCacheCreateInfo pipelineCacheInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = NULL,
            .initialDataSize = 0,
            .pInitialData = NULL,
            .flags = 0,
    };
    res = vkCreatePipelineCache(device_, &pipelineCacheInfo, NULL,
                                &pipelineCache);
    assert(res == VK_SUCCESS);
}

void VulkanDevice::init_pipeline(VkBool32 include_depth, VkBool32 include_vi) {
    VkResult U_ASSERT_ONLY res;

    VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = NULL;
    dynamicState.pDynamicStates = dynamicStateEnables;
    dynamicState.dynamicStateCount = 0;

    VkPipelineVertexInputStateCreateInfo vi;
    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (include_vi) {
        vi.pNext = NULL;
        vi.flags = 0;
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &vi_binding;
        vi.vertexAttributeDescriptionCount = 2;
        vi.pVertexAttributeDescriptions = vi_attribs;
    }
    VkPipelineInputAssemblyStateCreateInfo ia{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .primitiveRestartEnable = VK_FALSE,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineRasterizationStateCreateInfo rs;
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.pNext = NULL;
    rs.flags = 0;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthClampEnable = include_depth;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.depthBiasConstantFactor = 0;
    rs.depthBiasClamp = 0;
    rs.depthBiasSlopeFactor = 0;
    rs.lineWidth = 0;

    VkPipelineColorBlendStateCreateInfo cb;
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.flags = 0;
    cb.pNext = NULL;
    VkPipelineColorBlendAttachmentState att_state[1];
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable = VK_FALSE;
    att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
    att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
    att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cb.attachmentCount = 1;
    cb.pAttachments = att_state;
    cb.logicOpEnable = VK_FALSE;
    cb.logicOp = VK_LOGIC_OP_NO_OP;
    cb.blendConstants[0] = 1.0f;
    cb.blendConstants[1] = 1.0f;
    cb.blendConstants[2] = 1.0f;
    cb.blendConstants[3] = 1.0f;

    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.pNext = NULL;
    vp.flags = 0;
#ifndef __ANDROID__
    vp.viewportCount = NUM_VIEWPORTS;
    dynamicStateEnables[dynamicState.dynamicStateCount++] =
        VK_DYNAMIC_STATE_VIEWPORT;
    vp.scissorCount = NUM_SCISSORS;
    dynamicStateEnables[dynamicState.dynamicStateCount++] =
        VK_DYNAMIC_STATE_SCISSOR;
    vp.pScissors = NULL;
    vp.pViewports = NULL;
#else
    // Temporary disabling dynamic viewport on Android because some of drivers doesn't
    // support the feature.
    VkViewport viewports;
    viewports.minDepth = 0.0f;
    viewports.maxDepth = 1.0f;
    viewports.x = 0;
    viewports.y = 0;
    viewports.width = width;
    viewports.height = height;
    VkRect2D scissor;
    scissor.extent.width = width;
    scissor.extent.height = height;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    vp.viewportCount = NUM_VIEWPORTS;
    vp.scissorCount = NUM_SCISSORS;
    vp.pScissors = &scissor;
    vp.pViewports = &viewports;
#endif
    VkPipelineDepthStencilStateCreateInfo ds;
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.pNext = NULL;
    ds.flags = 0;
    ds.depthTestEnable = include_depth;
    ds.depthWriteEnable = include_depth;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.back.compareMask = 0;
    ds.back.reference = 0;
    ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.back.writeMask = 0;
    ds.minDepthBounds = 0;
    ds.maxDepthBounds = 0;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = ds.back;

    VkPipelineMultisampleStateCreateInfo ms;
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.pNext = NULL;
    ms.flags = 0;
    ms.pSampleMask = NULL;
    ms.rasterizationSamples = NUM_SAMPLES;
    ms.sampleShadingEnable = VK_FALSE;
    ms.alphaToCoverageEnable = VK_FALSE;
    ms.alphaToOneEnable = VK_FALSE;
    ms.minSampleShading = 0.0;

    // Specify vertex and fragment shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2] {
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertexShader,
                    .pSpecializationInfo = nullptr,
                    .flags = 0,
                    .pName = "main",
            },
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragmentShader,
                    .pSpecializationInfo = nullptr,
                    .flags = 0,
                    .pName = "main",
            }
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = NULL,
            .layout = pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
            .flags = 0,
            .pVertexInputState = &vi,
            .pInputAssemblyState = &ia,
            .pRasterizationState = &rs,
            .pColorBlendState = &cb,
            .pTessellationState = NULL,
            .pMultisampleState = &ms,
            .pDynamicState = &dynamicState,
            .pViewportState = &vp,
            .pDepthStencilState = &ds,
            .pStages = shaderStages,
            .stageCount = 2,
            .renderPass = render_pass,
            .subpass = 0,
    };

    res = vkCreateGraphicsPipelines(device_, pipelineCache, 1,
                                    &pipelineInfo, NULL, &pipeline);
    assert(res == VK_SUCCESS);
}

void init_viewports() {
#ifdef __ANDROID__
    // Disable dynamic viewport on Android. Some drive has an issue with the dynamic viewport
    // feature.
#else
    info.viewport.height = (float)info.height;
    info.viewport.width = (float)info.width;
    info.viewport.minDepth = (float)0.0f;
    info.viewport.maxDepth = (float)1.0f;
    info.viewport.x = 0;
    info.viewport.y = 0;
    vkCmdSetViewport(info.cmd, 0, NUM_VIEWPORTS, &info.viewport);
#endif
}

void init_scissors() {
#ifdef __ANDROID__
    // Disable dynamic viewport on Android. Some drive has an issue with the dynamic scissors
    // feature.
#else
    info.scissor.extent.width = info.width;
    info.scissor.extent.height = info.height;
    info.scissor.offset.x = 0;
    info.scissor.offset.y = 0;
    vkCmdSetScissor(info.cmd, 0, NUM_SCISSORS, &info.scissor);
#endif
}

void VulkanDevice::preDraw() {
    VkClearValue clear_values[2];
    clear_values[0].color.float32[0] = 0.2f;
    clear_values[0].color.float32[1] = 0.2f;
    clear_values[0].color.float32[2] = 0.2f;
    clear_values[0].color.float32[3] = 0.2f;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    VkSemaphoreCreateInfo presentCompleteSemaphoreCreateInfo;
    presentCompleteSemaphoreCreateInfo.sType =
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    presentCompleteSemaphoreCreateInfo.pNext = NULL;
    presentCompleteSemaphoreCreateInfo.flags = 0;

    VkResult res = vkCreateSemaphore(device_, &presentCompleteSemaphoreCreateInfo,
                            NULL, &presentCompleteSemaphore);
    assert(res == VK_SUCCESS);

    // Get the index of the next available swapchain image:
    res = vkAcquireNextImageKHR(device_, swap_chain, UINT64_MAX,
                                presentCompleteSemaphore, VK_NULL_HANDLE,
                                &current_buffer);
    // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
    // return codes
    assert(res == VK_SUCCESS);

    for (int i = 0; i < swapchainImageCount; i++) {
        VkRenderPassBeginInfo rp_begin{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext = NULL,
                .renderPass = render_pass,
                .framebuffer = framebuffers[i],
                .renderArea.offset.x = 0,
                .renderArea.offset.y = 0,
                .renderArea.extent.width = width,
                .renderArea.extent.height = height,
                .clearValueCount = 2,
                .pClearValues = clear_values,
        };

        vkCmdBeginRenderPass(cmdBuffer[i], &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, NUM_DESCRIPTOR_SETS,
                                desc_set.data(), 0, NULL);

        const VkDeviceSize offsets[1] = {0};
        vkCmdBindVertexBuffers(cmdBuffer[i], 0, 1, &vertex_buffer.buf, offsets);
        vkCmdBindIndexBuffer(cmdBuffer[i], indexBuf, offsets[0], VK_INDEX_TYPE_UINT16);

        //init_viewports(info);
        //init_scissors(info);

        //vkCmdDraw(cmdBuffer[i], 12 * 3, 1, 0, 0);
        vkCmdDrawIndexed(cmdBuffer[i], drawElementNum, drawInstanceNum, 0, 0, 0);
        vkCmdEndRenderPass(cmdBuffer[i]);

        VkImageMemoryBarrier prePresentBarrier {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .subresourceRange.baseMipLevel = 0,
                .subresourceRange.levelCount = 1,
                .subresourceRange.baseArrayLayer = 0,
                .subresourceRange.layerCount = 1,
                .image = buffers[i].image,
        };
        vkCmdPipelineBarrier(cmdBuffer[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                             NULL, 1, &prePresentBarrier);

        res = vkEndCommandBuffer(cmdBuffer[i]);
    }
    //const VkCommandBuffer cmd_bufs[] = {cmd};
    VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
    };
    vkCreateFence(device_, &fenceInfo, NULL, &drawFence);

    VkPipelineStageFlags pipe_stage_flags =
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkSubmitInfo submit_info[1] = {};
    submit_info[0].pNext = NULL;
    submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info[0].waitSemaphoreCount = 1;
    submit_info[0].pWaitSemaphores = &presentCompleteSemaphore;
    submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
    submit_info[0].commandBufferCount = 1;
    submit_info[0].pCommandBuffers = &cmdBuffer[current_buffer],
    submit_info[0].signalSemaphoreCount = 0;
    submit_info[0].pSignalSemaphores = NULL;

    /* Queue the command buffer for execution */
    res = vkQueueSubmit(queue_, 1, submit_info, drawFence);
    assert(res == VK_SUCCESS);

    /* Now present the image in the window */

    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = NULL;
    present.swapchainCount = 1;
    present.pSwapchains = &swap_chain;
    present.pImageIndices = &current_buffer;
    present.pWaitSemaphores = NULL;
    present.waitSemaphoreCount = 0;
    present.pResults = NULL;

    //
    /* Make sure command buffer is finished before presenting */
    do {
        res = vkWaitForFences(device_, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
        LOGI("draw %d", res);
    } while (res == VK_TIMEOUT);
    assert(res == VK_SUCCESS);
    res = vkQueuePresentKHR(queue_, &present);
    assert(res == VK_SUCCESS);

    LOGI("preDraw");
    //
}

VkResult VulkanDevice::draw() {
    VkResult res;

    // Get the framebuffer index we should draw in
    CALL_VK(vkAcquireNextImageKHR(device_, swap_chain,
                                  UINT64_MAX, presentCompleteSemaphore,
                                  VK_NULL_HANDLE, &current_buffer));
    CALL_VK(vkResetFences(device_, 1, &drawFence));
    VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &presentCompleteSemaphore,
            .pWaitDstStageMask = &pipe_stage_flags,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuffer[current_buffer],
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
    };

    CALL_VK(vkQueueSubmit(queue_, 1, &submit_info, drawFence));
    //do {
        res = vkWaitForFences(device_, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
    //} while(res == VK_TIMEOUT);

    present.pImageIndices = &current_buffer;
    vkQueuePresentKHR(queue_, &present);

    return res;
}

void VulkanDevice::rotateModel(float x, float y, float z) {
    LOGI("rotate y = %f", y);
    Model = glm::rotate(glm::mat4(1.0f), glm::radians(y), glm::vec3(0, 0, 1));
    updateMVP();
}

void VulkanDevice::updateMVP() {
    MVP = Clip * Projection * View * Model;

    uint8_t *pData;
    VkResult res = vkMapMemory(device_, uniform_data.mem, 0, sizeof(MVP), 0, (void **)&pData);
    assert(res == VK_SUCCESS);

    memcpy(pData, &MVP, sizeof(MVP));

    vkUnmapMemory(device_, uniform_data.mem);
}

void destroy(struct sample_info &info) {
}
