// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#include "IResolver.h"
#include <arpa/inet.h>
#include <common/settings.h>

namespace skyline::service::socket {
    static NetDbError AddrInfoErrorToNetDbError(i32 result) {
        switch (result) {
            case 0:
                return NetDbError::Success;
            case EAI_AGAIN:
                return NetDbError::TryAgain;
            case EAI_NODATA:
                return NetDbError::NoData;
            default:
                return NetDbError::HostNotFound;
        }
    }

    IResolver::IResolver(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IResolver::GetAddrInfoRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto [dataSize, resultCode]{GetAddrInfoRequestImpl(request)};
        response.Push<i32>(resultCode);  // errno
        response.Push(AddrInfoErrorToNetDbError(resultCode));  // NetDBErrorCode
        response.Push<u32>(dataSize);
        return {};
    }

    Result IResolver::GetHostByNameRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IResolver::GetAddrInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto [dataSize, resultCode]{GetAddrInfoRequestImpl(request)};
        response.Push<i32>(resultCode);  // errno
        response.Push(AddrInfoErrorToNetDbError(resultCode));  // NetDBErrorCode
        response.Push<u32>(dataSize);
        response.Push<u32>(0);
        return {};
    }

    Result IResolver::GetNameInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    std::pair<u32, i32> IResolver::GetAddrInfoRequestImpl(ipc::IpcRequest &request) {
        auto hostname{request.inputBuf.at(0).as_string(true)};
        auto service{request.inputBuf.at(1).as_string(true)};

        if (!(*state.settings->isInternetEnabled)) {
            Logger::Info("Internet access disabled, DNS Blocked: {}", hostname);
            return {0, -1};
        }

        addrinfo *result;
        i32 resultCode{getaddrinfo(hostname.data(), service.data(), nullptr, &result)};

        u32 dataSize{0};
        if (resultCode == 0 && result) {
            std::vector<u8> data{SerializeAddrInfo(result, resultCode, hostname)};
            dataSize = static_cast<u32>(data.size());
            request.outputBuf.at(0).copy_from(data);
            freeaddrinfo(result);
        }
        return {dataSize, resultCode};
    }

    // https://github.com/yuzu-emu/yuzu/blob/ce8f4da63834be0179d98a7720dee47d65f3ec06/src/core/hle/service/sockets/sfdnsres.cpp#L76
    std::vector<u8> IResolver::SerializeAddrInfo(const addrinfo* addrinfo, i32 result_code, std::string_view host) {
        std::vector<u8> data;

        auto* curAddrInfo{addrinfo};
        while (curAddrInfo != nullptr) {
            struct SerializedResponseHeader {
                u32 magic{util::SwapEndianness(0xbeefcafe)};
                u32 flags;
                u32 family;
                u32 socketType;
                u32 protocol;
                u32 addressLength;
            };
            static_assert(sizeof(SerializedResponseHeader) == 0x18);

            const auto addrSize{curAddrInfo->ai_addr && curAddrInfo->ai_addrlen > 0 ? curAddrInfo->ai_addrlen : 4};
            const auto canonnameSize{curAddrInfo->ai_canonname ? strlen(curAddrInfo->ai_canonname) + 1 : 1};

            const size_t lastSize{data.size()};
            auto curOutputRegion{span(data).subspan(lastSize)};
            data.resize(lastSize + sizeof(SerializedResponseHeader) + addrSize + canonnameSize);

            // Header in network byte order
            SerializedResponseHeader header{
                .family = htonl(curAddrInfo->ai_family),
                .flags = htonl(curAddrInfo->ai_flags),
                .socketType = htonl(curAddrInfo->ai_socktype),
                .protocol = htonl(curAddrInfo->ai_protocol),
                .addressLength = curAddrInfo->ai_addr ? htonl((u32)curAddrInfo->ai_addrlen) : 0,
            };

            auto* headerPtr{data.data() + lastSize};
            std::memcpy(headerPtr, &header, sizeof(SerializedResponseHeader));

            if (header.addressLength == 0) {
                std::memset(headerPtr + sizeof(SerializedResponseHeader), 0, 4);
            } else {
                switch (curAddrInfo->ai_family) {
                    case AF_INET: {
                        struct SockAddrIn {
                            u16 sin_family;
                            u16 sin_port;
                            u32 sin_addr;
                            u8 sin_zero[8];
                        };

                        const auto addr{*reinterpret_cast<sockaddr_in*>(curAddrInfo->ai_addr)};
                        SockAddrIn serializedAddr{
                            .sin_port = htons(addr.sin_port),
                            .sin_family = htons(addr.sin_family),
                            .sin_addr = htonl(addr.sin_addr.s_addr),
                        };
                        std::memcpy(headerPtr + sizeof(SerializedResponseHeader), &serializedAddr, sizeof(SockAddrIn));

                        char addrStringBuf[64]{};
                        inet_ntop(AF_INET, &addr.sin_addr, addrStringBuf, std::size(addrStringBuf));
                        Logger::Info("Resolved host '{}' to IPv4 address {}", host, addrStringBuf);
                        break;
                    }
                    case AF_INET6: {
                        struct SockAddrIn6 {
                            u16 sin6_family;
                            u16 sin6_port;
                            u32 sin6_flowinfo;
                            u8 sin6_addr[16];
                            u32 sin6_scope_id;
                        };

                        const auto addr{*reinterpret_cast<sockaddr_in6*>(curAddrInfo->ai_addr)};
                        SockAddrIn6 serializedAddr{
                            .sin6_family = htons(addr.sin6_family),
                            .sin6_port = htons(addr.sin6_port),
                            .sin6_flowinfo = htonl(addr.sin6_flowinfo),
                            .sin6_scope_id = htonl(addr.sin6_scope_id),
                        };
                        std::memcpy(serializedAddr.sin6_addr, &addr.sin6_addr, sizeof(SockAddrIn6::sin6_addr));
                        std::memcpy(headerPtr + sizeof(SerializedResponseHeader), &serializedAddr, sizeof(SockAddrIn6));

                        char addrStringBuf[64]{};
                        inet_ntop(AF_INET6, &addr.sin6_addr, addrStringBuf, std::size(addrStringBuf));
                        Logger::Info("Resolved host '{}' to IPv6 address {}", host, addrStringBuf);
                        break;
                    }
                    default:
                        std::memcpy(headerPtr + sizeof(SerializedResponseHeader), curAddrInfo->ai_addr, addrSize);
                        break;
                }
            }
            if (curAddrInfo->ai_canonname) {
                std::memcpy(headerPtr + addrSize, curAddrInfo->ai_canonname, canonnameSize);
            } else {
                *(headerPtr + sizeof(SerializedResponseHeader) + addrSize) = 0;
            }

            curAddrInfo = curAddrInfo->ai_next;
        }

        // 4-byte sentinel value
        data.push_back(0);
        data.push_back(0);
        data.push_back(0);
        data.push_back(0);

        return data;
    }
}
