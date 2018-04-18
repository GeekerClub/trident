// Copyright (c) 2016 The Trident Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: shichengyi (Shi Chengyi)

#include <gtest/gtest.h>
#include <trident/common_internal.h>
#include <trident/web_service.h>

namespace trident {

class WebServiceTest: public ::testing::Test
{
protected:
    WebServiceTest() { }

    virtual ~WebServiceTest() { }

    virtual void SetUp() {
        _service_pool.reset(new ServicePool(NULL));
    }

    virtual void TearDown() {
        _service_pool.reset();
    }

    ServicePoolPtr _service_pool;
};

TEST_F(WebServiceTest, RegisterServlet)
{
    WebServicePtr web_service(new WebService(_service_pool));
    Servlet s = (Servlet) 0x01;
    std::string path = "/a/b/c";
    bool ret = web_service->RegisterServlet(path, s);
    ASSERT_TRUE(ret);
    ret = web_service->RegisterServlet(path, s);
    ASSERT_FALSE(ret);
    ret = web_service->UnregisterServlet(path);
    ASSERT_TRUE(ret);
}

TEST_F(WebServiceTest, FindServlet)
{
    WebServicePtr web_service(new WebService(_service_pool));
    Servlet null_servlet = web_service->FindServlet("null");
    ASSERT_TRUE(NULL == null_servlet);

    Servlet s1 = (Servlet) 0x01;
    Servlet s2 = (Servlet) 0x02;
    Servlet s3 = (Servlet) 0x03;
    Servlet s4 = (Servlet) 0x04;

    std::string p1 = "/";
    std::string p2 = "/c";
    std::string p3 = "/c/b";
    std::string p4 = "/c/e";

    web_service->RegisterServlet(p1, s1);
    web_service->RegisterServlet(p2, s2);
    web_service->RegisterServlet(p3, s3);
    web_service->RegisterServlet(p4, s4);

    Servlet find = web_service->FindServlet(p1);
    ASSERT_EQ(find, s1);

    std::string path = "/null";
    find = web_service->FindServlet(path);
    ASSERT_TRUE(NULL == null_servlet);

    path = "/c";
    find = web_service->FindServlet(path);
    ASSERT_EQ(find, s2);

    path = "/c/b";
    find = web_service->FindServlet(path);
    ASSERT_EQ(find, s3);

    path = "/c/e";
    find = web_service->FindServlet(path);
    ASSERT_EQ(find, s4);

    web_service->UnregisterServlet(p1);
    web_service->UnregisterServlet(p2);
    web_service->UnregisterServlet(p3);
    web_service->UnregisterServlet(p4);
}

} // end trident
