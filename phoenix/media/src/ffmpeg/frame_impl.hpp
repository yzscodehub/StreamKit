/**
 * @file frame_impl.hpp
 * @brief Internal frame implementation structs
 * 
 * This file contains the Impl structs for VideoFrame and AudioFrame.
 * It should only be included by internal implementation files.
 */

#pragma once

#include <phoenix/media/frame.hpp>
#include "shared_avframe.hpp"
#include "ff_common.hpp"

namespace phoenix::media {

/**
 * @brief VideoFrame internal implementation
 */
struct VideoFrame::Impl {
    ff::SharedAVFrame frame;
    HWAccelType hwType = HWAccelType::None;
    int64_t frameNumber = -1;
    
    Impl() = default;
    
    explicit Impl(ff::SharedAVFrame f, HWAccelType hw = HWAccelType::None)
        : frame(std::move(f)), hwType(hw) {}
};

/**
 * @brief AudioFrame internal implementation
 */
struct AudioFrame::Impl {
    ff::SharedAVFrame frame;
    
    Impl() = default;
    explicit Impl(ff::SharedAVFrame f) : frame(std::move(f)) {}
};

} // namespace phoenix::media
