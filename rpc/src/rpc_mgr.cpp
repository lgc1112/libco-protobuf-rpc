/*
 * @file:
 * @Author: regangcli

 * @Date: 2023-06-19 20:13:51
 * @edit: regangcli
 * @brief:
 */
#include "rpc_mgr.h"
#include "conn_mgr.h"
#include "echo.pb.h"
#include "llbc.h"
#include "rpc_channel.h"
#include "rpc_coro_mgr.h"
#include <google/protobuf/descriptor.pb.h>
using namespace llbc;

#ifdef EnableRpcPerfStat
long long rpcCallCount = 0, printTime = 0, beginRpcReqTime = 0;
long long rpcCallTimeSum = 0;
long long maxRpcCallTime = 0;
long long minRpcCallTime = LLONG_MAX;
#endif

RpcMgr::RpcMgr(ConnMgr *connMgr) : connMgr_(connMgr) {
  connMgr_->Subscribe(RpcOpCode::RpcReq, LLBC_Delegate<void(LLBC_Packet &)>(
                                             this, &RpcMgr::HandleRpcReq));
  connMgr_->Subscribe(RpcOpCode::RpcRsp, LLBC_Delegate<void(LLBC_Packet &)>(
                                             this, &RpcMgr::HandleRpcRsp));
}

RpcMgr::~RpcMgr() {
  connMgr_->Unsubscribe(RpcOpCode::RpcReq);
  connMgr_->Unsubscribe(RpcOpCode::RpcRsp);
}

void RpcMgr::AddService(google::protobuf::Service *service) {
  ServiceInfo service_info;
  service_info.service = service;
  service_info.sd = service->GetDescriptor();
  for (int i = 0; i < service_info.sd->method_count(); ++i) {
    service_info.mds[service_info.sd->method(i)->name()] =
        service_info.sd->method(i);
  }

  _services[service_info.sd->name()] = service_info;
}

