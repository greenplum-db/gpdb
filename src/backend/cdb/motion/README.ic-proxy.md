# IC Proxy

ic-proxy is a new interconnect mode, it requires only one network connection
between every two segments, so it consumes less total amount of connections and
ports than the ic-tcp mode, and has better performance than ic-udp mode in
high-latency network.

## Installation

ic-proxy is developed with libuv, the minimal supported libuv version is
1.18.0, you could install it with rpm, apt, or build from the source code.

ic-proxy is disabled by default, we could enable it with `configure
--enable-ic-proxy`.

After the installation we also need to setup the ic-proxy network, it is done
by setting the GUC `gp_interconnect_proxy_addresses`, for example if a cluster
has one master, one standby master, one primary segment and one mirror segment,
we could set it like below:

    gpconfig --skipvalidation -c gp_interconnect_proxy_addresses -v "'1:-1:localhost:2000,2:0:localhost:2002,3:0:localhost:2003,4:-1:localhost:2001'"

It contains information for all the master, standby, primary and mirror
segments, the syntax is as below:

    dbid:segid:hostname:port[,dbid:segid:ip:port]

It is important to specify the value as a single-quoted string, otherwise it
will be parsed as an interger with invalid format.

An example script to setup this GUC automatically:

    #!/bin/sh
    
    : ${delta:=-5000}
    
    PGOPTIONS="-c gp_interconnect_type=udpifc" \
    psql -tqA -d postgres -P pager=off -F: -R, -c "
        SELECT dbid, content, address, port+$delta
          FROM gp_segment_configuration
         ORDER BY 1" \
    | xargs -rI'{}' \
        gpconfig --skipvalidation -c gp_interconnect_proxy_addresses -v "'{}'"

Reload the setting with `gpstop -u` for it to take effect.

Now we are able to run queries in the ic-proxy mode by setting the GUC
`gp_interconnect_type=proxy`, we could set it cluster level or session level,
for example:

    # launch a session in ic-proxy mode
    PGOPTIONS="-c gp_interconnect_type=proxy" psql

    # enable ic-proxy by default
    gpconfig -c gp_interconnect_type -v proxy
    gpstop -u

## Design

### Logical Connection

In ic-tcp mode, there are TCP connections between QEs (including QD).  Take an
gather motion for example:

    ┌    ┐
    │    │  <=====  [ QE1 ]
    │ QD │
    │    │  <=====  [ QE2 ]
    └    ┘

In ic-udp mode, there are no TCP connections, but there are still logical
connections: if two QEs communicate to each other, there is a logical
connection:

    ┌    ┐
    │    │  <-----  [ QE1 ]
    │ QD │
    │    │  <-----  [ QE2 ]
    └    ┘

In ic-proxy mode, we also use the concept of logical connections:

    ┌    ┐          ┌       ┐
    │    │          │       │  <====>  [ proxy ]  <~~~~>  [ QE1 ]
    │ QD │  <~~~~>  │ proxy │
    │    │          │       │  <====>  [ proxy ]  <~~~~>  [ QE2 ]
    └    ┘          └       ┘

- in a N:1 gather motion there are N logical connections;
- in a N:N redistribute / broadcast motion there are `N*N` logical connections;

### Logical Connection Direction

In ic-tcp and ic-udp modes every connection has a direction, in the above
gather motion example, the QD has two incoming connections, each QE has one
outgoing connection.  Besides the control messages, like the STOP in ic-tcp
mode and ACK in ic-udp mode, DATA are always transfered from the sender to the
receiver.

In ic-proxy mode we do not distinguish the direction.  In theory DATA could be
transfered bidirectionally, but the backends do not do this currently.

### Logical Connection Identifier

To identify a logical connection we need to know who is the sender and who is
the receiver.  In ic-proxy we do not distinguish the direction of a logical
connection, we use the names local and remote as the end points.  An end point
is at least identified by the content id (the segindex) and the pid, so a
logical connection could be identified by:

    seg1,p1->seg2,p2

However this is not enough to distinguish the logical connections of different
subplans in a query.  We must also put the sender & receiver slice indices into
consideration:

    slice[a->b] seg1,p1->seg2,p2

However consider that the backend processes can be used in different queries of
the session, and the lifecycle of them are not strictly in-sync, we must also
put the command id into the identifier:

    cmd1,slice[a->b] seg1,p1->seg2,p2

