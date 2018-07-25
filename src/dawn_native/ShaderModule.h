// Copyright 2017 The Dawn Authors
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

#ifndef DAWNNATIVE_SHADERMODULE_H_
#define DAWNNATIVE_SHADERMODULE_H_

#include "common/Constants.h"
#include "dawn_native/Builder.h"
#include "dawn_native/Forward.h"
#include "dawn_native/RefCounted.h"

#include "dawn_native/dawn_platform.h"

#include <array>
#include <bitset>
#include <vector>

namespace spirv_cross {
    class Compiler;
}

namespace dawn_native {

    class ShaderModuleBase : public RefCounted {
      public:
        ShaderModuleBase(ShaderModuleBuilder* builder);

        DeviceBase* GetDevice() const;

        void ExtractSpirvInfo(const spirv_cross::Compiler& compiler);

        struct PushConstantInfo {
            std::bitset<kMaxPushConstants> mask;

            std::array<std::string, kMaxPushConstants> names;
            std::array<uint32_t, kMaxPushConstants> sizes;
            std::array<PushConstantType, kMaxPushConstants> types;
        };

        struct BindingInfo {
            // The SPIRV ID of the resource.
            uint32_t id;
            uint32_t base_type_id;
            dawn::BindingType type;
            bool used = false;
        };
        using ModuleBindingInfo =
            std::array<std::array<BindingInfo, kMaxBindingsPerGroup>, kMaxBindGroups>;

        const PushConstantInfo& GetPushConstants() const;
        const ModuleBindingInfo& GetBindingInfo() const;
        const std::bitset<kMaxVertexAttributes>& GetUsedVertexAttributes() const;
        dawn::ShaderStage GetExecutionModel() const;

        bool IsCompatibleWithPipelineLayout(const PipelineLayoutBase* layout);

      private:
        bool IsCompatibleWithBindGroupLayout(size_t group, const BindGroupLayoutBase* layout);

        DeviceBase* mDevice;
        PushConstantInfo mPushConstants = {};
        ModuleBindingInfo mBindingInfo;
        std::bitset<kMaxVertexAttributes> mUsedVertexAttributes;
        dawn::ShaderStage mExecutionModel;
    };

    class ShaderModuleBuilder : public Builder<ShaderModuleBase> {
      public:
        ShaderModuleBuilder(DeviceBase* device);

        std::vector<uint32_t> AcquireSpirv();

        // Dawn API
        void SetSource(uint32_t codeSize, const uint32_t* code);

      private:
        friend class ShaderModuleBase;

        ShaderModuleBase* GetResultImpl() override;

        std::vector<uint32_t> mSpirv;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_SHADERMODULE_H_
