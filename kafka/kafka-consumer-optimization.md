# Day 23 — Kafka Consumer Optimization

Today’s main goal is to understand how a Kafka consumer controls how much data it receives, how fast it processes it, and why it can get removed from the consumer group.

---

## Table of Contents

- [1. fetch.min.bytes](#1-fetchminbytes)
- [2. fetch.max.wait.ms](#2-fetchmaxwaitms)
- [3. max.poll.records](#3-maxpollrecords)
- [4. Why max.poll.records Matters](#4-why-maxpollrecords-matters)
- [5. Consumer Concurrency](#5-consumer-concurrency)
- [6. Processing Time](#6-processing-time)
- [7. max.poll.interval.ms](#7-maxpollintervalms)
- [8. Why Does a Consumer Get Kicked Out of a Group?](#8-why-does-a-consumer-get-kicked-out-of-a-group)
- [9. Two Different Timeout Concepts](#9-two-different-timeout-concepts)
- [10. How to Fix a Slow Consumer](#10-how-to-fix-a-slow-consumer)
- [11. Consumer Lag](#11-consumer-lag)
- [12. Complete Consumer Optimization Picture](#12-complete-consumer-optimization-picture)
- [13. Interview Scenario](#13-interview-scenario)
- [14. Day 23 Cheat Sheet](#14-your-day-23-cheat-sheet)

---

## 1. fetch.min.bytes

This controls the minimum amount of data the broker should try to return in a fetch response.

Example:

```properties
fetch.min.bytes=1
```

The broker can return data as soon as it has some data.

If `fetch.min.bytes=1000000`, the broker tries to wait until around 1 MB of data is available before responding.

### Trade-off

```text
Lower fetch.min.bytes
        ↓
Responses come sooner
        ↓
Lower latency
        ↓
More network requests

Higher fetch.min.bytes
        ↓
Larger responses
        ↓
Better throughput
        ↓
Potentially higher latency
```

**Interview answer:**

> `fetch.min.bytes` controls the minimum amount of data the broker attempts to return in a fetch response. Increasing it can improve throughput by reducing the number of fetch requests, but can increase latency when traffic is low.

---

## 2. fetch.max.wait.ms

Suppose you configure:

```properties
fetch.min.bytes=1MB
fetch.max.wait.ms=500
```

But the topic is receiving data slowly.

The broker doesn't have 1 MB yet.

It can wait up to 500 ms before returning whatever data is available.

So:

```text
Need 1 MB
   ↓
Data arrives slowly
   ↓
Broker waits
   ↓
500 ms reached
   ↓
Return available data
```

### Important relationship

These two settings work together:

```text
fetch.min.bytes
       +
fetch.max.wait.ms
       ↓
How long / how much the broker waits
```

**Interview answer:**

> `fetch.max.wait.ms` is the maximum amount of time the broker waits to satisfy `fetch.min.bytes` before returning available data.

---

## 3. max.poll.records

This one is extremely important for consumer performance.

```properties
max.poll.records=500
```

It means one `poll()` returns at most 500 records.

Example:

```text
Kafka
  ↓
Partition
  ↓
Consumer
  ↓
poll()
  ↓
500 records maximum
```

It does **not** mean Kafka only has 500 records.

There could be millions of records waiting.

It only controls how many records the consumer returns from **one `poll()` call**.

---

## 4. Why max.poll.records Matters

Suppose processing one record takes 100 ms, and `max.poll.records=1000`.

Worst-case processing time: 1000 × 100 ms = **100 seconds**.

Now consider `max.poll.interval.ms=30000`.

The consumer is expected to call `poll()` again within 30 seconds.

But your consumer takes 100 seconds processing the batch.

Problem:

```text
poll()
  ↓
1000 records
  ↓
processing
  ↓
30 sec exceeded
  ↓
Consumer considered failed
  ↓
Removed from group
  ↓
Rebalance
```

This is one of the most important Kafka interview scenarios.

---

## 5. Consumer Concurrency

Suppose:

```text
Topic
 ├── P0
 ├── P1
 ├── P2
 └── P3
```

You have 4 partitions, 4 consumers.

Potentially:

```text
Consumer 1 → P0
Consumer 2 → P1
Consumer 3 → P2
Consumer 4 → P3
```

Processing happens in parallel.

But if 4 partitions, 8 consumers — only 4 consumers can actively consume partitions.

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

### Key rule

> Maximum useful consumer parallelism for a topic is generally bounded by its number of partitions.

So increasing consumers beyond partition count doesn't increase consumption parallelism for that topic.

---

## 6. Processing Time

Suppose your consumer does:

```text
poll()
 ↓
Read records
 ↓
Validate
 ↓
Call external API
 ↓
Update DB
 ↓
Commit offset
 ↓
poll()
```

If processing becomes slow:

```text
Kafka records
     ↓
Consumer
     ↓
Slow DB/API
     ↓
Processing takes longer
     ↓
Consumer lag increases
```

Therefore, Kafka performance isn't only about Kafka.

Your DB, external APIs, CPU, network, serialization/deserialization, and business logic can all affect consumer throughput.

---

## 7. max.poll.interval.ms

This is the key setting for today's interview question.

It defines approximately: **the maximum time allowed between successful calls to `poll()` before Kafka considers the consumer to have failed.**

For example:

```properties
max.poll.interval.ms=300000
```

means 5 minutes.

If your consumer doesn't call `poll()` again within that interval, Kafka can remove it from the group and trigger a rebalance.

---

## 8. Why Does a Consumer Get Kicked Out of a Group?

This is the main question.

Imagine:

```text
Consumer
   ↓
poll()
   ↓
Gets 500 records
   ↓
Process records
   ↓
DB is extremely slow
   ↓
Processing takes too long
   ↓
Doesn't call poll() in time
   ↓
max.poll.interval.ms exceeded
   ↓
Consumer leaves / is removed from group
   ↓
Rebalance
```

### Simple interview answer

> A consumer can be removed from the consumer group when it fails to poll within `max.poll.interval.ms`. This commonly happens when processing a batch takes too long. Kafka assumes the consumer is unhealthy, removes it, and triggers a rebalance.

---

## 9. Two Different Timeout Concepts

This is a common interview trap.

### max.poll.interval.ms

Concerned with: how long between `poll()` calls?

```text
poll()
 ↓
processing
 ↓
processing
 ↓
poll()
```

If this gap is too large → consumer can be removed.

### session.timeout.ms

Concerned with: consumer's session/heartbeat health.

Kafka consumers send heartbeats to the broker.

If heartbeats aren't received within the session timeout, Kafka can consider the consumer dead.

So remember:

```text
max.poll.interval.ms
        ↓
Processing / poll frequency

session.timeout.ms
        ↓
Heartbeat / liveness
```

---

## 10. How to Fix a Slow Consumer

Suppose `max.poll.records = 1000` and processing takes too long.

### Option 1 — Reduce max.poll.records

For example `max.poll.records=100`.

Now each batch is smaller.

```text
100 records
 ×
processing time
 ↓
Shorter processing batch
```

This allows the consumer to call `poll()` more frequently.

### Option 2 — Increase max.poll.interval.ms

If processing genuinely needs more time: `max.poll.interval.ms=300000`.

You can increase the interval appropriately.

But don't blindly increase it.

If processing is unexpectedly slow, you should investigate the underlying bottleneck.

### Option 3 — Improve processing

For example:

```text
Slow DB query
      ↓
Add proper index
      ↓
Faster processing
```

or:

```text
Sequential API calls
      ↓
Controlled parallel processing
      ↓
Higher throughput
```

### Option 4 — Increase consumer parallelism

If partitions are available:

```text
12 partitions
      ↓
More consumers
      ↓
More parallel processing
      ↓
Higher throughput
```

But remember:

```text
Consumers > partitions
        ↓
Extra consumers idle
```

---

## 11. Consumer Lag

Consumer lag tells us approximately how far behind the consumer is from the latest available records.

Conceptually:

```text
Latest Kafka offset
        -
Consumer committed offset
        =
Lag
```

Example: latest offset = 10,000, consumer offset = 8,000 → lag ≈ 2,000.

If producers keep producing faster than consumers process:

```text
Producer
   ↓
1000 msg/sec

Consumer
   ↓
700 msg/sec

Lag
   ↓
Growing
```

If consumer throughput becomes higher than producer throughput:

```text
Producer → 700 msg/sec
Consumer → 1000 msg/sec

Lag → decreases
```

---

## 12. Complete Consumer Optimization Picture

Think about the entire pipeline:

```text
                 KAFKA BROKER
                     │
                     │
           fetch.min.bytes
                     │
           fetch.max.wait.ms
                     ↓
                Consumer
                     │
                poll()
                     │
           max.poll.records
                     ↓
              Processing
                     │
          ┌──────────┴──────────┐
          ↓                     ↓
         DB                  API calls
          │                     │
          └──────────┬──────────┘
                     ↓
                commit offset
                     │
                     ↓
                  poll()
```

And parallelism:

```text
Topic
 ├── P0 ──→ Consumer 1
 ├── P1 ──→ Consumer 2
 ├── P2 ──→ Consumer 3
 └── P3 ──→ Consumer 4
```

---

## 13. Interview Scenario

**Interviewer:** Consumer lag is continuously increasing. What would you check?

A good answer:

> First, I would check whether the consumer processing throughput is lower than the producer throughput. Then I would check consumer CPU, DB latency, external API latency, max.poll.records, partition count, consumer concurrency, and whether the consumer is frequently rebalancing. I would also check whether max.poll.interval.ms is being exceeded because of slow processing.

---

## 14. Your Day 23 Cheat Sheet

| Setting | Controls | Main trade-off |
| --- | --- | --- |
| `fetch.min.bytes` | Minimum data broker tries to return | Throughput vs latency |
| `fetch.max.wait.ms` | Maximum broker fetch wait | Throughput vs latency |
| `max.poll.records` | Records returned per `poll()` | Batch size vs processing time |
| `max.poll.interval.ms` | Maximum gap between polls | Processing time vs failure detection |
| `session.timeout.ms` | Consumer session/heartbeat timeout | Liveness vs tolerance |
| Consumer concurrency | Parallel processing | Throughput vs resources |
| Consumer lag | Consumer falling behind | Indicates throughput problem |

### The most important chain to remember

```text
More records per poll
        ↓
More processing per batch
        ↓
Longer processing time
        ↓
If > max.poll.interval.ms
        ↓
Consumer removed from group
        ↓
Rebalance
        ↓
Temporary disruption + lag
```

**Interview one-liner:**

> A consumer gets kicked out when Kafka considers it unhealthy—commonly because it doesn't call `poll()` within `max.poll.interval.ms`, or because it stops heartbeating and exceeds the session timeout.
