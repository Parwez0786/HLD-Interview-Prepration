# Day 8 — Kafka Broker Internals

Today's goal is to understand what actually happens inside a Kafka broker when a producer sends a message.

For interviews, don't just say "producer sends message to broker." You should be able to explain the complete internal flow:

```text
Producer
   │
   │  ProduceRequest
   ▼
Kafka Broker
   │
   ├── Network Thread
   │
   ├── Request Queue
   │
   ├── I/O Thread
   │
   ├── Replica Manager
   │
   ├── Log Manager
   │
   ├── Page Cache
   │
   └── Disk
        │
        ▼
   Partition Log
```

---

## Table of Contents

- [1. What is a Kafka Broker?](#1-what-is-a-kafka-broker)
- [2. Topic vs Partition vs Replica](#2-topic-vs-partition-vs-replica)
- [3. Broker Architecture](#3-broker-architecture)
- [4. Network Threads](#4-network-threads)
- [5. Request Queue](#5-request-queue)
- [6. I/O Threads](#6-io-threads)
- [7. Replica Manager](#7-replica-manager)
- [8. Log Manager](#8-log-manager)
- [9. Append-Only Logs](#9-kafka-uses-append-only-logs)
- [10. Page Cache](#10-page-cache)
- [11. Disk Storage](#11-disk-storage)
- [12. Producer → Broker Flow](#12-most-important-producer--broker-flow)
- [13. Step 2 — Network Connection](#13-step-2--network-connection)
- [14. Step 3 — Network Thread Receives Request](#14-step-3--network-thread-receives-request)
- [15. Step 4 — I/O Thread Processes Request](#15-step-4--io-thread-processes-request)
- [16. Step 5 — Check Partition Leader](#16-step-5--check-partition-leader)
- [17. Step 6 — Replica Manager](#17-step-6--replica-manager)
- [18. Step 7 — Append to Log](#18-step-7--append-to-log)
- [19. Step 8 — Page Cache](#19-step-8--page-cache)
- [20. Step 9 — Replication](#20-step-9--replication)
- [21. ISR — In-Sync Replicas](#21-isr--in-sync-replicas)
- [22. What Does acks=all Mean?](#22-what-does-acksall-mean)
- [23. What Happens with acks=1?](#23-what-happens-with-acks1)
- [24. What Happens with acks=0?](#24-what-happens-with-acks0)
- [25. Where Does the Controller Come In?](#25-where-does-the-controller-come-in)
- [26. Modern Kafka Controller](#26-modern-kafka-controller)
- [27. Complete Producer Flow](#27-complete-producer-flow)
- [28. Paytm Money SIP Event](#28-example-paytm-money-sip-event)
- [29. Broker Components Table](#29-important-broker-components--interview-table)
- [30. Network Thread vs I/O Thread](#30-very-important-interview-distinction)
- [31. Broker vs Controller](#31-broker-vs-controller)
- [32. What If Broker Crashes?](#32-what-if-broker-crashes)
- [33. What Happens If Disk Is Slow?](#33-what-happens-if-disk-is-slow)
- [34. Broker Performance Bottlenecks](#34-broker-performance-bottlenecks)
- [35. Day 8 Must-Know Questions](#35-day-8-must-know-questions)

---

## 1. What is a Kafka Broker?

A Kafka broker is a Kafka server responsible for:

- accepting producer requests
- storing messages
- serving consumers
- replicating partitions
- managing partition replicas
- handling client requests
- maintaining logs on disk

For example:

```text
Kafka Cluster

        ┌──────────────┐
        │   Broker 1   │
        │ P0 Leader    │
        │ P1 Replica   │
        └──────────────┘

        ┌──────────────┐
        │   Broker 2   │
        │ P1 Leader    │
        │ P2 Replica   │
        └──────────────┘

        ┌──────────────┐
        │   Broker 3   │
        │ P2 Leader    │
        │ P0 Replica   │
        └──────────────┘
```

A broker doesn't necessarily own an entire topic.

It owns **partition replicas**.

---

## 2. Topic vs Partition vs Replica

Suppose we have:

- Topic: `orders`
- Partitions: P0, P1, P2
- Replication factor = 3

Kafka could distribute them like:

```text
Broker 1
   P0 Leader
   P1 Replica
   P2 Replica

Broker 2
   P0 Replica
   P1 Leader
   P2 Replica

Broker 3
   P0 Replica
   P1 Replica
   P2 Leader
```

So every broker may contain multiple partition replicas.

---

## 3. Broker Architecture

At a high level:

```text
                  Kafka Broker
                      │
          ┌───────────┴───────────┐
          │                       │
    Network Layer            Request Processing
          │                       │
   Network Threads           I/O Threads
                                  │
                           ┌──────┴──────┐
                           │             │
                    Replica Manager  Log Manager
                           │             │
                           │          Page Cache
                           │             │
                           └────────── Disk
```

There are several important components.

---

## 4. Network Threads

Kafka has network threads that handle network communication.

Think of them as **receptionists**.

Producer sends:

```text
Producer
   │
   │ TCP
   ▼
Broker
   │
Network Thread
```

The network thread:

- receives the request
- reads bytes from the socket
- converts the request into Kafka's request representation
- puts the request into a request queue

It should **not** perform heavy business/storage processing itself.

---

## 5. Request Queue

After the network thread receives the request:

```text
Producer
   │
   ▼
Network Thread
   │
   ▼
Request Queue
   │
   ▼
I/O Thread
```

The request queue acts as a buffer between:

```text
Network processing
        ↓
Request processing
```

This separation prevents network threads from getting stuck doing expensive disk or replication work.

---

## 6. I/O Threads

Kafka has request-handler/I/O threads that take requests from the queue.

For example:

```text
Request Queue

ProduceRequest
FetchRequest
ProduceRequest
MetadataRequest
FetchRequest
     │
     ▼
I/O Threads
```

These threads process requests.

For a producer request, they eventually interact with components such as:

```text
Replica Manager
       ↓
Partition
       ↓
Log
```

---

## 7. Replica Manager

The Replica Manager manages partition replicas on the broker.

It is responsible for operations related to:

- replicas
- partition leaders
- fetching replicas
- appending records
- replica synchronization
- ISR-related operations

For a producer request, an important question is:

> Is this broker the leader of the partition?

For example:

```text
orders

P0 Leader → Broker 1
P0 Replica → Broker 2
P0 Replica → Broker 3
```

Producer sends a message for P0 to Broker 1:

```text
Producer
   ↓
Broker 1
   ↓
Replica Manager
   ↓
P0 Leader
```

Good.

But if the producer sends the request to Broker 2:

```text
Producer
   ↓
Broker 2
   ↓
P0 is only a replica here
```

Broker 2 cannot normally accept the produce operation as the leader.

The client will generally be redirected to the current leader using metadata information.

---

## 8. Log Manager

This is one of the most important concepts.

Kafka stores messages in logs.

For each partition, Kafka maintains a separate **append-only log**.

Example:

```text
Topic: orders
Partition: P0

orders-0

Offset
  0     Order A
  1     Order B
  2     Order C
  3     Order D
```

On disk, Kafka doesn't store every message as an individual file.

It uses **log segments**.

```text
orders-0/

00000000000000000000.log
00000000000000000000.index
00000000000000000000.timeindex

00000000000000123456.log
00000000000000123456.index
00000000000000123456.timeindex
```

The Log Manager manages these partition logs and segments.

---

## 9. Kafka Uses Append-Only Logs

Kafka's fundamental storage model is:

```text
append
append
append
append
```

Instead of randomly modifying old records.

Example:

```text
Before:

P0
│
├── Offset 0
├── Offset 1
└── Offset 2


New message arrives

        ↓

P0
│
├── Offset 0
├── Offset 1
├── Offset 2
└── Offset 3   ← append
```

This is one of the reasons Kafka can achieve high throughput.

---

## 10. Page Cache

This is an important interview topic.

Kafka relies heavily on the **OS page cache**.

Conceptually:

```text
Kafka
  ↓
Operating System
  ↓
Page Cache
  ↓
Disk
```

Kafka writes data to the filesystem, while the operating system may keep recently written/read data in memory through the page cache.

This means Kafka can benefit significantly from RAM without implementing its own large in-memory cache for all records.

---

## 11. Disk Storage

Kafka persists partition logs to disk.

Example:

```text
/var/lib/kafka/data/

orders-0/
orders-1/
payments-0/
payments-1/
notifications-0/
```

Each partition is an independent log.

Kafka's storage model is therefore roughly:

```text
Topic
  ↓
Partition
  ↓
Partition Log
  ↓
Log Segments
  ↓
Disk
```

---

## 12. Most Important: Producer → Broker Flow

Now let's understand the complete flow.

Suppose:

```text
Producer

Order ID = 123
Amount = ₹500
```

Producer sends:

```text
Order Event
    ↓
Kafka Topic: orders
    ↓
Partition: P0
```

Assume: P0 Leader = Broker 1.

### Step 1 — Producer sends request

Producer creates a `ProducerRecord`.

```text
ProducerRecord
    |
    ├── topic = orders
    ├── key = orderId
    └── value = order data
```

The producer determines the partition.

For example:

```text
hash(orderId) % number_of_partitions
```

If the result is P0: `orders → P0`

---

## 13. Step 2 — Network Connection

Producer establishes/reuses a TCP connection to the broker.

```text
Producer
    │
    │ TCP
    ▼
Broker 1
```

The request is a Kafka `ProduceRequest`.

---

## 14. Step 3 — Network Thread Receives Request

Broker's network thread receives the request.

```text
             Broker 1

Producer
   │
   ▼
Network Thread
   │
   ▼
Request Queue
```

The network thread is mainly dealing with network communication rather than doing the complete storage operation.

---

## 15. Step 4 — I/O Thread Processes Request

An I/O/request-handler thread takes the request.

```text
Request Queue
      │
      ▼
I/O Thread
      │
      ▼
ProduceRequest
```

Now Kafka needs to process the request.

---

## 16. Step 5 — Check Partition Leader

Kafka determines:

```text
Topic = orders
Partition = P0
```

Then checks: Is Broker 1 the leader of P0?

Suppose P0 Leader = Broker 1. Then processing continues.

---

## 17. Step 6 — Replica Manager

The request goes through the replica-management layer.

Conceptually:

```text
ProduceRequest
      ↓
Replica Manager
      ↓
P0 Leader
```

Kafka prepares the records for appending to the partition log.

---

## 18. Step 7 — Append to Log

The message is appended to the partition log.

```text
P0

Offset 0 → Order A
Offset 1 → Order B
Offset 2 → Order C
Offset 3 → Order 123  ← new
```

Kafka assigns an offset as part of the append process.

The record is written into the appropriate log segment.

---

## 19. Step 8 — Page Cache

The write goes through the filesystem and can be cached by the OS page cache.

Conceptually:

```text
Kafka
  ↓
Filesystem
  ↓
Page Cache
  ↓
Disk
```

This is an important distinction:

Kafka does **not** require every producer message to be synchronously written to physical disk before acknowledging it.

What happens depends on Kafka's replication/acknowledgment semantics.

---

## 20. Step 9 — Replication

Suppose Replication Factor = 3.

```text
Broker 1 → P0 Leader
Broker 2 → P0 Replica
Broker 3 → P0 Replica
```

Leader handles the produce request.

Followers replicate the leader's data.

```text
             P0 Leader
             Broker 1
                 │
          ┌──────┴──────┐
          ▼             ▼
     Broker 2       Broker 3
      Replica        Replica
```

Followers fetch records from the leader.

---

## 21. ISR — In-Sync Replicas

Kafka maintains an ISR (In-Sync Replica) set.

Example:

```text
P0

Leader:
Broker 1

ISR:
Broker 1
Broker 2
Broker 3
```

If Broker 3 falls behind significantly:

```text
ISR:

Broker 1
Broker 2
```

Broker 3 may be removed from the ISR until it catches up.

---

## 22. What Does acks=all Mean?

Suppose Replication Factor = 3, `acks=all`.

Producer sends Order 123.

Leader receives it.

The leader waits according to Kafka's replication/acknowledgment rules for the record to be sufficiently replicated to the current ISR.

Then:

```text
Broker 1 Leader
     │
     ├── Broker 2
     │
     └── Broker 3
     
        ↓

Producer receives success
```

This gives stronger durability than simply acknowledging after the leader accepts the record.

**Interview point:**

> `acks=all` does not literally mean "wait for every broker in the cluster." It is based on the leader's ISR and the relevant replication settings.

---

## 23. What Happens with acks=1?

With `acks=1`, the leader acknowledges once it has accepted the record into its local log.

Conceptually:

```text
Producer
   ↓
Leader
   ↓
Append
   ↓
ACK
```

Followers may still replicate afterward.

So `acks=1` generally provides **lower latency** but **less replication acknowledgment protection** than `acks=all`.

---

## 24. What Happens with acks=0?

With `acks=0`, producer doesn't wait for a broker acknowledgment.

```text
Producer
   │
   │ send
   ▼
Broker
```

Producer essentially says: "I've sent the request; I don't need Kafka to acknowledge it."

This gives potentially lower latency but weaker delivery guarantees.

---

## 25. Where Does the Controller Come In?

This is another common interview question.

The controller is responsible for cluster-management tasks such as:

- partition leadership
- leader elections
- broker membership/state
- metadata-related coordination

Don't think: Controller handles every producer message.

It doesn't.

Normal producer traffic goes through the appropriate broker/partition leader.

For example:

```text
Producer
   ↓
Broker 1
   ↓
P0 Leader
```

The controller is mainly involved in cluster coordination and metadata/leadership management, **not** processing every record.

---

## 26. Modern Kafka Controller

In modern Kafka deployments using **KRaft**, Kafka no longer depends on ZooKeeper for cluster metadata management.

You can think of the architecture as:

```text
Kafka Cluster

┌───────────────────────────────┐
│        Controller Quorum       │
│                               │
│  Controller 1                 │
│  Controller 2                 │
│  Controller 3                 │
└───────────────┬───────────────┘
                │
                │ metadata
                ▼
┌───────────────────────────────┐
│          Brokers              │
│                               │
│ Broker 1                      │
│ Broker 2                      │
│ Broker 3                      │
└───────────────────────────────┘
```

For interviews, know both terms:

- Older Kafka → ZooKeeper + Controller
- Modern Kafka → KRaft controller quorum

---

## 27. Complete Producer Flow

This is the diagram I recommend memorizing.

```text
                 Producer
                    │
                    │ ProduceRequest
                    ▼
            ┌───────────────┐
            │ Network Thread│
            └───────┬───────┘
                    │
                    ▼
             Request Queue
                    │
                    ▼
            ┌───────────────┐
            │   I/O Thread  │
            └───────┬───────┘
                    │
                    ▼
             Replica Manager
                    │
                    ▼
               P0 Leader
                    │
                    ▼
               Log Manager
                    │
                    ▼
             Partition Log
                    │
                    ▼
              Page Cache
                    │
                    ▼
                  Disk

                    │
                    │ Replication
                    ▼

          ┌────────────────────┐
          │ Follower Replicas  │
          ├────────────────────┤
          │ Broker 2           │
          │ Broker 3           │
          └────────────────────┘

                    │
                    ▼
                 ACK
                    │
                    ▼
                Producer
```

---

## 28. Example: Paytm Money SIP Event

This is how you can relate broker internals to your Paytm Money experience.

Suppose your application produces SIP PDN Event to `sip.pdn.debit.events`.

Assume: 12 partitions, RF = 3.

```text
Notification Producer
       │
       │ SIP event
       ▼
Kafka Broker
       │
       ▼
Partition Leader
       │
       ▼
Partition Log
       │
       ├─────────────┐
       ▼             ▼
Follower 1       Follower 2
```

Then consumers read the event:

```text
Kafka
  ↓
Notification Consumer
  ↓
Send notification
```

If interviewer asks:

> What happens inside Kafka when your Spring Boot service publishes an event?

You can say:

> The producer sends a ProduceRequest to the broker that is leader for the target partition. A network thread receives the request and puts it into the request queue. An I/O thread processes it and the replica manager handles the partition. The record is appended to the partition's log, with the data going through the filesystem and OS page cache. The leader replicates the record to follower replicas. Depending on the producer's acks configuration, Kafka sends an acknowledgment back to the producer. Kafka stores the partition as an append-only log divided into segments.

That's a very strong SDE-1 Kafka answer.

---

## 29. Important Broker Components — Interview Table

| Component | Simple meaning |
| --- | --- |
| Broker | Kafka server |
| Network thread | Receives/sends network requests |
| Request queue | Holds incoming requests |
| I/O/request handler | Processes requests |
| Replica Manager | Manages partition replicas and replication operations |
| Log Manager | Manages partition logs/segments |
| Partition | Ordered stream of records |
| Log segment | Physical chunk of a partition log |
| Page cache | OS memory cache for filesystem data |
| Disk | Persistent storage |
| Controller | Manages cluster metadata/leadership |
| ISR | Replicas currently considered in sync |

---

## 30. Very Important Interview Distinction

### Network thread vs I/O thread

**Network thread:** "I received the request."

**I/O/request handler thread:** "I'll process the request."

Think:

```text
Network Thread
     ↓
"Receive"

Request Queue
     ↓
"Wait"

I/O Thread
     ↓
"Process"
```

---

## 31. Broker vs Controller

Don't confuse them.

**Broker** handles: Produce, Fetch, Store, Replicate, Serve consumers.

**Controller** handles: Partition leadership, Broker state, Cluster metadata, Leader elections.

So:

```text
Producer traffic
      ↓
Broker

while:

Cluster management
      ↓
Controller
```

---

## 32. What If Broker Crashes?

Suppose:

```text
P0

Broker 1 → Leader
Broker 2 → Replica
Broker 3 → Replica
```

Broker 1 crashes.

Controller detects the failure and chooses another eligible replica as leader.

```text
Before:

B1 → Leader
B2 → Replica
B3 → Replica


After:

B1 → DOWN
B2 → Leader
B3 → Replica
```

Producer metadata is updated and future requests go to Broker 2.

This is one of Kafka's key availability mechanisms.

---

## 33. What Happens If Disk Is Slow?

Potentially:

```text
Producer
   ↓
Broker
   ↓
Request Queue
   ↓
I/O
   ↓
Disk
```

If processing/storage can't keep up:

```text
Request Queue
   ↑
   │
Requests accumulating
```

This can contribute to increased producer latency and eventually backpressure/timeouts.

This is why Kafka performance isn't just about network bandwidth; disk throughput, page cache, replication, CPU and request handling all matter.

---

## 34. Broker Performance Bottlenecks

In an interview, mention:

```text
CPU
 │
 ├── request processing
 ├── compression
 └── networking

Memory
 │
 └── OS page cache

Disk
 │
 ├── sequential writes
 ├── reads
 └── replication

Network
 │
 ├── producer traffic
 ├── consumer traffic
 └── replica traffic
```

Kafka performance is therefore a combination of:

**CPU + memory/page cache + disk + network.**

---

## 35. Day 8 Must-Know Questions

Before moving to Day 9, you should be able to answer these without notes.

### Basic

- What is a Kafka broker?
- What is a partition?
- What is a partition replica?
- What does the broker store?
- What is a log segment?

### Internals

- What happens when a producer sends a message?
- What does a network thread do?
- What does an I/O/request-handler thread do?
- What is the request queue?
- What does Replica Manager do?
- What does Log Manager do?
- Why does Kafka use append-only logs?
- What is the OS page cache?

### Replication

- What is a leader replica?
- What is a follower replica?
- What is ISR?
- What happens when a leader broker crashes?
- How does `acks=all` work?
- Difference between `acks=1` and `acks=all`.

### Architecture

- What does the Kafka controller do?
- What is KRaft?
- Broker vs controller?
- Why doesn't the controller process every producer request?

### The one flow to memorize

If you remember only one thing from Day 8, remember:

```text
Producer
   ↓
ProduceRequest
   ↓
Network Thread
   ↓
Request Queue
   ↓
I/O Thread
   ↓
Replica Manager
   ↓
Partition Leader
   ↓
Log Manager
   ↓
Partition Log
   ↓
OS Page Cache
   ↓
Disk

       +
       │
       ▼
Follower Replicas
       │
       ▼
   Replication
       │
       ▼
      ACK
       │
       ▼
   Producer
```

Next logical step after Day 8: Kafka storage internals + log segments + offsets + indexes + retention + log compaction, because those concepts explain how Kafka can store and retrieve millions/billions of records efficiently.
