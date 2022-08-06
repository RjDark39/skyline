// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "command_executor.h"

namespace skyline::gpu::interconnect {
    CommandExecutor::CommandExecutor(const DeviceState &state) : gpu{*state.gpu}, activeCommandBuffer{gpu.scheduler.AllocateCommandBuffer()}, cycle{activeCommandBuffer.GetFenceCycle()}, megaBuffer{gpu.buffer.AcquireMegaBuffer(cycle)} {}

    CommandExecutor::~CommandExecutor() {
        cycle->Cancel();
    }

    bool CommandExecutor::CreateRenderPassWithSubpass(vk::Rect2D renderArea, span<TextureView *> inputAttachments, span<TextureView *> colorAttachments, TextureView *depthStencilAttachment) {
        auto addSubpass{[&] {
            renderPass->AddSubpass(inputAttachments, colorAttachments, depthStencilAttachment, gpu);

            lastSubpassAttachments.clear();
            auto insertAttachmentRange{[this](auto &attachments) -> std::pair<size_t, size_t> {
                size_t beginIndex{lastSubpassAttachments.size()};
                lastSubpassAttachments.insert(lastSubpassAttachments.end(), attachments.begin(), attachments.end());
                return {beginIndex, attachments.size()};
            }};

            auto rangeToSpan{[this](auto &range) -> span<TextureView *> {
                return {lastSubpassAttachments.data() + range.first, range.second};
            }};

            auto inputAttachmentRange{insertAttachmentRange(inputAttachments)};
            auto colorAttachmentRange{insertAttachmentRange(colorAttachments)};

            lastSubpassInputAttachments = rangeToSpan(inputAttachmentRange);
            lastSubpassColorAttachments = rangeToSpan(colorAttachmentRange);
            lastSubpassDepthStencilAttachment = depthStencilAttachment;
        }};

        if (renderPass == nullptr || (renderPass && (renderPass->renderArea != renderArea || subpassCount >= gpu.traits.quirks.maxSubpassCount))) {
            // We need to create a render pass if one doesn't already exist or the current one isn't compatible
            if (renderPass != nullptr)
                nodes.emplace_back(std::in_place_type_t<node::RenderPassEndNode>());
            renderPass = &std::get<node::RenderPassNode>(nodes.emplace_back(std::in_place_type_t<node::RenderPassNode>(), renderArea));
            addSubpass();
            subpassCount = 1;
            return false;
        } else {
            if (ranges::equal(lastSubpassInputAttachments, inputAttachments) &&
                ranges::equal(lastSubpassColorAttachments, colorAttachments) &&
                lastSubpassDepthStencilAttachment == depthStencilAttachment) {
                // The last subpass had the same attachments, so we can reuse them
                return false;
            } else {
                // The last subpass had different attachments, so we need to create a new one
                addSubpass();
                subpassCount++;
                return true;
            }
        }
    }

    void CommandExecutor::FinishRenderPass() {
        if (renderPass) {
            nodes.emplace_back(std::in_place_type_t<node::RenderPassEndNode>());

            renderPass = nullptr;
            subpassCount = 0;

            lastSubpassAttachments.clear();
            lastSubpassInputAttachments = nullptr;
            lastSubpassColorAttachments = nullptr;
            lastSubpassDepthStencilAttachment = nullptr;
        }
    }

    void CommandExecutor::AttachTexture(TextureView *view) {
        auto texture{view->texture.get()};
        if (!attachedTextures.contains(texture)) {
            texture->WaitOnFence();
            texture->cycle = cycle;
            attachedTextures.emplace(texture);
        }
        cycle->AttachObject(view->shared_from_this());
    }

    void CommandExecutor::AttachBuffer(BufferView &view) {
        view->buffer->SynchronizeHost();

        if (!attachedBuffers.contains(view.bufferDelegate)) {
            view.AttachCycle(cycle);
            attachedBuffers.emplace(view.bufferDelegate);
        }
    }

    void CommandExecutor::AttachDependency(const std::shared_ptr<FenceCycleDependency> &dependency) {
        cycle->AttachObject(dependency);
    }