For debugging purpose we also put the session id in the identifier, however in
practice it should never clash.

When putting mirrors or standby into consideration, we must realize that a
connection to seg1's primary is different with a connection to seg1's mirror,
so we also need to put dbid into the identifier:

    cmd1,slice[a->b] seg1,dbid3,p1->seg2,dbid5,p2

### Load balancer
TODO: a heavy-load logical connection should not consume all the bandwidth, and
should not cause high-latency in other light-load ones, not implemented yet.

### Flow control

When a proxy receives a DATA from the remote side, it routes it to the
corresponding backend.  If the backend is not receiving, the DATA needs to stay
in a write queue.  If the sender sends the DATA faster than the receiver, the
write queue could become too large and even cause OOM.

A flow control is needed due to this.  In ic-tcp mode if the receiver is not
receiving the sender can not send the DATA out; in ic-proxy mode flow control
is done by dropping packets.  In ic-proxy mode we could not stop receiving
because all the logical connections from the remote segment share the same
proxy-proxy connection; we could not drop packets, because currently the
proxy-proxy connection is tcp based, when we see a packet we already received
it, no chance to drop.

In ic-proxy mode we do flow control like this:
- when sender sends a packet, it increases the value of unackSendPkt. When the
number of unackSendPkt exceeds the pause threshold, the sender pause itself.
- when receiver receives a packet, it will duplicate the packet and wait for
the backend to consume the packet. After backend comsumed the packet, the
receiver increases the value of unackRecvPkt. When the number of unackRecvPkt
exceed the theshold: IC_PROXY_PACKET_INTERVAL_OF_ACK_MESSAGE, the receiver will
send a ACK message to the sender.
- when the sender receives the ACK message, it decreases the value of
unackSendPkt. When unackSendPkt is below the resume threshold, the sender can
continue to read data from the backend.

Note that we could not pause at any time.  In ic-proxy mode the backend sends
DATA as ic-tcp packets, it will retry infinitely until a complete packet is
sent, or being canceled.  So the sender proxy must only stop receiving from the
backend on ic-tcp packet boundaries.  Similarly, the backend expects to receive
only complete ic-tcp packets, it will also retry on incomplete ones.  If we do
not follow this policy there would be deadlocks or infinite waiting.

## Proxy-Backend Communication

The backends (QD/QE) communicate with the proxy via IPC, domain socket is used
in current implementation.

### Packet

To transfer data between proxies, a header must be added to form a packet.  In
ic-proxy we use the `icpkthdr`, the one used by ic-udp.  The header is 64
bytes.  TODO: as some fields are not actually used in ic-proxy mode, we might
remove them in the future.

Although it's possible to convert data of any length to a packet, we follow the
same size limit of ic-udp, `gp_max_packet_size`, which is 8192 bytes by
default.

### Outgoing buffer

On the other hand, to reduce the overhead of the packet header, the outgoing
data in a logical connection is cached in an outgoing buffer, it's only sent in
two conditions: 1) cached enough data to build a full-size packet, or 2) no
more data to sent (the BYE message).

TODO: maybe we could trigger the flush also with a timeout.

### Incoming buffer

As the data are transfered as packets, the proxy will have to save the incoming
data in a incoming buffer, when a complete packet is received it's routed to
the target backend.

TODO: in fact as long as the header is completely received we could begin
routing the data to the target backend, this could save some memory copying, as
well as reduce the latency.

### Control message

Besides the data there are also control messages:
- HELLO [backend -> proxy]: the first packet sent from a backend, contains the
  logical connection key;
- HELLO ACK [backend <- proxy]: once the proxy received the HELLO message it
  sends back the HELLO ACK;
- BYE [backend -> proxy -> proxy -> backend]: when a backend disconnects it
  sends BYE to the proxy, the proxy will route it to the target backend.  It is
  similar to the EOS in ic-tcp and ic-udp, however EOS will not trigger BYE,
  maybe we should do this;

TODO: there is also proxy-proxy HELLO message; we'd better separate the p2p
messages with the b2p messages; introduce the PAUSE/RESUME;

A control message is usually a header-only packet.

TODO: at the moment data and control messages are handled separately, we could
consider merge them.

### Local destination

Sometimes the source backend and target backend are on the same segment, in
such a case the data is routed back directly.

                          ┌       ┐
    [ backend1 ]  <~~~~>  │       │
                          │ proxy │
    [ backend2 ]  <~~~~>  │       │
                          └       ┘

