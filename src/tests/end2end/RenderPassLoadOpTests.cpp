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

#include "tests/NXTTest.h"

#include "utils/NXTHelpers.h"

#include <array>

constexpr static unsigned int kRTSize = 16;

class RenderPassLoadOpTests : public NXTTest {
    protected:
        void SetUp() override {
            NXTTest::SetUp();

            renderTarget = device.CreateTextureBuilder()
                .SetDimension(nxt::TextureDimension::e2D)
                .SetExtent(kRTSize, kRTSize, 1)
                .SetFormat(nxt::TextureFormat::R8G8B8A8Unorm)
                .SetMipLevels(1)
                .SetAllowedUsage(nxt::TextureUsageBit::OutputAttachment | nxt::TextureUsageBit::TransferSrc)
                .SetInitialUsage(nxt::TextureUsageBit::OutputAttachment)
                .GetResult();

            renderTargetView = renderTarget.CreateTextureViewBuilder().GetResult();

            RGBA8 zero(0, 0, 0, 0);
            std::fill(expectZero.begin(), expectZero.end(), zero);

            RGBA8 green(0, 255, 0, 255);
            std::fill(expectGreen.begin(), expectGreen.end(), green);
        }

        nxt::Texture renderTarget;
        nxt::TextureView renderTargetView;

        std::array<RGBA8, kRTSize * kRTSize> expectZero;
        std::array<RGBA8, kRTSize * kRTSize> expectGreen;
};

TEST_P(RenderPassLoadOpTests, ClearOnceLoadOnce) {
    // Part 1: clear once, check to make sure it's cleared

    auto renderpass1 = device.CreateRenderPassBuilder()
        .SetAttachmentCount(1)
        .SetSubpassCount(1)
        .AttachmentSetFormat(0, nxt::TextureFormat::R8G8B8A8Unorm)
        .AttachmentSetColorLoadOp(0, nxt::LoadOp::Clear)
        .SubpassSetColorAttachment(0, 0, 0)
        .GetResult();
    auto framebuffer1 = device.CreateFramebufferBuilder()
        .SetRenderPass(renderpass1)
        .SetDimensions(kRTSize, kRTSize)
        .SetAttachment(0, renderTargetView)
        .GetResult();
    framebuffer1.AttachmentSetClearColor(0, 0.0f, 1.0f, 0.0f, 1.0f); // green

    auto commands1 = device.CreateCommandBufferBuilder()
        .BeginRenderPass(renderpass1, framebuffer1)
        .BeginRenderSubpass()
            // Clear should occur implicitly
            // Store should occur implicitly
        .EndRenderSubpass()
        .EndRenderPass()
        .GetResult();

    // Initialized to 0 before the command buffer is submitted
    EXPECT_TEXTURE_RGBA8_EQ(expectZero.data(), renderTarget, 0, 0, kRTSize, kRTSize, 0);
    queue.Submit(1, &commands1);
    // Now cleared to green
    EXPECT_TEXTURE_RGBA8_EQ(expectGreen.data(), renderTarget, 0, 0, kRTSize, kRTSize, 0);

    // Part 2: load+store the texture, make sure its value doesn't change

    auto renderpass2 = device.CreateRenderPassBuilder()
        .SetAttachmentCount(1)
        .SetSubpassCount(1)
        .AttachmentSetFormat(0, nxt::TextureFormat::R8G8B8A8Unorm)
        .AttachmentSetColorLoadOp(0, nxt::LoadOp::Load)
        .SubpassSetColorAttachment(0, 0, 0)
        .GetResult();
    auto framebuffer2 = device.CreateFramebufferBuilder()
        .SetRenderPass(renderpass2)
        .SetDimensions(kRTSize, kRTSize)
        .SetAttachment(0, renderTargetView)
        .GetResult();
    framebuffer2.AttachmentSetClearColor(0, 1.0f, 0.0f, 0.0f, 1.0f); // red

    auto commands2 = device.CreateCommandBufferBuilder()
        .BeginRenderPass(renderpass2, framebuffer2)
        .BeginRenderSubpass()
            // No clear should occur
            // Store should occur implicitly
        .EndRenderSubpass()
        .EndRenderPass()
        .GetResult();

    queue.Submit(1, &commands2);
    // Should still be green after loading and storing back
    EXPECT_TEXTURE_RGBA8_EQ(expectGreen.data(), renderTarget, 0, 0, kRTSize, kRTSize, 0);
}

TEST_P(RenderPassLoadOpTests, LoadFromUninitialized) {
    auto renderpass = device.CreateRenderPassBuilder()
        .SetAttachmentCount(1)
        .SetSubpassCount(1)
        .AttachmentSetFormat(0, nxt::TextureFormat::R8G8B8A8Unorm)
        .AttachmentSetColorLoadOp(0, nxt::LoadOp::Load)
        .SubpassSetColorAttachment(0, 0, 0)
        .GetResult();
    auto framebuffer = device.CreateFramebufferBuilder()
        .SetRenderPass(renderpass)
        .SetDimensions(kRTSize, kRTSize)
        .SetAttachment(0, renderTargetView)
        .GetResult();

    framebuffer.AttachmentSetClearColor(0, 0.0f, 1.0f, 0.0f, 1.0f); // green

    EXPECT_TEXTURE_RGBA8_EQ(expectZero.data(), renderTarget, 0, 0, kRTSize, kRTSize, 0);

    auto commands = device.CreateCommandBufferBuilder()
        .BeginRenderPass(renderpass, framebuffer)
        .BeginRenderSubpass()
            // No clear should occur
            // Store should occur implicitly
        .EndRenderSubpass()
        .EndRenderPass()
        .GetResult();
    queue.Submit(1, &commands);

    EXPECT_TEXTURE_RGBA8_EQ(expectZero.data(), renderTarget, 0, 0, kRTSize, kRTSize, 0);
}

NXT_INSTANTIATE_TEST(RenderPassLoadOpTests, D3D12Backend, MetalBackend, OpenGLBackend)
