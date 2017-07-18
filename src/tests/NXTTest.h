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

#include "nxt/nxtcpp.h"

#include <gtest/gtest.h>

// Getting data back from NXT is done in an async manners so all expectations are "deferred"
// until the end of the test. Also expectations use a copy to a MapRead buffer to get the data
// so resources should have the TransferSrc allowed usage bit if you want to add expectations on them.
#define EXPECT_BUFFER_U32_EQ(expected, buffer, offset) \
    AddBufferExpectation(__FILE__, __LINE__, buffer, offset, sizeof(uint32_t), new detail::ExpectEq<uint32_t>(expected));

#define EXPECT_BUFFER_U32_RANGE_EQ(expected, buffer, offset, count) \
    AddBufferExpectation(__FILE__, __LINE__, buffer, offset, sizeof(uint32_t) * count, new detail::ExpectEq<uint32_t>(expected, count));

// Test a pixel of the mip level 0 of a 2D texture.
#define EXPECT_PIXEL_RGBA8_EQ(expected, texture, x, y) \
    AddTextureExpectation(__FILE__, __LINE__, texture, x, y, 1, 1, sizeof(RGBA8), new detail::ExpectEq<RGBA8>(expected));

struct RGBA8 {
    constexpr RGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a): r(r), g(g), b(b), a(a) {
    }
    bool operator==(const RGBA8& other) const;
    bool operator!=(const RGBA8& other) const;

    uint8_t r, g, b, a;
};
std::ostream& operator<< (std::ostream& stream, const RGBA8& color);

// Backend types used in the NXT_INSTANTIATE_TEST
enum BackendType {
    D3D12Backend,
    MetalBackend,
    OpenGLBackend,
    VulkanBackend,
    NumBackendTypes,
};
std::ostream &operator<<(std::ostream& stream, BackendType backend);

namespace utils {
    class BackendBinding;
}

namespace detail {
    class Expectation;
}

class NXTTest : public ::testing::TestWithParam<BackendType> {
    public:
        ~NXTTest();

        void SetUp() override;
        void TearDown() override;

        bool IsD3D12() const;
        bool IsMetal() const;
        bool IsOpenGL() const;
        bool IsVulkan() const;

    protected:
        nxt::Device device;
        nxt::Queue queue;

        // Helper methods to implement the EXPECT_ macros
        void AddBufferExpectation(const char* file, int line, const nxt::Buffer& buffer, uint32_t offset, uint32_t size, detail::Expectation* expectation);
        void AddTextureExpectation(const char* file, int line, const nxt::Texture& texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t pixelSize, detail::Expectation* expectation);

        void WaitABit();
        void SwapBuffers();

    private:
        // MapRead buffers used to get data for the expectations
        struct ReadbackSlot {
            nxt::Buffer buffer;
            uint32_t bufferSize;
            const void* mappedData = nullptr;
        };
        std::vector<ReadbackSlot> readbackSlots;

        // Maps all the buffers and fill ReadbackSlot::mappedData
        void MapSlotsSynchronously();
        static void SlotMapReadCallback(nxtBufferMapReadStatus status, const void* data, nxtCallbackUserdata userdata);
        size_t numPendingMapOperations = 0;

        // Reserve space where the data for an expectation can be copied
        struct ReadbackReservation {
            nxt::Buffer buffer;
            size_t slot;
            uint32_t offset;
        };
        ReadbackReservation ReserveReadback(uint32_t readbackSize);

        struct DeferredExpectation {
            const char* file;
            int line;
            size_t readbackSlot;
            uint32_t readbackOffset;
            uint32_t size;
            uint32_t rowBytes;
            uint32_t rowPitch;
            detail::Expectation* expectation;
        };
        std::vector<DeferredExpectation> deferredExpectations;

        // Assuming the data is mapped, checks all expectations
        void ResolveExpectations();

        utils::BackendBinding* binding = nullptr;
};

// Instantiate the test once for each backend provided after the first argument. Use it like this:
//     NXT_INSTANTIATE_TEST(MyTestFixture, OpenGLBackend, MetalBackend)
#define NXT_INSTANTIATE_TEST(testName, firstParam, ...) \
    const decltype(firstParam) testName##params[] = { firstParam, ##__VA_ARGS__ }; \
    INSTANTIATE_TEST_CASE_P(, testName, \
        testing::ValuesIn(::detail::FilterBackends(testName##params, sizeof(testName##params) / sizeof(firstParam))), \
        testing::PrintToStringParamName());

namespace detail {
    // Helper functions used for NXT_INSTANTIATE_TEST
    bool IsBackendAvailable(BackendType type);
    std::vector<BackendType> FilterBackends(const BackendType* types, size_t numParams);

    // All classes used to implement the deferred expectations should inherit from this.
    class Expectation {
        public:
            virtual ~Expectation() = default;

            // Will be called with the buffer or texture data the expectation should check.
            virtual testing::AssertionResult Check(const void* data, size_t size) = 0;
    };

    // Expectation that checks the data is equal to some expected values.
    template<typename T>
    class ExpectEq : public Expectation {
        public:
            ExpectEq(T singleValue);
            ExpectEq(const T* values, const unsigned int count);

            testing::AssertionResult Check(const void* data, size_t size) override;

        private:
            std::vector<T> expected;
    };
    extern template class ExpectEq<uint32_t>;
    extern template class ExpectEq<RGBA8>;
}

