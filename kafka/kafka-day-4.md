# Day 4 — Kafka Producer Architecture

Think of the producer flow like this:

```text
Application
    ↓
KafkaProducer
    ↓
Serializer
    ↓
Partitioner
    ↓
RecordAccumulator
    ↓
Sender Thread
    ↓
Broker
```

I'll explain each component + why it exists + important configs + interview follow-ups.

---

## Table of Contents

- [1. Big Picture](#1-big-picture)
- [2. Application](#2-application)
- [3. KafkaProducer](#3-kafkaproducer)
- [4. Serializer](#4-serializer)
- [5. Partitioner](#5-partitioner)
- [6. RecordAccumulator](#6-recordaccumulator)
- [7. Batching](#7-batching)
- [8. batch.size](#8-batchsize)
- [9. linger.ms](#9-lingerms)
- [10. Compression](#10-compression)
- [11. Sender Thread](#11-sender-thread)
- [12. Broker](#12-broker)
- [13. acks](#13-acks)
- [14. retries](#14-retries)
- [15. max.in.flight.requests.per.connection](#15-maxinflightrequestsperconnection)
- [16. delivery.timeout.ms](#16-deliverytimeoutms)
- [17. buffer.memory](#17-buffermemory)
- [18. Complete Flow](#18-complete-flow)
- [19. SIP Notification Example](#19-example-using-your-sip-notification-system)
- [20. Most Important Configs](#20-the-most-important-configs--interview-table)
- [21. Interview Follow-ups](#21-very-important-interview-follow-ups)
- [22. One-Line Interview Explanation](#22-one-line-interview-explanation)

---

## 1. Big Picture

The producer is the client-side path from `producer.send()` to the partition leader on a broker. Each step exists for a reason: convert objects to bytes, pick a partition, batch for throughput, then wait for acknowledgement.

---

## 2. Application

Your application wants to publish an event.

For example, in your Paytm Money SIP notification flow:

```java
ProducerRecord<String, SipEvent> record =
    new ProducerRecord<>(
        "sip.pdn.debit.events",
        userId,
        sipEvent
    );

producer.send(record);
```

The application gives Kafka:

- Topic
- Key
- Value
- Optional partition
- Optional timestamp
- Optional headers

Conceptually:

```text
Topic = sip.pdn.debit.events
Key   = userId
Value = SipEvent
```

### Why use a key?

The key is generally used by the partitioner to decide which partition should receive the message.

For example:

```text
user123 → Partition 2
user456 → Partition 5
user789 → Partition 1
```

The important benefit is that messages with the **same key** can consistently go to the **same partition**, which helps preserve ordering for that key.

---

## 3. KafkaProducer

The application normally does **not** directly communicate with the broker for every `send()` call.

Instead, `producer.send(record)` goes to the KafkaProducer client.

The producer is responsible for:

- serialization
- partition selection
- buffering
- batching
- compression
- sending requests
- retries
- handling acknowledgements

So you can think of **KafkaProducer** as the main client-side controller.

---

## 4. Serializer

Kafka brokers store **bytes**.

But your application works with objects: `SipEvent event`.

Kafka needs to convert this into bytes. That's the job of the serializer.

```text
Java Object
     ↓
Serializer
     ↓
Bytes
```

For example:

```text
SipEvent
   ↓
JSON / Avro / Protobuf
   ↓
byte[]
```

Kafka commonly has:

- `key.serializer`
- `value.serializer`

Example:

```text
key.serializer=StringSerializer
value.serializer=JsonSerializer
```

So:

```text
user123
   ↓
StringSerializer
   ↓
bytes

SipEvent
   ↓
JsonSerializer
   ↓
bytes
```

### Interview question

**Q: Does Kafka broker understand Java objects?**

No.

The producer serializes the key and value into bytes **before** sending them to Kafka.

---

## 5. Partitioner

After serialization, Kafka needs to decide: **which partition should receive this record?**

That's the job of the partitioner.

Suppose:

```text
Topic: sip.pdn.debit.events

Partition 0
Partition 1
Partition 2
Partition 3
```

If the record has `key = user123`, the partitioner determines a partition.

Conceptually:

```text
hash(user123) % number_of_partitions
```

So: `user123 → Partition 2`

### Why is this important?

Suppose we have:

```text
user123 → event1
user123 → event2
user123 → event3
```

If they all go to the same partition:

```text
Partition 2:

event1
event2
event3
```

Kafka can preserve their order **within that partition**.

**Important interview point:** Kafka guarantees ordering **within a partition**, not across the entire topic.

---

## 6. RecordAccumulator

This is one of the most important parts of the producer.

After partition selection, Kafka does **not** necessarily send the message immediately.

Instead, it puts records into an in-memory buffer called the **RecordAccumulator**.

Think:

```text
Application
     ↓
Partitioner
     ↓
RecordAccumulator
     ↓
[Partition 0 buffer]
[Partition 1 buffer]
[Partition 2 buffer]
```

**Why?** Because Kafka wants to **batch** multiple messages together.

Instead of:

```text
Message 1 → Broker
Message 2 → Broker
Message 3 → Broker
Message 4 → Broker
```

it can do:

```text
Message 1
Message 2
Message 3
Message 4
      ↓
One batch
      ↓
Broker
```

This reduces network calls, CPU overhead, and request overhead — and improves throughput.

---

## 7. Batching

Suppose `batch.size=16KB`.

Kafka tries to build batches up to approximately that size for a partition.

For example:

```text
Partition 2 buffer

Event 1 → 4 KB
Event 2 → 3 KB
Event 3 → 5 KB
Event 4 → 4 KB

Total = 16 KB
```

Now the batch is ready to send.

```text
RecordAccumulator
       ↓
16 KB batch
       ↓
Sender Thread
```

### But what if traffic is low?

Suppose only `Event 1 = 2 KB` arrives.

Kafka doesn't necessarily want to wait forever for the batch to become 16 KB.

That's where `linger.ms` comes in.

---

## 8. batch.size

`batch.size` controls the **target maximum size** of a batch per partition.

Example: `batch.size=16384` means approximately **16 KB**.

The producer tries to accumulate records into batches of this size.

### Larger batch

**Advantages:** better throughput, fewer requests, better compression efficiency.

**Disadvantages:** potentially more memory, potentially more waiting.

### Important clarification

`batch.size` does **not** mean Kafka waits until the batch reaches that size.

If the batch doesn't fill, `linger.ms` determines how long the producer is willing to wait before sending it.

---

## 9. linger.ms

`linger.ms` means: how long should the producer wait for additional records so that it can create a better batch?

Example:

```text
batch.size=16KB
linger.ms=5
```

Suppose:

```text
t=0 ms → Event 1 arrives
t=1 ms → Event 2 arrives
t=2 ms → Event 3 arrives
t=3 ms → Event 4 arrives
```

If the batch isn't full, producer can wait up to the configured linger period before sending.

Conceptually:

```text
Wait briefly
   ↓
Collect more records
   ↓
Create larger batch
   ↓
Send
```

### Tradeoff

```text
linger.ms ↑
    ↓
more batching
    ↓
higher throughput
    ↓
slightly higher latency
```

Whereas:

```text
linger.ms ↓
    ↓
less waiting
    ↓
lower latency
    ↓
potentially more requests
```

---

## 10. Compression

Before sending the batch to the broker, Kafka can compress it.

For example: `compression.type=lz4`

Flow:

```text
Events
  ↓
Batch
  ↓
Compression
  ↓
Network
  ↓
Broker
```

Common compression types include: `none`, `gzip`, `snappy`, `lz4`, `zstd`.

### Why compress?

Suppose the original batch is 1 MB. After compression: 300 KB. Then less data travels over the network.

**Benefits:** lower network bandwidth, potentially better throughput, often better storage efficiency.

But compression requires **CPU**.

---

## 11. Sender Thread

This is another important component.

The **Sender thread** takes ready batches from the RecordAccumulator and sends them to Kafka brokers.

Flow:

```text
RecordAccumulator
       ↓
Ready batch
       ↓
Sender Thread
       ↓
Network request
       ↓
Broker
```

Suppose:

```text
Partition 0 → Broker 1
Partition 1 → Broker 2
Partition 2 → Broker 1
```

The Sender knows which broker is the **leader** for each partition and sends the appropriate produce requests.

---

## 12. Broker

Finally, the request reaches the Kafka broker.

For a partition (`Partition 2`), one broker acts as the leader.

The producer sends the record to that partition's leader.

For example:

```text
Producer
   |
   | ProduceRequest
   ↓
Broker 1
   |
   ↓
Partition 2 Leader
```

The broker writes the records to its log.

If replication is configured:

```text
Leader
  ↓
Follower
  ↓
Follower
```

The replication and acknowledgement behavior then determine when the producer considers the send successful.

---

## 13. acks

This is one of the most important producer configurations.

It determines: **how much acknowledgement does the producer require from Kafka?**

### acks=0

Producer doesn't wait for acknowledgement.

```text
Producer → Broker
           X
       no waiting
```

Fastest, but weakest durability guarantee.

If something goes wrong, the producer may not know.

### acks=1

Producer waits for the **leader** to acknowledge the write.

```text
Producer
   ↓
Leader
   ↓
ACK
```

The leader has accepted the record, but replicas may not have replicated it yet.

So it gives better durability than `acks=0`, but less than `acks=all`.

### acks=all

Producer waits for the required **in-sync replicas** to acknowledge.

```text
Producer
   ↓
Leader
   ↓
Followers replicate
   ↓
ACK
```

This provides the strongest acknowledgement guarantee among these settings.

For an important financial event flow, `acks=all` is commonly a sensible choice when durability matters.

---

## 14. retries

What happens if a request fails temporarily?

Example:

```text
Producer
   ↓
Broker
   X
Network error
```

Producer can retry. `retries=...` means the producer can retry failed sends.

Typical temporary failures: network issue, broker temporarily unavailable, leader election, transient timeout.

Flow:

```text
Send
 ↓
Failure
 ↓
Retry
 ↓
Retry
 ↓
Success
```

**Important:** Retries can create **duplicate records** in some failure scenarios unless producer idempotence is used/configured appropriately.

---

## 15. max.in.flight.requests.per.connection

This controls how many **unacknowledged requests** a producer can have in flight on one broker connection.

Suppose `max.in.flight.requests.per.connection = 5`.

The producer can have multiple requests outstanding:

```text
Request 1 → waiting
Request 2 → waiting
Request 3 → waiting
Request 4 → waiting
Request 5 → waiting
```

This improves throughput because the producer doesn't have to wait for every request before sending another.

### Why can it affect ordering?

Imagine:

```text
Request 1
Request 2
```

Request 1 fails and gets retried, while Request 2 succeeds.

Depending on configuration and retries, Request 2 could potentially be observed **before** the retried Request 1.

That's why this setting matters when discussing: ordering, retries, idempotence.

Modern Kafka producer idempotence helps address these concerns.

---

## 16. delivery.timeout.ms

This is basically the **overall time limit** for successfully delivering a record.

Think:

```text
send()
 ↓
buffering
 ↓
batching
 ↓
network
 ↓
retry
 ↓
retry
 ↓
ACK
```

The producer cannot keep retrying forever.

`delivery.timeout.ms` puts an upper bound on how long the record can remain in the delivery process before it fails.

It works together with things like: `request.timeout.ms`, `retries`, `linger.ms`.

### Simple interview explanation

> `delivery.timeout.ms` is the overall deadline for a record to be successfully delivered. If the producer cannot successfully deliver it within that period, the send fails.

---

## 17. buffer.memory

This controls the amount of memory available to the producer for buffering records before they are sent.

Example: `buffer.memory=33554432` — approximately **32 MB**.

Conceptually:

```text
Producer Memory

+-----------------------+
| Partition 0 batches   |
| Partition 1 batches   |
| Partition 2 batches   |
| Partition 3 batches   |
| ...                   |
+-----------------------+
```

If the producer is generating records faster than Kafka can accept them, this buffer can fill.

Then `send()` can block/wait up to the relevant limit before failing.

---

## 18. Complete Flow

Now combine everything:

```text
                  APPLICATION
                       |
                       | producer.send()
                       ↓
                +--------------+
                | KafkaProducer |
                +--------------+
                       |
                       ↓
                 SERIALIZER
                       |
                       ↓
             key/value → bytes
                       |
                       ↓
                 PARTITIONER
                       |
                       ↓
             choose partition
                       |
                       ↓
             RECORD ACCUMULATOR
                       |
              +--------+--------+
              |        |        |
           P0 batch  P1 batch  P2 batch
              |        |        |
              +--------+--------+
                       |
                batch.size /
                linger.ms
                       |
                       ↓
                 COMPRESSION
                       |
                       ↓
                 SENDER THREAD
                       |
                       ↓
              ProduceRequest
                       |
                       ↓
               KAFKA BROKER
                       |
                       ↓
               PARTITION LEADER
                       |
                 replication
                       |
                       ↓
                     ACK
                       |
                       ↓
                  PRODUCER
```

---

## 19. Example Using Your SIP Notification System

Suppose your application receives:

- User = 101
- SIP = SIP123
- Amount = ₹5,000
- Event = PDN_SUCCESS

Application creates `ProducerRecord<String, SipEvent>`.

Then:

### Step 1 — Serializer

`key = "101"`, `value = SipEvent` becomes bytes.

### Step 2 — Partitioner

`hash("101") → Partition 4`

### Step 3 — RecordAccumulator

The record is placed into Partition 4 batch.

### Step 4 — Batching

More SIP events arrive: User 101, User 205, User 786, User 901, ...

Producer accumulates them into a batch.

### Step 5 — Compression

The batch is compressed.

### Step 6 — Sender Thread

Sender creates a produce request for the broker that is leader of Partition 4.

### Step 7 — Broker

Broker writes the batch.

### Step 8 — Replication

With `acks=all`, the producer waits for the required ISR acknowledgement.

### Step 9 — Success

Producer receives ACK.

```text
Application
    ↓
KafkaProducer
    ↓
Serializer
    ↓
Partitioner
    ↓
Accumulator
    ↓
Batch
    ↓
Sender
    ↓
Broker
    ↓
ACK
```

---

## 20. The Most Important Configs — Interview Table

| Config | Simple meaning | Main tradeoff |
| --- | --- | --- |
| `acks` | How much broker acknowledgement producer waits for | Durability vs latency |
| `linger.ms` | How long producer waits to build a batch | Latency vs throughput |
| `batch.size` | Target batch size per partition | Memory vs batching |
| `buffer.memory` | Total producer buffering memory | Memory vs backpressure |
| `compression.type` | Compress producer batches | Network vs CPU |
| `retries` | Retry failed sends | Reliability vs possible duplicates |
| `max.in.flight...` | Unacknowledged requests per connection | Throughput vs ordering considerations |
| `delivery.timeout.ms` | Overall delivery deadline | Reliability vs waiting time |

---

## 21. Very Important Interview Follow-ups

### Q1. Why doesn't Kafka send every message immediately?

Because batching improves network and CPU efficiency.

```text
100 messages
→ 100 network requests
```

is generally less efficient than:

```text
100 messages
→ several batches
→ several requests
```

### Q2. batch.size vs linger.ms?

`batch.size` = how large the batch can become.

`linger.ms` = how long producer waits for more records to fill the batch.

Easy way to remember:

```text
batch.size → SIZE
linger.ms  → TIME
```

### Q3. Where does batching happen?

On the producer side, primarily through the RecordAccumulator.

### Q4. Is there one batch for the entire topic?

No.

Producer batching is organized around **partitions**.

A topic with P0, P1, P2, P3 can have **separate batches** for those partitions.

### Q5. Who chooses the partition?

The partitioner, based on the record key and partitioning strategy.

### Q6. Does KafkaProducer create a network request for every send()?

Not necessarily.

`send()` generally adds the record to the producer's buffering/batching process. The Sender thread later sends batches.

### Q7. What happens if the producer is faster than Kafka?

The producer's buffers can fill.

Conceptually:

```text
Application rate
      >
Broker processing rate
      ↓
Producer buffer fills
      ↓
Backpressure / blocking
      ↓
Potential timeout/failure
```

This is where `buffer.memory`, batching, broker capacity, and throughput tuning become important.

### Q8. Where does compression happen?

On the producer, before the compressed batch is sent to the broker.

### Q9. What happens if the broker doesn't respond?

Depending on the failure and configuration:

```text
request fails
    ↓
retry
    ↓
retry
    ↓
success
```

or eventually:

```text
delivery.timeout.ms exceeded
    ↓
send fails
```

### Q10. If I use acks=all, is the message guaranteed to never be lost?

Don't say "100% guaranteed."

A better interview answer:

> `acks=all` provides a strong durability guarantee because the leader waits for the required in-sync replicas. But overall durability also depends on replication configuration, ISR health, broker configuration, and producer settings.

---

## 22. One-Line Interview Explanation

If the interviewer asks:

> Explain Kafka producer architecture.

You can say:

> When my application calls `producer.send()`, the KafkaProducer first serializes the key and value into bytes. The partitioner decides the target partition. The record is then placed into the RecordAccumulator, where records for the same partition are buffered and batched based on `batch.size` and `linger.ms`. The producer can compress these batches, and a background Sender thread sends the batches to the appropriate partition leader. The broker writes the records and sends an acknowledgement based on the `acks` configuration. If a transient failure occurs, the producer can retry within the configured delivery timeout.

That's the core Day 4 producer architecture you should be able to explain confidently.
