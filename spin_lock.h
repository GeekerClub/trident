// Copyright (c) 2014 The Trident Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// 

#ifndef _TRIDENT_SPIN_LOCK_H_
#define _TRIDENT_SPIN_LOCK_H_

#include <pthread.h>

namespace trident {

class SpinLock
{
public:
    SpinLock() { pthread_spin_init(&_lock, 0); }
    ~SpinLock() { pthread_spin_destroy(&_lock); }
    void lock() { pthread_spin_lock(&_lock); }
    bool try_lock() { return pthread_spin_trylock(&_lock) == 0; }
    void unlock() { pthread_spin_unlock(&_lock); }

private:
    pthread_spinlock_t _lock;
}; // class SpinLock


} // namespace trident

#endif // _TRIDENT_SPIN_LOCK_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 */
