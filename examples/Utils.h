// Copyright 2017 The NXT Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <nxt/nxtcpp.h>

bool InitUtils(int argc, const char** argv);
void DoSwapBuffers();
bool ShouldQuit();
void USleep(uint64_t usecs);

struct GLFWwindow;
struct GLFWwindow* GetGLFWWindow();

nxt::Device CreateCppNXTDevice();
nxt::ShaderModule CreateShaderModule(const nxt::Device& device, nxt::ShaderStage stage, const char* source);
void CreateDefaultRenderPass(const nxt::Device& device, nxt::RenderPass* renderPass, nxt::Framebuffer* framebuffer);
nxt::Buffer CreateFrozenBufferFromData(const nxt::Device& device, const void* data, uint32_t size, nxt::BufferUsageBit usage);