### Domain Socket

By using domain socket as the IPC, the communication is like this:

    backend  <~~~~>  proxy  <====>  proxy  <~~~~> backend

When there are multiple logical connections, multiple domain socket connections
need to be established between the backend and its proxy.  For example, for a
2:1 gather motion, the logical connections are like this:

    ┌    ┐
    │    │  <---->  [ QE1 ]
    │ QD │
    │    │  <---->  [ QE2 ]
    └    ┘

Then the physical connections are like this:

    ┌    ┐          ┌       ┐
    │    │  <~~~~>  │       │  <====>  [ proxy ]  <~~~~>  [ QE1 ]
    │ QD │          │ proxy │
    │    │  <~~~~>  │       │  <====>  [ proxy ]  <~~~~>  [ QE2 ]
    └    ┘          └       ┘

Note that the proxies are all on different segments.

We have to establish two domain socket connections between QD and proxy, this
is to reuse the ic-tcp sending & receving logic.

#### Establish the connection

A registering process is needed to let the proxy knows who the backend is, this
is similar to ic-tcp, but we call it HELLO in ic-proxy.

    ┌───────────────────────┐                    ┌───────────────────────┐
    │        backend        │                    │         proxy         │
    ╞═══════════════════════╡                    ╞═══════════════════════╡
    │ creates a socket      │                    │                       │
    │                       │ >>                 │                       │
    │                       │ >> connects        │                       │
    │                       │ >>                 │                       │
    │                       │                    │ accepts it as a       │
    │                       │                    │ unidentified client   │
    │                       │ >>                 │                       │
    │                       │ >> sends HELLO     │                       │
    │                       │ >>                 │                       │
    │                       │                    │ receives the HELLO,   │
    │                       │                    │ extracts the key,     │
    │                       │                    │ binds the client      │
    │                       │                 << │                       │
    │                       │ sends HELLO ACK << │                       │
    │                       │                 << │                       │
    │ receives HELLO ACK    │                    │ ready to receive data │
    └───────────────────────┘                    └───────────────────────┘

Now the connection is established.

#### Shutdown the connection

When a backend shuts down the connection to the proxy, the shutdown process is
triggered, its proxy will inform the remote proxy to shuts down the connection
to the remote backend, too.  When a backend shuts down on both incoming and
outgoing directions, the logical connection to the proxy can be closed.

    ┌─────────┐          ┌─────────┐       ┌─────────┐          ┌─────────┐
    │ backend │          │  proxy  │       │  proxy  │          │ backend │
    └────┬────┘          └────┬────┘       └────┬────┘          └────┬────┘
         │                    │                 │                    │
         ├───── shutdown ────>│                 │                    │
         │                    │                 │                    │
         │                    ╞══════ BYE ═════>│                    │
         │                    │                 │                    │
         │                    │                 ├───── shutdown ────>│
         │                    │                 │                    │
         │                    │                 │<──── shutdown ─────┤
         │                    │                 ├────────/  /────────┤
         │                    │                 │                    │
         │                    │<═════ BYE ══════╡                    │
         │                    │                 │                    │
         │<──── shutdown ─────┤                 │                    │
         ├────────/  /────────┤                 │                    │
         │                    │                 │                    │
         ┴                    ┴                 ┴                    ┴

A special case is that both backends are on the same segment, the BYE message
is not needed:

    ┌─────────┐          ┌─────────┐          ┌─────────┐
    │ backend │          │  proxy  │          │ backend │
    └────┬────┘          └────┬────┘          └────┬────┘
         │                    │                    │
         ├───── shutdown ────>│                    │
         │                    │                    │
         │                    ├───── shutdown ────>│
         │                    │                    │
         │                    │<──── shutdown ─────┤
         │                    ├────────/  /────────┤
         │                    │                    │
         │<──── shutdown ─────┤                    │
         ├────────/  /────────┤                    │
         │                    │                    │
         ┴                    ┴                    ┴

#### spike: use single domain socket between QD and proxy

It is possible to use only one single connection between QD and proxy:

    ┌    ┐          ┌       ┐
    │    │          │       │  <====>  [ proxy ]  <~~~~>  [ QE1 ]
    │ QD │  <~~~~>  │ proxy │
    │    │          │       │  <====>  [ proxy ]  <~~~~>  [ QE2 ]
    └    ┘          └       ┘

