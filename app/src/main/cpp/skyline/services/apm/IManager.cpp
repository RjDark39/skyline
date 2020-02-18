#include "ISession.h"
#include "IManager.h"

namespace skyline::service::apm {
    IManager::IManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::apm_IManager, "apm:IManager", {
        {0x0, SFUNC(IManager::OpenSession)}
    }) {}

    void IManager::OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISession>(state, manager), session, response);
    }
}
