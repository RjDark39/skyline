// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IPsmServer.h"
#include "IPsmSession.h"

namespace skyline::service::psm {
    IPsmServer::IPsmServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IPsmServer::OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IPsmSession), session, response);
        return {};
    }
}
