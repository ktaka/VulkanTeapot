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

#include <android/log.h>
#include <android_native_app_glue.h>
#include "VulkanMain.hpp"

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      // The window is being shown, get it ready.
      InitVulkan(app);
      break;
    case APP_CMD_TERM_WINDOW:
      // The window is being hidden or closed, clean it up.
      DeleteVulkan();
      break;
    default:
      __android_log_print(ANDROID_LOG_INFO, "Vulkan Tutorials",
                          "event not handled: %d", cmd);
  }
}

static const char *LOG_TAG = "VulkanTeapot";
static float xpos;
static float ypos;

/*
 * 入力イベントがあった時に呼び出される関数
 */
static int32_t handle_input(struct android_app* app, AInputEvent* event)
{
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    /* 画面のタップ操作の場合 */
    __android_log_print (ANDROID_LOG_INFO, LOG_TAG,
                         "Motion event: action=%d flag=%d metaState=0x%x",
                         AMotionEvent_getAction(event),
                         AMotionEvent_getFlags(event),
                         AMotionEvent_getMetaState(event));
    int32_t eventAction = AMotionEvent_getAction(event);
    if (eventAction == AMOTION_EVENT_ACTION_DOWN) {
      /* タップ操作が画面に触れた状態の場合 */
      /* タップ位置の取得 */
      xpos = AMotionEvent_getX(event, 0); /* X座標 */
      ypos = AMotionEvent_getY(event, 0); /* Y座標 */
      __android_log_print (ANDROID_LOG_INFO, LOG_TAG,
                           "pos = (%f, %f)",
                           xpos, ypos);
    } else if (eventAction == AMOTION_EVENT_ACTION_UP) {
      /* タップ操作の指が画面から離れた時 */
      /* タップ位置の取得 */
      float x = AMotionEvent_getX(event, 0); /* X座標 */
      float y = AMotionEvent_getY(event, 0); /* Y座標 */
      __android_log_print (ANDROID_LOG_INFO, LOG_TAG,
                           "up = (%f, %f)",
                           x, y);
      if (x == xpos && y == ypos) {
        /*
         * 指が触れた時と離れた時の座標に差がなければタップ操作を有効
         */
      }
    } else if (eventAction == AMOTION_EVENT_ACTION_MOVE) {
      /* 画面に指が触れた状態で動かした場合 */
      /* タップ位置の取得 */
      float x = AMotionEvent_getX(event, 0); /* X座標 */
      float y = AMotionEvent_getY(event, 0); /* Y座標 */
      __android_log_print (ANDROID_LOG_INFO, LOG_TAG,
                           "move = (%f, %f) [%f]",
                           x, y, x - xpos);
      /*
       * タッチ操作を回転に置き換える
       * 横方向の移動量を Y 軸の回転量に変換
       */
      //float yrot = 360.0f / engine->width * (x - xpos);
      VulkanOnDrag(x - xpos, y - ypos);
      /* 画面を描画するフラグを ON */
      //needToDraw = 1;
    }
    return 1;
  } else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
    /* キー入力操作の場合 */
    __android_log_print (ANDROID_LOG_INFO, LOG_TAG,
                         "Key event: action=%d keyCode=%d metaState=0x%x",
                         AKeyEvent_getAction(event),
                         AKeyEvent_getKeyCode(event),
                         AKeyEvent_getMetaState(event));
  }

  return 0;
}

void android_main(struct android_app* app) {
  // Magic call, please ignore it (Android specific).
  app_dummy();

  // Set the callback to process system events
  app->onAppCmd = handle_cmd;
  app->onInputEvent = handle_input;

  // Used to poll the events in the main loop
  int events;
  android_poll_source* source;

  // Main loop
  do {
    if (ALooper_pollAll(IsVulkanReady() ? 1 : 0, nullptr,
                        &events, (void**)&source) >= 0) {
      if (source != NULL) source->process(app, source);
    }

    // render if vulkan is ready
    if (IsVulkanReady()) {
      VulkanDrawFrame();
    }
  } while (app->destroyRequested == 0);
}
