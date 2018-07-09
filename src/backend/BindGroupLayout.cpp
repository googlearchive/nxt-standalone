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

#include "backend/BindGroupLayout.h"

#include "backend/Device.h"
#include "backend/ValidationUtils_autogen.h"
#include "common/BitSetIterator.h"
#include "common/HashUtils.h"

#include <functional>

namespace backend {

    MaybeError ValidateBindGroupLayoutDescriptor(DeviceBase*,
                                                 const nxt::BindGroupLayoutDescriptor* descriptor) {
        NXT_TRY_ASSERT(descriptor->nextInChain == nullptr, "nextInChain must be nullptr");

        std::bitset<kMaxBindingsPerGroup> bindingsSet;
        for (uint32_t i = 0; i < descriptor->numBindings; ++i) {
            auto& binding = descriptor->bindings[i];
            NXT_TRY_ASSERT(binding.binding <= kMaxBindingsPerGroup,
                           "some binding index exceeds the maximum value");
            NXT_TRY(ValidateShaderStageBit(binding.visibility));
            NXT_TRY(ValidateBindingType(binding.type));

            NXT_TRY_ASSERT(!bindingsSet[i], "some binding index was specified more than once");
            bindingsSet.set(binding.binding);
        }
        return {};
    }

    namespace {
        size_t HashBindingInfo(const BindGroupLayoutBase::LayoutBindingInfo& info) {
            size_t hash = Hash(info.mask);

            for (uint32_t binding : IterateBitSet(info.mask)) {
                HashCombine(&hash, info.visibilities[binding], info.types[binding]);
            }

            return hash;
        }

        bool operator==(const BindGroupLayoutBase::LayoutBindingInfo& a,
                        const BindGroupLayoutBase::LayoutBindingInfo& b) {
            if (a.mask != b.mask) {
                return false;
            }

            for (uint32_t binding : IterateBitSet(a.mask)) {
                if ((a.visibilities[binding] != b.visibilities[binding]) ||
                    (a.types[binding] != b.types[binding])) {
                    return false;
                }
            }

            return true;
        }
    }  // namespace

    // BindGroupLayoutBase

    BindGroupLayoutBase::BindGroupLayoutBase(DeviceBase* device,
                                             const nxt::BindGroupLayoutDescriptor* descriptor,
                                             bool blueprint)
        : mDevice(device), mIsBlueprint(blueprint) {
        for (uint32_t i = 0; i < descriptor->numBindings; ++i) {
            auto& binding = descriptor->bindings[i];

            uint32_t index = binding.binding;
            mBindingInfo.visibilities[index] = binding.visibility;
            mBindingInfo.types[index] = binding.type;

            ASSERT(!mBindingInfo.mask[index]);
            mBindingInfo.mask.set(index);
        }
    }

    BindGroupLayoutBase::~BindGroupLayoutBase() {
        // Do not register the actual cached object if we are a blueprint
        if (!mIsBlueprint) {
            mDevice->UncacheBindGroupLayout(this);
        }
    }

    const BindGroupLayoutBase::LayoutBindingInfo& BindGroupLayoutBase::GetBindingInfo() const {
        return mBindingInfo;
    }

    DeviceBase* BindGroupLayoutBase::GetDevice() const {
        return mDevice;
    }

    // BindGroupLayoutCacheFuncs

    size_t BindGroupLayoutCacheFuncs::operator()(const BindGroupLayoutBase* bgl) const {
        return HashBindingInfo(bgl->GetBindingInfo());
    }

    bool BindGroupLayoutCacheFuncs::operator()(const BindGroupLayoutBase* a,
                                               const BindGroupLayoutBase* b) const {
        return a->GetBindingInfo() == b->GetBindingInfo();
    }

}  // namespace backend
