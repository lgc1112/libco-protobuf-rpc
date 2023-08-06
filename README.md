# Introduction 
libco-protobuf-rpc is a lightweight, simple, and easy-to-understand high-performance RPC implementation framework based on libco stackful coroutines and protobuf. With a small amount of code, it implements all the basic features of RPC, making it suitable for learning or as a basis for stackful coroutine-based RPC development. 

# Installation
The project depends on three third-party libraries, [libco](https://github.com/Tencent/libco), [llbc](https://github.com/lailongwei/llbc) and [protobuf](https://github.com/protocolbuffers/protobuf). Since using git submodule is inconvenient, the third-party library code has been directly committed to the repository source code. Users can also download it from the corresponding repository. Here are the installation steps for the project:

- Clone the repository: git clone https://github.com/lgc1112/cppcoro-protobuf-rpc.git
- Enter the repository directory: cd cppcoro-protobuf-rpc
- Compile the llbc library: ./build.sh llbc
- Compile the protobuf-3.20.0 library: ./build.sh proto
- Compile the protobuf-3.20.0 library: ./build.sh libco
- Compile the RPC code: ./build.sh

To run the tests:

- Run the server: ./rpc/bin/server
- Run the client: ./rpc/bin/client

If you need to recompile the RPC code, you can execute the './build.sh' or './build.sh rebuild' command.

Please make sure that you have installed the necessary dependencies and tools, such as a compiler, CMake, etc., before executing these commands.

# Example
## RPC method definition
According to the pb format, define req and rsp protocols, rpc service EchoService, Echo and RelayEcho two rpc methods as follows. Use Google's protoc to generate the corresponding stub for client invocation and service code for server invocation.
```
message EchoRequest {
    optional string msg = 1;
}
message EchoResponse {
    optional string msg = 2;
}

service EchoService {
    rpc Echo(EchoRequest) returns (EchoResponse);
    rpc RelayEcho(EchoRequest) returns (EchoResponse) { option (is_coro_required) = true;};
}
```
In this case, the Echo method is mainly used to send RPC requests to the server and receive direct responses; RelayEcho is mainly used to send requests to server 1, which then sends another nested RPC request to server 2, and finally, the response is sent back to the client. As shown in the diagram below.

![image](https://github.com/lgc1112/cpp20coroutine-protobuf-rpc/assets/40829436/829a3448-0335-4766-81c6-f461714175c5)

## Client RPC call example
```
  // 1. Get or create rpc channel
  RpcChannel *channel = s_ConnMgr->GetRpcChannel("127.0.0.1", 6688);

  // 2. Create rpc req, resp
  echo::EchoRequest req;
  req.set_msg("Hello, Echo.");
  echo::EchoResponse rsp;

  // 3. Create rpc controller stub
  RpcController cntl;
  echo::EchoService_Stub stub(channel);

  // 4. Call the generated rpc method Echo, and then Echo will internally suspend the coroutine and wait for the return
  stub.Echo(cntl, req, rsp, nullptr);
  
  // 5. When the rsp packet is received or a timeout or other error occurs, the coroutine is awakened, and the rpc call status can be read from the proto controller. If the call is successful, the coroutine return value rsp can be read at this time, and the coroutine return result is printed
  LOG_INFO(
       "Recv Echo Rsp, status:%s, rsp:%s",
       cntl.Failed() ? cntl.ErrorText().c_str() : "success", rsp.msg().c_str());

  // 6. Perform RelayEcho call within the coroutine and get the return...
  stub.RelayEcho(cntl, req, rsp, nullptr);
  LOG_INFO(
       "Recv RelayEcho Rsp, status:%s, rsp:%s",
       cntl.Failed() ? cntl.ErrorText().c_str() : "success", rsp.msg().c_str());
```

![image](https://github.com/lgc1112/cpp20coroutine-protobuf-rpc/assets/40829436/72d9e1bc-2f65-46a9-bdaf-2822c2d6abc0)

The detailed process and client timing diagram of the code are shown above. In the fourth step, the protobuf-generated stub.Echo method is called to complete the rpc call. The Echo function internally records the current coroutine stack position and suspends the coroutine. The coroutine will only be awakened when the rpc return packet is received or a timeout or other error occurs. After filling in the rsp and controller status, the Echo function will return.



## Server service example:
Server-side RPC service code, implementing the Echo and RelayEcho methods:
```
// Inherit and implement pb-generated echo::EchoService, override rpc methods Echo, RelayEcho
class MyEchoService : public echo::EchoService {
public:
  void Echo(::google::protobuf::RpcController *controller,
            const ::echo::EchoRequest *request, ::echo::EchoResponse *response,
            ::google::protobuf::Closure *done) override;
  void RelayEcho(::google::protobuf::RpcController *controller,
             const ::echo::EchoRequest *request, ::echo::EchoResponse *response,
             ::google::protobuf::Closure *done) override;
};

// In the Echo method, fill in the response according to the rpc request rsp, and then return, the framework will complete the return package logic internally
void MyEchoService::Echo(::google::protobuf::RpcController *controller,
                         const ::echo::EchoRequest *request,
                         ::echo::EchoResponse *response,
                         ::google::protobuf::Closure *done) {
  // Fill in the coroutine return value
  response->set_msg(std::string(" Echo >>>>>>> ") + request->msg());
  done->Run();
}

// In the RelayEcho method, which involves nested rpc calls, just continue calling
void MyEchoService::RelayEcho(::google::protobuf::RpcController *controller,
                              const ::echo::EchoRequest *req,
                              ::echo::EchoResponse *rsp,
                              ::google::protobuf::Closure *done) {
  // Initialize internal rpc req, rsp
  echo::EchoRequest innerReq;
  innerReq.set_msg("Relay Call >>>>>>" + req->msg());
  echo::EchoResponse innerRsp;
  
  // Create or get rpc channel
  RpcChannel *channel = s_ConnMgr->GetRpcChannel("127.0.0.1", 6688);

  // Create rpc controller stub
  RpcController cntl;
  echo::EchoService_Stub stub(channel);

  stub.Echo(cntl, innerReq, innerRsp, nullptr);
  LOG_INFO("Recv rsp, status:%s, rsp:%s",
       cntl.Failed() ? cntl.ErrorText().c_str() : "success",
       innerRsp.msg().c_str());
  if (cntl.Failed()) {
    controller->SetFailed(cntl.ErrorText());
  }

  rsp->set_msg(innerRsp.msg());
}
```
Server-side RPC service process, including Echo and RelayEcho methods:

![image](https://github.com/lgc1112/cpp20coroutine-protobuf-rpc/assets/40829436/9c1dcea5-5d3c-42e7-85de-a08847dd856d)

The Echo method is processed directly in the main coroutine and returns, while the RelayEcho method requires a nested Rpc call, so a new thread is created internally to complete the call.

##  Client log print results
```
Recv Echo Rsp, status:success, rsp: Echo >>>>>>> hello, Echo.
Recv RelayEcho Rsp, status:success, rsp: Echo >>>>>>> Relay Call >>>>>> hello, RelayEcho.
```
As you can see, the first Echo method received the echo return, and the second RelayEcho method received the echo return after two server relay accesses, which can achieve the basic rpc call and nested rpc call of rpc.

## Rpc implementation framework

<img width="1479" alt="企业微信截图_80776009-59f2-4f9f-806a-00e838d83358" src="https://github.com/lgc1112/cpp20coroutine-protobuf-rpc/assets/40829436/2ce0f3e4-d8e9-4f50-a5e5-961afbc76c36">


This implementation framework is based on protobuf and C++20 coroutines.

protobuf: Define req and rsp protocols, service, and corresponding rpc methods according to the pb format. Using the protoc interpreter, you can generate rpc stub classes and interfaces for clients and rpc service classes and interfaces for servers. Using protobuf can greatly reduce the amount of code and the difficulty of understanding our rpc framework (although I personally think that the performance of the rpc code generated by protobuf is not particularly high...). Those who are not familiar with protobuf-generated rpc code can first get a general understanding. Since they are all classic rpc solutions, they will not be introduced too much here. In short, after defining the rpc methods and services through pb, rpc stub classes and rpc call methods will be generated for clients. After users call the corresponding rpc methods, the internal google::protobuf::RpcChannel class's CallMethod method will be called. Developers can rewrite CallMethod to do their own packet sending logic; At the same time, rpc service classes and rpc service methods will be generated for the server. The server needs to inherit and override these methods to implement its own rpc processing logic and fill in rsp (as in Section 2.3). So when an rpc request is received, the service class is found according to the service name and method name, and its CallMethod interface is called, which will eventually call the user-overridden method and obtain rsp.

libco: libco is a very classic stackful coroutine framework with concise and clear code and decent performance. Compared to stackless coroutines, the advantage of stackful coroutines is that they can suspend coroutines at any position, unlike stackless coroutines that can only be suspended at the outermost layer. This article only provides a simple encapsulation of libco coroutines, implementing the basic functions required for Rpc (such as timeout, cancellation, suspension, awakening, indexing, etc.), which is simple and easy to understand.

Tcp communication: For the Tcp communication part, in order to avoid reinventing the wheel, this article mainly uses a third-party library llbc to handle Tcp connection, disconnection, and exception handling. On this basis, a multi-threaded Tcp communication manager is implemented to reduce the overhead of the main thread and improve performance.

# Performance Testing
In this framework, a single RPC call on the client side only involves the creation and switching of one coroutine, while the server side can either not create a coroutine or only create one. The connection management module uses a dual-thread approach to reduce the load on the main thread. Therefore, theoretically, the performance of this framework is excellent among stackful coroutine-based RPC frameworks. The main performance bottlenecks lie in the switching efficiency of individual libco coroutines and the efficiency of the code generated by protobuf.

Specific test data and results will be supplemented later. 

// TODO: Specific data to be supplemented...


