/*
 * @file:
 * @Author: regangcli
 * @copyright: Tencent Technology (Shenzhen) Company Limited
 * @Date: 2023-06-15 20:21:17
 * @edit: regangcli
 * @brief:
 */

#pragma once

#include <iostream>

#include <google/protobuf/message.h>
#include "google/protobuf/service.h"
#include "google/protobuf/stubs/common.h"
class ConnMgr;
class MyController : public ::google::protobuf::RpcController {
public:
    virtual void Reset(){};

    virtual bool Failed() const { return false; };
    virtual std::string ErrorText() const { return ""; };
    virtual void StartCancel(){};
    virtual void SetFailed(const std::string & /* reason */){};
    virtual bool IsCanceled() const { return false; };
    virtual void NotifyOnCancel(::google::protobuf::Closure * /* callback */){};
};

class RpcChannel : public ::google::protobuf::RpcChannel {
public:
    RpcChannel(ConnMgr *connMgr, int sessionId) : connMgr_(connMgr), sessionId_(sessionId) { }
    virtual ~RpcChannel();

    virtual void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                            ::google::protobuf::RpcController * /* controller */,
                            const ::google::protobuf::Message *request, ::google::protobuf::Message *response,
                            ::google::protobuf::Closure *);

private:
    ConnMgr *connMgr_ = nullptr;
    int sessionId_ = 0;
};
