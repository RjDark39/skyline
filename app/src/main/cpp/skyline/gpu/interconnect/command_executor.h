// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/stable_vector.hpp>
#include <unordered_set>
#include "command_nodes.h"

namespace skyline::gpu::interconnect {
    /**
     * @brief Assembles a Vulkan command stream with various nodes and manages execution of the produced graph
     * @note This class is **NOT** thread-safe and should **ONLY** be utilized by a single thread
     */
    class CommandExecutor {
      private:
        GPU &gpu;
        CommandScheduler::ActiveCommandBuffer activeCommandBuffer;
        std::shared_ptr<FenceCycle> cycle;
        boost::container::stable_vector<node::NodeVariant> nodes;
        node::RenderPassNode *renderPass{};
        std::unordered_set<Texture *> syncTextures; //!< All textures that need to be synced prior to and after execution
        std::unordered_set<Buffer *> syncBuffers; //!< All buffers that need to be synced prior to and after execution

        /**
         * @return If a new render pass was created by the function or the current one was reused as it was compatible
         */
        bool CreateRenderPass(vk::Rect2D renderArea);

      public:
        CommandExecutor(const DeviceState &state);

        ~CommandExecutor();

        /**
         * @brief Attach the lifetime of the texture to the command buffer
         * @note The supplied texture **must** be locked by the calling thread
         * @note This'll automatically handle syncing of the texture in the most optimal way possible
         */
        void AttachTexture(TextureView *view);

        /**
         * @brief Attach the lifetime of a buffer to the command buffer
         * @note The supplied buffer **must** be locked by the calling thread
         * @note This'll automatically handle syncing of the buffer in the most optimal way possible
         */
        void AttachBuffer(BufferView *view);

        /**
         * @brief Attach the lifetime of the fence cycle dependency to the command buffer
         */
        void AttachDependency(std::shared_ptr<FenceCycleDependency> dependency);

        /**
         * @brief Adds a command that needs to be executed inside a subpass configured with certain attachments
         * @note Any texture supplied to this **must** be locked by the calling thread, it should also undergo no persistent layout transitions till execution
         * @note All attachments will automatically be attached and aren't required to be attached prior
         */
        void AddSubpass(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)> &&function, vk::Rect2D renderArea, span<TextureView *> inputAttachments = {}, span<TextureView *> colorAttachments = {}, TextureView *depthStencilAttachment = {});

        /**
         * @brief Adds a subpass that clears the entirety of the specified attachment with a color value, it may utilize VK_ATTACHMENT_LOAD_OP_CLEAR for a more efficient clear when possible
         * @note Any texture supplied to this **must** be locked by the calling thread, it should also undergo no persistent layout transitions till execution
         */
        void AddClearColorSubpass(TextureView *attachment, const vk::ClearColorValue &value);

        /**
         * @brief Adds a subpass that clears the entirety of the specified attachment with a depth/stencil value, it may utilize VK_ATTACHMENT_LOAD_OP_CLEAR for a more efficient clear when possible
         * @note Any texture supplied to this **must** be locked by the calling thread, it should also undergo no persistent layout transitions till execution
         */
        void AddClearDepthStencilSubpass(TextureView *attachment, const vk::ClearDepthStencilValue &value);

        /**
         * @brief Execute all the nodes and submit the resulting command buffer to the GPU
         */
        void Execute();
    };
}
