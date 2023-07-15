/*
 * @file:
 * @Author: regangcli
 * @copyright: Tencent Technology (Shenzhen) Company Limited
 * @Date: 2023-06-20 20:13:23
 * @edit: regangcli
 * @brief:
 */

#pragma once

#include "llbc.h"
#include <co_routine_inner.h>

#define EnableCoroRpc

class RpcCoroMgr;

// Coro Entry定义
typedef llbc::LLBC_Delegate<void(void *)> CoroEntry;

// Coro对象包装
#pragma pack(push, 1)
class Coro
{
public:
    Coro(RpcCoroMgr *coMgr,
         int coroId,
         const CoroEntry &entry,
         void *args);
    ~Coro();

public:
    RpcCoroMgr *GetCoroMgr() const { return coMgr_; }

    int GetId() const { return coroId_; }
    stCoRoutine_t *GetInnerCoro() const { return innerCoro_; }

    const CoroEntry &GetEntry() { return entry_; };
    int SetEntry(const CoroEntry &entry);

    void *GetArgs() const { return args_; }
    int SetArgs(void *args);

    bool IsMainCoro() const { return innerCoro_ ? innerCoro_->cIsMain != 0 : false; }

    bool IsYielded() const { return yielded_; }
    bool IsTimeouted() const { return timeouted_; }
    bool IsCancelled() const { return cancelled_; }
    bool IsTimeoutedOrCancelled() const { return IsTimeouted() || IsCancelled(); }

    size_t GetYieldTimes() const { return yieldTimes_; }
    size_t GetResumeTimes() const { return resumeTimes_; }

    const llbc::LLBC_Variant &GetPassData() const { return passData_; }
    // 获取 coro param1
    llbc::uint64 GetParam1() const { return param1; }
    void SetParam1(llbc::uint64 param) { param1 = param; }
    // 获取 coro param2
    llbc::uint64 GetParam2() const { return param2; }
    void SetParam2(llbc::uint64 param) { param2 = param; }
    // 获取 coro ptrParam1
    void *GetPtrParam1() const { return ptrParam1; }
    void SetPtrParam1(void *param) { ptrParam1 = param; }

public:
    int Yield(const llbc::LLBC_TimeSpan &timeout = llbc::LLBC_TimeSpan::zero);
    int Resume(const llbc::LLBC_Variant &passData = llbc::LLBC_Variant::nil);
    int Cancel();

public:
    int GetOpcode() const { return opcode_; }
    void SetOpcode(int opcode) { opcode_ = opcode; }

protected:
    void OnCoroTimeout(llbc::LLBC_Timer *timer);
    void OnCoroCancel(llbc::LLBC_Timer *timer);

private:
    friend class RpcCoroMgr;
    void SetInnerCoro(stCoRoutine_t *innerCoro);

private:
    int coroId_;
    RpcCoroMgr *coMgr_;

    stCoRoutine_t *innerCoro_;
    CoroEntry entry_;
    void *args_;

    llbc::LLBC_Timer timer_;
    llbc::LLBC_Variant passData_;
    
    llbc::uint64 param1 = 0;
    llbc::uint64 param2 = 0;
    void *ptrParam1 = nullptr;

    bool yielded_;
    bool timeouted_;
    bool cancelled_;
    bool unused_;

    size_t yieldTimes_;
    size_t resumeTimes_;

    int opcode_;
};
#pragma pack(pop)

class RpcCoroMgr
{
public:
    RpcCoroMgr();
    virtual ~RpcCoroMgr();

    bool Init();
    void Stop();
    void Update();

    Coro *GetCoro(int coroId);
    Coro *GetCurCoro();
    int GetCurCoroId() { return GetCurCoro()->GetId();};

    Coro *CreateCoro(const CoroEntry &entry, void *args, size_t stackSize = 0);

    int Yield(const llbc::LLBC_TimeSpan &timeout = llbc::LLBC_TimeSpan::zero);
    int Resume(int coroId, const llbc::LLBC_Variant &passData = llbc::LLBC_Variant::nil);
    int Cancel(int coroId);

    size_t GetCorosSize() const { return coros_.size(); }
    size_t GetWaitingForRecycleCorosSize() const { return waitingForRecycleCoros_.size(); }

protected:


private:
    friend class Coro;

