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

#include "utils/BackendBinding.h"

#include "common/Platform.h"
#include "nxt/nxt_wsi.h"

#include <cstdio>
#include "glad/glad.h"
#include "GLFW/glfw3.h"

namespace backend {
    namespace opengl {
        void Init(void* (*getProc)(const char*), nxtProcTable* procs, nxtDevice* device);
        void HACKCLEAR(nxtDevice device);
        void InitBackbuffer(nxtDevice device);
        void CommitBackbuffer(nxtDevice device);
    }
}

namespace utils {
    // TODO(kainino@chromium.org): probably make this reference counted
    class SwapChainGL {
        public:
            static nxtSwapChainImplementation Create(GLFWwindow* window) {
                nxtSwapChainImplementation impl = {};
                impl.Init = Init;
                impl.Destroy = Destroy;
                impl.Configure = Configure;
                impl.GetNextTexture = GetNextTexture;
                impl.Present = Present;
                impl.userData = new SwapChainGL(window);
                return impl;
            }

        private:
            GLFWwindow* window = nullptr;
            uint32_t cfgWidth = 0;
            uint32_t cfgHeight = 0;
            GLuint backFBO = 0;
            GLuint backTexture = 0;

            SwapChainGL(GLFWwindow* window)
                : window(window) {
            }

            ~SwapChainGL() {
                glDeleteTextures(1, &backTexture);
                glDeleteFramebuffers(1, &backFBO);
            }

            void HACKCLEAR() {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backFBO);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            void Init(nxtWSIContextGL*) {
                glGenTextures(1, &backTexture);
                glBindTexture(GL_TEXTURE_2D, backTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

                glGenFramebuffers(1, &backFBO);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, backFBO);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D, backTexture, 0);
            }

            nxtSwapChainError Configure(nxtTextureFormat format,
                    uint32_t width, uint32_t height) {
                if (format != NXT_TEXTURE_FORMAT_R8_G8_B8_A8_UNORM) {
                    return "unsupported format";
                }
                cfgWidth = width;
                cfgHeight = height;

                glBindTexture(GL_TEXTURE_2D, backTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

                return NXT_SWAP_CHAIN_NO_ERROR;
            }

            nxtSwapChainError GetNextTexture(nxtSwapChainNextTexture* nextTexture) {
                nextTexture->texture = reinterpret_cast<void*>(backTexture);
                return NXT_SWAP_CHAIN_NO_ERROR;
            }

            nxtSwapChainError Present() {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, backFBO);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, cfgWidth, cfgHeight, 0, 0, cfgWidth, cfgHeight,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glfwSwapBuffers(window);
                HACKCLEAR();

                return NXT_SWAP_CHAIN_NO_ERROR;
            }

            static void Init(void* userData, void* wsiContext) {
                auto* ctx = reinterpret_cast<nxtWSIContextGL*>(wsiContext);
                reinterpret_cast<SwapChainGL*>(userData)->Init(ctx);
            }

            static void Destroy(void* userData) {
                delete reinterpret_cast<SwapChainGL*>(userData);
            }

            static nxtSwapChainError Configure(void* userData, nxtTextureFormat format, uint32_t width, uint32_t height) {
                return reinterpret_cast<SwapChainGL*>(userData)->Configure(
                        format, width, height);
            }

            static nxtSwapChainError GetNextTexture(void* userData, nxtSwapChainNextTexture* nextTexture) {
                return reinterpret_cast<SwapChainGL*>(userData)->GetNextTexture(
                        nextTexture);
            }

            static nxtSwapChainError Present(void* userData) {
                return reinterpret_cast<SwapChainGL*>(userData)->Present();
            }
    };

    class OpenGLBinding : public BackendBinding {
        public:
            void SetupGLFWWindowHints() override {
                //#if defined(NXT_PLATFORM_APPLE)
                #if 1
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
                    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
                    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                #else
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
                    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
                    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                #endif
            }
            void GetProcAndDevice(nxtProcTable* procs, nxtDevice* device) override {
                glfwMakeContextCurrent(window);
                backend::opengl::Init(reinterpret_cast<void*(*)(const char*)>(glfwGetProcAddress), procs, device);

                backendDevice = *device;
                backend::opengl::InitBackbuffer(backendDevice);
            }

            void SwapBuffers() override {
            }

            nxtSwapChainImplementation GetSwapChainImplementation() override {
                return SwapChainGL::Create(window);
            }

        private:
            nxtDevice backendDevice = nullptr;
    };

    BackendBinding* CreateOpenGLBinding() {
        return new OpenGLBinding;
    }

}