This requires several changes:
- moving the outgoing buffer to QD: a buffer for outgoing data is always needed
  to reduce the overhead of the packet header, now the QD must handle it by
  itself; further more, the data sent to QE1 and QE2 can be interlaced, so a
  separate outgoing buffer is needed for every destination;
- adding a incoming buffer in QD: the proxy won't send data to a backend unless
  a complete packet is received, it has incoming buffers to do this, then why a
  backend needs a incoming buffer, too?  Because the data might not arrive in
  the reading order.  When waiting for data from QE1 the QE2's data might
  arrive earlier, these packets must be by cached QD itself;
- adding a packet selector in QD: when a packet is received, QD needs to
  identify the source of it manually from the packet header;

So it might be better to stick with the current implementation, by establishing
a domain socket connection for each logical connection we let the proxy handle
all of these.

#### spike: reuse domain socket connections

In current implmentation the domain socket connections are established in
`SetupInterconnect()` and closed in `TeardownInterconnect()`.  For OLTP queries
it adds a significant overhead doing the connecting and registering logic.  In
fact we are able to cache these connections, we only need to re-register on
next query.

### spike: Shared Memory Message Queue

The domain socket approach is easy to implement, and can reuse most of the
ic-tcp logic, however data has to be transfered 3 times to reach the target,
this increases the latency significantly.  To improve this we could use a
shared memory message queue.

To use the shared memory based IPC we still need to handle below problems, like
in any other implementations:

1. the mapping between the logical connections and the source/destination
   backends;
2. packet queueing in every logical connection;
3. block reading when incoming queue is empty;
4. block sending when outgoing queue is full;

So a general design is like this:

For incoming data:

                                          ┌     ┐         ┌       ┐
    [ backend ]  <~~~~  [ queue ]  <~~~~  │     │         │       │
                                          │ map │  <~~~~  │ proxy │
    [ backend ]  <~~~~  [ queue ]  <~~~~  │     │         │       │
                                          └     ┘         └       ┘

For outgoing data, we could use a similar structure, but in different
directions:

                                          ┌     ┐         ┌       ┐
    [ backend ]  ~~~~>  [ queue ]  ~~~~>  │     │         │       │
                                          │ map │  ~~~~>  │ proxy │
    [ backend ]  ~~~~>  [ queue ]  ~~~~>  │     │         │       │
                                          └     ┘         └       ┘

An easy implementation is to use a shmem hashtab as the map, and use a shmem
ring buffer as the queue.

The problem for shared memory is about waiting.  The proxy or the backend have
to wait for multiple shared memory queues to become readable or writable, then
if one of the queues becomes available, how could we tell which one is it
efficiently?  We need to check all the queues one by one.  The good part for
sockets is that epoll()/poll()/select() can be used to wait for multiple
objects, but there is not such thing for locks.

### spike: proxy pool

The global view of the proxy network of one segment is like this:

    ╔═════════════════════════╗
    ║                         ║          ╔═════════════════════════╗
    ║                         ║  <====>  ║           seg           ║
    ║                         ║          ╚═════════════════════════╝
    ║                         ║
    ║                         ║          ╔═════════════════════════╗
    ║                         ║  <====>  ║           seg           ║
    ║                         ║          ╚═════════════════════════╝
    ║           seg           ║
    ║                         ║          ╔═════════════════════════╗
    ║                         ║  <====>  ║           seg           ║
    ║                         ║          ╚═════════════════════════╝
    ║                         ║
    ║                         ║          ╔═════════════════════════╗
    ║                         ║  <====>  ║           seg           ║
    ║                         ║          ╚═════════════════════════╝
    ╚═════════════════════════╝

If we look inside it, take a gather motion for example, it's like this:

    ╔═════════════════════════╗
    ║ ┌    ┐        ┌       ┐ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        │       │ ║          ╚═════════════════════════╝
    ║ │    │        │       │ ║
    ║ │    │        │       │ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        │       │ ║          ╚═════════════════════════╝
    ║ │ QD │        │ proxy │ ║
    ║ │    │        │       │ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        │       │ ║          ╚═════════════════════════╝
    ║ │    │        │       │ ║
    ║ │    │        │       │ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ └    ┘        └       ┘ ║          ╚═════════════════════════╝
    ╚═════════════════════════╝

Each segment only has one proxy process, each process can use at most 100% of
one cpu core, then can the proxy process become the performance bottleneck?

