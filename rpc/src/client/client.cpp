#include <csignal>
#include <iostream>
#include "conn_mgr.h"
#include "echo.pb.h"
#include "llbc.h"
#include "rpc_channel.h"
#include "rpc_coro_mgr.h"
#include "rpc_service_mgr.h"

using namespace llbc;

bool stop = false;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    stop = true;
}

int main() {
    // 注册信号 SIGINT 和信号处理程序
    signal(SIGINT, signalHandler);

    // 初始化llbc库
    LLBC_Startup();
    LLBC_Defer(LLBC_Cleanup());

    // 初始化日志
    const std::string path = __FILE__;
    const std::string logPath = path.substr(0, path.find_last_of("/\\"))+ "/../../log/cfg/server_log.cfg";
    auto ret = LLBC_LoggerMgrSingleton->Initialize(logPath);
    if (ret == LLBC_FAILED) {
        std::cout << "Initialize logger failed, error: " << LLBC_FormatLastError()
                << "path:" << logPath
                << std::endl;
        return -1;
    }

    // 初始化连接管理器
    ConnMgr *connMgr = s_ConnMgr;
    ret = connMgr->Init();
    if (ret != LLBC_OK) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Initialize connMgr failed, error:%s", LLBC_FormatLastError());
        return -1;
    }

    // 创建rpc channel
    RpcChannel *channel = connMgr->GetRpcChannel("127.0.0.1", 6688);
    LLBC_Defer(delete channel);

    if (!channel) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "GetRpcChannel Fail");
        return -1;
    }

    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Client Start!");

    // 创建rpc req & resp
    echo::EchoRequest req;
    echo::EchoResponse rsp;
    req.set_msg("hello, myrpc.");

    // 创建rpc controller & stub
    MyController cntl;
    echo::EchoService_Stub stub(channel);

    s_RpcCoroMgr->Init();
    LLBC_Defer(s_RpcCoroMgr->Stop());
    RpcMgr serviceMgr(connMgr);

    // 创建协程并Resume
    auto func = [&cntl, &req, &rsp, &channel, &stub](void *){

        req.set_msg("Hello, Echo.");
        LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "Rpc Echo Call, msg:%s",
            req.msg().c_str());
        // 调用生成的rpc方法Echo,然后挂起协程等待返回
        stub.Echo(&cntl, &req, &rsp, nullptr);
        LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "Recv Echo Rsp, status:%s, rsp:%s", cntl.Failed() ? cntl.ErrorText().c_str() : "success", 
            rsp.msg().c_str());


        req.set_msg("Hello, RelayEcho.");
        LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "Rpc RelayEcho Call, msg:%s",
            req.msg().c_str());
        // 调用生成的rpc方法RelayEcho,然后挂起协程等待返回
        stub.RelayEcho(&cntl, &req, &rsp, nullptr);
        LLOG(nullptr, nullptr, LLBC_LogLevel::Info, "Recv RelayEcho Rsp, status:%s, rsp:%s", cntl.Failed() ? cntl.ErrorText().c_str() : "success", 
        rsp.msg().c_str());
    };

    // 主循环处理 rpc 请求
    int count = 0;
    while (!stop) {
        // 更新协程管理器，处理超时协程
        s_RpcCoroMgr->Update();
        // 更新连接管理器，处理接收到的rpc req和rsp
        auto isBusy = s_ConnMgr->Tick();
        if (!isBusy) {
            LLBC_Sleep(1);
            ++count;
            // 满1s就创建一个新协协程发rpc包
            if (count == 1000)
            {
                count = 0;
                s_RpcCoroMgr->CreateCoro(func, nullptr)->Resume();
            }
        }
    }

    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "client Stop");

    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
