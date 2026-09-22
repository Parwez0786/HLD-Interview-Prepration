# Day 21 — Kafka Performance

Today’s goal is to understand what makes Kafka fast, what limits Kafka performance, and how to tune it in an interview.

Think about Kafka performance using this flow:

```text
Producer
   ↓
Network
   ↓
Kafka Broker
   ↓
Disk
   ↓
Network
   ↓
Consumer
```

Performance can be measured mainly using:

- **Throughput** → how much data Kafka processes
- **Latency** → how long one message takes
- **Concurrency** → how many producers/consumers work in parallel

---

## Table of Contents

- [1. Throughput](#1-throughput)
- [2. Latency](#2-latency)
- [3. Batch Size](#3-batch-size)
- [4. linger.ms](#4-lingerms)
- [5. Compression](#5-compression)
- [6. Number of Partitions](#6-number-of-partitions)
- [7. Producer Concurrency](#7-producer-concurrency)
- [8. Consumer Concurrency](#8-consumer-concurrency)
- [9. Disk I/O](#9-disk-io)
- [10. Page Cache](#10-page-cache)
- [11. Network I/O](#11-network-io)
- [12. How Everything Connects](#12-how-everything-connects)
- [13. Important Performance Trade-offs](#13-important-performance-trade-offs)
- [14. Real Interview Scenario](#14-real-interview-scenario)
- [15. The Most Important Formula](#15-the-most-important-formula)
- [16. Your Paytm SIP Example](#16-your-paytm-sip-example)
- [17. Quick Interview Cheat Sheet](#17-quick-interview-cheat-sheet)
- [Day 21 Interview Questions](#day-21-interview-questions)

---

## 1. Throughput

Throughput = amount of data/messages processed per unit time.

Example: Kafka processes 1,000,000 messages/minute, or 500 MB/sec.

For your SIP notification system, you might say:

> "Our Kafka pipeline handled millions of events per day, so we focused on partitioning, batching and consumer concurrency to maintain throughput."

### What increases throughput?

```text
More partitions
        ↓
More consumer parallelism
        ↓
More processing in parallel
        ↓
Higher throughput
```

Also: batching, compression, larger messages/batches, efficient consumers, fast network, fast disks.

---

## 2. Latency

Latency = time taken for a message to travel through the system.

Example:

```text
Producer sends event
        ↓
Kafka receives it
        ↓
Consumer reads it
        ↓
Consumer processes it

Total = 50 ms
```

Kafka performance is often a trade-off:

> Higher throughput ↔ Higher latency

For example, if you wait to accumulate 100 messages before sending:

- Batch size = 100
- Throughput ↑
- But latency can ↑

Because the first message may wait for the other messages.

---

## 3. Batch Size

One of the most important Kafka producer concepts.

Instead of:

```text
Message 1 → Network
Message 2 → Network
Message 3 → Network
Message 4 → Network
```

Kafka can batch:

```text
Message 1
Message 2
Message 3
Message 4
      ↓
One network request
```

This reduces network calls, CPU overhead, request overhead, and generally increases throughput.

### Producer configuration

```properties
batch.size=16384
```

This is 16 KB by default in many Kafka client versions/configurations.

If messages are small (100 bytes, 100 bytes, 100 bytes...), Kafka can group many messages into one batch.

---

## 4. linger.ms

Batching has one problem. What if traffic is low?

Message 1 arrives. There aren't enough messages to fill the batch.

Kafka can wait for a short period:

```properties
linger.ms=5
```

Meaning approximately:

```text
Wait up to 5 ms
     ↓
Collect more messages
     ↓
Send batch
```

So:

```text
batch.size ↑
linger.ms ↑
      ↓
throughput ↑
      ↓
latency may ↑
```

### Interview answer

> "I would tune batch.size and linger.ms based on the workload. For high-throughput workloads, slightly larger batches can improve throughput, but excessive batching can increase latency."

---

## 5. Compression

Kafka supports compression such as: gzip, snappy, lz4, zstd.

Example:

```properties
compression.type=lz4
```

Instead of 10 MB data, Kafka might send 4 MB compressed data.

**Benefits:** Network I/O ↓, Disk usage ↓, Network bandwidth ↓.

But compression requires CPU: compression → CPU work ↑.

So there is a trade-off.

### Typical interview discussion

```text
Compression enabled
       ↓
Less network traffic
       ↓
Less disk traffic
       ↓
Potentially higher throughput
       ↓
But CPU usage increases
```

lz4 and zstd are commonly discussed for Kafka workloads because they offer good performance/compression trade-offs.

---

## 6. Number of Partitions

This is extremely important.

Suppose:

```text
Topic
 ├── P0
 ├── P1
 ├── P2
 └── P3
```

Four partitions allow multiple consumers to process data concurrently.

```text
P0 → Consumer 1
P1 → Consumer 2
P2 → Consumer 3
P3 → Consumer 4
```

So partitions provide parallelism.

More partitions → potentially higher throughput.

But don't think: *"More partitions always means better performance."*

There are costs: more metadata, more replication traffic, more files/log segments, more leader/follower work, more recovery work, more controller/cluster overhead.

So partition count should be chosen based on expected throughput and consumer parallelism.

---

## 7. Producer Concurrency

Suppose you have one producer:

```text
Producer 1
   ↓
Kafka
```

Now imagine:

```text
Producer 1 ─┐
Producer 2 ─┤
Producer 3 ─┼──→ Kafka
Producer 4 ─┘
```

Multiple producers can send data concurrently. This increases the ability to generate throughput.

But more producers also mean: network connections ↑, CPU usage ↑, broker request load ↑.

So again, more isn't automatically better.

---

## 8. Consumer Concurrency

This is particularly important for your interview.

Suppose:

```text
Topic
 ├── P0
 ├── P1
 ├── P2
 └── P3
```

Consumer group:

```text
C1 → P0
C2 → P1
C3 → P2
C4 → P3
```

You have 4 partitions, 4 consumers. Good parallelism.

But 4 partitions, 8 consumers — only 4 consumers can actively consume:

```text
C1 → P0
C2 → P1
C3 → P2
C4 → P3

C5 → idle
C6 → idle
C7 → idle
C8 → idle
```

### Golden rule

For a single consumer group:

> Active consumers ≤ number of partitions

This is why partition count determines the maximum consumer parallelism for that group.

---

## 9. Disk I/O

Kafka heavily relies on the filesystem.

Data is written to:

```text
Kafka log
   ↓
Disk
```

But Kafka doesn't need to load everything into application memory.

Kafka benefits heavily from **sequential I/O**.

Kafka writes logs sequentially:

```text
100
101
102
103
104
105
```

rather than randomly jumping around the disk.

This is efficient.

---

## 10. Page Cache

This is an important Kafka interview topic.

Kafka relies heavily on the OS page cache.

Conceptually:

```text
Kafka
  ↓
OS Page Cache
  ↓
Disk
```

When consumers request recently accessed data, the OS may serve it from memory instead of reading from disk.

So:

```text
Consumer
   ↓
Page Cache
   ↓
Data
```

can be much faster than:

```text
Consumer
   ↓
Disk
   ↓
Data
```

This is one reason Kafka can achieve high throughput using ordinary filesystem operations.

---

## 11. Network I/O

Kafka is fundamentally a distributed system, so network bandwidth matters.

Consider replication:

```text
Producer
   ↓
Leader Broker
   ↓
Follower 1
   ↓
Follower 2
```

With `Replication Factor = 3`, there is replication traffic between brokers.

Consumers also pull data:

```text
Broker
   ↓
Network
   ↓
Consumer
```

Therefore network can become a bottleneck.

Example:

```text
Disk can handle:       2 GB/s
Network can handle:    500 MB/s
```

Then your practical throughput may be limited by Network ≈ 500 MB/s.

---

## 12. How Everything Connects

Think of Kafka performance as:

```text
                 Kafka Performance
                        |
       ┌────────────────┼────────────────┐
       ↓                ↓                ↓
   Producer          Broker           Consumer
       |                |                |
   batching          disk I/O       concurrency
   compression       network        processing
   concurrency       replication    commits
```

And:

```text
Partitions
     ↓
Parallelism
     ↓
Throughput
```

while:

```text
Batching + compression
     ↓
Network efficiency
     ↓
Throughput ↑
```

---

## 13. Important Performance Trade-offs

### Case 1 — Increase batch size

```text
batch.size ↑
     ↓
More messages/request
     ↓
Network overhead ↓
     ↓
Throughput ↑
```

But: waiting for batch → latency ↑.

### Case 2 — Increase partitions

```text
Partitions ↑
     ↓
Parallelism ↑
     ↓
Potential throughput ↑
```

But: resource overhead ↑.

### Case 3 — Enable compression

```text
Compression
     ↓
Data size ↓
     ↓
Network I/O ↓
```

But: CPU usage ↑.

### Case 4 — Increase consumers

```text
Consumers ↑
     ↓
Parallel processing ↑
```

Only until consumers ≈ partitions.

After that: extra consumers → mostly idle.

---

## 14. Real Interview Scenario

**Interviewer:** "Kafka throughput is low. How will you debug it?"

Don't immediately say: *"Increase partitions."*

Instead, investigate systematically.

### Step 1 — Check producer

Are producers generating enough traffic?

Check: batch size, `linger.ms`, compression, producer CPU, producer network, request latency.

### Step 2 — Check brokers

Broker CPU? Disk utilization? Disk latency? Network bandwidth? Replication traffic? Under-replicated partitions?

### Step 3 — Check partitions

Are partitions evenly distributed? Is one partition receiving most traffic?

Potential problem:

```text
P0 → 90% traffic
P1 → 3%
P2 → 3%
P3 → 4%
```

This is a **hot partition**.

Increasing total partitions doesn't necessarily fix an existing hot-key distribution problem.

### Step 4 — Check consumers

Consumer count? Partition count? Consumer lag? Processing time? CPU? DB/API dependency?

Example:

```text
Kafka
  ↓
Consumer
  ↓
Slow DB
```

Kafka may be perfectly healthy. The bottleneck is the DB.

---

## 15. The Most Important Formula

For consumer parallelism:

> Maximum active consumers ≈ Number of partitions

For example: 12 partitions, 6 consumers — you can have 6 consumers, each handling roughly 2 partitions, assuming even assignment.

If you have 12 partitions, 12 consumers — you can get up to 12-way consumer parallelism.

If you have 12 partitions, 20 consumers — approximately 12 active, 8 idle.

---

## 16. Your Paytm SIP Example

You previously designed:

```text
Producer
   ↓
sip.pdn.debit.events
   ↓
12 partitions
   ↓
6 consumers
   ↓
Notification service
```

Performance reasoning:

```text
12 partitions
      ↓
Parallelism
      ↓
6 consumers
      ↓
Each consumer handles multiple partitions
```

If throughput becomes low, investigate:

```text
Producer
 ├── batching
 ├── compression
 └── producer CPU/network

Kafka
 ├── broker CPU
 ├── disk I/O
 ├── network I/O
 └── replication

Consumer
 ├── consumer concurrency
 ├── processing time
 ├── DB/API latency
 └── consumer lag
```

This is a much stronger interview answer than simply saying "increase partitions."

---

## 17. Quick Interview Cheat Sheet

| Factor | Increase | Possible Benefit | Possible Cost |
| --- | --- | --- | --- |
| Batch size | ↑ | Throughput ↑ | Latency ↑ |
| `linger.ms` | ↑ | Batching ↑ | Latency ↑ |
| Compression | Enable | Network I/O ↓ | CPU ↑ |
| Partitions | ↑ | Parallelism ↑ | Broker overhead ↑ |
| Producers | ↑ | Write concurrency ↑ | CPU/network ↑ |
| Consumers | ↑ | Processing parallelism ↑ | Only useful up to partitions |
| Disk speed | ↑ | I/O throughput ↑ | Cost |
| Network bandwidth | ↑ | Data transfer ↑ | Cost |
| Consumer processing | Optimize | Throughput ↑, lag ↓ | Development effort |

---

## Day 21 Interview Questions

You should be able to answer these without notes:

- How does Kafka achieve high throughput?
- Why does batching improve Kafka performance?
- What is `batch.size`?
- What is `linger.ms`?
- How does compression improve Kafka performance?
- Why do partitions increase throughput?
- Can we keep increasing partitions indefinitely?
- What happens if consumers > partitions?
- What happens if consumers < partitions?
- How can disk I/O become a Kafka bottleneck?
- How can network I/O become a bottleneck?
- What is a hot partition?
- Kafka throughput is low. How would you debug it?
- How would you increase consumer throughput?
- Why can increasing consumers fail to improve throughput?

**One line to remember:**

> Kafka performance = batching + compression + partition parallelism + producer/consumer concurrency + efficient disk and network I/O, while balancing throughput against latency.
