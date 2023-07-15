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
using namespace llbc;
void MyEchoService::Echo(::google::protobuf::RpcController * /* controller */, const ::echo::EchoRequest *request,
                         ::echo::EchoResponse *response, ::google::protobuf::Closure *done) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "received, msg:%s", request->msg().c_str());
    response->set_msg(std::string("I have received '") + request->msg() + std::string("'"));
}