
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/detail/worker_fiber.hpp"

#include <exception>

#include <boost/exception_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/locks.hpp>

#include "boost/fiber/detail/scheduler.hpp"
#include "boost/fiber/exceptions.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace detail {

void * worker_fiber::null_ptr = 0;

worker_fiber::worker_fiber( coro_t::yield_type * callee) :
    fiber_base(),
    use_count_( 1), // allocated on stack
    fss_data_(),
    prev_( 0),
    nxt_( 0),
    tp_( (chrono::high_resolution_clock::time_point::max)() ),
    callee_( callee),
    caller_(),
    state_( READY),
    flags_( 0),
    priority_( 0),
    except_(),
    waiting_(),
    phead_(0),
    ptail_(0)
{ BOOST_ASSERT( callee_); }

worker_fiber::~worker_fiber()
{
    BOOST_ASSERT( is_terminated() );
    BOOST_ASSERT( waiting_.empty() );
}

void
worker_fiber::release()
{
    BOOST_ASSERT( is_terminated() );

    std::vector< worker_fiber * > waiting;

    // get all waiting fibers
    splk_.lock();
    waiting.swap( waiting_);
    splk_.unlock();

    // notify all waiting fibers
    BOOST_FOREACH( worker_fiber * p, waiting)
    { p->set_ready(); }

    // release all fiber-specific-pointers
    BOOST_FOREACH( fss_data_t::value_type & data, fss_data_)
    { data.second.do_cleanup(); }
}

void
worker_fiber::set_ready()
{
    state_t previous = state_.exchange( READY);
    BOOST_ASSERT( WAITING == previous || RUNNING == previous || READY == previous);
    (void)previous;
    scheduler::instance()->move_to_run(this);
}

bool
worker_fiber::join( worker_fiber * p)
{
    unique_lock< spinlock > lk( splk_);
    if ( is_terminated() ) return false;
    waiting_.push_back( p);
    return true;
}

void
worker_fiber::interruption_blocked( bool blck) BOOST_NOEXCEPT
{
    if ( blck)
        flags_ |= flag_interruption_blocked;
    else
        flags_ &= ~flag_interruption_blocked;
}

void
worker_fiber::request_interruption( bool req) BOOST_NOEXCEPT
{
    if ( req)
    {
        flags_ |= flag_interruption_requested;
        set_ready();
    }
    else
        flags_ &= ~flag_interruption_requested;
}

void
worker_fiber::thread_affinity( bool req) BOOST_NOEXCEPT
{
    if ( req)
        flags_ |= flag_thread_affinity;
    else
        flags_ &= ~flag_thread_affinity;
}

void *
worker_fiber::get_fss_data( void const* vp) const
{
    uintptr_t key( reinterpret_cast< uintptr_t >( vp) );
    fss_data_t::const_iterator i( fss_data_.find( key) );

    return fss_data_.end() != i ? i->second.vp : 0;
}

void
worker_fiber::set_fss_data(
    void const* vp,
    fss_cleanup_function::ptr_t const& cleanup_fn,
    void * data, bool cleanup_existing)
{
    BOOST_ASSERT( cleanup_fn);

    uintptr_t key( reinterpret_cast< uintptr_t >( vp) );
    fss_data_t::iterator i( fss_data_.find( key) );

    if ( fss_data_.end() != i)
    {
        if( cleanup_existing) i->second.do_cleanup();
        if ( data)
            fss_data_.insert(
                    i,
                    std::make_pair(
                        key,
                        fss_data( data, cleanup_fn) ) );
        else fss_data_.erase( i);
    }
    else
        fss_data_.insert(
            std::make_pair(
                key,
                fss_data( data, cleanup_fn) ) );
}

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
