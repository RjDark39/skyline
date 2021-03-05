// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::loader {
    /**
     * @brief The contents of an executable binary abstracted away from the derivatives of Loader
     */
    struct Executable {
        /**
         * @brief The contents and offset of an executable segment
         */
        struct Segment {
            std::vector<u8> contents; //!< The raw contents of the segment
            size_t offset; //!< The offset from the base address to load the segment at
        };

        Segment text; //!< The .text segment container
        Segment ro; //!< The .rodata segment container
        Segment data; //!< The .data segment container
        size_t bssSize; //!< The size of the .bss segment

        struct RelativeSegment {
            size_t offset; //!< The offset from the base address of the related segment that this is segment is located at
            size_t size; //!< The size of the segment
        };

        RelativeSegment dynsym; //!< The .dynsym segment relative to .rodata
        RelativeSegment dynstr; //!< The .dynstr segment relative to .rodata
    };
}
