Directory layout
----------------

`include/`
Is the public API that clients of this library are allowed to depend on.
Headers inside include/ cannot depend on anything else.

`src/`
Is the actual implementation that clients can link but not expected to access
at a source-code level.


**Both have the following sub-structure**:

`{include,src}/core/`
"Core" is the pure c++11 tracing machinery that deals with bookkeeping,
ring-buffering, partitioning and multiplexing but knows nothing about
platform-specific things like implementation of shared memory and RPC mechanism.

`include/transport/`
Defines the interfaces that the transport layer has to implement in order to
provide a concrete RPC mechanism to both the core business logic and the clients
of libtracing.

`{include,src}/unix_transport/`
A concrete implementation of the transport layer based on unix domain sockets
and posix shared memory.