    int GenCoroId();

    void MaskWillRecycle(Coro *coro);
    static void *Entry(void *args);

private:
    int maxCoroId_;
    std::map<int, Coro *> coros_;
    std::map<int, Coro *>waitingForRecycleCoros_;
};


// class BaseRpcCoroMgr;
// class Coro {
//     LLBC_DISABLE_ASSIGNMENT(Coro);

// public:
//     Coro(const std::function<void(void *)> &entry, void *args, const llbc::LLBC_Variant &user_data);
//     ~Coro();

// public:
//     // 挂起 coro
//     int Yield(const llbc::LLBC_TimeSpan &timeout = llbc::LLBC_TimeSpan::zero);
//     // 恢复 coro
//     int Resume(const llbc::LLBC_Variant &pass_data = llbc::LLBC_Variant::nil);
//     // 判断是否为主协程
//     bool IsMainCoro() const;

//     // 取消 coro
//     int Cancel();

// public:
//     // 获取 coro ID
//     llbc::uint64 GetId() const { return coroId_; }

//     // 获取 coro entry
//     const std::function<void(void *)> &GetEntry() const { return entry_; }
//     // 获取 coro args
//     void *GetArgs() const { return args_; }

//     // 获取 coro param1
//     llbc::uint64 GetParam1() const { return param1; }
//     void SetParam1(llbc::uint64 param) { param1 = param; }
//     // 获取 coro param2
//     llbc::uint64 GetParam2() const { return param2; }
//     void SetParam2(llbc::uint64 param) { param2 = param; }
//     // 获取 coro ptrParam1
//     void *GetPtrParam1() const { return ptrParam1; }
//     void SetPtrParam1(void *param) { ptrParam1 = param; }

//     // 获取 coro user_data
//     std::string &GetUserData() { return userData_; }

// private:
//     BaseRpcCoroMgr *coroMgr{nullptr};             // coro component
//     llbc::uint64 coroId_{0};                      // coro id
//     std::function<void(void *)> entry_{nullptr};  // coro 逻辑执行 delegate
//     void *args_{nullptr};                         // coro 逻辑执行 delegate 参数
//     std::string &userData_;                       // coro user data
//     llbc::uint64 param1 = 0;
//     llbc::uint64 param2 = 0;
//     void *ptrParam1 = nullptr;
// };

// class BaseRpcCoroMgr {
// public:
//     BaseRpcCoroMgr();
//     virtual ~BaseRpcCoroMgr();

//     virtual bool HasCoro() = 0;

//     // Create coroutine.
//     virtual Coro *CreateCoro(const std::function<void(void *)> &entry, void *args, const std::string &userData) = 0;

//     virtual Coro *GetCoro(const int64_t &coroId) = 0;

//     // Yield coroutine.
//     virtual void YieldCoro(const int64_t &timeout = -1) = 0;

//     // Resume coroutine.
//     virtual void ResumeCoro(int64_t coroId, const std::string &pass_data = "") = 0;

//     // Get coroutine
//     virtual Coro *GetCurCoro() = 0;

//     // Get coroutine
//     virtual int64_t GetCurCoroId() = 0;
// };

// 协程管理器、Todo:待实现
// class RpcCoroMgr
// {
// public:
//     RpcCoroMgr();
//     virtual ~RpcCoroMgr();

//     virtual bool HasCoro() {};

//     // Create coroutine.
//     virtual int64_t CreateCoro(const std::function<void(void *)> &entry,
//                               void *args,
//                               const std::string &userData) { return 0; };

//     // Yield coroutine.
//     virtual void YieldCoro(const int64_t &timeout = -1) {};

//     // Resume coroutine.
//     virtual void ResumeCoro(int64_t coroId, const std::string &pass_data = "") {};

//     // Get coroutine
//     virtual int64_t GetCurCoroId() { return 0; };
// };

// 全局协程管理单例,Todo:待实现
// extern BaseRpcCoroMgr *g_rpcCoroMgr;
#define g_rpcCoroMgr ::llbc::LLBC_Singleton<RpcCoroMgr>::Instance()
