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
platform-specific stuff like implementation of shared memory and IPC mechanism.
Instead those are delegated to the embedder, for instance via the
`ServiceImpl::Delegate` interface in `src/core/service_impl.h`

`{include,src}/unix_rpc/`
Defines an implemetation of the core tracing machinery using unix domain sockets
and posix shared memory.
