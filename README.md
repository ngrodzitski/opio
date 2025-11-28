# OPIO — Overengineered Protobuf I/O

Yet another Overengineered Protobuf IO (OPIO).

# What OPIO is (and isn’t)

OPIO is a thin transport + framing layer for Protocol Buffers over TCP
with an option to attach a custom binary to a message.
It gives you control over sockets, buffers, and back-pressure
without forcing an RPC model.

It is **not** an RPC framework (like gRPC).
You define custom protocol in `.proto` for which **OPIO** generates necessary code,
and wire it to handlers, and decide your own request/response patterns.

## Typical use cases

* Reasonably low-latency client/server services that speak a compact,
  binary protocol over TCP.
* Gateways or bridges that need fine-grained control of read/write paths
  and buffer management.
* Systems where you want Protobuf serialization but do **not** want
  an opinionated RPC stack.

## Key capabilities (user-visible)

* **Client/server roles**: start an acceptor (server) or a connector (client)
  and obtain a live connection.
* **Traits-based customization**: swap socket/executor types,
  choose strand model (single vs multi-thread), plug in your logger/stats,
  and select a buffer driver.
* **Messages-in / messages-out API**:
  send a message calling a send function of an entry object and receive
  incoming message via a callback you provide.
* **Attach custom buffer**: `proto_entry` gives an option to atach custom
  binary when sending a message (thus on receiving message may come with
  an attached buffer).
* **ASIO or Boost.Asio**: build against either, with the same higher-level API.
* **Protobuf integration**: `proto_entry` provides shared handling and
  generated glue for your `.proto` messages.

# High-level idea

