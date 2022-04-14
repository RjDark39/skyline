// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/thread_local.h>
#include "fence_cycle.h"

namespace skyline::gpu {
    /**
     * @brief The allocation and synchronized submission of command buffers to the host GPU is handled by this class
     */
    class CommandScheduler {
      private:
        /**
         * @brief A wrapper around a command buffer which tracks its state to avoid concurrent usage
         */
        struct CommandBufferSlot {
            std::atomic_flag active{true}; //!< If the command buffer is currently being recorded to
            const vk::raii::Device &device;
            vk::raii::CommandBuffer commandBuffer;
            vk::raii::Fence fence; //!< A fence used for tracking all submits of a buffer
            std::shared_ptr<FenceCycle> cycle; //!< The latest cycle on the fence, all waits must be performed through this

            CommandBufferSlot(vk::raii::Device &device, vk::CommandBuffer commandBuffer, vk::raii::CommandPool &pool);

            /**
             * @brief Attempts to allocate the buffer if it is free (Not being recorded/executing)
             * @return If the allocation was successful or not
             */
            static bool AllocateIfFree(CommandBufferSlot &slot);
        };

        GPU &gpu;

        /**
         * @brief A command pool designed to be thread-local to respect external synchronization for all command buffers and the associated pool
         * @note If we utilized a single global pool there would need to be a mutex around command buffer recording which would incur significant costs
         */
        struct CommandPool {
            vk::raii::CommandPool vkCommandPool;
            std::list<CommandBufferSlot> buffers;

            template<typename... Args>
            constexpr CommandPool(Args &&... args) : vkCommandPool(std::forward<Args>(args)...) {}
        };
        ThreadLocal<CommandPool> pool;

      public:
        /**
         * @brief An active command buffer occupies a slot and ensures that its status is updated correctly
         */
        class ActiveCommandBuffer {
          private:
            CommandBufferSlot &slot;

          public:
            constexpr ActiveCommandBuffer(CommandBufferSlot &slot) : slot(slot) {}

            ~ActiveCommandBuffer() {
                slot.active.clear(std::memory_order_release);
            }

            vk::Fence GetFence() {
                return *slot.fence;
            }

            std::shared_ptr<FenceCycle> GetFenceCycle() {
                return slot.cycle;
            }

            vk::raii::CommandBuffer &operator*() {
                return slot.commandBuffer;
            }

            vk::raii::CommandBuffer *operator->() {
                return &slot.commandBuffer;
            }

            /**
             * @brief Resets the state of the command buffer with a new FenceCycle
             * @note This should be used when a single allocated command buffer is used for all submissions from a component
             */
            std::shared_ptr<FenceCycle> Reset() {
                slot.cycle->Wait();
                slot.cycle = std::make_shared<FenceCycle>(slot.device, *slot.fence);
                slot.commandBuffer.reset();
                return slot.cycle;
            }
        };

        CommandScheduler(GPU &gpu);

        /**
         * @brief Allocates an existing or new primary command buffer from the pool
         */
        ActiveCommandBuffer AllocateCommandBuffer();

        /**
         * @brief Submits a single command buffer to the GPU queue with an optional fence
         */
        void SubmitCommandBuffer(const vk::raii::CommandBuffer &commandBuffer, vk::Fence fence = {});

        /**
         * @brief Submits a command buffer recorded with the supplied function synchronously
         */
        template<typename RecordFunction>
        std::shared_ptr<FenceCycle> Submit(RecordFunction recordFunction) {
            auto commandBuffer{AllocateCommandBuffer()};
            try {
                commandBuffer->begin(vk::CommandBufferBeginInfo{
                    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                });
                recordFunction(*commandBuffer);
                commandBuffer->end();
                SubmitCommandBuffer(*commandBuffer, commandBuffer.GetFence());
                return commandBuffer.GetFenceCycle();
            } catch (...) {
                commandBuffer.GetFenceCycle()->Cancel();
                std::rethrow_exception(std::current_exception());
            }
        }

        /**
         * @note Same as Submit but with FenceCycle as an argument rather than return value
         */
        template<typename RecordFunction>
        std::shared_ptr<FenceCycle> SubmitWithCycle(RecordFunction recordFunction) {
            auto commandBuffer{AllocateCommandBuffer()};
            try {
                commandBuffer->begin(vk::CommandBufferBeginInfo{
                    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                });
                recordFunction(*commandBuffer, commandBuffer.GetFenceCycle());
                commandBuffer->end();
                SubmitCommandBuffer(*commandBuffer, commandBuffer.GetFence());
                return commandBuffer.GetFenceCycle();
            } catch (...) {
                commandBuffer.GetFenceCycle()->Cancel();
                std::rethrow_exception(std::current_exception());
            }
        }
    };
}
