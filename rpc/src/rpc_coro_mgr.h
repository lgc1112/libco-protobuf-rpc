/*
 * @file:
 * @Author: ligengchao

 * @Date: 2023-06-20 20:13:23
 * @edit: ligengchao
 * @brief:
 */

#pragma once

#include "llbc.h"
#include <co_routine_inner.h>

class RpcCoroMgr;

// Coro Entry定义
typedef llbc::LLBC_Delegate<void(void *)> CoroEntry;
#define CoroTimeoutTime 5000 // milli seconds

// Coro对象包装
#pragma pack(push, 1)
class Coro {
public:
  Coro(RpcCoroMgr *coMgr, int coroId, const CoroEntry &entry, void *args,
       llbc::LLBC_TimerScheduler *timerScheduler);
  ~Coro();

public:
  RpcCoroMgr *GetCoroMgr() const { return coMgr_; }

  int GetId() const { return coroId_; }
  stCoRoutine_t *GetInnerCoro() const { return innerCoro_; }

  const CoroEntry &GetEntry() { return entry_; };
  int SetEntry(const CoroEntry &entry);

  void *GetArgs() const { return args_; }
  int SetArgs(void *args);

  bool IsMainCoro() const {
    return innerCoro_ ? innerCoro_->cIsMain != 0 : false;
  }

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

class RpcCoroMgr {
public:
  RpcCoroMgr();
  virtual ~RpcCoroMgr();

  bool Init();
  void Stop();
  void Update();

  Coro *GetCoro(int coroId);
  Coro *GetCurCoro();
  int GetCurCoroId() { return GetCurCoro()->GetId(); };

  Coro *CreateCoro(const CoroEntry &entry, void *args, size_t stackSize = 0);

  int Yield(const llbc::LLBC_TimeSpan &timeout = llbc::LLBC_TimeSpan::zero);
  int Resume(int coroId,
             const llbc::LLBC_Variant &passData = llbc::LLBC_Variant::nil);
  int Cancel(int coroId);

  size_t GetCorosSize() const { return coros_.size(); }
  size_t GetWaitingForRecycleCorosSize() const {
    return waitingForRecycleCoros_.size();
  }

protected:
private:
  friend class Coro;

  int GenCoroId();

  void MaskWillRecycle(Coro *coro);
  static void *Entry(void *args);

private:
  int maxCoroId_;
  std::map<int, Coro *> coros_;
  std::map<int, Coro *> waitingForRecycleCoros_;
  llbc::LLBC_TimerScheduler timerScheduler_;
};

#define s_RpcCoroMgr ::llbc::LLBC_Singleton<RpcCoroMgr>::Instance()
