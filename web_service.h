// Copyright (c) 2015 The Trident Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Author: shichengyi (Shi Chengyi)

#ifndef _TRIDENT_WEB_SERVICE_H_
#define _TRIDENT_WEB_SERVICE_H_

#include <trident/common_internal.h>
#include <trident/ext_closure.h>
#include <trident/rpc_request.h>

namespace trident {

class HTTPRequest;
class HTTPResponse;
typedef ExtClosure<bool(const HTTPRequest&, HTTPResponse&)>* Servlet;

typedef std::map<const std::string, std::pair<Servlet, bool> > ServletMap;
typedef trident::shared_ptr<ServletMap> ServletMapPtr;

class WebService
{
public:
    WebService(const ServicePoolWPtr& service_pool);
    ~WebService();

    void Init();

    bool RegisterServlet(const std::string& path, Servlet servlet, bool take_ownership = true);

    Servlet UnregisterServlet(const std::string& path);

    bool RoutePage(
        const RpcRequestPtr& rpc_request,
        const RpcServerStreamWPtr& server_stream);

    Servlet FindServlet(const std::string& path);

private:
    bool DefaultHome(const HTTPRequest& request, HTTPResponse& response);

    bool DefaultOptions(const HTTPRequest& request, HTTPResponse& response);

    bool DefaultStatus(const HTTPRequest& request, HTTPResponse& response);

    bool DefaultServices(const HTTPRequest& request, HTTPResponse& response);

    bool DefaultService(const HTTPRequest& request, HTTPResponse& response);

    static void PageHeader(std::ostream& out);

    static void PageFooter(std::ostream& out);

    void ServerBrief(std::ostream& out,
                     const HTTPRequest& request);

    void ServerOptions(std::ostream& out);

    void ServerStatus(std::ostream& out);

    void ServiceList(std::ostream& out);

    static void MethodList(std::ostream& out,
                           ServiceBoard* svc_board);

    static void ErrorPage(std::ostream& out,
                          const std::string& reason);

    void ServletList(std::ostream& out);

    ServletMapPtr GetServletPtr();

private:
    ServicePoolPtr _service_pool;

    FastLock _servlet_map_lock;
    ServletMapPtr _servlet_map;

    Servlet _default_home;
    Servlet _default_options;
    Servlet _default_status;
    Servlet _default_services;
    Servlet _default_service;

    TRIDENT_DISALLOW_EVIL_CONSTRUCTORS(WebService);
};

} // namespace trident

#endif // _TRIDENT_WEB_SERVICE_H_

