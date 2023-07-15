/*
 * @file:
 * @Author: regangcli
 * @copyright: Tencent Technology (Shenzhen) Company Limited
 * @Date: 2023-06-19 20:14:54
 * @edit: regangcli
 * @brief:
 */
#pragma once

#include <map>
#include "google/protobuf/message.h"
#include "google/protobuf/service.h"
#include "google/protobuf/stubs/common.h"

namespace llbc
{
    class LLBC_Packet;
}

class ConnMgr;
class RpcServiceMgr {
public:
    RpcServiceMgr(ConnMgr *connMgr);
    virtual ~RpcServiceMgr();

    // // 初始化
    // int Init();
    // // 更新，弃用
    // void OnUpdate();
    // 添加服务
    void AddService(::google::protobuf::Service *service);

private:
    // 处理RPC请求和返回包
    void HandleRpcReq(llbc::LLBC_Packet &packet);
    void HandleRpcRsp(llbc::LLBC_Packet &packet);
    // 处理RPC处理结束
    void OnRpcDone(::google::protobuf::Message *recv_msg, ::google::protobuf::Message *resp_msg, int sessionId);
    // void dispatch_msg(
    //         const std::string& service_name,
    //         const std::string& method_name,
    //         const std::string& serialzied_data);
    // void pack_message(
    //         const ::google::protobuf::Message* msg,
    //         std::string* serialized_data);

private:
    struct ServiceInfo {
        ::google::protobuf::Service *service;
        const ::google::protobuf::ServiceDescriptor *sd;
        std::map<std::string, const ::google::protobuf::MethodDescriptor *> mds;
    };  // ServiceInfo

    std::map<std::string, ServiceInfo> _services;
    ConnMgr *connMgr_ = nullptr;
    bool stop_ = false;
};  // RpcServiceMgr