    void CommandExecutor::AddSubpass(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)> &&function, vk::Rect2D renderArea, span<TextureView *> inputAttachments, span<TextureView *> colorAttachments, TextureView *depthStencilAttachment, bool exclusiveSubpass) {
        if (exclusiveSubpass)
            FinishRenderPass();

        bool gotoNext{CreateRenderPassWithSubpass(renderArea, inputAttachments, colorAttachments, depthStencilAttachment ? &*depthStencilAttachment : nullptr)};
        if (gotoNext)
            nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), std::forward<decltype(function)>(function));
        else
            nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), std::forward<decltype(function)>(function));

        if (exclusiveSubpass)
            FinishRenderPass();
    }

    void CommandExecutor::AddOutsideRpCommand(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> &&function) {
        if (renderPass)
            FinishRenderPass();

        nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), std::forward<decltype(function)>(function));
    }

    void CommandExecutor::AddClearColorSubpass(TextureView *attachment, const vk::ClearColorValue &value) {
        bool gotoNext{CreateRenderPassWithSubpass(vk::Rect2D{.extent = attachment->texture->dimensions}, {}, attachment, nullptr)};
        if (renderPass->ClearColorAttachment(0, value, gpu)) {
            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[scissor = attachment->texture->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                commandBuffer.clearAttachments(vk::ClearAttachment{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .colorAttachment = 0,
                    .clearValue = value,
                }, vk::ClearRect{
                    .rect = vk::Rect2D{.extent = scissor},
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                });
            }};

            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
            else
                nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), function);
        }
    }

    void CommandExecutor::AddClearDepthStencilSubpass(TextureView *attachment, const vk::ClearDepthStencilValue &value) {
        bool gotoNext{CreateRenderPassWithSubpass(vk::Rect2D{.extent = attachment->texture->dimensions}, {}, {}, attachment)};
        if (renderPass->ClearDepthStencilAttachment(value, gpu)) {
            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[aspect = attachment->format->vkAspect, extent = attachment->texture->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                commandBuffer.clearAttachments(vk::ClearAttachment{
                    .aspectMask = aspect,
                    .clearValue = value,
                }, vk::ClearRect{
                    .rect.extent = extent,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                });
            }};

            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
            else
                nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), function);
        }
    }

    void CommandExecutor::AddFlushCallback(std::function<void()> &&callback) {
        flushCallbacks.emplace_back(std::forward<decltype(callback)>(callback));
    }

    void CommandExecutor::SubmitInternal() {
        if (renderPass)
            FinishRenderPass();

        {
            auto &commandBuffer{*activeCommandBuffer};
            commandBuffer.begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            });

            for (auto texture : attachedTextures) {
                texture->SynchronizeHostWithBuffer(commandBuffer, cycle, true);
                texture->MarkGpuDirty();
            }

            for (const auto &delegate : attachedBuffers)
                delegate->usageCallback = nullptr;

            vk::RenderPass lRenderPass;
            u32 subpassIndex;

            using namespace node;
            for (NodeVariant &node : nodes) {
                #define NODE(name) [&](name& node) { node(commandBuffer, cycle, gpu); }
                std::visit(VariantVisitor{
                    NODE(FunctionNode),

                    [&](RenderPassNode &node) {
                        lRenderPass = node(commandBuffer, cycle, gpu);
                        subpassIndex = 0;
                    },

                    [&](NextSubpassNode &node) {
                        node(commandBuffer, cycle, gpu);
                        ++subpassIndex;
                    },
                    [&](SubpassFunctionNode &node) { node(commandBuffer, cycle, gpu, lRenderPass, subpassIndex); },
                    [&](NextSubpassFunctionNode &node) { node(commandBuffer, cycle, gpu, lRenderPass, ++subpassIndex); },

                    NODE(RenderPassEndNode),
                }, node);
                #undef NODE
            }

            commandBuffer.end();
            gpu.scheduler.SubmitCommandBuffer(commandBuffer, activeCommandBuffer.GetFence());

                for (const auto &delegate : attachedBuffers)
                    delegate->view->megabufferOffset = 0;

            nodes.clear();
            attachedTextures.clear();
            attachedBuffers.clear();
        }
    }

    void CommandExecutor::Submit() {
        if (!nodes.empty()) {
            TRACE_EVENT("gpu", "CommandExecutor::Submit");
            SubmitInternal();
            activeCommandBuffer = gpu.scheduler.AllocateCommandBuffer();
            cycle = activeCommandBuffer.GetFenceCycle();
            megaBuffer = gpu.buffer.AcquireMegaBuffer(cycle);
        }
    }

    void CommandExecutor::SubmitWithFlush() {
        if (!nodes.empty()) {
            TRACE_EVENT("gpu", "CommandExecutor::SubmitWithFlush");
            SubmitInternal();
            cycle = activeCommandBuffer.Reset();
            megaBuffer.Reset();
        }
    }
}
