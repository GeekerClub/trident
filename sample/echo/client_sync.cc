/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 * Author: qinzuoyan01@baidu.com (Qin Zuoyan)
 * CreateTime: 2013-02-22
 *
 * Description:
 *
 **************************************************************************/

#include <trident/pbrpc.h>
#include "trident/sample/echo/echo_service.pb.h"

int main()
{
    SOFA_PBRPC_SET_LOG_LEVEL(NOTICE);

    // Define an rpc client.
    trident::pbrpc::RpcClientOptions client_options;
    trident::pbrpc::RpcClient rpc_client(client_options);

    // Define an rpc channel.
    trident::pbrpc::RpcChannelOptions channel_options;
    trident::pbrpc::RpcChannel rpc_channel(&rpc_client, "127.0.0.1:12321", channel_options);

    // Prepare parameters.
    trident::pbrpc::RpcController* cntl = new trident::pbrpc::RpcController();
    cntl->SetTimeout(3000);
    trident::pbrpc::test::EchoRequest* request =
        new trident::pbrpc::test::EchoRequest();
    request->set_message("Hello from qinzuoyan01");
    trident::pbrpc::test::EchoResponse* response =
        new trident::pbrpc::test::EchoResponse();

    // Sync call.
    trident::pbrpc::test::EchoServer_Stub* stub =
        new trident::pbrpc::test::EchoServer_Stub(&rpc_channel);
    stub->Echo(cntl, request, response, NULL);

    // Check if the request has been sent.
    // If has been sent, then can get the sent bytes.
    SLOG(NOTICE, "RemoteAddress=%s", cntl->RemoteAddress().c_str());
    SLOG(NOTICE, "IsRequestSent=%s", cntl->IsRequestSent() ? "true" : "false");
    if (cntl->IsRequestSent())
    {
        SLOG(NOTICE, "LocalAddress=%s", cntl->LocalAddress().c_str());
        SLOG(NOTICE, "SentBytes=%ld", cntl->SentBytes());
    }

    // Check if failed.
    if (cntl->Failed())
    {
        SLOG(ERROR, "request failed: %s", cntl->ErrorText().c_str());
    }
    else
    {
        SLOG(NOTICE, "request succeed: %s", response->message().c_str());
    }

    // Destroy objects.
    delete cntl;
    delete request;
    delete response;
    delete stub;

    return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