The answer is yes.  When running the select-only test with `pgbench -S` on a
8x32 cluster (8 hosts, 32 segments per host) on GCP, the master proxy bgworker
process will reach 100% cpu usage at concurrency=15, the overall cpu usage of
all the QD processes are 25%, which indicates the master proxy process is the
bottleneck.

To leverage more cpu cores we could consider using a proxy pool, like this:

    ╔═════════════════════════╗
    ║ ┌    ┐        ┌       ┐ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        │       │ ║          ╚═════════════════════════╝
    ║ │    │        │ proxy │ ║
    ║ │    │        │       │ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        └       ┘ ║          ╚═════════════════════════╝
    ║ │ QD │                  ║
    ║ │    │        ┌       ┐ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        │       │ ║          ╚═════════════════════════╝
    ║ │    │        │ proxy │ ║
    ║ │    │        │       │ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │       │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ └    ┘        └       ┘ ║          ╚═════════════════════════╝
    ╚═════════════════════════╝

In this example, the pool size is 2, the `proxy[0]` only connects to
`seg[0~1]`, the `proxy[1]` only connects to `seg[2~3]`.  In general if the pool
size is K, then there are K proxy bgworkers:
- the `proxy[0]` connects to `seg[0 ~ K-1]`;
- the `proxy[i]` connects to `seg[i*K ~ (i+1)K-1]`.

When the remote content id of a logical connection is X, the backend needs to
connect to the `proxy[X / K]`, and sends the data through the `peer[X % K]` of
it.

With above two charts we could also see that, the flow count is unrelative to
the pool size K, it's because that there is always only 1 connection tween 2
segments.

The port count is changed, there is still at most `N-1` incoming connections,
but it needs to listen on K ports.  So in worst case, where `K=N-1`, the total
needed port count is doubled, but that should still be acceptable, and in
theory we do not need K to be large as that, in our example, let `K=4` is
enough to resolve the bottleneck.

Specifically, when `K=1` it's exactly the same with our original design, when
`K=N-1`, where N is the count of segments (including the master), a proxy
bgworker only connects to one remote segment:

    ╔═════════════════════════╗
    ║ ┌    ┐        ┌       ┐ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │ proxy │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        └       ┘ ║          ╚═════════════════════════╝
    ║ │    │                  ║
    ║ │    │        ┌       ┐ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │ proxy │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        └       ┘ ║          ╚═════════════════════════╝
    ║ │ QD │                  ║
    ║ │    │        ┌       ┐ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │ proxy │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ │    │        └       ┘ ║          ╚═════════════════════════╝
    ║ │    │                  ║
    ║ │    │        ┌       ┐ ║          ╔═════════════════════════╗
    ║ │    │ <~~~~> │ proxy │ ║  <====>  ║ [ proxy ] <~~~~> [ QE ] ║
    ║ └    ┘        └       ┘ ║          ╚═════════════════════════╝
    ╚═════════════════════════╝

When `K=N-1`, there is an extra benefit.  We do not need a proxy-proxy listener
in this case, when establishing a proxy-proxy connection between segment X and
Y, we could do it in a similar way as WalRep:

1. segment X launchs a proxy process X;
2. X connects to the postmaster of segment Y;
3. the postmaster Y accepts the connection and forks the proxy process Y;

So we do not need to consume the listener ports.

However launching so many proxy bgworkers might not be a good idea.

## Proxy-Proxy Communication

In a Greenplum cluster there is one master segment and N primary segments,
there is a connection between every two of them, all these connections make the
proxy network.  In current implementation the proxy-proxy communication is via
TCP, but it is possible to switch to something like QUIC or udpifc.

### Setup

To setup the proxy network, every proxy process connects to all the other
segments on startup, obviously this will result in two connections between
segment I and segment J: `I ==> J` and `J ==> I`.

It is not really a problem to have two connections between two segments, but we
could solve it easily: segment I only connects to segment J if `I < J`.  It is
simply enough and is what we used initially, but is it good enough?

For any solution it must be able to handle several cases:

0. establish the connections between proxies;
1. if the a proxy is relaunched, due to crash or critical errors, the
   connections between it and all the other segments must be re-established;
2. if the corresponding standby, or mirror, is launched, the standby proxy must
   be able to establish the connections to all the other segments, too,
   furthermore, the connections with the previous master proxy must be replaced
   with the standby ones;

