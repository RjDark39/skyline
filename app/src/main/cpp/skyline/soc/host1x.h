// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "host1x/syncpoint.h"

namespace skyline::soc::host1x {
    /**
     * @brief An abstraction for the graphics host, this handles DMA on behalf of the CPU when communicating to it's clients alongside handling syncpts
     * @note This is different from the GM20B Host, it serves a similar function and has an interface for accessing Host1X syncpts
     */
    class Host1X {
      public:
        std::array<Syncpoint, SyncpointCount> syncpoints{};
    };
}
