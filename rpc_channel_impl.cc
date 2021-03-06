// Copyright (c) 2014 The Trident Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// 

#include <trident/rpc_channel_impl.h>
#include <trident/mock_test_helper.h>
#include <trident/closure.h>

namespace trident {


RpcChannelImpl::RpcChannelImpl(const RpcClientImplPtr& rpc_client_impl,
                               const std::string& server_address,
                               const RpcChannelOptions& options)
    : _client_impl(rpc_client_impl)
    , _server_address(server_address)
    , _options(options)
    , _is_mock(false)
    , _resolve_address_succeed(false)
{
    if (g_enable_mock && _server_address.find(TRIDENT_MOCK_CHANNEL_ADDRESS_PREFIX) == 0)
    {
#if 0
        LOG(INFO) << "RpcChannelImpl(): use mock channel";
#else
        SLOG(INFO, "RpcChannelImpl(): use mock channel");
#endif
        _is_mock = true;
        return;
    }

    if (_client_impl->ResolveAddress(_server_address, &_remote_endpoint))
    {
#if 0
        LOG(INFO) << "RpcChannelImpl(): resolve address succeed: " << _server_address
                  << " [" << RpcEndpointToString(_remote_endpoint) << "]";
#else
        SLOG(INFO, "RpcChannelImpl(): resolve address succeed: %s [%s]",
                _server_address.c_str(), RpcEndpointToString(_remote_endpoint).c_str());
#endif
        _resolve_address_succeed = true;
    }
    else
    {
#if 0
        LOG(ERROR) << "RpcChannelImpl(): resolve address failed: " << _server_address;
#else
        SLOG(ERROR, "RpcChannelImpl(): resolve address failed: %s",
                _server_address.c_str());
#endif
    }
}

RpcChannelImpl::~RpcChannelImpl()
{
}

bool RpcChannelImpl::IsAddressValid()
{
    return _resolve_address_succeed;
}

void RpcChannelImpl::CallMethod(const ::google::protobuf::MethodDescriptor* method,
                                ::google::protobuf::RpcController* controller,
                                const ::google::protobuf::Message* request,
                                ::google::protobuf::Message* response,
                                ::google::protobuf::Closure* done)
{
    SCHECK(method != NULL);
    SCHECK(controller != NULL);
    SCHECK(request != NULL);
    SCHECK(response != NULL);

    // prepare controller
    RpcController* trident_controller = dynamic_cast<RpcController*>(controller);
    SCHECK(trident_controller != NULL); // should pass trident::RpcController to CallMethod
    RpcControllerImplPtr cntl = trident_controller->impl();
    cntl->PushDoneCallback(boost::bind(&RpcChannelImpl::DoneCallback, shared_from_this(), done, _1));
    cntl->FillFromMethodDescriptor(method);
    if (done == NULL)
    {
        cntl->SetSync(); // null done means sync call
        cntl->SetWaitEvent(WaitEventPtr(new WaitEvent()));
    }

    // check if mocked
    if (g_enable_mock && _is_mock)
    {
        MockMethodHookFunction* mock_closure =
            MockTestHelper::GlobalInstance()->GetMockMethod(method->full_name());
        if (mock_closure)
        {
            // mock method registered
#if 0
            LOG(INFO) << "CallMethod(): mock method ["
                      << method->full_name() << "] called";
#else
            SLOG(INFO, "CallMethod(): mock method [%s] called",
                    method->full_name().c_str());
#endif
            ::google::protobuf::Closure* mock_done =
                NewClosure(&RpcChannelImpl::MockDoneCallback, cntl, request, response);
            mock_closure->Run(controller, request, response, mock_done);
        }
        else
        {
            // mock method not registered, but it is in mock channel
#if 0
            LOG(ERROR) << "CallMethod(): mock method ["
                       << method->full_name() << "] not registered"
                       << ", but used in mock channel";
#else
            SLOG(ERROR, "CallMethod(): mock method [%s] not registered"
                    ", but used in mock channel", method->full_name().c_str());
#endif
            cntl->Done(RPC_ERROR_FOUND_METHOD, "mock method not registered: "
                    + method->full_name());
        }
        WaitDone(cntl);
        return;
    }

    if (!_resolve_address_succeed)
    {
        // TODO resolve address failed, retry resolve?
#if 0
        LOG(ERROR) << "CallMethod(): resolve address failed: " << _server_address;
#else
        SLOG(ERROR, "CallMethod(): resolve address failed: %s", _server_address.c_str());
#endif
        cntl->Done(RPC_ERROR_RESOLVE_ADDRESS, _server_address);
        WaitDone(cntl);
        return;
    }

    // ready, go ahead to do real call
    cntl->SetRemoteEndpoint(_remote_endpoint);
    cntl->StartTimer();
    _client_impl->CallMethod(request, response, cntl);
    WaitDone(cntl);
}

void RpcChannelImpl::WaitDone(const RpcControllerImplPtr& cntl)
{
    // if sync, wait for callback done
    if (cntl->IsSync())
    {
        cntl->WaitEvent()->Wait();
        SCHECK(cntl->IsDone());
    }
}

void RpcChannelImpl::DoneCallback(google::protobuf::Closure* done,
        const RpcControllerImplPtr& cntl)
{
    if (cntl->IsSync())
    {
        SCHECK(done == NULL);
        SCHECK(cntl->WaitEvent());
        cntl->WaitEvent()->Signal();
    }
    else
    {
        SCHECK(done != NULL);
        _client_impl->GetCallbackThreadGroup()->post(done);
    }
}

void RpcChannelImpl::MockDoneCallback(RpcControllerImplPtr cntl,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* /*response*/)
{
    if (!cntl->Failed())
    {
        cntl->NotifyRequestSent(RpcEndpoint(), request->ByteSize());
    }
    cntl->Done(cntl->ErrorCode(), cntl->Reason());
}


} // namespace trident

/* vim: set ts=4 sw=4 sts=4 tw=100 */
