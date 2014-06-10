// Copyright (c) 2014 Baidu.com, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: qinzuoyan01@baidu.com (Qin Zuoyan)

#ifndef _SOFA_PBRPC_MOCK_TEST_HELPER_H_
#define _SOFA_PBRPC_MOCK_TEST_HELPER_H_

#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

#include <trident/common.h>
#include <trident/ext_closure.h>

namespace trident {
namespace pbrpc {

// All mock method implements should use this function signature.
typedef ExtClosure<void(
        ::google::protobuf::RpcController*,
        const ::google::protobuf::Message*,
        ::google::protobuf::Message*,
        ::google::protobuf::Closure*)> MockMethodHookFunction;

// Mock test helper, it manages all mock functions.
class MockTestHelper
{
public:
    // Get the singleton instance.
    static MockTestHelper* GlobalInstance();

    virtual ~MockTestHelper();

    // Register a mock method implement, which will be mapped to the "method_name".
    //
    // The "method_name" should be full name of method.
    // The "mock_method" should be a permanenet closure, ownership of which is always take by user.
    //
    // If mock method already exist, the old one will be replaced.
    virtual void RegisterMockMethod(const std::string& method_name,
                                    MockMethodHookFunction* mock_method) = 0;

    // Clear all the registered mock method mapping.
    virtual void ClearMockMethod() = 0;

    // Get the mock method mapped to the "method_name".  Returns NULL if not registered.
    virtual MockMethodHookFunction* GetMockMethod(const std::string& method_name) const = 0;

protected:
    MockTestHelper();

private:
    SOFA_PBRPC_DISALLOW_EVIL_CONSTRUCTORS(MockTestHelper);
};

extern bool g_enable_mock;
extern void enable_mock();
extern void disable_mock();

} // namespace pbrpc
} // namespace trident

// Enable or disable the mock feature.  Default disabled.
// The mock channel and mock methods will take effect iff mock enabled.
#define SOFA_PBRPC_ENABLE_MOCK() ::trident::pbrpc::enable_mock()
#define SOFA_PBRPC_DISABLE_MOCK() ::trident::pbrpc::disable_mock()

// If you create a channel with address prefix of SOFA_PBRPC_MOCK_CHANNEL_ADDRESS_PREFIX, then the
// channel is a mock channel.  The mock channel will not create real socket connection, but
// just uses mock methods.
#define SOFA_PBRPC_MOCK_CHANNEL_ADDRESS_PREFIX "/mock/"

// Register a mock method implement.  If mock enabled, all channels will prefer to call mock
// method first. If the corresponding mock method is not registered, then call the real method.
//
// "method_name" is the full name of the method to be mocked, should be a c-style string.
// "mock_method" is the mock method hook function, should be type of "MockMethodHookFunction*".
//
// For example:
//     MockMethodHookFunction* mock_method = trident::pbrpc::NewPermanentExtClosure(&MockEcho);
//     SOFA_PBRPC_REGISTER_MOCK_METHOD("trident.pbrpc.test.EchoServer.Echo", mock_method);
#define SOFA_PBRPC_REGISTER_MOCK_METHOD(method_name, mock_method) \
    ::trident::pbrpc::MockTestHelper::GlobalInstance()->RegisterMockMethod(method_name, (mock_method))

// Clear all registered mock methods.  This will not delete the cleared hook functions, which
// are take ownership by user.
#define SOFA_PBRPC_CLEAR_MOCK_METHOD() \
    ::trident::pbrpc::MockTestHelper::GlobalInstance()->ClearMockMethod()

#endif // _SOFA_PBRPC_MOCK_TEST_HELPER_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 */
