/*
 * @file:
 * @Author: ligengchao

 * @Date: 2023-06-15 20:21:17
 * @edit: ligengchao
 * @brief:
 */
#include "rpc_channel.h"
#include "conn_mgr.h"
#include "llbc.h"
#include "rpc_coro_mgr.h"

using namespace llbc;

RpcChannel::~RpcChannel() {}; // { connMgr_->CloseSession(sessionId_); }

void RpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                            ::google::protobuf::RpcController *controller,
                            const ::google::protobuf::Message *request,
                            ::google::protobuf::Message *response,
                            ::google::protobuf::Closure *) {
  response->Clear();

  // 不允许在主协程中call Rpc
  auto coro = s_RpcCoroMgr->GetCurCoro();
  if (coro->IsMainCoro()) {
    LOG_ERROR(
         "Main coro not allow call Yield() method!");
    return;
  }

  // 创建并编码发送包
  LOG_TRACE("CallMethod!");
  LLBC_Packet *sendPacket = LLBC_GetObjectFromSafetyPool<LLBC_Packet>();
  sendPacket->SetHeader(0, RpcOpCode::RpcReq, 0);
  sendPacket->SetSessionId(sessionId_);
  sendPacket->Write(method->service()->name());
  sendPacket->Write(method->name());
  sendPacket->Write(s_RpcCoroMgr->GetCurCoroId());
  sendPacket->Write(*request);

  connMgr_->PushPacket(sendPacket);
  LOG_TRACE("Waiting!");

  coro->Yield(); // yield当前协程，直到收到回包

  // 协程被唤醒时，判断协程是否超时或者被取消。如果是，则设置协程状态并返回
  if (coro->IsTimeouted()) {
    LOG_ERROR("Coro %d timeouted",
         coro->GetId());
    controller->SetFailed("Coro timeouted");
    return;
  } else if (coro->IsCancelled()) {
    LOG_ERROR("Coro %d cancelled",
         coro->GetId());
    controller->SetFailed("Coro cancelled");
    return;
  }

  // 没有出错的情况，从协程获取接收到的回包
  LLBC_Packet *recvPacket =
      reinterpret_cast<LLBC_Packet *>(coro->GetPtrParam1());
  LOG_TRACE("PayLoad:%s",
       recvPacket->ToString().c_str());

  // 读取包状态及rsp
  std::string errText;
  if (recvPacket->Read(errText) != LLBC_OK) {
    LOG_ERROR("Read errText fail");
    controller->SetFailed("Read errText fail");
    return;
  }
  if (recvPacket->Read(*response) != LLBC_OK) {
    LOG_ERROR("Read recvPacket fail");
    controller->SetFailed("Read recvPacket fail");
    return;
  }

  // 填充错误信息，如有
  if (!errText.empty()) {
    LOG_ERROR("Recv errText: %s",
         errText.c_str());
    controller->SetFailed(errText);
  }

  LOG_TRACE("Recved: %s",
       response->DebugString().c_str());
}