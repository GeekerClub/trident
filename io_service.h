// Copyright (c) 2014 The Trident Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// 

#ifndef _TRIDENT_IO_SERVICE_H_
#define _TRIDENT_IO_SERVICE_H_

/*************************************************************************
 * ATTENTION: we suppose that epoll is always supported on the platform.

// Linux: epoll, eventfd and timerfd.
#if defined(__linux__)
# include <linux/version.h>
# if !defined(BOOST_ASIO_DISABLE_EPOLL)
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,45)
#   define BOOST_ASIO_HAS_EPOLL 1
#  endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,45)
# endif // !defined(BOOST_ASIO_DISABLE_EVENTFD)
# if !defined(BOOST_ASIO_DISABLE_EVENTFD)
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#   define BOOST_ASIO_HAS_EVENTFD 1
#  endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
# endif // !defined(BOOST_ASIO_DISABLE_EVENTFD)
# if defined(BOOST_ASIO_HAS_EPOLL)
#  if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8)
#   define BOOST_ASIO_HAS_TIMERFD 1
#  endif // (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8)
# endif // defined(BOOST_ASIO_HAS_EPOLL)
#endif // defined(__linux__)

**************************************************************************/

#define BOOST_ASIO_HAS_EPOLL 1

#include <boost/asio.hpp>
#include <trident/common.h>

namespace trident {


typedef boost::asio::io_service IOService;
typedef trident::shared_ptr<IOService> IOServicePtr;

typedef boost::asio::io_service::work IOServiceWork;
typedef trident::shared_ptr<IOServiceWork> IOServiceWorkPtr;

typedef boost::asio::io_service::strand IOServiceStrand;
typedef trident::shared_ptr<IOServiceStrand> IOServiceStrandPtr;

typedef boost::asio::deadline_timer IOServiceTimer;
typedef trident::shared_ptr<IOServiceTimer> IOServiceTimerPtr;


} // namespace trident

#endif // _TRIDENT_IO_SERVICE_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 */
