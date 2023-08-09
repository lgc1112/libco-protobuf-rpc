/*
 * @Author: ligengchao ligengchao@pku.edu.cn
 * @Date: 2023-07-16 14:27:21
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2023-08-09 15:10:22
 * @FilePath: /projects/libco-protobuf-rpc/rpc/src/server/server.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#include "conn_mgr.h"
#include "echo.pb.h"
#include "llbc.h"
#include "rpc_channel.h"
#include "rpc_coro_mgr.h"
#include "rpc_mgr.h"
#include <climits>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sys/epoll.h>

using namespace llbc;

bool stop = false;

void signalHandler(int signum) {
  std::cout << "Interrupt signal (" << signum << ") received.\n";
  stop = true;
}

void testCoroTime() {
  long long rpcCallCount = 0;
  long long rpcCallTimeSum = 0;

  s_RpcCoroMgr->Init();
  // LLBC_Defer(s_RpcCoroMgr->Stop());

  auto func = [&rpcCallCount](void *) { rpcCallCount++; };

  for (int i = 0; i < 100000; i++) {
    s_RpcCoroMgr->CreateCoro(func, nullptr)->Resume();
  }
  rpcCallCount = 0;

  long long beginTime = llbc::LLBC_GetMicroSeconds();
  for (int i = 0; i < 1000000; i++) {
    auto coro = s_RpcCoroMgr->CreateCoro(func, nullptr);
    coro->Resume();
    delete coro;
  }

  long long endTime = llbc::LLBC_GetMicroSeconds();
  rpcCallTimeSum = endTime - beginTime;
  LOG_INFO("Test fin, Count:%lld, Total sum Time:%lld, Avg Time:%.2f",
           rpcCallCount, rpcCallTimeSum, (double)rpcCallTimeSum / rpcCallCount);
}

int main() {
  // testCoroTime();
  // exit(0);

  // 注册信号 SIGINT 和信号处理程序
  signal(SIGINT, signalHandler);

  // 初始化llbc库
  LLBC_Startup();
  LLBC_Defer(LLBC_Cleanup());

  // 初始化日志
  const std::string path = __FILE__;
  const std::string logPath = path.substr(0, path.find_last_of("/\\")) +
                              "/../../log/cfg/server_log.cfg";
  auto ret = LLBC_LoggerMgrSingleton->Initialize(logPath);
  if (ret == LLBC_FAILED) {
    std::cout << "Initialize logger failed, error: " << LLBC_FormatLastError()
              << "path:" << logPath << std::endl;
    return -1;
  }

  // 初始化连接管理器
  s_ConnMgr->Init();
  RpcMgr serviceMgr(s_ConnMgr);
  ret = s_ConnMgr->Start();
  if (ret != LLBC_OK) {
    LOG_TRACE("Initialize connMgr failed, error:%s", LLBC_FormatLastError());
    return -1;
  }

  // 1.获取 or 创建rpc channel
  RpcChannel *channel = s_ConnMgr->GetRpcChannel("127.0.0.1", 6688);
  LLBC_Defer(delete channel);

  if (!channel) {
    LOG_ERROR("GetRpcChannel Fail");
    return -1;
  }

  LOG_TRACE("Client Start!");

  // 创建rpc req & resp
  echo::EchoRequest req;
  echo::EchoResponse rsp;

  // 创建rpc controller & stub
  RpcController cntl;
  echo::EchoService_Stub stub(channel);

  s_RpcCoroMgr->Init();
  LLBC_Defer(s_RpcCoroMgr->Stop());

  long long printTime = 0;
  long long rpcCallCount = 0, failCount = 0, finishCount = 0;
  long long rpcCallTimeSum = 0;
  long long maxRpcCallTime = 0;
  long long minRpcCallTime = LLONG_MAX;

  req.set_msg("Hello, Echo.");
  // 创建协程并Resume
  auto func = [&cntl, &req, &rsp, &channel, &stub, &failCount, &rpcCallCount,
               &printTime, &maxRpcCallTime, &minRpcCallTime,
               &rpcCallTimeSum](void *) {
    long long beginRpcReqTime = llbc::LLBC_GetMicroSeconds();
    // 调用生成的rpc方法Echo,然后挂起协程等待返回
    stub.Echo(&cntl, &req, &rsp, nullptr);
    // LOG_INFO(
    //      "Recv Echo Rsp, status:%s, rsp:%s",
    //      cntl.Failed() ? cntl.ErrorText().c_str() : "success",
    //      rsp.msg().c_str());

    long long endTime = llbc::LLBC_GetMicroSeconds();
    long long tmpTime = endTime - beginRpcReqTime;
    rpcCallTimeSum += tmpTime;
    rpcCallCount++;
    if (cntl.Failed()) {
      ++failCount;
    }
    if (tmpTime > maxRpcCallTime)
      maxRpcCallTime = tmpTime;
    if (tmpTime < minRpcCallTime)
      minRpcCallTime = tmpTime;

    if (endTime - printTime >= 1000000) {
      LOG_INFO("Rpc Statistic fin, Count:%lld, Fail Count:%lld, Total sum "
               "Time:%lld, Avg Time:%.2f, Max Time:%lld, Min Time:%lld",
               rpcCallCount, failCount, rpcCallTimeSum,
               (double)rpcCallTimeSum / rpcCallCount, maxRpcCallTime,
               minRpcCallTime);
      printTime = endTime;
      rpcCallTimeSum = 0;
      rpcCallCount = 0;
      failCount = 0;
      maxRpcCallTime = 0;
      minRpcCallTime = LLONG_MAX;
    }
  };

  // 主循环处理 rpc 请求
  int count = 0;
  while (!stop) {
    // 更新协程管理器，处理超时协程
    s_RpcCoroMgr->Update();
    // 更新连接管理器，处理接收到的rpc req和rsp
    auto isBusy = s_ConnMgr->Tick();
    if (s_RpcCoroMgr->GetCorosSize() < 200)
      s_RpcCoroMgr->CreateCoro(func, nullptr)->Resume();
    else
      LLBC_Sleep(1);
  }

  LOG_TRACE("client Stop");

  return 0;
}
