/*
 * @file:
 * @Author: regangcli
 * @copyright: Tencent Technology (Shenzhen) Company Limited
 * @Date: 2023-06-19 20:13:51
 * @edit: regangcli
 * @brief:
 */
#include "rpc_mgr.h"
#include "conn_mgr.h"
#include "rpc_channel.h"
#include "rpc_coro_mgr.h"
#include "llbc.h"
using namespace llbc;

RpcMgr::RpcMgr(ConnMgr *connMgr) : connMgr_(connMgr) {
    connMgr_->Subscribe(RpcOpCode::RpcReq, LLBC_Delegate<void(LLBC_Packet &)>(this, &RpcMgr::HandleRpcReq));
    connMgr_->Subscribe(RpcOpCode::RpcRsp, LLBC_Delegate<void(LLBC_Packet &)>(this, &RpcMgr::HandleRpcRsp));
}

RpcMgr::~RpcMgr() {
    connMgr_->Unsubscribe(RpcOpCode::RpcReq);
    connMgr_->Unsubscribe(RpcOpCode::RpcRsp);
}

void RpcMgr::AddService(::google::protobuf::Service *service) {
    ServiceInfo service_info;
    service_info.service = service;
    service_info.sd = service->GetDescriptor();
    for (int i = 0; i < service_info.sd->method_count(); ++i) {
        service_info.mds[service_info.sd->method(i)->name()] = service_info.sd->method(i);
    }

    _services[service_info.sd->name()] = service_info;
}

void RpcMgr::HandleRpcReq(LLBC_Packet &packet) {
    // 读取serviceName&methodName
    llbc::uint64 srcCoroId;
    std::string serviceName, methodName;
    if( packet.Read(serviceName) != LLBC_OK ||
        packet.Read(methodName) != LLBC_OK ||
        packet.Read(srcCoroId) != LLBC_OK) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "read packet failed");
        return;
    }
    
    auto service = _services[serviceName].service;
    auto md = _services[serviceName].mds[methodName];
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "recv service_name:%s", serviceName.c_str());
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "recv method_name:%s", methodName.c_str());
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "srcCoroId:%llu", srcCoroId);

    // 解析req&创建rsp
    auto req = service->GetRequestPrototype(md).New();
    if (packet.Read(*req) != LLBC_OK) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "read packet failed");
        delete req;
        return;
    }
    auto rsp = service->GetResponsePrototype(md).New();

    // TODO: 协程方案, 在新协程中处理rpc请求
    auto func = [packet, service, md, req, rsp, this](void *) {
        RpcController controller;
        // 创建rpc完成回调函数
        service->CallMethod(md, &controller, req, rsp, nullptr);
        OnRpcDone(req, rsp, 0);
        delete req;
        delete rsp;
    };
    Coro *coro = s_RpcCoroMgr->CreateCoro(func, nullptr);
    coro->SetParam1(packet.GetSessionId());
    coro->SetParam2(srcCoroId);
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "SessionId:%d, srcCoroId:%llu", packet.GetSessionId(), srcCoroId);
    coro->Resume();  // 启动协程，如协程内部有内嵌发起rpc请求，会在协程内部发起请求后，直接回到此处
}

void RpcMgr::HandleRpcRsp(LLBC_Packet &packet) {
    uint64 dstCoroId = 0;
    // LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "dstCoroId:%llu, packet:%s", dstCoroId, packet.ToString().c_str());
    packet.Read(dstCoroId);
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "dstCoroId:%llu, packet:%s", dstCoroId, packet.ToString().c_str());
    Coro *coro = s_RpcCoroMgr->GetCoro(dstCoroId);
    if (!coro) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "coro not found, coroId:%llu", dstCoroId);
    }
    else {
        coro->SetPtrParam1(&packet);
        coro->Resume();
    }
}

void RpcMgr::OnRpcDone(::google::protobuf::Message *req, ::google::protobuf::Message *rsp, int sessionId) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "OnRpcDone, req:%s, rsp:%s", req->DebugString().c_str(),
         rsp->DebugString().c_str());
    // 协程方案
    auto coro = s_RpcCoroMgr->GetCurCoro();
    auto coroSessionId = coro->GetParam1();
    auto srcCoroId = coro->GetParam2();
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "coroSessionId:%llu, srcCoroId:%llu", coroSessionId, srcCoroId);
    auto packet = LLBC_GetObjectFromUnsafetyPool<LLBC_Packet>();
    packet->SetSessionId(coroSessionId);
    packet->SetOpcode(RpcOpCode::RpcRsp);
    packet->Write(srcCoroId);
    packet->Write(*rsp);
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "rsp packet:%s", packet->ToString().c_str());
    // 回包
    connMgr_->PushPacket(packet);
}