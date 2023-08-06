/*
 * @file:
 * @Author: ligengchao

 * @Date: 2023-06-20 20:13:31
 * @edit: ligengchao
 * @brief:
 */

#include "rpc_coro_mgr.h"

// // 用协程管理器单例，TODO: 用单例实现较好
// BaseRpcCoroMgr *s_RpcCoroMgr = nullptr;

using namespace llbc;

Coro::Coro(RpcCoroMgr *coMgr, int coroId, const CoroEntry &entry, void *args,
           llbc::LLBC_TimerScheduler *timerScheduler)
    : coroId_(coroId), coMgr_(coMgr)

      ,
      innerCoro_(nullptr), entry_(entry), args_(args),
      timer_(nullptr, nullptr, timerScheduler)

      ,
      yielded_(false), timeouted_(false), cancelled_(false), unused_(false)

      ,
      yieldTimes_(0), resumeTimes_(0)

      ,
      opcode_(0) {
  timer_.SetTimeoutHandler(this, &Coro::OnCoroTimeout);
  timer_.SetCancelHandler(this, &Coro::OnCoroCancel);
}

Coro::~Coro() {
  LLOG(nullptr, nullptr, LLBC_LogLevel::Trace,
       "Delete coro, id:%d, scheduleFlag:[yielded:%s, timeouted:%s, "
       "cancelled:%s], times[yield:%lu, resume:%lu]",
       coroId_, yielded_ ? "true" : "false", timeouted_ ? "true" : "false",
       cancelled_ ? "true" : "false", yieldTimes_, resumeTimes_);

  if (IsYielded())
    timer_.Cancel();

  if (innerCoro_ && !innerCoro_->cIsMain)
    co_release(innerCoro_);
}

int Coro::SetEntry(const CoroEntry &entry) {
  if (resumeTimes_ != 0) {
    LOG_ERROR(
         "Coro[%d] not allow set entry when coro already resumed[times:%lu]",
         coroId_, resumeTimes_);
    LLBC_SetLastError(LLBC_ERROR_NOT_ALLOW);

    return LLBC_FAILED;
  }

  entry_ = entry;

  return LLBC_OK;
}

int Coro::SetArgs(void *args) {
  if (resumeTimes_ != 0) {
    LOG_ERROR(
         "CORO[%d] not allow set args when coro already resumed[times:%lu]",
         coroId_, resumeTimes_);
    LLBC_SetLastError(LLBC_ERROR_NOT_ALLOW);

    return LLBC_FAILED;
  }

  args_ = args;

  return LLBC_OK;
}

int Coro::Yield(const LLBC_TimeSpan &timeout) {
  // 重复挂起检查
  if (IsYielded()) {
    LOG_ERROR("Coro[%d] already yielded",
         coroId_);
    LLBC_SetLastError(LLBC_ERROR_REPEAT);

    return LLBC_FAILED;
  }

  // 不允许挂起非当前协程
  if (this != coMgr_->GetCurCoro()) {
    LOG_ERROR(
         "Not allow yield non-current coro, coro:%d, cur-coro:%d", coroId_,
         coMgr_->GetCurCoro()->coroId_);
    LLBC_SetLastError(LLBC_ERROR_NOT_ALLOW);

    return LLBC_FAILED;
  }

  // 不允许挂起main coro
  if (IsMainCoro()) {
    LOG_ERROR(
         "Not allow yield main coro, coro:%d", coroId_);
    LLBC_SetLastError(LLBC_ERROR_NOT_ALLOW);

    return LLBC_FAILED;
  }

  // 记录开始挂起时间
  const auto begYieldTime = LLBC_RdTsc();
  const auto actualTimeout = timeout <= LLBC_TimeSpan::zero
                                 ? LLBC_TimeSpan::FromMillis(CoroTimeoutTime)
                                 : timeout; // 超时时间

  // 自增yield次数 并 Log
  ++yieldTimes_;
  LLOG(nullptr, nullptr, LLBC_LogLevel::Trace,
       "Yield coro:%d, yield times:%lu, timeout:%d.%03d secs", coroId_,
       yieldTimes_, actualTimeout.GetTotalSeconds(),
       actualTimeout.GetMilliSeconds());

  // 调用Timer
  timer_.Schedule(actualTimeout);

  // 标记 已挂起
  yielded_ = true;
  // 重置旧Resume设置的标记位
  timeouted_ = false;
  cancelled_ = false;
  // 重置passData
  passData_.BecomeNil();
  // 挂起协程
  co_yield_ct();

  // 断言
  ASSERT(yielded_ == false && "Coro internal error");
  // 计算Cost Time 并 Log
  const LLBC_CPUTime costTime(LLBC_RdTsc() - begYieldTime);
  const auto logLv =
      IsTimeoutedOrCancelled() ? LLBC_LogLevel::Warn : LLBC_LogLevel::Trace;
  LLOG(nullptr, nullptr, logLv,
       "Yield end, coro:%d, timeouted?:%s, cancelled?:%s, cost:%lld.%03lld us",
       coroId_, IsTimeouted() ? "true" : "false",
       IsCancelled() ? "true" : "false", costTime.ToMicroSeconds(),
       costTime.ToNanoSeconds() % 1000);

  if (IsTimeoutedOrCancelled()) {
    LLBC_SetLastError(LLBC_ERROR_UNKNOWN);
    return LLBC_FAILED;
  }

  return LLBC_OK;
}

