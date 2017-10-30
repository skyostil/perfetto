ProtoRPC: A simple and lightweight UNIX RPC
-------------------------------------------

ProtoRPC is a simple and lightweight RPC mechanism implemented using protobuf
over UNIX socket. Think to gRPC, but without a HTTP2 stack and with a simpler
API surface. It reuses the same .proto definitions and workflow of gRPC.

Design goals:
 - RPC between two UNIX processes running on the same system.
 - API simplicity.
 - Ability to transfer file descriptors, for shared memory support.
 - Minimal binary and memory footprint.
 - Single threaded server.
 - [Server streaming](https://grpc.io/docs/guides/concepts.html)
 - C++ and Java (TODO) support.

Non goals:
 - RPC over the network.
 - Flow control.
 - Receiving RPCs on a thread pool.
 - [Client streaming](https://grpc.io/docs/guides/concepts.html)


## Quick start

1) Define RPC interfaces using protobuf syntax in the same way of gRPC:
```
$ cat src/test/greeter_service.proto

syntax = "proto3";

service Greeter {
  rpc SayHello(GreeterRequestMsg) returns (GreeterReplyMsg) {}
  rpc WaveGoodbye(GreeterRequestMsg) returns (GreeterReplyMsg) {}
}

message GreeterRequestMsg {
  string name = 1;
}

message GreeterReplyMsg {
  string message = 1;
}
```

2) Generate proto messages and RPC stubs using `protoc` and passing the protorpc
plugin:
```
protoc
  --proto_path ../../protorpc/src/test
  --cpp_out gen/protorpc/src/test
  --plugin=protorpc_cpp_plugin  # TODO not implemented yet
  ../../protorpc/src/test/greeter_service.proto
```

This will generate the usual `greeter_service.pb.(cc,h)` files containing the
usual message definitions for `GreeterRequestMsg` and `GreeterReplyMsg` and
also [src/test/greeter_service-gen.h](src/test/greeter_service-gen.h).

At this point in the main():

### Host code:
```
#include "greeter_service.pb.h"
...
std::shared_ptr<GreeterImpl> svc(new GreeterImpl());
std::shared_ptr<Host> host(Host::CreateInstance(kSocketName, &task_runner));
host->ExposeService(svc);
host->Start();
```

### Client code:
```
#include "greeter_service.pb.h"
...
std::shared_ptr<Client> client(
    Client::CreateInstance(kSocketName, &task_runner));
std::shared_ptr<GreeterProxy> svc_proxy(new GreeterProxy());
client->BindService(svc_proxy);

// Prepare the request:
GreeterRequestMsg req;
req.set_name("Jack");
Deferred<GreeterReplyMsg> reply;

// Setup the reply callback:
reply.Bind([](Deferred<GreeterReplyMsg> r) {
  printf("SayHello() replied -> %s!",
         r.success() ? r->message().c_str() : "FAIL");
});

// Send the request:
svc_proxy->SayHello(req, std::move(reply));
```

See [src/test/protorpc_test.cc](src/test/protorpc_test.cc) for a complete
example.
