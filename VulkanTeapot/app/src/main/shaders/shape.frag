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
layout (location = 0) in vec3 normal;
layout (location = 1) in vec4 position;
layout (location = 0) out vec4 outColor;
void main() {

   vec3 vLight0 = vec3(0, -1, 0);
   vec3 halfVector = normalize(vLight0 + position.xyz);
   float NdotH = max(dot(normalize(normal), halfVector), 0.0);
   float fPower = 0.7;
   float specular = pow(NdotH, fPower);
   vec4 colorSpecular = vec4( vec3(0.7, 0.7, 0.7) * specular, 1);
   vec4 colorDiffuse = vec4(0.3, 0.3, 0.3, 1);
   outColor = colorDiffuse + colorSpecular;
}
