// Copyright (c) 2015-2019 The Khronos Group Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// ---- Exceptions to the Apache 2.0 License: ----
// 
// As an exception, if you use this Software to generate code and portions of
// this Software are embedded into the generated code as a result, you may
// redistribute such product without providing attribution as would otherwise
// be required by Sections 4(a), 4(b) and 4(d) of the License.
// 
// In addition, if you combine or link code generated by this Software with
// software that is licensed under the GPLv2 or the LGPL v2.0 or 2.1
// ("`Combined Software`") and if a court of competent jurisdiction determines
// that the patent provision (Section 3), the indemnity provision (Section 9)
// or other Section of the License conflicts with the conditions of the
// applicable GPL or LGPL license, you may retroactively and prospectively
// choose to deem waived or otherwise exclude such Section(s) of the License,
// but only in their entirety and only with respect to the Combined Software.
//     

// This header is generated from the Khronos Vulkan XML API Registry.

#pragma once

#include "../handles.hpp"
#include "VkAcquire.hpp"
#include "VkAcceleration.hpp"
#include "VkApplication.hpp"
#include "VkAllocation.hpp"
#include "VkBind.hpp"
#include "VkCooperative.hpp"
#include "VkAndroid.hpp"
#include "VkBase.hpp"
#include "VkAttachment.hpp"
#include "VkBuffer.hpp"
#include "VkCalibrated.hpp"
#include "VkCheckpoint.hpp"
#include "VkConformance.hpp"
#include "VkClear.hpp"
#include "VkCmd.hpp"
#include "VkCoarse.hpp"
#include "VkCommand.hpp"
#include "VkComponent.hpp"
#include "VkCopy.hpp"
#include "VkCompute.hpp"
#include "VkConditional.hpp"

namespace VULKAN_HPP_NAMESPACE
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
  struct D3D12FenceSubmitInfoKHR
  {
    VULKAN_HPP_CONSTEXPR D3D12FenceSubmitInfoKHR( uint32_t waitSemaphoreValuesCount_ = {},
                                                  const uint64_t* pWaitSemaphoreValues_ = {},
                                                  uint32_t signalSemaphoreValuesCount_ = {},
                                                  const uint64_t* pSignalSemaphoreValues_ = {} ) VULKAN_HPP_NOEXCEPT
      : waitSemaphoreValuesCount( waitSemaphoreValuesCount_ )
      , pWaitSemaphoreValues( pWaitSemaphoreValues_ )
      , signalSemaphoreValuesCount( signalSemaphoreValuesCount_ )
      , pSignalSemaphoreValues( pSignalSemaphoreValues_ )
    {}

    VULKAN_HPP_CONSTEXPR D3D12FenceSubmitInfoKHR( D3D12FenceSubmitInfoKHR const& rhs ) VULKAN_HPP_NOEXCEPT
      : pNext( rhs.pNext )
      , waitSemaphoreValuesCount( rhs.waitSemaphoreValuesCount )
      , pWaitSemaphoreValues( rhs.pWaitSemaphoreValues )
      , signalSemaphoreValuesCount( rhs.signalSemaphoreValuesCount )
      , pSignalSemaphoreValues( rhs.pSignalSemaphoreValues )
    {}

    D3D12FenceSubmitInfoKHR & operator=( D3D12FenceSubmitInfoKHR const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      memcpy( &pNext, &rhs.pNext, sizeof( D3D12FenceSubmitInfoKHR ) - offsetof( D3D12FenceSubmitInfoKHR, pNext ) );
      return *this;
    }

    D3D12FenceSubmitInfoKHR( VkD3D12FenceSubmitInfoKHR const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      *this = rhs;
    }

    D3D12FenceSubmitInfoKHR& operator=( VkD3D12FenceSubmitInfoKHR const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      *this = *reinterpret_cast<VULKAN_HPP_NAMESPACE::D3D12FenceSubmitInfoKHR const *>(&rhs);
      return *this;
    }

    D3D12FenceSubmitInfoKHR & setPNext( const void* pNext_ ) VULKAN_HPP_NOEXCEPT
    {
      pNext = pNext_;
      return *this;
    }

    D3D12FenceSubmitInfoKHR & setWaitSemaphoreValuesCount( uint32_t waitSemaphoreValuesCount_ ) VULKAN_HPP_NOEXCEPT
    {
      waitSemaphoreValuesCount = waitSemaphoreValuesCount_;
      return *this;
    }

    D3D12FenceSubmitInfoKHR & setPWaitSemaphoreValues( const uint64_t* pWaitSemaphoreValues_ ) VULKAN_HPP_NOEXCEPT
    {
      pWaitSemaphoreValues = pWaitSemaphoreValues_;
      return *this;
    }

    D3D12FenceSubmitInfoKHR & setSignalSemaphoreValuesCount( uint32_t signalSemaphoreValuesCount_ ) VULKAN_HPP_NOEXCEPT
    {
      signalSemaphoreValuesCount = signalSemaphoreValuesCount_;
      return *this;
    }

    D3D12FenceSubmitInfoKHR & setPSignalSemaphoreValues( const uint64_t* pSignalSemaphoreValues_ ) VULKAN_HPP_NOEXCEPT
    {
      pSignalSemaphoreValues = pSignalSemaphoreValues_;
      return *this;
    }

    operator VkD3D12FenceSubmitInfoKHR const&() const VULKAN_HPP_NOEXCEPT
    {
      return *reinterpret_cast<const VkD3D12FenceSubmitInfoKHR*>( this );
    }

    operator VkD3D12FenceSubmitInfoKHR &() VULKAN_HPP_NOEXCEPT
    {
      return *reinterpret_cast<VkD3D12FenceSubmitInfoKHR*>( this );
    }

#if defined(VULKAN_HPP_HAS_SPACESHIP_OPERATOR)
    auto operator<=>( D3D12FenceSubmitInfoKHR const& ) const = default;
#else
    bool operator==( D3D12FenceSubmitInfoKHR const& rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return ( sType == rhs.sType )
          && ( pNext == rhs.pNext )
          && ( waitSemaphoreValuesCount == rhs.waitSemaphoreValuesCount )
          && ( pWaitSemaphoreValues == rhs.pWaitSemaphoreValues )
          && ( signalSemaphoreValuesCount == rhs.signalSemaphoreValuesCount )
          && ( pSignalSemaphoreValues == rhs.pSignalSemaphoreValues );
    }

    bool operator!=( D3D12FenceSubmitInfoKHR const& rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return !operator==( rhs );
    }
#endif

  public:
    const VULKAN_HPP_NAMESPACE::StructureType sType = StructureType::eD3D12FenceSubmitInfoKHR;
    const void* pNext = {};
    uint32_t waitSemaphoreValuesCount = {};
    const uint64_t* pWaitSemaphoreValues = {};
    uint32_t signalSemaphoreValuesCount = {};
    const uint64_t* pSignalSemaphoreValues = {};
  };
  static_assert( sizeof( D3D12FenceSubmitInfoKHR ) == sizeof( VkD3D12FenceSubmitInfoKHR ), "struct and wrapper have different size!" );
  static_assert( std::is_standard_layout<D3D12FenceSubmitInfoKHR>::value, "struct wrapper is not a standard layout!" );
#endif /*VK_USE_PLATFORM_WIN32_KHR*/
} // namespace VULKAN_HPP_NAMESPACE
