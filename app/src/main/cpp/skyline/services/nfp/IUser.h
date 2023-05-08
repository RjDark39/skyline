// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>

namespace skyline::service::nfp {
    /**
     * @brief IUser is used by applications to access NFP (Nintendo Figurine Protocol) devices
     * @url https://switchbrew.org/wiki/NFC_services#IUser_3
     */
    class IUser : public BaseService {
      private:
        std::shared_ptr<type::KEvent> attachAvailabilityChangeEvent; //!< Signalled on NFC device availability changes

        enum class State : u32 {
            NotInitialized = 0,
            Initialized = 1,
        } nfpState{State::NotInitialized};

        enum class IUDeviceState : u32 {
            Initialized = 0,
            Searching = 1,
            Found = 2,
            Removed = 3,
            Mounted = 4,
            Unavailable = 5,
            Finalized = 6
        } nfpDeviceState{IUDeviceState::Unavailable};

        enum class ModelType : u32 {
            Amiibo = 0
        } nfpModelType{ModelType::Amiibo};

        enum class MountTarget : u32 {
            ROM = 1,
            RAM = 2,
            All = 3
        } nfpMountTarget{MountTarget::All};

      public:
        IUser(const DeviceState &state, ServiceManager &manager);

        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#Finalize
         */
        Result Finalize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#ListDevices
         */
        Result ListDevices(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#StartDetection
         */
        Result StartDetection(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#StopDetection
         */
        Result StopDetection(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#Mount
         */
        Result Mount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#Unmount
         */
        Result Unmount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#OpenApplicationArea
         */
        Result OpenApplicationArea(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetApplicationArea
         */
        Result GetApplicationArea(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#SetApplicationArea
         */
        Result SetApplicationArea(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#Flush
         */
        Result Flush(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#Restore
         */
        Result Restore(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#CreateApplicationArea
         */
        Result CreateApplicationArea(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetTagInfo
         */
        Result GetTagInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetRegisterInfo
         */
        Result GetRegisterInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetCommonInfo
         */
        Result GetCommonInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetModelInfo
         */
        Result GetModelInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#AttachActivateEvent
         */
        Result AttachActivateEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#AttachDeactivateEvent
         */
        Result AttachDeactivateEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetState_2
         */
        Result GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetDeviceState
         */
        Result GetDeviceState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetNpadId
         */
        Result GetNpadId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetApplicationAreaSize
         */
        Result GetApplicationAreaSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#AttachAvailabilityChangeEvent
         */
        Result AttachAvailabilityChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#RecreateApplicationArea
         */
        Result RecreateApplicationArea(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IUser, Initialize),
            SFUNC(0x1, IUser, Finalize),
            SFUNC(0x2, IUser, ListDevices),
            SFUNC(0x3, IUser, StartDetection),
            SFUNC(0x4, IUser, StopDetection),
            SFUNC(0x5, IUser, Mount),
            SFUNC(0x6, IUser, Unmount),
            SFUNC(0x7, IUser, OpenApplicationArea),
            SFUNC(0x8, IUser, GetApplicationArea),
            SFUNC(0x9, IUser, SetApplicationArea),
            SFUNC(0x10, IUser, Flush),
            SFUNC(0x11, IUser, Restore),
            SFUNC(0x12, IUser, CreateApplicationArea),
            SFUNC(0x13, IUser, GetTagInfo),
            SFUNC(0x14, IUser, GetRegisterInfo),
            SFUNC(0x15, IUser, GetCommonInfo),
            SFUNC(0x16, IUser, GetModelInfo),
            SFUNC(0x17, IUser, AttachActivateEvent),
            SFUNC(0x18, IUser, AttachDeactivateEvent),
            SFUNC(0x19, IUser, GetState),
            SFUNC(0x20, IUser, GetDeviceState),
            SFUNC(0x21, IUser, GetNpadId),
            SFUNC(0x22, IUser, GetApplicationAreaSize),
            SFUNC(0x23, IUser, AttachAvailabilityChangeEvent),
            SFUNC(0x24, IUser, RecreateApplicationArea)
        )
    };
}
