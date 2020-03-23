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
#include "VkAcceleration.hpp"

namespace VULKAN_HPP_NAMESPACE
{
  struct AcquireNextImageInfoKHR
  {
    VULKAN_HPP_CONSTEXPR AcquireNextImageInfoKHR( VULKAN_HPP_NAMESPACE::SwapchainKHR swapchain_ = {},
                                                  uint64_t timeout_ = {},
                                                  VULKAN_HPP_NAMESPACE::Semaphore semaphore_ = {},
                                                  VULKAN_HPP_NAMESPACE::Fence fence_ = {},
                                                  uint32_t deviceMask_ = {} ) VULKAN_HPP_NOEXCEPT
      : swapchain( swapchain_ )
      , timeout( timeout_ )
      , semaphore( semaphore_ )
      , fence( fence_ )
      , deviceMask( deviceMask_ )
    {}

    VULKAN_HPP_CONSTEXPR AcquireNextImageInfoKHR( AcquireNextImageInfoKHR const& rhs ) VULKAN_HPP_NOEXCEPT
      : pNext( rhs.pNext )
      , swapchain( rhs.swapchain )
      , timeout( rhs.timeout )
      , semaphore( rhs.semaphore )
      , fence( rhs.fence )
      , deviceMask( rhs.deviceMask )
    {}

    AcquireNextImageInfoKHR & operator=( AcquireNextImageInfoKHR const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      memcpy( &pNext, &rhs.pNext, sizeof( AcquireNextImageInfoKHR ) - offsetof( AcquireNextImageInfoKHR, pNext ) );
      return *this;
    }

    AcquireNextImageInfoKHR( VkAcquireNextImageInfoKHR const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      *this = rhs;
    }

    AcquireNextImageInfoKHR& operator=( VkAcquireNextImageInfoKHR const & rhs ) VULKAN_HPP_NOEXCEPT
    {
      *this = *reinterpret_cast<VULKAN_HPP_NAMESPACE::AcquireNextImageInfoKHR const *>(&rhs);
      return *this;
    }

    AcquireNextImageInfoKHR & setPNext( const void* pNext_ ) VULKAN_HPP_NOEXCEPT
    {
      pNext = pNext_;
      return *this;
    }

    AcquireNextImageInfoKHR & setSwapchain( VULKAN_HPP_NAMESPACE::SwapchainKHR swapchain_ ) VULKAN_HPP_NOEXCEPT
    {
      swapchain = swapchain_;
      return *this;
    }

    AcquireNextImageInfoKHR & setTimeout( uint64_t timeout_ ) VULKAN_HPP_NOEXCEPT
    {
      timeout = timeout_;
      return *this;
    }

    AcquireNextImageInfoKHR & setSemaphore( VULKAN_HPP_NAMESPACE::Semaphore semaphore_ ) VULKAN_HPP_NOEXCEPT
    {
      semaphore = semaphore_;
      return *this;
    }

    AcquireNextImageInfoKHR & setFence( VULKAN_HPP_NAMESPACE::Fence fence_ ) VULKAN_HPP_NOEXCEPT
    {
      fence = fence_;
      return *this;
    }

    AcquireNextImageInfoKHR & setDeviceMask( uint32_t deviceMask_ ) VULKAN_HPP_NOEXCEPT
    {
      deviceMask = deviceMask_;
      return *this;
    }

    operator VkAcquireNextImageInfoKHR const&() const VULKAN_HPP_NOEXCEPT
    {
      return *reinterpret_cast<const VkAcquireNextImageInfoKHR*>( this );
    }

    operator VkAcquireNextImageInfoKHR &() VULKAN_HPP_NOEXCEPT
    {
      return *reinterpret_cast<VkAcquireNextImageInfoKHR*>( this );
    }

#if defined(VULKAN_HPP_HAS_SPACESHIP_OPERATOR)
    auto operator<=>( AcquireNextImageInfoKHR const& ) const = default;
#else
    bool operator==( AcquireNextImageInfoKHR const& rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return ( sType == rhs.sType )
          && ( pNext == rhs.pNext )
          && ( swapchain == rhs.swapchain )
          && ( timeout == rhs.timeout )
          && ( semaphore == rhs.semaphore )
          && ( fence == rhs.fence )
          && ( deviceMask == rhs.deviceMask );
    }

    bool operator!=( AcquireNextImageInfoKHR const& rhs ) const VULKAN_HPP_NOEXCEPT
    {
      return !operator==( rhs );
    }
#endif

  public:
    const VULKAN_HPP_NAMESPACE::StructureType sType = StructureType::eAcquireNextImageInfoKHR;
    const void* pNext = {};
    VULKAN_HPP_NAMESPACE::SwapchainKHR swapchain = {};
    uint64_t timeout = {};
    VULKAN_HPP_NAMESPACE::Semaphore semaphore = {};
    VULKAN_HPP_NAMESPACE::Fence fence = {};
    uint32_t deviceMask = {};
  };
  static_assert( sizeof( AcquireNextImageInfoKHR ) == sizeof( VkAcquireNextImageInfoKHR ), "struct and wrapper have different size!" );
  static_assert( std::is_standard_layout<AcquireNextImageInfoKHR>::value, "struct wrapper is not a standard layout!" );
} // namespace VULKAN_HPP_NAMESPACE
