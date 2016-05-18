
//          Copyright Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/fiber/algo/random_chase_lev.hpp"

#include <boost/assert.hpp>

#include "boost/fiber/type.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace algo {

random_chase_lev::random_chase_lev( bool suspend) :
    generator_{ rd_device_() },
    suspend_{ suspend } {
    std::unique_lock< std::mutex > lk( schedulers_mutex_);
    idx_ = schedulers_.size();
    schedulers_.push_back( this);
}

void
random_chase_lev::awakened( context * ctx) noexcept {
    if ( ! ctx->is_context( type::pinned_context) ) {
        ctx->detach();
        rqueue_.push( ctx);
    } else {
        ctx->ready_link( lqueue_);
    }
}

context *
random_chase_lev::pick_next() noexcept {
    context * ctx = rqueue_.pop();
    if ( nullptr != ctx) {
        context::active()->attach( ctx);
    } else if ( ! lqueue_.empty() ) {
        ctx = & lqueue_.front();
        lqueue_.pop_front();
    } else if ( nullptr == ctx) {
        random_chase_lev * other;
        {
            std::unique_lock< std::mutex > lk( schedulers_mutex_);
            std::uniform_int_distribution< std::size_t >
                distribution_{ static_cast< std::size_t >( 0), schedulers_.size() - 1};
            std::size_t idx = 0;
            do {
                idx = distribution_( generator_);
            } while ( idx == idx_);
            other = schedulers_[idx];
        } // unlock lk
        ctx = other->steal();
        if ( nullptr != ctx) {
            context::active()->attach( ctx);
        }
    }
    return ctx;
}

void
random_chase_lev::suspend_until( std::chrono::steady_clock::time_point const& time_point) noexcept {
    if ( suspend_) {
        if ( (std::chrono::steady_clock::time_point::max)() == time_point) {
            std::unique_lock< std::mutex > lk( mtx_);
            cnd_.wait( lk, [this](){ return flag_; });
            flag_ = false;
        } else {
            std::unique_lock< std::mutex > lk( mtx_);
            cnd_.wait_until( lk, time_point, [this](){ return flag_; });
            flag_ = false;
        }
    }
}

void
random_chase_lev::notify() noexcept {
    if ( suspend_) {
        std::unique_lock< std::mutex > lk( mtx_);
        flag_ = true;
        lk.unlock();
        cnd_.notify_all();
    }
}

std::vector< random_chase_lev * > random_chase_lev::schedulers_{};
std::mutex                        random_chase_lev::schedulers_mutex_{};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