Library utilizes [ASIO](https://think-async.com/Asio/)
([Boost::asio](https://www.boost.org/doc/libs/1_85_0/doc/html/boost_asio.html))
as a low-level layer which deals with OS socket APIs
to handle socket programming.

On top of it, **OPIO** provides two layers:

* `opio::net`: a protocol-agnostic part which lets you do a basic TCP communication:
  streaming output bytes to the socket (and does associated bookkeeping
  of buffers and queues) and provides input from the socket via a notification
  (e.g., by calling a callback).

* `opio::proto_entry`: The second part handles the protocol implementation:
    common protocol handling routines and generating necessary protocol-biased code.

Here is the high-level overview of opio library:

![Highlevel overview](/docs/opio_short_diagram.svg "opio highlevel high-level overview").

# Build

## Minimum supported toolchain

| Component | Minimum |
|---|---|
| **C++** | 20 |
| **GCC** | 11.0 |
| **MSVC** | 19.30 (aka “193\*”) |
| **CMake** | 3.21 |

## Linux (verified on Ubuntu)

### Prepare Python Environment

Make sure you have a python `protobuf` package with matching major version:

```bash
pip install "protobuf>=6,<7"
```

If the package is not available you can use `venv`:

```bash
python3.13 -m venv .venv
. ./.venv/bin/activate
pip install "protobuf>=6,<7" Cheetah3 legacy-cgi

# legacy-cgi is needed to comfort Cheetah3 that requires the module
# which was removed from the standard library. Likely in later versions
# Cheetah3 will adapt for Python 3.13.
```

### Build the project

```bash
# ============================================
# Standalone asio
# ============================================

# Debug
conan install -pr:a ubu-gcc-11 -s:a build_type=Debug --build missing -of _build .
( source ./_build/conanbuild.sh && cmake -B_build . -DCMAKE_TOOLCHAIN_FILE=_build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug )
cmake --build _build -j 6


# RelWithDebInfo
conan install -pr:a ubu-gcc-11 -s:a build_type=RelWithDebInfo --build missing -of _build_release .
( source ./_build_release/conanbuild.sh && cmake -B_build_release . -DCMAKE_TOOLCHAIN_FILE=_build_release/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo )
cmake --build _build_release -j 6


# ASAN:
conan install -pr:a ubu-gcc-11-asan --build missing -of _build_asan .
( source ./_build_asan/conanbuild.sh && cmake -B_build_asan . -DCMAKE_TOOLCHAIN_FILE=_build_asan/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo )
cmake --build _build_asan -j 6

# ============================================
# Boost asio
# ============================================
conan install -pr:a ubu-gcc-11 -s:a build_type=Debug --build missing -o opio/*:asio=boost -o boost/*:header_only=True -of _build_boost .
( source ./_build_boost/conanbuild.sh && cmake -B_build_boost . -DCMAKE_TOOLCHAIN_FILE=_build_boost/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DOPIO_ASIO_SOURCE=boost)
cmake --build _build_boost -j 6
```

# Implementation Details

This section describes the necessary concepts and how they map into implementation.
Both `opio::net` and `opio::proto_entry` operate with specific concepts described
in its subsection.

![opio core principles](/docs/opio_diagram.svg "opio detailed diagram").

## `opio::net`

`opio::net` library focuses on solving the following tasks:

* Maintain a queue of outgoing raw bytes and write to the socket
  following the order they were scheduled.

* Continuously read from the socket and notify the consumer via a callback.

This library provides three core abstractions to deal with communication via TCP:

* **Connector** (client-role connection factory).

* **Acceptor** (server role connection factory).

* **Connection** - a wrapper around the connected socket itself.

**Connector** and **Acceptor** are helper concepts.
Their main task is to provide an instance of a connected socket
(`asio::ip::tcp::socket`); thus, they are completely decoupled
from the *Connection*.

The core concept of the library is **Connection** which is implemented by
`opio::net::tcp::connection_t<Traits>`.
It encapsulates the low-level "socket" (which is a means of doing IO)
and provides a simple client API to send buffers and handle incoming data
(which also comes in a form of buffers). The intended profile of working with a
connection can be described as _bytes-in/bytes-out_.

It is important to understand that `opio::net::tcp::connector_t`
is designed to be used as `std::shared_ptr` and it wraps all
the necessary context for performing IO function.
**Connection**  runs its operations on `asio::io_context`.

![opio::net core principles](/docs/opio_net_diagram.svg "opio::net diagram").

To handle incoming bytes, the user provides a "consumer",
which acts as an entry point to whatever processing logic the client wants to perform.
The input data is fed to the consumer ASAP via the input handler.
To initiate output client calls a function that schedules sending of some bytes.
The outgoing data queue is handled in chunks served with a gathering write
so outgoing data can be stored as separate buffers but a write operation
would consider `N` buffers at once.

The internal implementation of `opio::net` does its best to optimize sending/receiving,
allowing various dimensions of customization.
The implementation of the **Connection** concept suggests several
customizations through its traits class:

```C++
struct connection_traits_t{
    // Type of socket to use.
    // Usually, it is just `asio::ip::tcp::socket`,
    // but for tests or benchmarks purposes, it can be some kind of a
    // mock or a stub.
    using socket_t = /*...*/;

    // An executor to use when running the internal logic of the connection.
    // This is usually:
    //     * `asio::strand< asio_ns::any_io_executor >` for cases
    //       `asio::io_context` is running on multiple threads)
    //     * `asio::any_io_executor` for cases `asio::io_context`
    //       runs on a single thread.
    using strand_t = /*...*/;


    // Logger type to use for internal logging.
    // The type has a set of requirements, like having a set of functions
    // that can be called with certain signatures.
    using logger_t = /*...*/;

    // A type that provides an implementation of statistics counter.
    // The type is obligated to have a certain API
    // in order to be used by the library.
    // See `opio/net/stats.hpp`  for more details.
    using operation_watchdog_t = /*...*/;

    // A type that provides a buffer abstraction.
    // The type is obligated to have a certain API
    // in order to be used by the library.
    // See `opio/net/buffer.hpp` and `opio/net/heterogeneous_buffer.hpp`
    // for more details.
    using buffer_driver_t = /*...*/;

    // A type that provides an implementation of statistics counter.
    // The type is obligated to have a certain API
    // in order to be used by the library.
    // See `opio/net/stats.hpp`  for more details.
    using stats_driver_t = /*...*/;

    // A function-like type that implements handling for incoming buffers.
    // The instance of the type must be invokable with
    // `opio::net::input_ctx_t< Traits >` as a parameter.
    // Simplest example is
    // `std::function< void( input_ctx_t<Traits>& ) >`.
    using input_handler_t = /*...*/;
};

```

Important properties of `opio::net`:

* It is mostly header-only, template-based library with several customization points.

* It supports both [Boost::asio](https://www.boost.org/doc/libs/1_85_0/doc/html/boost_asio.html)
  and standalone [ASIO](https://think-async.com/Asio/).

* Customizable strand (ASIO synchronization primitive):
  for single-threaded event-loop and multi-threaded event-loop.

* Control of write operation timeout calculated on configurable
  expected speed measured in Mb/sec.

* It supports standard socket options configuration.

* Customizable inner logging routine (can be adapted for different loggers frameworks).

### `opio::net` Essential Concepts

<table>
  <tr>
    <th>Concept</th>
    <th>Details</th>
  </tr>
  <tr>
    <td><b>Connection</b></td>
    <td>
      Stands for a thing that privately runs all handling around the socket
      to do IO and does bookkeeping like keeping a queue of output buffers,
      controlling write timeouts, getting timestamps, and dealing with error handling,
      at the same time, exposing a simple client interface for client
      "bytes-in/bytes-out" interface.
      <br/>
      Associated routines: <code>opio::net::tcp::connection_t&lt;Traits&gt;</code>.
      <br/>
      <code>Traits</code> class is intended to customize connection behavior.
      Also, it is designed to be used as <code>std::shared_ptr</code>
      hosted context object that runs its operations on <code>asio::io_context</code>.
    </td>
  </tr>
  <tr>
    <td><b>Buffer_Driver</b></td>
    <td>
      An umbrella concept that defines what is <b>Input Buffer</b> and <b>Output Buffer</b>
      and provides routines to manipulate them. The <b>Connection</b> includes an
      instance of the <b>Buffer_Driver</b> type as its data member.
      That allows using stateful kinds of <b>Buffer_Driver</b> implementation
      (think of implementing reusable buffer's pool mechanics).
      <br/>
      Associated routines: <code>opio::net::simple_buffer_driver_t</code>,
      <code>opio::net::heterogeneous_buffer_driver_t</code>.
    </td>
  </tr>
  <tr>
    <td><b>Buffer</b></td>
    <td>
      This is a subconcept of <b>Buffer_Driver</b>.
      It stands for a single continuous array of bytes which is identified
      by pointer and size. The true nature of the buffer is a private knowledge
      of <b>Buffer_Driver</b>, and <b>Connection</b> does
      all the necessary manipulations through <b>Buffer_Driver</b>.
    </td>
  </tr>
  <tr>
    <td><b>Strand</b></td>
    <td>
      An executor used for running <b>Connection's</b> operations
      on <code>asio::io_context</code>.
      <br/>
      Associated routines:
      <br/><code>opio::net::noop_strand_t</code>
      (an alias for <code>asio::any_io_executor</code>),
      <br/><code>opio::net::real_strand_t</code> (
        an alias for <code>asio::strand&lt;asio::any_io_executor&gt;</code>).
    </td>
  </tr>
  <tr>
    <td><b>Input_Handler</b><br/>aka <b>Consumer</b></td>
    <td>
      <b>Consumer</b> is a logic that handles the input bytes from <b>Connection</b>.
      Connection uses <b>Input_Handler</b> as an entry point to feed consumer with data.
      <br/>
      For example:
<pre lang="C++">
std::function<
    void( input_ctx_t<Traits>& )>;
</pre>
    </td>
  </tr>
  </tr>
  <tr>
    <td><b>Traits</b><br/><b>Connection_Traits</b></td>
    <td>
      A defined set of types to be used as customizations to construct an eventual
      type of <b>Connection</b>.
      <br/>
      Associated routines: <code>opio::net::default_traits_st_t</code>,
      <code>opio::net::default_traits_mt_t</code>.
    </td>
  </tr>
  <tr>
    <td><b>Connector</b></td>
    <td>
      An async primitive to create connected instance of
      <code>asio::ip::tcp::socket</code>.
      <br/>
      Associated routines: <code>opio::net::connector_t</code>.
      <br/>
      Acts as a factory.
    </td>
  </tr>
  <tr>
    <td><b>Acceptor</b></td>
    <td>
      An async primitive to listen on a given endpoint and
      feeding instances of connected sockets to a specified callback.
      <code>asio::ip::tcp::socket</code>.
      <br/>
      Associated routines: <code>opio::net::acceptor_t</code>.
    </td>
  </tr>
</table>

## `opio::proto_entry`

This sublibrary is tightly coupled with the protocol approach used in `opio`.
So first it is necessary to have an understanding of how the protocol works.

Regardless of what the specific set of messages are there in a given protocol,
we have the following:

* We start from a proto file (or a set of files);

* We have an agreement regarding which messages can travel from the
  A-side of the communication channel to the B-side and
  which messages can travel in the opposite direction,
  and which can travel both ways;


As a reference example let's take the following **sample.proto**,
that contains a set of messages:

![sample.proto](/docs/opio_sample_proto.svg "sample.proto")

To adopt the above set of messages for `opio` we need to label
messages with tags that indicate IO direction of the message.
Also each message must have a unique identificator
(`opio` uses `enum` to achieve it).


```protobuf
// Message direction with respect to server.
enum ProtoEntryIODirection {
  PROTO_ENTRY_IO_INCOMING = 0;          // Server <<< Client
  PROTO_ENTRY_IO_OUTGOING = 1;          // Server >>> Client
  PROTO_ENTRY_IO_INCOMING_OUTGOING = 2; // Server <=> Client
}

extend google.protobuf.MessageOptions {
    ProtoEntryIODirection proto_entry_io_direction = 110001;
    string                proto_entry_enum_id = 110002;
}
```

The above snippet defines new options and `ProtoEntryIODirection` enum
that defines directions.
It can be placed into "sample.proto" directly or be extracted
into separate file. `opio` expects messages subjected to be messages of the protocol
to be labeled with `proto_entry_io_direction` and `proto_entry_enum_id` options:

```protobuf
// Enum to uniquely identify messages of the protocol:
enum MessageType
{
    XXX_REQUEST = 1;
    // ...
}

//
message XxxRequest
{
    option (proto_entry_io_direction) = PROTO_ENTRY_IO_INCOMING;
    option (proto_entry_enum_id) = "XXX_REQUEST";

    // fields...
}
```

![sample.proto opio labels](/docs/opio_sample_proto_labeled.svg "sample.proto opio labeles")

Protocol specification gives `opio` a model which we it uses to generate
the necessary boilerplate to handle the mechanics of multiplexing/demultiplexing
messages.

Having such attributes doesn't affect serialization or
parsing routines generated by Protobuf and only adds some code to reflection
routines.

To create a specification in a convenient format (JSON),
`opio` uses python module generated by Protobuf.
Knowing protocol details the protocol specific code can be generated.
(you can see relevant scripting here:
[run_cheetah.py](./proto_entry/run_cheetah.py)).
Speaking about implementation
`opio::proto_entry` takes the protocol specs and generates a set
of routines necessary to compose a type, and the instance of such type.
Implementation follows "send by call & receive by invoke" approach.
To send a message to remote party user calls on of the send-functions provided by entry
and to receive the message from a remote party user provides a callback object
capable of receiving incoming (for its role: client or server) messages of the protocol.


The code `opio::proto_entry` generates focuses on solving the following tasks:

* Parse the stream of incoming bytes and create meaningful protocol messages and
  pass them to internal application logic by invoking hook-method
  of a consumer provided upon creation of the entry.

* Get messages from the application's logic – receive meaningful protocol messages
  via send API of the entry, serialize them to raw bytes, and send them to the network
  (via `opio::net`).

![bytes <=> messages](/docs/opio_bytes_messages.svg "bytes-messages-bytes transition")

### `opio::proto_entry` Essential Concepts

<table>
  <tr>
    <th>Concept</th>
    <th>Details</th>
  </tr>
  <tr>
    <td><b>Entry</b></td>
    <td>
      It means a type (or an instance of a such type) that abstracts the communication
      channel that implements a given protocol while giving a user
      a "send by call & receive by invoke" look&feel.
      <br/>
      Associated routines: <code>opio::proto_entry::entry_base_t&lt;Traits&gt;</code>
      which Contains common routines not biased to specific protocol.
      The types (class) of entries generated according to protocol spec are
      derived from it.
    </td>
  </tr>
  <tr>
    <td><b>Server_Role</b><br/><b>Client_Role</b></td>
    <td>
      This is a side of the protocol communication channel.
      And that is used when generating the specification.
      Because the meaning of <code>ProtoEntryIODirection</code> in the concrete
      <code>*.proto</code>-file are given with respect to Server.
      But for <b>Client_Role</b> the meaning should be inverted before generating code.
    </td>
  </tr>
  <tr>
    <td><b>Connection_Strand</b><br/><b>Strand</b></td>
    <td>
      <b>Connection_Strand</b> is strand used for underlying <b>Connection</b>
      (<code>opio::net</code>). While <b>Strand</b> in the context of
      <code>opio::proto_entry</code> is an executor used to run <b>Entries</b> events
      (like parsing, heartbeat handling).
      <br/>
      Reuses:  <code>opio::net::noop_strand_t</code> and
      <code>opio::net::real_strand_t</code>
    </td>
  </tr>
  <tr>
    <td><b>Consumer</b></td>
    <td>
      An entity that has a set of
      <code>on_mesage(message_carrier, client)</code> methods
      that accept all incoming messages (marked
      <code>PROTO_ENTRY_IO_INCOMING_OUTGOING</code> or
      <code>PROTO_ENTRY_IO_INCOMING</code>
      for server role entries or <code>PROTO_ENTRY_IO_OUTGOING</code>
      for client role clients).
    </td>
  </tr>
  <tr>
    <td><b>Traits</b><br/><b>Entry_Traits</b></td>
    <td>
      A defined set of types to be used as customizations to construct an eventual type
      of Entry.
      <br/>
      Standard base traits:
      <code>opio::proto_entry::common_traits_base_t</code>,
      <code>opio::proto_entry::singlethread_traits_base_t</code>,
      <code>opio::proto_entry::multithread_traits_base_t</code>
      (see <a href="proto_entry/include/opio/proto_entry/entry_base.hpp">entry_base.hpp</a>
      for more details).
      <br/>
      See also <b>Traits</b> for <code>opio::net</code>.
    </td>
  </tr>
</table>
