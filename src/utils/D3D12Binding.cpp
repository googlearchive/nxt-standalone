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

#include "common/Assert.h"
#include "nxt/nxt_wsi.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include <initializer_list>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

using Microsoft::WRL::ComPtr;

namespace backend {
namespace d3d12 {
    void Init(ComPtr<ID3D12Device> d3d12Device, nxtProcTable* procs, nxtDevice* device);
    ComPtr<ID3D12CommandQueue> GetCommandQueue(nxtDevice device);
    uint64_t GetSerial(const nxtDevice device);
    void NextSerial(nxtDevice device);
    void ExecuteCommandLists(nxtDevice device, std::initializer_list<ID3D12CommandList*> commandLists);
    void WaitForSerial(nxtDevice device, uint64_t serial);
    void OpenCommandList(nxtDevice device, ComPtr<ID3D12GraphicsCommandList>* commandList);
}
}

namespace utils {
    namespace {
        void ASSERT_SUCCESS(HRESULT hr) {
            ASSERT(SUCCEEDED(hr));
        }

        ComPtr<IDXGIFactory4> CreateFactory() {
            ComPtr<IDXGIFactory4> factory;

            uint32_t dxgiFactoryFlags = 0;
#ifdef _DEBUG
            // Enable the debug layer (requires the Graphics Tools "optional feature").
            // NOTE: Enabling the debug layer after device creation will invalidate the active device.
            {
                ComPtr<ID3D12Debug> debugController;
                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
                {
                    debugController->EnableDebugLayer();

                    // Enable additional debug layers.
                    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
                }
            }
#endif

            ASSERT_SUCCESS(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

            return factory;
        }
    }

    class SwapChainImplD3D12 {
        public:
            static nxtSwapChainImplementation Create(HWND window) {
                nxtSwapChainImplementation impl = {};
                impl.Init = Init;
                impl.Destroy = Destroy;
                impl.Configure = Configure;
                impl.GetNextTexture = GetNextTexture;
                impl.Present = Present;
                impl.userData = new SwapChainImplD3D12(window);
                return impl;
            }

        private:
            nxtDevice backendDevice = nullptr;

            static constexpr unsigned int kFrameCount = 2;

            HWND window = 0;
            ComPtr<IDXGIFactory4> factory = {};
            ComPtr<ID3D12CommandQueue> commandQueue = {};
            ComPtr<IDXGISwapChain3> swapChain = {};
            ComPtr<ID3D12Resource> renderTargetResources[kFrameCount] = {};

            // Frame synchronization. Updated every frame
            uint32_t renderTargetIndex = 0;
            uint32_t previousRenderTargetIndex = 0;
            uint64_t lastSerialRenderTargetWasUsed[kFrameCount] = {};

            SwapChainImplD3D12(HWND window)
                : window(window), factory(CreateFactory()) {
            }

            ~SwapChainImplD3D12() {
            }

            void Init(nxtWSIContextD3D12* ctx) {
                backendDevice = ctx->device;
                commandQueue = backend::d3d12::GetCommandQueue(backendDevice);
            }

            nxtSwapChainError Configure(nxtTextureFormat format,
                    uint32_t width, uint32_t height) {
                if (format != NXT_TEXTURE_FORMAT_R8_G8_B8_A8_UNORM) {
                    return "unsupported format";
                }
                ASSERT(width > 0);
                ASSERT(height > 0);

                DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
                swapChainDesc.Width = width;
                swapChainDesc.Height = height;
                swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swapChainDesc.BufferCount = kFrameCount;
                swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                swapChainDesc.SampleDesc.Count = 1;
                swapChainDesc.SampleDesc.Quality = 0;

                ComPtr<IDXGISwapChain1> swapChain1;
                ASSERT_SUCCESS(factory->CreateSwapChainForHwnd(
                    commandQueue.Get(),
                    window,
                    &swapChainDesc,
                    nullptr,
                    nullptr,
                    &swapChain1
                ));
                ASSERT_SUCCESS(swapChain1.As(&swapChain));

                for (uint32_t n = 0; n < kFrameCount; ++n) {
                    ASSERT_SUCCESS(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargetResources[n])));
                }

                // Get the initial render target and arbitrarily choose a "previous" render target that's different
                previousRenderTargetIndex = renderTargetIndex = swapChain->GetCurrentBackBufferIndex();
                previousRenderTargetIndex = renderTargetIndex == 0 ? 1 : 0;

                // Initial the serial for all render targets
                const uint64_t initialSerial = backend::d3d12::GetSerial(backendDevice);
                for (uint32_t n = 0; n < kFrameCount; ++n) {
                    lastSerialRenderTargetWasUsed[n] = initialSerial;
                }

                return NXT_SWAP_CHAIN_NO_ERROR;
            }

