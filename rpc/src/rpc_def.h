/*
 * @Author: regangcli regangcli@tencent.com
 * @Date: 2023-08-05 14:25:43
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2023-08-09 15:18:52
 * @FilePath: /projects/libco-protobuf-rpc/rpc/src/rpc_def.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

// #define EnableRpcPerfStat 1 

#ifdef EnableRpcPerfStat
#define LOG_ERROR(format, ...)                                                 \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Error, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)                                                  \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Warn, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)                                                  \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Info, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...)
#define LOG_TRACE(format, ...)
#else
#define LOG_ERROR(format, ...)                                                 \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Error, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)                                                  \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Warn, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)                                                  \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Info, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...)                                                 \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Debug, format, ##__VA_ARGS__)
#define LOG_TRACE(format, ...)                                                 \
  LLOG(nullptr, nullptr, LLBC_LogLevel::Trace, format, ##__VA_ARGS__)
#endif
