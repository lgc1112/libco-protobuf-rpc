/*
 * @file:
 * @Author: ligengchao

 * @Date: 2023-06-15 20:21:17
 * @edit: ligengchao
 * @brief:
 */

#pragma once

#include <iostream>

#include "google/protobuf/service.h"
#include "google/protobuf/stubs/common.h"
#include "rpc_def.h"
#include <google/protobuf/message.h>
class ConnMgr;

class RpcController : public ::google::protobuf::RpcController {
public:
  virtual void Reset(){};

  virtual bool Failed() const { return isFailed_; };
  virtual std::string ErrorText() const { return errorText_; };
  virtual void StartCancel(){};
  virtual void SetFailed(const std::string &reason) {
    isFailed_ = true;
    errorText_ = reason;
  };
  virtual bool IsCanceled() const { return false; };
  virtual void NotifyOnCancel(::google::protobuf::Closure * /* callback */){};

private:
  bool isFailed_ = false;
  std::string errorText_;
};

class RpcChannel : public ::google::protobuf::RpcChannel {
public:
  RpcChannel(ConnMgr *connMgr, int sessionId)
      : connMgr_(connMgr), sessionId_(sessionId) {}
  virtual ~RpcChannel();

  virtual void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                          ::google::protobuf::RpcController * /* controller */,
                          const ::google::protobuf::Message *request,
                          ::google::protobuf::Message *response,
                          ::google::protobuf::Closure *);

private:
  ConnMgr *connMgr_ = nullptr;
  int sessionId_ = 0;
};
