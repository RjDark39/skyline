// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IPsmSession.h"

namespace skyline::service::psm {
    IPsmSession::IPsmSession(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}