            nxtSwapChainError GetNextTexture(nxtSwapChainNextTexture* nextTexture) {
                // Transition last frame's render target back to being a render target
                {
                    ComPtr<ID3D12GraphicsCommandList> commandList = {};
                    backend::d3d12::OpenCommandList(backendDevice, &commandList);

                    D3D12_RESOURCE_BARRIER resourceBarrier;
                    resourceBarrier.Transition.pResource = renderTargetResources[renderTargetIndex].Get();
                    resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                    resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                    commandList->ResourceBarrier(1, &resourceBarrier);
                    ASSERT_SUCCESS(commandList->Close());
                    backend::d3d12::ExecuteCommandLists(backendDevice, { commandList.Get() });
                }

                backend::d3d12::NextSerial(backendDevice);

                previousRenderTargetIndex = renderTargetIndex;
                renderTargetIndex = swapChain->GetCurrentBackBufferIndex();

                // If the next render target is not ready to be rendered yet, wait until it is ready.
                // If the last completed serial is less than the last requested serial for this render target,
                // then the commands previously executed on this render target have not yet completed
                backend::d3d12::WaitForSerial(backendDevice, lastSerialRenderTargetWasUsed[renderTargetIndex]);

                lastSerialRenderTargetWasUsed[renderTargetIndex] = backend::d3d12::GetSerial(backendDevice);

                nextTexture->texture = renderTargetResources[renderTargetIndex].Get();
                return NXT_SWAP_CHAIN_NO_ERROR;
            }

            nxtSwapChainError Present() {
                // Transition current frame's render target for presenting
                {
                    ComPtr<ID3D12GraphicsCommandList> commandList = {};
                    backend::d3d12::OpenCommandList(backendDevice, &commandList);
                    D3D12_RESOURCE_BARRIER resourceBarrier;
                    resourceBarrier.Transition.pResource = renderTargetResources[renderTargetIndex].Get();
                    resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
                    resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                    commandList->ResourceBarrier(1, &resourceBarrier);
                    ASSERT_SUCCESS(commandList->Close());
                    backend::d3d12::ExecuteCommandLists(backendDevice, { commandList.Get() });
                }

                ASSERT_SUCCESS(swapChain->Present(1, 0));

                return NXT_SWAP_CHAIN_NO_ERROR;
            }

            // C stubs for C++ methods

            static void Init(void* userData, void* wsiContext) {
                auto* ctx = reinterpret_cast<nxtWSIContextD3D12*>(wsiContext);
                reinterpret_cast<SwapChainImplD3D12*>(userData)->Init(ctx);
            }

            static void Destroy(void* userData) {
                delete reinterpret_cast<SwapChainImplD3D12*>(userData);
            }

            static nxtSwapChainError Configure(void* userData, nxtTextureFormat format, uint32_t width, uint32_t height) {
                return reinterpret_cast<SwapChainImplD3D12*>(userData)->Configure(
                        format, width, height);
            }

            static nxtSwapChainError GetNextTexture(void* userData, nxtSwapChainNextTexture* nextTexture) {
                return reinterpret_cast<SwapChainImplD3D12*>(userData)->GetNextTexture(
                        nextTexture);
            }

            static nxtSwapChainError Present(void* userData) {
                return reinterpret_cast<SwapChainImplD3D12*>(userData)->Present();
            }
    };

    class D3D12Binding : public BackendBinding {
        public:
            void SetupGLFWWindowHints() override {
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            }

            void GetProcAndDevice(nxtProcTable* procs, nxtDevice* device) override {
                factory = CreateFactory();
                ASSERT(GetHardwareAdapter(factory.Get(), &hardwareAdapter));
                ASSERT_SUCCESS(D3D12CreateDevice(
                    hardwareAdapter.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    IID_PPV_ARGS(&d3d12Device)
                ));

                backend::d3d12::Init(d3d12Device, procs, device);
                backendDevice = *device;
            }

            uint64_t GetSwapChainImplementation() override {
                if (swapchainImpl.userData == nullptr) {
                    HWND win32Window = glfwGetWin32Window(window);
                    swapchainImpl = SwapChainImplD3D12::Create(win32Window);
                }
                return reinterpret_cast<uint64_t>(&swapchainImpl);
            }

        private:
            nxtDevice backendDevice = nullptr;
            nxtSwapChainImplementation swapchainImpl = {};

            // Initialization
            ComPtr<IDXGIFactory4> factory;
            ComPtr<IDXGIAdapter1> hardwareAdapter;
            ComPtr<ID3D12Device> d3d12Device;

            static bool GetHardwareAdapter(IDXGIFactory4* factory, IDXGIAdapter1** hardwareAdapter) {
                *hardwareAdapter = nullptr;
                for (uint32_t adapterIndex = 0; ; ++adapterIndex) {
                    IDXGIAdapter1* adapter = nullptr;
                    if (factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND) {
                        break; // No more adapters to enumerate.
                    }

                    // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                    if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
                        *hardwareAdapter = adapter;
                        return true;
                    }
                    adapter->Release();
                }
                return false;
            }
    };

    BackendBinding* CreateD3D12Binding() {
        return new D3D12Binding;
    }

}
