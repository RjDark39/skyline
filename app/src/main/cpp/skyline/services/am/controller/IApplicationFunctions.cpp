#include "IApplicationFunctions.h"

namespace skyline::service::am {
    IApplicationFunctions::IApplicationFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IApplicationFunctions, "am:IApplicationFunctions", {
        {0x28, SFUNC(IApplicationFunctions::NotifyRunning)}
    }) {}

    void IApplicationFunctions::NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(1);
    }
}
