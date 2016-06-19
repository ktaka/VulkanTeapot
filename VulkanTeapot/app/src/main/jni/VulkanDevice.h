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

#ifndef VULKANARMADILLO_VULKANDEVICEINFO_H
#define VULKANARMADILLO_VULKANDEVICEINFO_H

#include <vector>

#include "vulkan_wrapper.h"
#include "glm/glm.hpp"

struct android_app;

enum ShaderType { VERTEX_SHADER, FRAGMENT_SHADER };

/*
 * A layer can expose extensions, keep track of those
 * extensions here.
 */
typedef struct {
    VkLayerProperties properties;
    std::vector<VkExtensionProperties> extensions;
} layer_properties;

/*
 * Keep each of our swap chain buffers' image, command buffer and view in one
 * spot
 */
typedef struct _swap_chain_buffers {
    VkImage image;
    VkImageView view;
} swap_chain_buffer;

class VulkanDevice {
public:
    VulkanDevice(android_app *app);
    ~VulkanDevice();

    bool isReady();
    void setReady();
    void initPipeLineLayout();
    VkResult loadShaderFromFile(const char* filePath, VkShaderModule* shaderOut);
    bool MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex);
    VkResult draw();
    void rotateModel(float x, float y, float z);
    void updateMVP();

    VkInstance          instance_;
    VkPhysicalDevice    gpuDevice_;
    VkDevice            device_;

    VkSurfaceKHR        surface_;
    VkQueue             queue_;

    VkDescriptorSetLayout descLayout;
    VkPipelineLayout  pipelineLayout;

    uint32_t width;
    uint32_t height;

private:
    bool initialized;
    android_app *androidAppCtx;
    //
    std::vector<layer_properties> instance_layer_properties;
    std::vector<const char *> instance_layer_names;
    std::vector<const char *> instance_extension_names;
    std::vector<const char *> device_extension_names;
    std::vector<const char *> device_layer_names;
    std::vector<VkPhysicalDevice> gpus;
    VkPhysicalDeviceProperties gpu_props;
    std::vector<VkQueueFamilyProperties> queue_props;
    uint32_t queue_count;
    VkPhysicalDeviceMemoryProperties memory_properties;
    PFN_vkCreateAndroidSurfaceKHR fpCreateAndroidSurfaceKHR;
    uint32_t graphics_queue_family_index;
    VkFormat format;
    VkCommandPool cmd_pool;
    //VkCommandBuffer cmd; // Buffer for initialization commands
    VkCommandBuffer *cmdBuffer;
    uint32_t swapchainImageCount;
    VkSwapchainKHR swap_chain;
    std::vector<swap_chain_buffer> buffers;
    uint32_t current_buffer;

    struct {
        VkFormat format;

        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } depth;

    glm::mat4 Projection;
    glm::mat4 View;
    glm::mat4 Model;
    glm::mat4 Clip;
    glm::mat4 MVP;

    struct {
        VkBuffer buf;
        VkDeviceMemory mem;
        VkDescriptorBufferInfo buffer_info;
    } uniform_data;

    std::vector<VkDescriptorSetLayout> desc_layout;
    VkRenderPass render_pass;
    VkShaderModule vertexShader,fragmentShader;
    VkFramebuffer *framebuffers;

    struct {
        VkBuffer buf;
        VkDeviceMemory mem;
        VkDescriptorBufferInfo buffer_info;
    } vertex_buffer;
    VkVertexInputBindingDescription vi_binding;
    VkVertexInputAttributeDescription vi_attribs[2];
    VkBuffer indexBuf;
    size_t drawElementNum;
    size_t drawInstanceNum;

    VkDescriptorPool desc_pool;
    std::vector<VkDescriptorSet> desc_set;

    struct {
        VkDescriptorImageInfo image_info;
    } texture_data;

    VkPipelineCache pipelineCache;
    VkPipeline pipeline;

    VkFence drawFence;
    VkSemaphore presentCompleteSemaphore;
    VkPresentInfoKHR present;

    //

    void init();
    VkResult init_global_layer_properties();
    void init_instance_extension_names();
    void init_device_extension_names();
    VkResult init_instance(char const *const app_short_name);
    VkResult init_enumerate_device(uint32_t gpu_count);
    bool init_swapchain_extension();
    VkResult init_device();
    void init_command_pool();
    void init_command_buffer();
    void execute_begin_command_buffer();
    void init_device_queue();
    void init_swap_chain(VkImageUsageFlags usageFlags);
    void initSwapChainImages();
    bool init_depth_buffer();
    void init_uniform_buffer();
    void init_descriptor_and_pipeline_layouts(bool use_texture);

        //
    void set_image_layout(uint32_t cmdIndex, VkImage image,
                          VkImageAspectFlags aspectMask,
                          VkImageLayout old_image_layout,
                          VkImageLayout new_image_layout);
    bool memory_type_from_properties(uint32_t typeBits,
                                     VkFlags requirements_mask,
                                     uint32_t *typeIndex);
    void init_renderpass(bool include_depth, bool clear);
    void init_shaders();
    void init_framebuffers(bool include_depth);
    void init_vertex_buffer(const void *vertexData,
                            uint32_t dataSize, uint32_t dataStride,
                            bool use_texture);

    void initVertexBufferWithNormal(const float *vertexData,
          uint32_t vertexDataSize, const float *normalData, uint32_t normalDataSiza);
    void initIndexForVertex(const uint16_t *indexData, size_t indexDataSize);
    void init_descriptor_pool(bool use_texture);
    void init_descriptor_set(bool use_texture);
    void init_pipeline_cache();
    void init_pipeline(VkBool32 include_depth, VkBool32 include_vi);

    void preDraw();

};

/* Number of samples needs to be the same at image creation,      */
/* renderpass creation and pipeline creation.                     */
#define NUM_SAMPLES VK_SAMPLE_COUNT_1_BIT

/* Number of descriptor sets needs to be the same at alloc,       */
/* pipeline layout creation, and descriptor set layout creation   */
#define NUM_DESCRIPTOR_SETS 1

/* Number of viewports and number of scissors have to be the same */
/* at pipeline creation and in any call to set them dynamically   */
/* They also have to be the same as each other                    */
#define NUM_VIEWPORTS 1
#define NUM_SCISSORS NUM_VIEWPORTS

/* Amount of time, in nanoseconds, to wait for a command buffer to complete */
#define FENCE_TIMEOUT 100000000

#endif //VULKANARMADILLO_VULKANDEVICEINFO_H
