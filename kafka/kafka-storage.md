# Day 9 — Kafka Storage Deep Dive

Today's main question is:

> Kafka stores messages on disk. If disk is slow, how can Kafka still handle millions of messages per second?

The answer is mainly:

**Append-only logs + sequential writes + batching + OS Page Cache + zero-copy reads.**

---

## Table of Contents

- [1. Kafka's Storage Model](#1-first-understand-kafkas-storage-model)
- [2. What is a Kafka Log?](#2-what-is-a-kafka-log)
- [3. Kafka Does NOT Keep One Huge File](#3-kafka-does-not-keep-one-huge-file)
- [4. What is a Log Segment?](#4-what-is-a-log-segment)
- [5. .log File](#5-log-file)
- [6. .index File](#6-index-file)
- [7. .timeindex File](#7-timeindex-file)
- [8. The Three Files Together](#8-the-three-files-together)
- [9. Append-Only Architecture](#9-append-only-architecture)
- [10. Why Append-Only Is Fast](#10-why-append-only-is-fast)
- [11. Sequential Writes](#11-sequential-writes)
- [12. But Isn't Disk Slow?](#12-but-isnt-disk-slow)
- [13. Page Cache](#13-page-cache)
- [14. Why Page Cache Helps Kafka](#14-why-page-cache-helps-kafka)
- [15. Kafka Uses RAM Intelligently](#15-kafka-uses-ram-intelligently)
- [16. Producer → Disk Flow](#16-producer--disk-flow)
- [17. What Happens When a Consumer Reads?](#17-what-happens-when-a-consumer-reads)
- [18. Why Doesn't Kafka Load Everything Into RAM?](#18-why-doesnt-kafka-load-everything-into-ram)
- [19. Kafka's Read Path](#19-kafkas-read-path)
- [20. Sequential Reads](#20-sequential-reads-are-also-important)
- [21. Kafka + Batching](#21-kafka--batching)
- [22. Compression Also Helps](#22-compression-also-helps)
- [23. Zero-Copy](#23-zero-copy)
- [24. Why Kafka Storage Is So Fast](#24-why-kafka-storage-is-so-fast)
- [25. Partition Parallelism](#25-partition-parallelism)
- [26. Segment Rolling](#26-segment-rolling)
- [27. Retention](#27-retention)
- [28. Delete vs Update](#28-important-interview-distinction-delete-vs-update)
- [29. What Exactly Is Stored on Disk?](#29-what-exactly-is-stored-on-disk)
- [30. Complete Example](#30-complete-example)
- [31. The Most Important Mental Model](#31-the-most-important-mental-model)
- [32. Interview Answer](#32-interview-answer-how-can-kafka-be-fast-if-it-stores-data-on-disk)
- [33. Day 9 — What You Should Remember](#33-day-9--what-you-should-remember)

---

## 1. First Understand Kafka's Storage Model

Suppose you have:

```text
Producer
   |
   v
Kafka Broker
   |
   v
Topic: orders
   |
   +---- Partition 0
   |
   +---- Partition 1
   |
   +---- Partition 2
```

Each partition is basically an **ordered append-only log**.

For example:

```text
Partition 0

Offset
  0    Order A
  1    Order B
  2    Order C
  3    Order D
  4    Order E
```

Kafka doesn't modify old messages.

It mainly does:

```text
append new message
       ↓
append new message
       ↓
append new message
```

This is extremely important for performance.

---

## 2. What is a Kafka Log?

A Kafka log is the sequence of records belonging to a partition.

Example:

```text
Topic: orders

Partition 0

0 → Order-101
1 → Order-102
2 → Order-103
3 → Order-104
4 → Order-105
```

The log is stored on the broker's disk.

Conceptually:

```text
Kafka Broker
│
└── Kafka Data Directory
     │
     └── orders-0
          │
          ├── segment 1
          ├── segment 2
          └── segment 3
```

Here `orders-0` means:

- topic = `orders`
- partition = 0

---

## 3. Kafka Does NOT Keep One Huge File

A partition can potentially contain enormous amounts of data.

Kafka therefore divides the log into **segments**.

For example:

```text
Partition 0

┌─────────────────────┐
│ Segment 0            │
│ offsets 0 - 999     │
└─────────────────────┘

┌─────────────────────┐
│ Segment 1            │
│ offsets 1000 - 1999 │
└─────────────────────┘

┌─────────────────────┐
│ Segment 2            │
│ offsets 2000 - 2999 │
└─────────────────────┘
```

Usually, the newest segment is the **active segment**.

New messages are appended to it.

When it reaches the configured size/time limit, Kafka rolls over to a new segment.

---

## 4. What is a Log Segment?

A log segment is a chunk of a partition's log.

A segment contains several files.

The most important ones are:

```text
00000000000000000000.log
00000000000000000000.index
00000000000000000000.timeindex
```

For example:

```text
orders-0/
│
├── 00000000000000000000.log
├── 00000000000000000000.index
├── 00000000000000000000.timeindex
│
├── 00000000000000123456.log
├── 00000000000000123456.index
├── 00000000000000123456.timeindex
```

The number represents the **base offset** of that segment.

---

## 5. .log File

This is where the actual Kafka records are stored.

Example: `00000000000000000000.log`

Conceptually:

```text
Offset    Message
-------------------------
0         Order A
1         Order B
2         Order C
3         Order D
4         Order E
```

The actual file contains Kafka's binary record format, not plain text.

**Important:** `.log` contains the actual message records.

---

## 6. .index File

Now imagine a partition has 10 million messages.

A consumer asks: Give me offset 8,500,000.

Kafka doesn't want to scan:

```text
0
1
2
3
...
8,500,000
```

That would be extremely slow.

Instead, Kafka maintains an **offset index**.

Conceptually:

```text
Offset Index

Offset       Position in .log
--------------------------------
0            0
100           5000
200           10000
300           15000
...
```

So Kafka can approximately locate the desired record.

### Important detail

The index is generally **sparse**, not one entry for every message.

So Kafka:

```text
find approximate position using index
              ↓
jump to that position
              ↓
scan a small number of records
              ↓
find exact offset
```

This is very efficient.

---

## 7. .timeindex File

Kafka can also find records based on timestamps.

The `.timeindex` helps map: **timestamp → offset**.

For example:

```text
Time                  Offset
--------------------------------
10:00:00              0
10:05:00              500
10:10:00              1000
10:15:00              1500
```

Suppose a consumer/application asks: Give me records starting around 10:10.

Kafka can use the time index to find an approximate offset.

Then:

```text
timestamp
    ↓
.timeindex
    ↓
approximate offset
    ↓
.index
    ↓
position in .log
    ↓
actual records
```

---

## 8. The Three Files Together

Think of them like this:

```text
                Kafka Segment
                     │
       ┌─────────────┼─────────────┐
       │             │             │
       ▼             ▼             ▼
     .log          .index      .timeindex
       │             │             │
       │             │             │
   actual data    offset →     time →
                  position     offset
```

| File | Role |
| --- | --- |
| `.log` | Actual messages |
| `.index` | Offset → approximate physical position |
| `.timeindex` | Timestamp → approximate offset |

---

## 9. Append-Only Architecture

This is one of Kafka's biggest performance advantages.

Traditional database operations might look like: INSERT, UPDATE, DELETE.

Kafka's primary write pattern is:

```text
APPEND
APPEND
APPEND
APPEND
```

Example:

```text
Disk

| A | B | C | D | E | F | G |
                              ↑
                            append
```

New data goes to the end.

Kafka doesn't need to search for some free location in the middle.

---

## 10. Why Append-Only Is Fast

Imagine writing to a disk.

**Random writes**

```text
write → location 500
write → location 20
write → location 900
write → location 100
```

The storage system may need to jump around.

**Sequential writes**

```text
write → 1
write → 2
write → 3
write → 4
write → 5
```

Much more efficient.

Kafka tries to turn message writes into largely sequential appends.

---

## 11. Sequential Writes

Suppose producer sends: A, B, C, D, E.

Kafka can append them:

```text
.log

A B C D E
        ↑
      append
```

Instead of:

```text
A     C
   B       E
      D
```

Sequential I/O is much faster and easier for storage systems to handle efficiently.

This is one reason Kafka can get high throughput from disks.

---

## 12. But Isn't Disk Slow?

This is the important interview question.

You might say:

> Kafka writes messages to disk. Isn't disk much slower than RAM?

Yes.

But Kafka takes advantage of the operating system's **page cache**.

This is extremely important.

---

## 13. Page Cache

The operating system uses available RAM as a cache for files.

Conceptually:

```text
Application
    |
    v
Operating System
    |
    v
Page Cache (RAM)
    |
    v
Disk
```

When Kafka writes data, the data can first be handled through the OS page cache before being persisted to the physical disk.

So you shouldn't think:

```text
Producer
   ↓
Kafka
   ↓
WAIT for slow disk every time
```

It's more like:

```text
Producer
   ↓
Kafka
   ↓
OS Page Cache
   ↓
Disk
```

The OS manages the caching and eventual persistence.

---

## 14. Why Page Cache Helps Kafka

Suppose Kafka receives 10,000 messages.

The OS can keep recently written/read file pages in RAM.

So frequently accessed data may already be in memory.

Example:

```text
Kafka .log
     ↓
OS Page Cache
     ↓
RAM
```

If a consumer requests data that's already cached:

```text
Consumer
   ↓
Kafka
   ↓
Page Cache
   ↓
Data
```

There may be no physical disk read required at that moment.

---

## 15. Kafka Uses RAM Intelligently

Kafka doesn't try to store the entire topic in application-managed memory.

Instead:

```text
Kafka
   ↓
OS
   ↓
Page Cache
```

The operating system decides which file pages should remain cached.

This is useful because the OS already has sophisticated mechanisms for:

- caching
- flushing
- memory management
- disk I/O

Kafka leverages those mechanisms.

---

## 16. Producer → Disk Flow

Let's connect everything.

Suppose:

```text
Producer
   |
   | Order #101
   v
Kafka Broker
```

Kafka receives the record.

Conceptually:

```text
Producer
   ↓
Network
   ↓
Kafka Broker
   ↓
Partition
   ↓
Active Log Segment
   ↓
.log
```

The write goes to the active segment.

For example:

```text
orders-0/

00000000000001000000.log
                         ↑
                     new records
```

The OS page cache may hold the recently written pages.

Eventually the data is persisted to storage according to the durability configuration and OS/storage behavior.

---

## 17. What Happens When a Consumer Reads?

Suppose consumer wants offset 1050.

Kafka knows which segment contains it.

```text
Partition 0
│
├── segment 0
│
├── segment 1  ← offset 1050
│
└── segment 2
```

Then:

```text
offset 1050
     ↓
segment
     ↓
.index
     ↓
approximate .log position
     ↓
small scan
     ↓
record 1050
```

Very efficient.

---

## 18. Why Doesn't Kafka Load Everything Into RAM?

Imagine: Topic size = 10 TB, RAM = 64 GB.

Obviously Kafka can't keep all 10 TB in RAM.

Instead:

```text
10 TB data
    ↓
Disk

Frequently accessed data
    ↓
Page Cache
    ↓
RAM
```

The OS keeps useful portions cached.

This allows Kafka to work with datasets much larger than RAM.

---

## 19. Kafka's Read Path

A simplified view:

```text
Consumer
    |
    | fetch(offset=1050)
    v
Kafka Broker
    |
    v
Find partition
    |
    v
Find segment
    |
    v
Use .index
    |
    v
Find approximate position
    |
    v
Read .log
    |
    v
Page Cache / Disk
    |
    v
Return records
    |
    v
Consumer
```

---

## 20. Sequential Reads Are Also Important

Kafka doesn't randomly retrieve completely unrelated messages in normal sequential consumption.

A consumer often reads:

```text
100
101
102
103
104
105
...
```

So Kafka can efficiently read a contiguous range from the log.

This is another reason throughput can be high.

---

## 21. Kafka + Batching

Kafka doesn't necessarily send one tiny disk operation for every individual message.

Producers batch records.

For example: Order 1, Order 2, Order 3, Order 4, Order 5 can be grouped into a batch.

Conceptually:

```text
Producer
   ↓
Batch
   ↓
Kafka
   ↓
Append batch
```

Fewer operations + sequential appends = better throughput.

---

## 22. Compression Also Helps

Kafka can compress batches using codecs such as: `gzip`, `snappy`, `lz4`, `zstd`.

Conceptually:

```text
100 MB messages
      ↓
compression
      ↓
30 MB
      ↓
network + storage
```

This can reduce: network bandwidth, disk space, disk I/O.

There is CPU overhead for compression/decompression, but the tradeoff can be very favorable.

---

## 23. Zero-Copy

Another important Kafka performance optimization is **zero-copy**.

Normally, moving data from disk to a consumer can involve multiple copies between kernel and application memory.

Kafka can use OS facilities such as `sendfile` to efficiently transfer file data from the page cache toward the network without unnecessarily copying the entire payload through Kafka's application memory.

Conceptually:

**Traditional-ish path**

```text
Disk
 ↓
Kernel
 ↓
Kafka memory
 ↓
Kernel
 ↓
Network
```

**Zero-copy style path**

```text
Disk
 ↓
Page Cache
 ↓
Network
```

The exact path depends on the platform and Kafka implementation, but the key idea is: **avoid unnecessary data copying**.

This reduces CPU and memory overhead.

---

## 24. Why Kafka Storage Is So Fast

Now combine everything:

```text
                 Kafka Performance
                        |
       ┌────────────────┼────────────────┐
       ↓                ↓                ↓
 Append-only       Sequential I/O    Page Cache
       ↓                ↓                ↓
 No random       Efficient disk      RAM caching
 updates             access
       │                │                │
       └────────────────┼────────────────┘
                        ↓
                    High Throughput
```

And additionally: Batching, Compression, Zero-copy, Partition parallelism all contribute.

---

## 25. Partition Parallelism

This is another huge point.

Suppose:

```text
Topic
│
├── P0
├── P1
├── P2
└── P3
```

These partitions can be processed in parallel.

So Kafka doesn't depend on one giant sequential pipeline.

You can have:

```text
P0 → Broker/storage work
P1 → Broker/storage work
P2 → Broker/storage work
P3 → Broker/storage work
```

This allows Kafka to scale throughput with partitions and broker resources.

---

## 26. Segment Rolling

Suppose the active segment becomes large: `segment-0.log`.

Kafka rolls to `segment-1.log`.

Now:

```text
Partition

segment 0 → old
segment 1 → old
segment 2 → active
```

New writes go to segment 2.

Old segments can be managed according to retention policies.

---

## 27. Retention

Kafka isn't supposed to store messages forever by default.

You can configure retention based on things such as:

- `retention.ms`
- `retention.bytes`

For example:

```text
Today
 ↓
Segment A
Segment B
Segment C
Segment D ← active
```

If old data exceeds the retention policy:

```text
Segment A → deleted
```

Kafka typically deletes **whole old segments**, rather than deleting individual messages from the middle of a segment.

That's another benefit of segment-based storage.

---

## 28. Important Interview Distinction: Delete vs Update

Suppose you have: Order 101, Order 102, Order 103.

Kafka normally doesn't do: find Order 102, modify bytes in middle of `.log`.

Instead, new events are appended.

For example: `OrderCreated`, `OrderPaid`, `OrderShipped`.

Each is another record.

This append-oriented design is fundamental to Kafka's architecture.

---

## 29. What Exactly Is Stored on Disk?

A simplified partition directory:

```text
orders-0/
│
├── 00000000000000000000.log
├── 00000000000000000000.index
├── 00000000000000000000.timeindex
│
├── 00000000000000100000.log
├── 00000000000000100000.index
├── 00000000000000100000.timeindex
│
└── 00000000000000200000.log
    00000000000000200000.index
    00000000000000200000.timeindex
```

Think:

```text
Partition
   ↓
Segments
   ↓
.log + .index + .timeindex
```

---

## 30. Complete Example

Suppose your Paytm-style system produces SIP notification events:

- User 101 → SIP 5001 → ₹5,000
- User 102 → SIP 5002 → ₹2,000
- User 103 → SIP 5003 → ₹1,500

Producer sends Event 1, Event 2, Event 3.

```text
sip.pdn.debit.events
          |
          v
     Partition 0
          |
          v
    Active Segment
          |
          v
       .log file
```

The `.log` contains the actual events.

The `.index` helps Kafka find an offset.

The `.timeindex` helps with timestamp-based lookup.

The OS page cache keeps recently used file pages in RAM.

Consumer:

```text
Notification Service
        |
        | fetch offset 5000
        v
Kafka Broker
        |
        v
.index
        |
        v
.log
        |
        v
records
```

---

## 31. The Most Important Mental Model

Remember this diagram:

```text
                    KAFKA PARTITION
                          |
                          v
                 Append-only Log
                          |
             ┌────────────┴────────────┐
             ↓                         ↓
        Log Segments              Ordered Offsets
             |
     ┌───────┼────────┐
     ↓       ↓        ↓
   .log    .index  .timeindex
     |       |        |
     |       |        |
  data    offset    time
          → pos     → offset
     |
     v
 OS Page Cache
     |
     v
   Disk
```

---

## 32. Interview Answer: "How Can Kafka Be Fast If It Stores Data on Disk?"

If interviewer asks this, answer:

> Kafka uses an append-only log, so writes are mostly sequential rather than random. Sequential disk I/O is much faster than random I/O. Kafka also uses batching, compression, and the operating system's page cache, so frequently accessed data can be served from RAM without going to physical disk every time. Kafka uses sparse indexes to quickly locate offsets inside log segments, and zero-copy techniques can reduce unnecessary data copying when serving consumers. Finally, partitions allow the workload to be processed in parallel. Because of all these optimizations, Kafka can achieve very high throughput even though its durable storage is disk-based.

---

## 33. Day 9 — What You Should Remember

| Concept | Meaning |
| --- | --- |
| Kafka Log | Partition = ordered append-only log |
| Segment | Large log split into segments |
| `.log` | Actual records |
| `.index` | Offset → approximate file position |
| `.timeindex` | Timestamp → approximate offset |
| Append-only | Don't modify old data; append new data |
| Sequential I/O | Sequential writes → high throughput |

### Page Cache

```text
Disk files
   ↕
OS Page Cache
   ↕
RAM
```

### Performance formula

```text
Append-only
    +
Sequential I/O
    +
Page Cache
    +
Batching
    +
Compression
    +
Zero-copy
    +
Partition parallelism
    =
High Kafka throughput
```

### One line to memorize

> Kafka is fast not because disk is faster than memory, but because it turns message storage into sequential append operations and heavily leverages batching, OS page cache, efficient indexing, and parallelism.
