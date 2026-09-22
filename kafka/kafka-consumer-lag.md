# Day 24 — Kafka Consumer Lag

Consumer lag is one of the most important Kafka interview topics because it tells you whether consumers are keeping up with producers.

---

## Table of Contents

- [1. What is Consumer Lag?](#1-what-is-consumer-lag)
- [2. Lag Is Per Partition](#2-important-lag-is-per-partition)
- [3. How Is Lag Calculated?](#3-how-is-lag-calculated)
- [4. Why Does Consumer Lag Increase?](#4-why-does-consumer-lag-increase)
- [5. Major Causes of Consumer Lag](#5-major-causes-of-consumer-lag)
- [6. How Do You Monitor Lag?](#6-how-do-you-monitor-lag)
- [7. Lag Is Not Automatically a Problem](#7-lag-is-not-automatically-a-problem)
- [8. How Do We Reduce Consumer Lag?](#8-how-do-we-reduce-consumer-lag)
- [9. Increase Consumer Concurrency](#9-increase-consumer-concurrency)
- [10. Increase Partitions](#10-increase-partitions)
- [11. Increasing Partitions Has Tradeoffs](#11-but-increasing-partitions-has-tradeoffs)
- [12. Consumer Scaling Example](#12-consumer-scaling-example)
- [13. How Do You Know What to Increase?](#13-how-do-you-know-what-to-increase)
- [14. Lag Recovery](#14-lag-recovery)
- [15. Lag vs Throughput](#15-very-important-interview-concept-lag-vs-throughput)
- [16. Interview Scenario](#16-interview-scenario)
- [17. Your Example](#17-your-example)
- [Day 24 Interview Cheat Sheet](#day-24-interview-cheat-sheet)

---

## 1. What is Consumer Lag?

Consumer lag = how many records have been produced but not yet processed/committed by a consumer.

Simple formula:

```text
Lag = Latest Offset - Consumer Committed Offset
```

Example:

```text
Latest offset   = 1,000,000
Consumer offset = 900,000

Lag = 1,000,000 - 900,000
    = 100,000 messages
```

So the consumer is 100,000 messages behind.

### Think of it like a queue

```text
Producer
   |
   v
Kafka
1 2 3 4 5 6 7 8 9 10
                ^
                |
            Consumer
```

Consumer processed till 7.

Latest = 10, Consumer = 7, Lag = 3.

---

## 2. Important: Lag Is Per Partition

Kafka doesn't have one global offset for a topic.

Each partition has its own offset.

Example — Topic: `orders`

```text
Partition 0:
Latest offset = 1000
Consumer offset = 900
Lag = 100

Partition 1:
Latest offset = 800
Consumer offset = 750
Lag = 50

Partition 2:
Latest offset = 1200
Consumer offset = 1000
Lag = 200
```

Total consumer lag: 100 + 50 + 200 = **350**

So when monitoring Kafka, you usually look at:

```text
Topic
   ↓
Partition
   ↓
Consumer Group
   ↓
Committed Offset
   ↓
Lag
```

---

## 3. How Is Lag Calculated?

Suppose Partition 0:

```text
Latest offset    = 1,000,000
Committed offset = 900,000

Lag = 1,000,000 - 900,000
    = 100,000
```

But there is an important detail: **Kafka consumer position vs committed offset**.

A consumer may have already fetched/processed records but not committed the offset yet.

So when discussing consumer-group lag, we generally use the **committed offset**.

For example:

```text
Latest offset       = 1,000
Committed offset    = 900
Current position    = 950

Group lag:
1000 - 900 = 100
```

Even though the consumer may currently be processing around offset 950.

---

## 4. Why Does Consumer Lag Increase?

The most important reason:

> Producer rate > Consumer processing rate

Example:

```text
Producer:  10,000 messages/sec
Consumer:   7,000 messages/sec

Difference: 10,000 - 7,000 = 3,000 messages/sec
```

So lag keeps increasing.

```text
Time
 ↓

0 sec     Lag = 0
10 sec    Lag = 30,000
20 sec    Lag = 60,000
30 sec    Lag = 90,000
```

---

## 5. Major Causes of Consumer Lag

### 1. Slow processing

```text
Kafka
 ↓
Consumer
 ↓
Database API
 ↓
External API
 ↓
Complex processing
```

If processing each message takes too long, lag increases.

### 2. Database is slow

```text
Consumer
   ↓
DB UPDATE
   ↓
500 ms
```

If every message takes 500 ms, consumer throughput can become low.

Possible solutions: DB indexing, batch writes, connection pooling, optimizing queries, caching, asynchronous processing.

### 3. External API is slow

```text
Kafka
 ↓
Consumer
 ↓
Payment Service
 ↓
800 ms response
```

The consumer may spend most of its time waiting.

### 4. Too few consumers

Suppose 12 partitions, 3 consumers.

Each consumer may handle approximately 4 partitions.

If processing is heavy, these 3 consumers may not keep up.

### 5. Too few partitions

Suppose topic = 3 partitions, consumers = 10.

Only 3 consumers can actively consume.

```text
P0 → C1
P1 → C2
P2 → C3

C4 → idle
C5 → idle
...
C10 → idle
```

Increasing consumers alone won't help. You may need more partitions.

### 6. Consumer failures

```text
C1
 ↓
CRASH
```

Its partitions get reassigned during rebalance.

During that period, consumption may slow down and lag can increase.

### 7. Rebalancing

Frequent consumer joins/leaves can cause repeated rebalances.

```text
Consumption
    ↓
temporarily reduced/stopped
    ↓
lag increases
```

### 8. CPU / memory limitations

If the consumer machine is overloaded: CPU → 100%, Memory → high, GC → frequent.

Processing becomes slower.

---

## 6. How Do You Monitor Lag?

Kafka monitoring tools can show: Consumer Group, Topic, Partition, Current Offset, Log End Offset, Lag.

For example:

```text
Group: notification-group

Partition    Current    Latest       Lag
------------------------------------------------
0            900000     1000000      100000
1            950000     1000000       50000
2            980000     1000000       20000

Total: 100000 + 50000 + 20000 = 170000
```

Common monitoring approaches include: Kafka CLI tools, Kafka exporter, Prometheus, Grafana, Confluent monitoring, Cloud Kafka monitoring, Application metrics.

In your Paytm-style setup, you could monitor the consumer group's lag through your monitoring stack and alert when lag crosses a threshold.

---

## 7. Lag Is Not Automatically a Problem

This is an important interview point.

Suppose Lag = 50,000. You shouldn't immediately say *"Kafka is broken."*

Because lag can be temporarily high.

Example:

```text
10:00 → Producer spike
10:01 → Lag = 100,000
10:05 → Lag = 20,000
10:07 → Lag = 0
```

The consumer caught up.

So we care about **lag trend**:

| Trend | Meaning |
| --- | --- |
| Increasing continuously | Problem |
| Stable | Depends |
| Decreasing | Consumer catching up |
| Near zero | Consumer keeping up |

---

## 8. How Do We Reduce Consumer Lag?

There are several approaches.

### Approach 1 — Increase consumer processing speed

Optimize: DB queries, API calls, serialization, business logic, network calls.

Example:

- Before: processing = 100 ms/message
- After: processing = 40 ms/message

Consumer throughput increases.

---

## 9. Increase Consumer Concurrency

Suppose 12 partitions, 3 consumers.

You could increase consumers: 3 → 6.

Now approximately:

```text
P0 P1 → C1
P2 P3 → C2
P4 P5 → C3
P6 P7 → C4
P8 P9 → C5
P10 P11 → C6
```

More consumers can increase throughput if the workload can be processed in parallel.

---

## 10. Increase Partitions

Suppose 3 partitions, 3 consumers. All consumers are already busy.

Adding consumers (3 partitions, 10 consumers) doesn't increase parallelism.

Instead: 3 partitions → increase to 12 partitions.

Then 12 partitions, 12 consumers. Now you can have much greater parallelism.

**Important interview statement:**

> Maximum active consumers in a consumer group for a topic cannot exceed the number of partitions.

Extra consumers remain idle for that topic.

---

## 11. But Increasing Partitions Has Tradeoffs

Don't blindly say: *"Increase partitions."*

More partitions can mean: more consumer parallelism, more open connections, more metadata, more files/log segments, more resource usage, more operational complexity.

Also, partition count cannot simply be reduced later.

So partition planning should be deliberate.

---

## 12. Consumer Scaling Example

Suppose producer = 120,000 msg/sec.

Current: 12 partitions, 4 consumers.

Each consumer = 20,000 msg/sec.

Total consumer capacity: 4 × 20,000 = **80,000 msg/sec**.

Producer: 120,000 msg/sec.

Therefore:

```text
Incoming   = 120k
Processing =  80k
Difference =  40k/sec
```

Lag will continuously increase.

### Solution

Increase consumers: 4 → 6.

Capacity: 6 × 20,000 = **120,000 msg/sec**.

Now incoming = 120k, processing = 120k. Lag can stop growing.

If consumers are already limited by the number of partitions, increase partitions first.

---

## 13. How Do You Know What to Increase?

This is a very common interview question.

### Case 1

Partitions = 12, Consumers = 3, Lag = increasing.

Consumers are fewer than partitions. You can investigate increasing consumer instances/concurrency.

### Case 2

Partitions = 3, Consumers = 10, Lag = increasing.

Only 3 consumers can actively consume. Adding more consumers won't solve the partition-parallelism bottleneck.

Consider increasing partitions if the workload and keying model allow it.

### Case 3

Partitions = 12, Consumers = 12, Lag = increasing, CPU = 100%.

Adding more consumers may not help. First optimize processing or add compute capacity.

### Case 4

Partitions = 12, Consumers = 12, CPU = 30%, DB latency = 500ms, Lag = increasing.

The DB is likely the bottleneck. Optimize the DB path rather than blindly adding Kafka consumers.

---

## 14. Lag Recovery

Imagine:

- Normal traffic: 50,000 msg/sec
- Consumer capacity: 50,000 msg/sec
- Everything is fine

Suddenly producer traffic becomes 100,000 msg/sec for 5 minutes. Lag grows.

After the spike:

```text
Producer = 50,000/sec
Consumer = 70,000/sec
```

Now Consumer > Producer. Therefore lag decreases.

This is called **catching up**.

---

## 15. Very Important Interview Concept: Lag vs Throughput

Don't look at lag alone.

Consider: producer rate, consumer processing rate, lag.

Example:

```text
Producer = 100k/sec
Consumer =  80k/sec
→ Lag increasing

Producer = 100k/sec
Consumer = 120k/sec
→ Lag decreasing
```

So the key relationship is:

```text
Consumer throughput > Producer throughput
        ↓
Lag decreases

Consumer throughput < Producer throughput
        ↓
Lag increases
```

---

## 16. Interview Scenario

**Interviewer:** Your Kafka consumer lag is continuously increasing. What will you do?

A strong simple answer:

> First, I would check whether the lag is increasing continuously or only because of a temporary traffic spike. Then I would check producer throughput, consumer throughput, partition distribution, consumer CPU/memory, processing latency, database/API latency, and rebalance activity. If consumers are underutilized because there are too few consumer instances, I would scale consumers. If the topic doesn't have enough partitions, I would consider increasing partitions. If the bottleneck is DB or external API processing, I would optimize that instead of blindly adding consumers.

That's a good SDE-1/HLD interview answer.

---

## 17. Your Example

Given:

```text
Latest offset   = 1,000,000
Consumer offset = 900,000

Lag = 1,000,000 - 900,000
    = 100,000
```

Now suppose producer rate = 10,000 msg/sec, consumer rate = 8,000 msg/sec.

Lag increases by approximately 10,000 - 8,000 = **2,000 messages/sec**.

After 10 seconds: additional lag ≈ 20,000.

So: 100,000 → 120,000.

If you increase consumer capacity to 12,000 msg/sec:

```text
Producer = 10,000
Consumer = 12,000
```

Lag starts decreasing at roughly 2,000 messages/sec.

---

## Day 24 Interview Cheat Sheet

| Question | Answer |
| --- | --- |
| What is lag? | Difference between latest offset and consumer committed offset |
| Formula? | Latest Offset − Committed Offset |
| Where is lag measured? | Per partition / consumer group |
| Lag increasing means? | Consumer can't keep up |
| Main cause? | Producer rate > consumer processing rate |
| How reduce it? | Optimize processing, scale consumers, increase partitions when needed |
| More consumers always help? | No |
| Maximum active consumers? | Limited by partition count |
| 3 partitions + 10 consumers? | Only 3 can actively consume that topic |
| Consumer CPU 100%? | Processing/compute may be bottleneck |
| DB slow? | Optimize DB path rather than blindly scaling Kafka |
| Temporary lag? | Can recover if consumer throughput exceeds producer throughput |

**One line to remember:**

> Consumer lag tells us how far behind the consumer group is from the latest data, and we reduce it by making consumers process faster than producers.
