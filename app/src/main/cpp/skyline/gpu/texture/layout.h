// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "texture.h"

namespace skyline::gpu::texture {
    /**
     * @return The size of a layer of the specified non-mipmapped block-slinear surface in bytes
     */
    size_t GetBlockLinearLayerSize(Dimensions dimensions,
                                   size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                   size_t gobBlockHeight, size_t gobBlockDepth);

    /**
     * @param isMultiLayer If the texture has more than one layer, a multi-layer texture requires alignment to a block at layer end
     * @return The size of a layer of the specified block-linear surface in bytes
     */
    size_t GetBlockLinearLayerSize(Dimensions dimensions,
                                   size_t formatBlockHeight, size_t formatBlockWidth, size_t formatBpb,
                                   size_t gobBlockHeight, size_t gobBlockDepth,
                                   size_t levelCount, bool isMultiLayer);

    /**
     * @return A vector of metadata about every mipmapped level of the supplied block-linear surface
     */
    std::vector<MipLevelLayout> GetBlockLinearMipLayout(Dimensions dimensions,
                                                        size_t formatBlockHeight, size_t formatBlockWidth, size_t formatBpb,
                                                        size_t gobBlockHeight, size_t gobBlockDepth,
                                                        size_t levelCount);

    /**
     * @brief Copies the contents of a blocklinear texture to a linear output buffer
     */
    void CopyBlockLinearToLinear(Dimensions dimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *blockLinear, u8 *linear);

    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    void CopyBlockLinearToLinear(const GuestTexture &guest, u8 *blockLinear, u8 *linear);

    /**
     * @brief Copies the contents of a blocklinear texture to a linear output buffer
     */
    void CopyLinearToBlockLinear(Dimensions dimensions,
                                 size_t formatBlockWidth, size_t formatBlockHeight, size_t formatBpb,
                                 size_t gobBlockHeight, size_t gobBlockDepth,
                                 u8 *linear, u8 *blockLinear);

    /**
     * @brief Copies the contents of a blocklinear guest texture to a linear output buffer
     */
    void CopyLinearToBlockLinear(const GuestTexture &guest, u8 *linear, u8 *blockLinear);

    /**
     * @brief Copies the contents of a pitch-linear guest texture to a linear output buffer
     * @note This does not support 3D textures
     */
    void CopyPitchLinearToLinear(const GuestTexture &guest, u8 *guestInput, u8 *linearOutput);

    /**
     * @brief Copies the contents of a linear buffer to a pitch-linear guest texture
     * @note This does not support 3D textures
     */
    void CopyLinearToPitchLinear(const GuestTexture &guest, u8 *linearInput, u8 *guestOutput);
}
