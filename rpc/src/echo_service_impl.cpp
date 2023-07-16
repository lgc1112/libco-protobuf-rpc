/*
 * @file:
 * @Author: regangcli
 * @copyright: Tencent Technology (Shenzhen) Company Limited
 * @Date: 2023-06-21 15:47:04
 * @edit: regangcli
 * @brief:
 */
#include "echo_service_impl.h"
#include "llbc.h"
#include "conn_mgr.h"
#include "rpc_channel.h"
#include "rpc_coro_mgr.h"
using namespace llbc;
void MyEchoService::Echo(::google::protobuf::RpcController * /* controller */, const ::echo::EchoRequest *request,
                         ::echo::EchoResponse *response, ::google::protobuf::Closure *done) {
  LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "received, msg:%s",
       request->msg().c_str());
  //   LLBC_Sleep(5000); timeout test
  response->set_msg(std::string(" Echo >>>>>>> ") + request->msg());
}

void MyEchoService::RelayEcho(::google::protobuf::RpcController *controller,
                          const ::echo::EchoRequest *req,
                          ::echo::EchoResponse *rsp,
                          ::google::protobuf::Closure *done) {
  LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "RECEIVED MSG: %s",
       req->msg().c_str());

  // 初始化内部 rpc req & rsp
  echo::EchoRequest innerReq;
  innerReq.set_msg("Relay Call >>>>>>" + req->msg());
  echo::EchoResponse innerRsp;
  // 创建 rpc channel
  RpcChannel *channel = s_ConnMgr->GetRpcChannel("127.0.0.1", 6688);
  if (!channel) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "GetRpcChannel Fail");
    rsp->set_msg(req->msg() + " ---- inner rpc call server not exist");
    controller->SetFailed("GetRpcChannel Fail");
  }

  LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "call, msg:%s",
       innerReq.msg().c_str());

    // 创建rpc controller & stub
    RpcController cntl;
    echo::EchoService_Stub stub(channel);

    stub.Echo(&cntl, &innerReq, &innerRsp, nullptr);
  LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "Recv rsp, status:%s, rsp:%s",
       cntl.Failed() ? cntl.ErrorText().c_str() : "success",
       innerRsp.msg().c_str());
  if (cntl.Failed()) {
    controller->SetFailed(cntl.ErrorText());
  }

  rsp->set_msg(innerRsp.msg());
}

void MyEchoService::GetData(::google::protobuf::RpcController *controller,
                            const ::echo::GetDataReq *request,
                            ::echo::GetDataRsp *response,
                            ::google::protobuf::Closure *done) {
  LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "RECEIVED get MSG");
  response->set_msg(" ---- data from inner rpc call");
  done->Run();
}