void RpcMgr::HandleRpcReq(LLBC_Packet &packet) {
#ifdef EnableRpcPerfStat
  beginRpcReqTime = llbc::LLBC_GetMicroSeconds();
#endif

  // 读取serviceName&methodName
  int srcCoroId;
  std::string serviceName, methodName;
  if (packet.Read(serviceName) != LLBC_OK ||
      packet.Read(methodName) != LLBC_OK || packet.Read(srcCoroId) != LLBC_OK) {
    LOG_ERROR("read packet failed");
    return;
  }

  auto service = _services[serviceName].service;
  auto md = _services[serviceName].mds[methodName];
  LOG_TRACE("recv service_name:%s", serviceName.c_str());
  LOG_TRACE("recv method_name:%s", methodName.c_str());
  LOG_TRACE("srcCoroId:%d", srcCoroId);

  // 解析req & 创建rsp
  auto req = service->GetRequestPrototype(md).New();
  if (packet.Read(*req) != LLBC_OK) {
    LOG_ERROR("read packet failed");
    delete req;
    return;
  }
  auto rsp = service->GetResponsePrototype(md).New();
  auto sessionId = packet.GetSessionId();

  bool isCoroRequired = md->options().GetExtension(echo::is_coro_required);
  // LOG_INFO("recv method_name:%s, isCoroRequired:%d",
  //      methodName.c_str(), isCoroRequired);
  // 如果不需要协程，直接在当前协程中处理rpc请求并返回
  if (!isCoroRequired) {
    RpcController controller;
    // 调用CallMethod接口，CallMethod内部就会最终调用到用户重写的方法service方法，在用户重写的service方法中读取req，产生rsp
    service->CallMethod(md, &controller, req, rsp, nullptr);
    // 最后调用OnRpcDone回包
    OnRpcDone(controller, rsp, sessionId, srcCoroId);

#ifdef EnableRpcPerfStat
    long long endTime = llbc::LLBC_GetMicroSeconds();
    long long tmpTime = endTime - beginRpcReqTime;
    rpcCallTimeSum += tmpTime;
    rpcCallCount++;
    if (tmpTime > maxRpcCallTime)
      maxRpcCallTime = tmpTime;
    if (tmpTime < minRpcCallTime)
      minRpcCallTime = tmpTime;

    if (endTime - printTime >= 1000000) {
      LOG_INFO("Rpc Statistic fin, Count:%lld, Total sum Time:%lld, Avg "
               "Time:%.2f, Max Time:%lld, Min Time:%lld",
               rpcCallCount, rpcCallTimeSum,
               (double)rpcCallTimeSum / rpcCallCount, maxRpcCallTime,
               minRpcCallTime);
      printTime = endTime;
      rpcCallTimeSum = 0;
      rpcCallCount = 0;
      maxRpcCallTime = 0;
      minRpcCallTime = LLONG_MAX;
    }
#endif

    delete req;
    delete rsp;
    return;
  }

  //  需要协程处理，创建新协程处理rpc请求
  auto func = [&packet, service, md, req, rsp, sessionId, srcCoroId,
               this](void *) {
    RpcController controller;
    // 调用CallMethod接口，CallMethod内部就会最终调用到用户重写的方法service方法，在用户重写的service方法中读取req，产生rsp
    service->CallMethod(md, &controller, req, rsp, nullptr);
    // 最后调用OnRpcDone回包
    OnRpcDone(controller, rsp, sessionId, srcCoroId);
    delete req;
    delete rsp;
  };
  Coro *coro = s_RpcCoroMgr->CreateCoro(func, nullptr);

  LOG_TRACE("SessionId:%d, srcCoroId:%d", packet.GetSessionId(), srcCoroId);
  // 启动协程，如协程内部有内嵌发起rpc请求，会在协程内部发起请求后，直接回到此处
  coro->Resume();

#ifdef EnableRpcPerfStat
  long long endTime = llbc::LLBC_GetMicroSeconds();
  long long tmpTime = endTime - beginRpcReqTime;
  rpcCallTimeSum += tmpTime;
  rpcCallCount++;
  if (tmpTime > maxRpcCallTime)
    maxRpcCallTime = tmpTime;
  if (tmpTime < minRpcCallTime)
    minRpcCallTime = tmpTime;

  if (endTime - printTime >= 1000000) {
    LOG_INFO("Rpc Statistic fin, Count:%lld, Total sum Time:%lld, Avg "
             "Time:%.2f, Max Time:%lld, Min Time:%lld",
             rpcCallCount, rpcCallTimeSum,
             (double)rpcCallTimeSum / rpcCallCount, maxRpcCallTime,
             minRpcCallTime);
    printTime = endTime;
    rpcCallTimeSum = 0;
    rpcCallCount = 0;
    maxRpcCallTime = 0;
    minRpcCallTime = LLONG_MAX;
  }
#endif
}

void RpcMgr::HandleRpcRsp(LLBC_Packet &packet) {
  // 读取源协程Id
  int dstCoroId = 0;
  if (packet.Read(dstCoroId) != LLBC_OK) {
    LOG_ERROR("read packet failed");
    return;
  }

  LOG_TRACE("dstCoroId:%d, packet:%s", dstCoroId, packet.ToString().c_str());

  // 获取并恢复对应协程处理
  Coro *coro = s_RpcCoroMgr->GetCoro(dstCoroId);
  if (!coro) {
    LOG_ERROR("coro not found, coroId:%d", dstCoroId);
  } else {
    coro->SetPtrParam1(&packet);
    coro->Resume();
  }
}

void RpcMgr::OnRpcDone(RpcController &controller,
                       google::protobuf::Message *rsp, int sessionId,
                       int srcCoroId) {
  LOG_TRACE("OnRpcDone, rsp:%s", rsp->DebugString().c_str());

  LOG_TRACE("coroSessionId:%d, srcCoroId:%d", sessionId, srcCoroId);
  auto packet = LLBC_GetObjectFromSafetyPool<LLBC_Packet>();
  packet->SetSessionId(sessionId);
  packet->SetOpcode(RpcOpCode::RpcRsp);
  packet->Write(srcCoroId);
  packet->Write(controller.ErrorText());
  packet->Write(*rsp);
  LOG_TRACE("rsp packet:%s", packet->ToString().c_str());
  // 回包
  connMgr_->PushPacket(packet);
}