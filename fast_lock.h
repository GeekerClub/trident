// Copyright (c) 2014 Baidu.com, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: qinzuoyan01@baidu.com (Qin Zuoyan)

#ifndef _SOFA_PBRPC_FAST_LOCK_H_
#define _SOFA_PBRPC_FAST_LOCK_H_

#include <trident/mutex_lock.h>
#include <trident/spin_lock.h>

namespace trident {
namespace pbrpc {

#if defined( SOFA_PBRPC_USE_SPINLOCK )
    typedef SpinLock FastLock;
#else
    typedef MutexLock FastLock;
#endif // defined( SOFA_PBRPC_USE_SPINLOCK )

} // namespace pbrpc
} // namespace trident

#endif // _SOFA_PBRPC_FAST_LOCK_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 */
