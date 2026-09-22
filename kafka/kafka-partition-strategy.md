# Day 25 — Kafka Partition Strategy

This is an important Kafka interview topic because partitioning affects throughput, ordering, scalability, and consumer parallelism.

---

## Table of Contents

- [1. What is a Partition?](#1-what-is-a-partition)
- [2. How Do You Decide the Number of Partitions?](#2-how-do-you-decide-the-number-of-partitions)
- [3. How Do You Choose the Partition Key?](#3-how-do-you-choose-the-partition-key)
- [4. Ordering Requirement](#4-ordering-requirement)
- [5. Hot Partitions](#5-hot-partitions)
- [6. How Do You Solve Hot Partitions?](#6-how-do-you-solve-hot-partitions)
- [7. Uneven Traffic](#7-uneven-traffic)
- [8. Consumer Parallelism](#8-consumer-parallelism)
- [9. Practical Partition Calculation](#9-practical-partition-calculation)
- [10. Interview Question](#10-interview-question)
- [11. Very Important Interview Follow-ups](#11-very-important-interview-follow-ups)
- [12. Your Paytm-Style Example](#12-your-paytm-style-example)
- [Remember This for Interviews](#remember-this-for-interviews)

---

## 1. What is a Partition?

A Kafka topic is divided into multiple partitions.

```text
Topic: orders

Partition 0 → Order events
Partition 1 → Order events
Partition 2 → Order events
Partition 3 → Order events
```

Each partition is an **ordered append-only log**.

Kafka can process different partitions in parallel.

---

## 2. How Do You Decide the Number of Partitions?

Don't randomly choose 10, 20, or 100.

Consider:

### A. Throughput requirement

Suppose incoming traffic = 50 MB/sec, and one partition can handle ≈ 10 MB/sec.

You may need: 50 / 10 = **5 partitions**.

In practice, keep some headroom: 5 → maybe choose 6–8 partitions.

The exact capacity depends on message size, brokers, hardware, producer/consumer configuration, replication, etc.

### B. Consumer parallelism

Suppose topic = 12 partitions.

Consumer group: C1, C2, C3, C4.

Kafka can distribute:

```text
C1 → P0 P1 P2
C2 → P3 P4 P5
C3 → P6 P7 P8
C4 → P9 P10 P11
```

So: **maximum useful consumer parallelism is bounded by the number of partitions.**

If you have 4 partitions, 10 consumers, only about 4 consumers can actively own partitions at a time.

```text
C1 → P0
C2 → P1
C3 → P2
C4 → P3

C5 → idle
C6 → idle
...
```

---

## 3. How Do You Choose the Partition Key?

The partition key should generally be something that provides:

- Required **ordering**
- Good **distribution** of traffic

Example: `userId`.

Kafka hashes the key:

```text
hash(userId) % number_of_partitions
```

Example:

```text
userId = 101 → P2
userId = 102 → P5
userId = 103 → P1
```

The same key normally goes to the same partition while the partition count remains unchanged.

---

## 4. Ordering Requirement

This is one of the most important questions.

Suppose you have User 101:

```text
Payment Created
Payment Completed
Payment Failed
```

You may need:

```text
Created
  ↓
Completed
  ↓
Failed
```

So use `partitionKey = userId`.

Then events for the same user go to the same partition.

```text
P0 → user 101 events
P1 → user 102 events
P2 → user 103 events
```

Kafka guarantees ordering **within a partition**, not across the entire topic.

---

## 5. Hot Partitions

This is a very common interview follow-up.

Suppose topic = 10 partitions, but one user generates 50% of all traffic: `userId = 999999`.

If you use `key = userId`, all events for that user go to one partition.

```text
P0 → 5%
P1 → 5%
P2 → 5%
P3 → 5%
P4 → 5%
P5 → 5%
P6 → 5%
P7 → 5%
P8 → 5%
P9 → 55%  ← HOT
```

Now P9 becomes the bottleneck even though other partitions are underutilized.

This is called a **hot partition**.

---

## 6. How Do You Solve Hot Partitions?

It depends on whether strict ordering is required.

### Case 1 — Ordering is NOT required

You can distribute events more randomly.

For example: `key = userId + random/shard number`.

So:

```text
userId 999 → shard 0 → P2
userId 999 → shard 1 → P7
userId 999 → shard 4 → P1
```

Traffic becomes more balanced.

But you **lose ordering** for that user.

### Case 2 — Ordering IS required

You cannot simply randomly distribute the events.

You need to rethink the ordering requirement.

For example, maybe you only require ordering per `orderId` instead of `userId`.

Then:

```text
user 999

Order A → P2
Order B → P7
Order C → P1
```

Events for each order remain ordered, while different orders can be processed in parallel.

---

## 7. Uneven Traffic

Imagine:

```text
P0 → 10,000 events
P1 → 11,000
P2 → 9,500
P3 → 10,200
P4 → 10,100
P5 → 100,000  ← problem
```

Adding consumers won't necessarily solve this.

Why? Because the problem is **partition distribution**, not simply the number of consumers.

```text
More consumers
       ↓
P5 still belongs to one consumer
       ↓
P5 still processes 100,000 events
```

You need to address the partition-key strategy or increase/shard the key space where ordering allows it.

---

## 8. Consumer Parallelism

Suppose 12 partitions, 6 consumers.

You can have:

```text
C1 → P0 P1
C2 → P2 P3
C3 → P4 P5
C4 → P6 P7
C5 → P8 P9
C6 → P10 P11
```

Now suppose 12 partitions, 20 consumers.

Only 12 consumers can actively consume partitions.

```text
12 active
8 idle
```

Therefore: **increasing consumers beyond the number of partitions does not increase partition-level parallelism.**

---

## 9. Practical Partition Calculation

Suppose your system receives 100,000 messages/sec.

Average message size: 2 KB.

Approximate incoming bandwidth: 100,000 × 2 KB = **200 MB/sec**.

Suppose testing shows one partition safely handles 10 MB/sec.

Then: 200 / 10 = **20 partitions**.

You might choose something like **24 partitions** to provide headroom.

But this isn't a universal formula. **Benchmark your actual workload.**

---

## 10. Interview Question

**Q: How would you decide the number of partitions for a Kafka topic?**

A good interview answer:

> "I would first estimate the expected message rate, message size, and required throughput. Then I would benchmark the producer and consumer to determine the sustainable throughput per partition. Based on that, I would calculate the required partitions and keep some headroom for traffic growth. I would also consider the required consumer parallelism and ordering requirements. Finally, I would choose a partition key that provides the required ordering while distributing traffic evenly and avoiding hot partitions."

---

## 11. Very Important Interview Follow-ups

**Q: Can I increase partitions later?**

Yes.

```text
10 partitions
      ↓
increase
      ↓
20 partitions
```

But be careful: increasing partitions can change the mapping of keyed messages to partitions, which can affect ordering assumptions for future events.

So partition count should be planned carefully.

**Q: Can I decrease partitions?**

Generally, Kafka does not support decreasing the partition count of an existing topic.

You typically create a new topic and migrate data.

**Q: Does more partitions always mean better performance?**

No.

More partitions can increase: broker resource usage, file handles, metadata, replication overhead, consumer coordination, recovery/reassignment work.

So the goal is **enough partitions for required throughput and parallelism**, not maximum partitions.

---

## 12. Your Paytm-Style Example

For your SIP notification system:

Topic: `sip.pdn.debit.events`

Suppose: 12 partitions, 6 consumers, RF = 3.

You could explain:

```text
Producer
   ↓
sip.pdn.debit.events
   ↓
12 partitions
   ↓
6 consumers
   ↓
Notification processing
```

If ordering is required per SIP/user:

`partition key = userId or sipId`

Then events for the same entity stay in the same partition while different entities can be processed in parallel.

**Interview follow-up:** "What happens if one user generates extremely high traffic?"

Answer:

> "If I use userId as the key, that user's events can create a hot partition. If strict ordering for the entire user's events is required, I cannot arbitrarily distribute them across partitions. If ordering can instead be scoped to a smaller entity such as SIP or order, I can use that as the key to improve distribution. Otherwise, I would need to handle the hot key through application-level sharding or rethink the ordering requirement."

---

## Remember This for Interviews

```text
Partitions
    ↓
Throughput + Parallelism

Partition Key
    ↓
Ordering + Distribution

Bad Key
    ↓
Hot Partition

Too Few Partitions
    ↓
Low Parallelism

Too Many Partitions
    ↓
More Kafka overhead
```

**The core interview principle:**

> Choose partitions based on throughput and consumer parallelism, and choose the partition key based on ordering requirements while keeping traffic evenly distributed.
