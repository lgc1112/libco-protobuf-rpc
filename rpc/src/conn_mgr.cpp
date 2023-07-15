/*
 * @file:
 * @Author: regangcli
 * @copyright: Tencent Technology (Shenzhen) Company Limited
 * @Date: 2023-06-19 20:34:15
 * @edit: regangcli
 * @brief:
 */
#include "conn_mgr.h"
#include "rpc_channel.h"
#include <co_routine_inner.h>

ConnComp::ConnComp() : LLBC_Component(LLBC_ComponentEvents::DefaultEvents | LLBC_ComponentEvents::OnUpdate) { }

bool ConnComp::OnInit(bool &initFinished) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Service create!");
    return true;
}

void ConnComp::OnDestroy(bool &destroyFinished) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Service destroy!");
}

void ConnComp::OnSessionCreate(const LLBC_SessionInfo &sessionInfo) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Session Create: %s", sessionInfo.ToString().c_str());
}

void ConnComp::OnSessionDestroy(const LLBC_SessionDestroyInfo &destroyInfo) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Session Destroy, info: %s", destroyInfo.ToString().c_str());
}

void ConnComp::OnAsyncConnResult(const LLBC_AsyncConnResult &result) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Async-Conn result: %s", result.ToString().c_str());
}

void ConnComp::OnUnHandledPacket(const LLBC_Packet &packet) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Unhandled packet, sessionId: %d, opcode: %d, payloadLen: %ld",
         packet.GetSessionId(), packet.GetOpcode(), packet.GetPayloadLength());
}

void ConnComp::OnProtoReport(const LLBC_ProtoReport &report) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Proto report: %s", report.ToString().c_str());
}

void ConnComp::OnUpdate() {
    auto *sendPacket = sendQueue_.Pop();
    while (sendPacket) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "sendPacket:%s", sendPacket->ToString().c_str());
        auto ret = GetService()->Send(sendPacket);
        if (ret != LLBC_OK) {
            LLOG(nullptr, nullptr, LLBC_LogLevel::Error, "Send packet failed, err: %s", LLBC_FormatLastError());
        }

        // LLBC_Recycle(sendPacket);
        sendPacket = sendQueue_.Pop();
    }
}

void ConnComp::OnRecvPacket(LLBC_Packet &packet) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "OnRecvPacket:%s", packet.ToString().c_str());
    LLBC_Packet *recvPacket = LLBC_GetObjectFromUnsafetyPool<LLBC_Packet>();
    recvPacket->SetHeader(packet, packet.GetOpcode(), 0);
    recvPacket->SetPayload(packet.DetachPayload());
    recvQueue_.Push(recvPacket);
}

int ConnComp::PushPacket(LLBC_Packet *sendPacket) {
    if (sendQueue_.Push(sendPacket))
        return LLBC_OK;

    return LLBC_FAILED;
}

ConnMgr::ConnMgr() {
    // Init();
}

ConnMgr::~ConnMgr() { }

int ConnMgr::Init() {
    // Create service
    svc_ = LLBC_Service::Create("SvcTest");
    comp_ = new ConnComp;
    svc_->AddComponent(comp_);
    svc_->Subscribe(RpcOpCode::RpcReq, comp_, &ConnComp::OnRecvPacket);
    svc_->Subscribe(RpcOpCode::RpcRsp, comp_, &ConnComp::OnRecvPacket);
    svc_->SuppressCoderNotFoundWarning();
    auto ret = svc_->Start(1);
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Service start, ret: %d", ret);
    return ret;
}

int ConnMgr::StartRpcService(const char *ip, int port) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "ConnMgr StartRpcService");
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Server Will listening in %s:%d", ip, port);
    int serverSessionId_ = svc_->Listen(ip, port);
    if (serverSessionId_ == 0) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Create session failed, reason: %s", LLBC_FormatLastError());
        return LLBC_FAILED;
    }

    isServer_ = true;
    return LLBC_OK;
}

// 创建rpc客户端通信通道
RpcChannel *ConnMgr::CreateRpcChannel(const char *ip, int port) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "CreateRpcChannel");
    auto sessionId = svc_->Connect(ip, port);
    if (sessionId == 0) {
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Create session failed, reason: %s", LLBC_FormatLastError());
        return nullptr;
    }

    return new RpcChannel(this, sessionId);
}

int ConnMgr::CloseSession(int sessionId) {
    LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "CloseSession, %d", sessionId);
    return svc_->RemoveSession(sessionId);
}

bool ConnMgr::Tick() {
    auto ret = false;
    // LLOG(nullptr, nullptr, LLBC_LogLevel::Warn, "Tick");
    // 读取接收到的数据包并给对应的订阅者处理
    auto packet = PopPacket();
    while (packet) {
        ret = true;
        LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Tick");
        auto it = packetDelegs_.find(packet->GetOpcode());
        if (it == packetDelegs_.end())
            LLOG(nullptr, nullptr, LLBC_LogLevel::Warn, "Recv Untapped opcode:%d", packet->GetOpcode());
        else
            (it->second)(*packet);

        // 取下一个包
        LLBC_Recycle(packet);
        packet = PopPacket();
    }

    return ret;
}

int ConnMgr::Subscribe(int cmdId, const LLBC_Delegate<void(LLBC_Packet &)> &deleg) {
    auto pair = packetDelegs_.emplace(cmdId, deleg);
    if (!pair.second) {
        return LLBC_FAILED;
    }

    return LLBC_OK;
}

void ConnMgr::Unsubscribe(int cmdId) {
    packetDelegs_.erase(cmdId);
}

// int ConnMgr::Init(const char *ip, int port, bool asClient)
// {
//     LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "ConnMgr Init As %s", asClient ? "Client" : "Server");

//     // Create service
//     LLBC_Service *svc = LLBC_Service::Create("SvcTest");
//     comp_ = new ConnComp;
//     svc->AddComponent(comp_);
//     svc->Subscribe(RpcOpCode::RpcReq, comp_, &ConnComp::OnRecvData);
//     svc->Subscribe(RpcOpCode::RpcRsp, comp_, &ConnComp::OnRecvData);
//     svc->SuppressCoderNotFoundWarning();
//     svc->Start(1);

//     // Connect to server / Create listen session to wait client connect.
//     // LLBC_SessionOpts sessionOpts;
//     // sessionOpts.SetSockSendBufSize(1 * 1024 * 1024);
//     // sessionOpts.SetSockRecvBufSize(1 * 1024 * 1024);
//     if (!asClient)
//     {
//         isServer_ = true;
//         LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Server Will listening in %s:%d", ip, port);
//         int sessionId = svc->Listen(ip, port);
//         if (sessionId == 0)
//         {
//             LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Create session failed, reason: %s",
//             LLBC_FormatLastError()); delete svc;

//             return LLBC_FAILED;
//         }
//     }
//     else
//     {
//         LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Client Start");
//         sendSessionId_ = svc->Connect(ip, port);
//         if (sendSessionId_ == 0)
//         {
//             LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, "Create session failed, reason: %s",
//             LLBC_FormatLastError()); delete svc;

//             return LLBC_FAILED;
//         }
//         isServer_ = false;
//     }
//     return LLBC_OK;
// }