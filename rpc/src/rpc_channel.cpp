/*
 * @file:
 * @Author: regangcli
 * @copyright: Tencent Technology (Shenzhen) Company Limited
 * @Date: 2023-06-15 20:21:17
 * @edit: regangcli
 * @brief:
 */
#include "rpc_channel.h"
#include "conn_mgr.h"
#include "llbc.h"
#include "rpc_coro_mgr.h"

using namespace llbc;

RpcChannel::~RpcChannel() {
    connMgr_->CloseSession(sessionId_);
}

void RpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                            ::google::protobuf::RpcController *controller,
                            const ::google::protobuf::Message *request, ::google::protobuf::Message *response,
                            ::google::protobuf::Closure *) {
    response->Clear();

    // 协程方案, 不允许在主协程中call Rpc
    auto coro = s_RpcCoroMgr->GetCurCoro();
    if (coro->IsMainCoro()) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "Main coro not allow call Yield() method!");
        return;
    }

    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "CallMethod!");
    LLBC_Packet *sendPacket = LLBC_GetObjectFromUnsafetyPool<LLBC_Packet>();
    sendPacket->SetHeader(0, RpcOpCode::RpcReq, 0);
    sendPacket->SetSessionId(sessionId_);
    sendPacket->Write(method->service()->name());
    sendPacket->Write(method->name());

    sendPacket->Write(uint64(s_RpcCoroMgr->GetCurCoroId()));
    sendPacket->Write(*request);

    connMgr_->PushPacket(sendPacket);
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Waiting!");

    coro->Yield();  // yield当前协程，直到收到回包


    // 协程被唤醒时，判断协程是否超时或者被取消
    if (coro->IsTimeouted())
    {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "Coro %d timeouted", coro->GetId());
        controller->SetFailed("Coro timeouted");
        return;
    }
    else if(coro->IsCancelled())
    {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "Coro %d cancelled", coro->GetId());
        controller->SetFailed("Coro cancelled");
        return;
    }


    // 没有出错的情况，从协程获取接收到的回包
    LLBC_Packet *recvPacket = reinterpret_cast<LLBC_Packet *>(coro->GetPtrParam1());
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "PayLoad:%s", recvPacket->ToString().c_str());

    std::string errText;
    if (recvPacket->Read(errText) != LLBC_OK)
    {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "Read errText fail");
        controller->SetFailed("Read errText fail");
        return;
    }

    if (recvPacket->Read(*response) != LLBC_OK)
    {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "Read recvPacket fail");
        controller->SetFailed("Read recvPacket fail");
        return;
    }

    if (!errText.empty())
    {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "Recv errText: %s", errText.c_str());
        controller->SetFailed(errText);
    }

    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Recved: %s", response->DebugString().c_str());

}