So we only need to improve our simple solution slightly: a timer is put in the
proxy bgworker, it is triggered every second.  On every trigger the proxy
attempts to connect to every other proxies as long as `I < J`, no matter the
remote is a primary or mirror; an existing connection will not be reconnected
unless the previous connection is dropped, which usually means the remote is
down or crashed.

The solution can handle mirror promotion and recover from proxy crash
automatically with a low enough latency, the only bad part is that it needs to
retry to mirror connections again and again.

### Standby and Mirror

The ic-proxy bgworker processes are launched on all the master and primary
segments, as long as a ic-proxy bgworker is launched it listens to its port,
and connects to other proxies.  The master and standby proxies will listen on
different ports, so they can co-exist even if they are on the same host; the
same to primary and mirror proxies.

By doing so, the ic-proxy does not need to care about the standby activation
and mirror promotion:

- if a standby or mirror is activated, its proxy bgworker is launched
  automatically, then the proxy-proxy connections are established
  accordiningly;
- if a primary segment is marked as down it will stop its proxy bgworker
  automatically;
- a motion slice contains information about which dbid it communicates to, the
  primary or the mirror, so the packets will be routed to the primary or mirror
  proxies accordiningly;

In summary, a proxy bgworker's life cycle is controled by the postmaster during
the standby activation and mirror promotion, the bgworker itself can establish
the proxy-proxy connections unconditionally.

### Cluster Expansion

A cluster can be expanded online with the `gpexpand` command, it includes two
stages:

1. the cluster expansion (adding new segments to the cluster);
2. the data expansion (expand the data to the new segments);

When ic-proxy is enabled we need a stage 1.5 between them:

1.5. update the GUC `gp_interconnect_proxy_addresses` to include the new
     segments, then reload with SIGHUP by `gpstop -u`;

TODO: we may want to introduce a hook in `gpexpand` to automate this job.
 
## Misc

### Packet types

#### Chunk

```C
struct TupleChunkHeader
{
    uint16      size;
    uint16      type;       /* TupleChunkType */
};
```

- This struct is not defined, only described in `"cdb/tupchunk.h"`;
- Chunks are always aligned to 4 bytes;
- The `size` does not contain the header itself;
- The header and the body are always in native byte order, not in network byte
  order;
- There are some helpers like `GetChunkDataSize()`, they are implemented via
  `memcpy()` to support the non-aligned case, but the performance is bad;

Chunks can be very small, so are usually transferred in batches, as ic-tcp or
ic-udp packets.

#### ic-tcp packet

```C
struct TcpPacketHeader
{
    uint32      length;
};
```

- The struct is not defined, only described in `"cdb/ml_ipc.h"`;
- The `length` is the total size, including the header itself;
- The max packet size is `Gp_max_packet_size`;
- A ic-tcp packet only contains complete chunks;

### ic-udp packet

```C
typedef struct icpkthdr
{
	int32		motNodeId;

	/*
	 * three pairs which seem useful for identifying packets.
	 *
	 * MPP-4194:
	 * It turns out that these can cause collisions; but the
	 * high bit (1<<31) of the dstListener port is now used
	 * for disambiguation with mirrors.
	 */
	int32		srcPid;
	int32		srcListenerPort;

	int32		dstPid;
	int32		dstListenerPort;

    int32       sessionId;
    uint32      icId;

    int32       recvSliceIndex;
    int32       sendSliceIndex;
	int32       srcContentId;
	int32		dstContentId;

	/* MPP-6042: add CRC field */
	uint32		crc;

	/* packet specific info */
	int32		flags;
	int32		len;

    uint32      seq;
    uint32      extraSeq;
} icpkthdr;
```

- Defined in `"cdb/cdbinterconnect.h"`;
- The header is 64 bytes;
- The `len` is the total size, including the header itself;
- The max packet size is `Gp_max_packet_size`;
- A udp-tcp packet only contains complete chunks;

### ic-proxy packet

```C
struct ICProxyPkt
{
	uint16		len;
	uint16		type;

	int16		srcContentId;
	uint16		srcDbid;
	int32		srcPid;

	int16		dstContentId;
	uint16		dstDbid;
	int32		dstPid;

	int32		sessionId;
	uint32		commandId;
	int16		sendSliceIndex;
	int16		recvSliceIndex;
};
```

The ic-proxy packet is similar to the ic-udp one, the essential different is
that the ic-proxy packet also stores the content ids in the packet.  The field
widths are also tailered so few overhead is added, the ic-proxy packet header
is only 32 bytes.

 vi: et ai :
