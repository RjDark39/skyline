// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::psm {

    /**
     * @url https://switchbrew.org/wiki/PTM_services#IPsmSession
     */
    class IPsmSession : public BaseService {
      public:
        IPsmSession(const DeviceState &state, ServiceManager &manager);
    };
}
