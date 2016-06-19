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

#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout (std140, binding = 0) uniform bufferVals {
    mat4 mvp;
} myBufferVals;
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 inNormal;
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec4 position;
out gl_PerVertex {
    vec4 gl_Position;
};
void main() {
   outNormal = inNormal;
   gl_Position = myBufferVals.mvp * vec4(pos, 1);
   position = gl_Position;
}