int Coro::Resume(const LLBC_Variant &passData) {
  // 检查是否未挂起
  if (!IsYielded()) {
    LOG_ERROR(
         "Coro %d not yield, resume failed", coroId_);
    LLBC_SetLastError(LLBC_ERROR_NOT_ALLOW);
    return LLBC_FAILED;
  }

  // 自增resume次数 并 Log
  ++resumeTimes_;
  LLOG(nullptr, nullptr, LLBC_LogLevel::Trace,
       "Resume coro %d, resume times:%lu", coroId_, resumeTimes_);

  // 重置 挂起标记
  yielded_ = false;
  // 保存passData
  passData_ = passData;
  // 如果非timeout/cancel, cancel timer
  if (!IsTimeoutedOrCancelled())
    timer_.Cancel();

  // 执行coro resume
  co_resume(innerCoro_);

  return LLBC_OK;
}

int Coro::Cancel() {
  if (!IsYielded()) {
    LOG_ERROR(
         "Coro %d not yield, cancel failed", coroId_);
    LLBC_SetLastError(LLBC_ERROR_NOT_ALLOW);
    return LLBC_FAILED;
  }

  LOG_TRACE("Cancel coro %d", coroId_);
  timer_.Cancel();

  return 0;
}

void Coro::OnCoroTimeout(LLBC_Timer *timer) {
  LOG_WARN("Coro %d timeout", coroId_);

  timeouted_ = true;
  LLBC_SetLastError(LLBC_ERROR_TIMEOUTED);
  timer_.Cancel();

  Resume();
}

void Coro::OnCoroCancel(LLBC_Timer *timer) {
  if (IsYielded() && !timer_.IsTimeouting()) {
    LOG_TRACE("Coro %d cancel", coroId_);

    cancelled_ = true;
    LLBC_SetLastError(LLBC_ERROR_CANCELLED);

    Resume();
  }
}

void Coro::SetInnerCoro(stCoRoutine_t *innerCoro) {
  innerCoro_ = innerCoro;
  yielded_ = innerCoro->cIsMain ? false : true;
}

RpcCoroMgr::RpcCoroMgr() : maxCoroId_(0) {}

RpcCoroMgr::~RpcCoroMgr() {}

Coro *RpcCoroMgr::GetCoro(int coroId) {
  auto it = coros_.find(coroId);
  return it != coros_.end() ? it->second : nullptr;
}

Coro *RpcCoroMgr::GetCurCoro() {
  auto innerCoro = co_self();

  int coroId;
  memcpy(&coroId, &innerCoro->aSpec[0].value, sizeof(coroId));
  return GetCoro(coroId);
}

