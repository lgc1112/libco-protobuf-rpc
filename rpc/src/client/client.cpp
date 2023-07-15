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

// int main() 
// {
//     echo::EchoRequest req;
//     req.set_msg("hello, myrpc.");
//     echo::EchoRequest req2;
//     LLBC_Packet packet;
//     packet.Write(req);
//     LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "req: %s", req.DebugString().c_str());
//     packet.Read(req2);
//     LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "req2: %s", req2.DebugString().c_str());
//     return 0;
// }

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
    RpcChannel *channel = connMgr->CreateRpcChannel("127.0.0.1", 6688);
    LLBC_Defer(delete channel);

    if (!channel) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "CreateRpcChannel Fail");
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

#ifdef EnableCoroRpc
    g_rpcCoroMgr->Init();
    LLBC_Defer(g_rpcCoroMgr->Stop());
    RpcServiceMgr serviceMgr(connMgr);

    // 创建协程并Resume
    auto func = [&cntl, &req, &rsp, &channel, &stub](void *){
        rsp.Clear();
        stub.Echo(&cntl, &req, &rsp, nullptr);
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "RECEIVED RSP: %s", rsp.msg().c_str());
    };

    // for (int i = 0; i < 2; i++)
    // {
    //     auto coro = g_rpcCoroMgr->CreateCoro(func, nullptr);
    //     coro->Resume();
    // }

    // 死循环处理 rpc 请求
    int count = 0;
    while (!stop) {
        // tick 处理接收到的 rpc req和rsp
        // 若有rsp，Tick内部会调用Rsp处理函数，从而唤醒对应休眠的协程
        g_rpcCoroMgr->Update();
        auto isBusy = connMgr->Tick();
        if (!isBusy) {
            LLBC_Sleep(1);
            ++count;
            // 满1s就创建一个新协协程发一个包
            if (count == 1000)
            {
                count = 0;
                auto coro = g_rpcCoroMgr->CreateCoro(func, nullptr);
                coro->Resume();
            }
        }
    }

#else
    // 直接调用方案
    while (!stop) {
        stub.Echo(&cntl, &req, &rsp, nullptr);
        // std::cout << "resp:" << response.msg() << std::endl;
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "recv rsp:%s", rsp.msg().c_str());
        LLBC_Sleep(1000);
    }
#endif

    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "client Stop");

    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
