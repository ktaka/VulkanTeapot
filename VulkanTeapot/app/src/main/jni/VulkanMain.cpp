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
#include <android/log.h>
#include <android_native_app_glue.h>
#include "vulkan_wrapper.h"
#include "VulkanMain.hpp"
#include "VulkanDevice.h"

// Android log function wrappers
static const char* kTAG = "Vulkan-Tutorial04";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Vulkan call wrapper
#define CALL_VK(func)                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "Tutorial ",               \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }

VulkanDevice *device = nullptr;

// Android Native App pointer...
android_app* androidAppCtx = nullptr;

// InitVulkan:
//   Initialize Vulkan Context when android application window is created
//   upon return, vulkan is ready to draw frames
bool InitVulkan(android_app* app) {
    androidAppCtx = app;

    if (!InitVulkan()) {
        LOGW("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    device = new VulkanDevice(app);

  return true;
}


// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady(void) {
    if (device) {
        return device->isReady();
    } else {
        return false;
    }
}

void DeleteVulkan() {
    delete device;
}

// Draw one frame
bool VulkanDrawFrame(void) {
    //device->draw();
    return true;
}

void VulkanOnDrag(float x, float y) {
    if (device) {
        float yrot = 360.0f / device->width * x;
        device->rotateModel(0, yrot, 0);
    }
}