Coro *RpcCoroMgr::CreateCoro(const CoroEntry &entry, void *args,
                             size_t stackSize) {
  auto coroId = GenCoroId();

  auto coro = new Coro(this, coroId, entry, args, &timerScheduler_);

  stCoRoutine_t *innerCoro;
  stCoRoutineAttr_t coroAttr;
  if (stackSize != 0)
    coroAttr.stack_size = stackSize;
  co_create(&innerCoro, &coroAttr, &RpcCoroMgr::Entry, coro);
  memcpy(&innerCoro->aSpec[0].value, &coroId, sizeof(coroId));

  coro->SetInnerCoro(innerCoro);

  coros_.emplace(coroId, coro);

  LOG_TRACE("Create coro %d", coroId);

  return coro;
}

int RpcCoroMgr::Yield(const LLBC_TimeSpan &timeout) {
  return GetCurCoro()->Yield(timeout);
}

int RpcCoroMgr::Resume(int coroId, const LLBC_Variant &passData) {
  auto coro = GetCoro(coroId);
  if (!coro) {
    LOG_ERROR(
         "Coro %d not found, resume failed", coroId);
    LLBC_SetLastError(LLBC_ERROR_INVALID);
    return LLBC_FAILED;
  }

  return coro->Resume(passData);
}

int RpcCoroMgr::Cancel(int coroId) {
  auto coro = GetCoro(coroId);
  if (!coro) {
    LOG_ERROR(
         "Coro %d not found, cancel failed", coroId);
    LLBC_SetLastError(LLBC_ERROR_INVALID);
    return LLBC_FAILED;
  }

  return coro->Cancel();
}

bool RpcCoroMgr::Init() {

  // Log
  // Start()
  return true;
}

bool RpcCoroMgr::Start() {
  // 初始化maxCoroId_
  maxCoroId_ = 0;

  // 获取libco主协程对象
  auto mainInnerCoro = co_self();
  if (!mainInnerCoro->cIsMain)
  {
    LLBC_SetLastError(LLBC_ERROR_INITED);
    LOG_ERROR("Start failed, not main coro");
    return false;
  }

  // 创建包装主协程对象
  const auto coroId = GenCoroId();
  auto mainCoro = new Coro(this, coroId, nullptr, nullptr, &timerScheduler_);
  memcpy(&mainInnerCoro->aSpec[0].value, &coroId, sizeof(coroId));
  mainCoro->SetInnerCoro(mainInnerCoro);

  // 添加到协程集
  coros_.emplace(coroId, mainCoro);

  // Log
  LOG_INFO("Start");

  return true;
}

void RpcCoroMgr::Stop() {
  LOG_WARN("Stop");

  // 取消所有挂起状态的coros
  for (auto it = coros_.begin(); it != coros_.end();) {
    auto &coro = it++->second;
    if (coro->IsYielded() && !coro->IsMainCoro())
      coro->Cancel();
  }

  // 清理调度完的coros
  Update();
  // // 清理其他coros
  // LLBC_STLHelper::RecycleContainer(coros_, true);

  // 重置maxCoroId_
  maxCoroId_ = 0;
}

void RpcCoroMgr::Update() {
  timerScheduler_.Update();

  if (waitingForRecycleCoros_.empty())
    return;

  LLOG(nullptr, nullptr, LLBC_LogLevel::Trace,
       "Delete all waiting for release coros, size:%lu",
       waitingForRecycleCoros_.size());
  LLBC_STLHelper::RecycleContainer(waitingForRecycleCoros_);

  LLBC_AutoReleasePoolStack::GetCurrentThreadReleasePoolStack()->Purge();
}

int RpcCoroMgr::GenCoroId() {
  ++maxCoroId_;
  if (UNLIKELY(maxCoroId_ < 0))
    maxCoroId_ = 2;

  return maxCoroId_;
}

void RpcCoroMgr::MaskWillRecycle(Coro *coro) {
  if (!waitingForRecycleCoros_.insert(std::make_pair(coro->GetId(), coro))
           .second) {
    LOG_ERROR(
         "Repeated recycle coro, coroId:%d", coro->GetId());
    return;
  }

  coros_.erase(coro->GetId());
  LOG_TRACE("Recycle coro, coroId:%d",
       coro->GetId());
}

void *RpcCoroMgr::Entry(void *args) {
  auto coro = reinterpret_cast<Coro *>(args);
  (coro->GetEntry())(coro->GetArgs());

  coro->GetCoroMgr()->MaskWillRecycle(coro);

  return nullptr;
}
