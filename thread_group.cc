// Copyright (c) 2014 The Trident Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// 

#include <trident/thread_group.h>
#include <trident/thread_group_impl.h>

namespace trident {


ThreadGroup::ThreadGroup(int thread_num)
{
    _imp.reset(new ThreadGroupImpl(thread_num));
    _imp->start();
}

ThreadGroup::~ThreadGroup()
{
    _imp->stop();
    _imp.reset();
}

int ThreadGroup::thread_num() const
{
    return _imp->thread_num();
}

void ThreadGroup::dispatch(google::protobuf::Closure* handle)
{
    _imp->dispatch(handle);
}

void ThreadGroup::post(google::protobuf::Closure* handle)
{
    _imp->post(handle);
}

void ThreadGroup::dispatch(ExtClosure<void()>* handle)
{
    _imp->dispatch(handle);
}

void ThreadGroup::post(ExtClosure<void()>* handle)
{
    _imp->post(handle);
}


} // namespace trident

/* vim: set ts=4 sw=4 sts=4 tw=100 */
