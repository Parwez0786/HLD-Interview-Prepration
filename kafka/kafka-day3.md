# Day 3 — Kafka Topics & Partitions

Today is one of the most important Kafka concepts for interviews.

If you understand **Topic → Partition → Key → Hashing → Ordering**, you can answer a large number of Kafka interview questions.

---

## Table of Contents

- [1. What is a Kafka Topic?](#1-what-is-a-kafka-topic)
- [2. Why Do We Need Partitions?](#2-why-do-we-need-partitions)
- [3. What Does a Partition Look Like?](#3-what-does-a-partition-look-like)
- [4. Partition Ordering](#4-partition-ordering)
- [5. Why Is Partition Ordering Important?](#5-why-is-partition-ordering-important)
- [6. What Is a Partition Key?](#6-what-is-a-partition-key)
- [7. Same Key → Same Partition](#7-same-key--same-partition)
- [8. How Does Hashing Work?](#8-how-does-hashing-work)
- [9. What If No Key Is Provided?](#9-what-if-no-key-is-provided)
- [10. The Most Important Interview Question](#10-the-most-important-interview-question)
- [11. Complete Flow](#11-complete-flow)
- [12. What Is Partition Assignment?](#12-what-is-partition-assignment)
- [13. Consumer Group and Partitions](#13-consumer-group-and-partitions)
- [14. Partition Scalability](#14-partition-scalability)
- [15. Partition Distribution Across Brokers](#15-partition-distribution-across-brokers)
- [16. Partition Count Selection](#16-partition-count-selection)
- [17. Increasing Partition Count](#17-increasing-partition-count)
- [18. Why Increasing Partitions Can Affect Ordering](#18-why-increasing-partitions-can-affect-ordering)
- [19. Partition Imbalance](#19-partition-imbalance)
- [20. Example of Bad Partition Key](#20-example-of-bad-partition-key)
- [21. Better Partition Key](#21-better-partition-key)
- [22. Don't Blindly Choose Random Keys](#22-dont-blindly-choose-random-keys)
- [23. Real-World Example — Order Processing](#23-real-world-example--order-processing)
- [24. Topic vs Partition](#24-important-distinction-topic-vs-partition)
- [25. Interview Questions](#25-interview-questions-you-should-know)
- [26. The 30-Second Interview Answer](#26-the-30-second-interview-answer)
- [27. One Diagram to Memorize](#27-one-diagram-to-memorize)

---

## 1. What is a Kafka Topic?

A topic is a **logical category/name** under which Kafka stores messages.

For example, in an e-commerce system:

- `order-created`
- `payment-completed`
- `order-cancelled`
- `user-created`

A producer sends messages to a topic:

```text
Producer
   |
   | OrderCreated event
   v
order-created
```

A consumer reads messages from that topic.

### Important

A topic is **not** where Kafka physically stores all messages as one single file.

A topic is divided into **partitions**.

```text
Topic: order-created
  ├── Partition 0
  ├── Partition 1
  ├── Partition 2
  └── Partition 3
```

Partitions are the actual units where Kafka stores and processes records.

---

## 2. Why Do We Need Partitions?

Suppose we have a topic with **1 billion messages**.

If the topic had only one partition:

```text
Producer
   |
   v
+------------------+
|   Partition 0    |
| 1 billion msgs   |
+------------------+
       |
       v
   Consumers
```

There is a major limitation:

> Only **one consumer** within a consumer group can actively consume that partition at a time.

So processing is limited by that single partition.

### With multiple partitions

```text
                 Topic
                  |
       +----------+----------+
       |          |          |
       v          v          v
      P0         P1         P2
       |          |          |
      C1         C2         C3
```

Now three consumers can process messages in parallel.

### Therefore partitions provide

| Benefit | What it means |
| --- | --- |
| **1. Parallelism** | Multiple consumers can process different partitions simultaneously |
| **2. Scalability** | You can distribute data across multiple brokers |
| **3. Ordering** | Kafka provides ordering within a partition |
| **4. Load distribution** | Messages can be distributed across multiple partitions |

---

## 3. What Does a Partition Look Like?

Think of a partition as an **append-only ordered log**.

```text
Partition 0

Offset
  0     Order A
  1     Order B
  2     Order C
  3     Order D
  4     Order E
```

Every message gets an **offset**.

The offset uniquely identifies the position of a record inside that partition.

```text
Partition 0:
  offset 0 → A
  offset 1 → B
  offset 2 → C
  offset 3 → D
```

Kafka generally appends new records to the end.

```text
A → B → C → D → E
                ^
              newest
```

---

## 4. Partition Ordering

This is extremely important.

Kafka guarantees ordering **inside a partition**.

Suppose:

```text
Partition 0:
  Offset 0 → A
  Offset 1 → B
  Offset 2 → C
  Offset 3 → D
```

A consumer sees:

```text
A → B → C → D
```

in that order.

### But what about multiple partitions?

```text
Partition 0:  A  B  C
Partition 1:  X  Y  Z
```

Kafka does **not** guarantee:

```text
A → X → B → Y → C → Z
```

There is **no global ordering** across partitions.

You only have:

```text
P0: A → B → C
P1: X → Y → Z
```

Each partition has its own ordering.

> **Interview line:** Kafka guarantees ordering within a partition, but not across partitions.

Remember this.

---

## 5. Why Is Partition Ordering Important?

Imagine an order-processing system.

**Events:**

- `OrderCreated`
- `PaymentCompleted`
- `OrderShipped`
- `OrderDelivered`

You don't want:

```text
OrderShipped
OrderCreated
PaymentCompleted
```

because the **order of events matters**.

Therefore, related events should ideally go to the **same partition**.

This is where the **partition key** becomes important.

---

## 6. What Is a Partition Key?

A producer can specify a **key** when sending a Kafka message.

For example:

| Field | Value |
| --- | --- |
| Key | `userId` |
| Value | `OrderCreated` |

Suppose `userId = 101`.

Kafka uses the key to determine the partition.

```text
userId
   |
   v
Hash function
   |
   v
partition number
```

For example:

```text
hash(101) % 4 = 2
```

So:

**User 101 → Partition 2**

---

## 7. Same Key → Same Partition

This is one of the most important Kafka concepts.

Suppose we have:

- Topic: `orders`
- Partitions: `4`

Messages:

```text
Key = user101
Key = user101
Key = user101
```

Kafka will normally route them to the **same partition** as long as the partitioning setup remains compatible.

```text
user101
   |
   v
hash(user101)
   |
   v
Partition 2
```

So Partition 2 contains:

```text
OrderCreated
PaymentCompleted
OrderShipped
OrderDelivered
```

This gives us **ordering for that key**.

---

## 8. How Does Hashing Work?

Conceptually, Kafka does something like:

```text
partition = hash(key) % numberOfPartitions
```

### Example

Number of partitions = 4

```text
hash("user101") = 17
17 % 4 = 1
→ user101 → Partition 1

hash("user102") = 22
22 % 4 = 2
→ user102 → Partition 2
```

So we might get:

```text
             orders topic

       +---------------------+
       | P0                  |
       +---------------------+
       | P1 ← user101        |
       +---------------------+
       | P2 ← user102        |
       +---------------------+
       | P3                  |
       +---------------------+
```

### Important caveat

Don't state in an interview that Kafka always literally does `hash(key) % partitionCount`.

The exact partitioner behavior can depend on the Kafka client, version, and configuration.

**The safe interview answer is:**

> When a key is provided, the Kafka producer's partitioner hashes the key and deterministically maps it to a partition. This ensures records with the same key are routed to the same partition, assuming the partitioning configuration remains unchanged.

---

## 9. What If No Key Is Provided?

This is another common interview question.

Suppose:

```java
producer.send(
    new ProducerRecord<>("orders", null, order)
);
```

There is no key.

Kafka's producer partitioner decides where the record goes.

Modern Kafka clients generally use a **sticky partitioning strategy** for records without keys, rather than simply doing round-robin for every individual record.

```text
Message 1 ──┐
Message 2 ──┤
Message 3 ──┼──> Partition 1
Message 4 ──┤
             └──> then producer may switch to another partition
```

The purpose is to improve **batching efficiency**.

**Interview answer:**

> If no key is provided, the producer's partitioner chooses a partition, with modern Kafka clients using sticky behavior to improve batching. Therefore, you should not rely on no-key messages for per-key ordering.

---

## 10. The Most Important Interview Question

> **How does Kafka decide which partition receives a message?**

This is a very common interview question. Answer it **step-by-step**.

### Case 1: Producer explicitly specifies a partition

Suppose:

| Field | Value |
| --- | --- |
| Topic | `orders` |
| Partition | `3` |
| Key | `user101` |

The producer sends directly to:

```text
orders → Partition 3
```

The partitioner does not need to choose another partition.

### Case 2: Key is provided

Suppose:

| Field | Value |
| --- | --- |
| Topic | `orders` |
| Key | `user101` |
| Value | `OrderCreated` |

The producer's partitioner uses the key to **deterministically** choose a partition.

```text
user101
   |
   v
Hash
   |
   v
Partition selection
   |
   v
Partition 2
```

So:

```text
user101 → P2
user101 → P2   (next message, same key)
```

Therefore P2 contains:

```text
OrderCreated
PaymentCompleted
OrderShipped
```

This preserves ordering for `user101`.

### Case 3: No key

If no key is provided (`Key = null`), the producer's partitioner chooses the partition, with modern Kafka clients using **sticky partitioning** to improve batching.

---

## 11. Complete Flow

This is a good diagram to remember for interviews.

```text
             Producer
                |
                v
          Kafka Producer
                |
                v
          Is partition
          explicitly given?
             /       \
           Yes        No
            |          |
            v          v
        Use given    Is key
        partition    provided?
                      /   \
                    Yes    No
                     |      |
                     v      v
                  Hash key  Producer
                     |      partitioner
                     |      chooses
                     |      partition
                     v
                  Partition
```

Then:

```text
                  Topic
                    |
       +------------+------------+
       |            |            |
       v            v            v
      P0           P1           P2
       |            |            |
       v            v            v
     Broker       Broker       Broker
```

---

## 12. What Is Partition Assignment?

Don't confuse **producer partition assignment** with **consumer partition assignment**.

They are different concepts.

### Producer side

**Question:** Which partition should this message go to?

The producer's partitioner handles this.

```text
Producer
   |
   v
Partitioner
   |
   v
P0 / P1 / P2 / P3
```

### Consumer side

**Question:** Which consumer should process this partition?

Kafka's consumer group coordination handles this.

```text
Topic
 |
 +-- P0 ----> Consumer 1
 |
 +-- P1 ----> Consumer 2
 |
 +-- P2 ----> Consumer 3
```

---

## 13. Consumer Group and Partitions

Suppose:

- Topic = `orders`
- Partitions = `4`

Consumer group: C1, C2, C3, C4

Kafka can assign:

| Partition | Consumer |
| --- | --- |
| P0 | C1 |
| P1 | C2 |
| P2 | C3 |
| P3 | C4 |

Each partition is processed by **only one consumer** within that consumer group at a time.

### What if we have 6 consumers?

Partitions = 4, Consumers = 6

You might get:

| Partition | Consumer |
| --- | --- |
| P0 | C1 |
| P1 | C2 |
| P2 | C3 |
| P3 | C4 |
| — | C5 **idle** |
| — | C6 **idle** |

Therefore:

> Maximum useful consumer parallelism within a consumer group is bounded by the **number of partitions**.

This is why partition count matters.

---

## 14. Partition Scalability

Suppose:

```text
Topic
  ├── P0
  ├── P1
  └── P2
```

Three partitions give you up to roughly **three-way** partition-level parallelism within a consumer group.

If traffic increases, you can add more partitions:

```text
P0  P1  P2  P3  P4  P5  P6  P7
```

You can then have more consumers processing in parallel.

**Example:** 8 partitions, 8 consumers

| Partition | Consumer |
| --- | --- |
| P0 | C0 |
| P1 | C1 |
| P2 | C2 |
| P3 | C3 |
| P4 | C4 |
| P5 | C5 |
| P6 | C6 |
| P7 | C7 |

This is one reason Kafka **scales horizontally**.

---

## 15. Partition Distribution Across Brokers

Partitions are also distributed across Kafka brokers.

Suppose Broker 1, Broker 2, Broker 3, and topic `orders`:

| Partition | Broker |
| --- | --- |
| P0 | Broker 1 |
| P1 | Broker 2 |
| P2 | Broker 3 |
| P3 | Broker 1 |
| P4 | Broker 2 |
| P5 | Broker 3 |

Now data and workload can be distributed across machines.

---

## 16. Partition Count Selection

This is a system-design interview favorite.

**Question:** How many partitions should you create?

There is **no universal number** like "always use 10 partitions."

You need to consider:

### 1. Expected throughput

Suppose one partition can handle approximately **10 MB/s**, and you expect **100 MB/s**.

You need roughly:

```text
100 / 10 = 10 partitions
```

Then add some **headroom**.

### 2. Consumer parallelism

If you expect **20 consumers** and want all consumers to work in parallel, you need enough partitions.

For example: **20 partitions, 20 consumers**.

### 3. Ordering requirements

More partitions can mean more parallelism, but ordering is only guaranteed **within each partition**.

If all events for a particular entity need ordering:

```text
key = entityId
```

and ensure they map to the same partition.

### 4. Future growth

You don't want:

```text
Today:     3 partitions
Tomorrow:  massive traffic
```

and then discover you need much more parallelism.

Partition count should consider expected future growth.

---

## 17. Increasing Partition Count

Kafka allows you to **increase** the number of partitions.

**Before:**

```text
P0  P1  P2
```

**Increase to:**

```text
P0  P1  P2  P3  P4  P5
```

But there is an important problem.

Suppose `hash(key) % 3` previously gave:

```text
user101 → P1
```

After increasing to 6, `hash(key) % 6` may give:

```text
user101 → P4
```

Therefore, the **mapping can change**.

---

## 18. Why Increasing Partitions Can Affect Ordering

Suppose you had 3 partitions:

```text
user101 → P1

Events:
  A → P1
  B → P1
  C → P1
```

Now increase partitions: **3 → 6**

The partition mapping may change. You could potentially have:

```text
A → P1
B → P4
C → P4
```

Now the previous per-key ordering assumptions become more complicated.

**Interview point:**

> Increasing the number of partitions can change the key-to-partition mapping, so applications that depend on stable partition mapping and ordering should consider this carefully.

---

## 19. Partition Imbalance

This is another important production problem.

Suppose 4 partitions:

| Partition | Size |
| --- | --- |
| P0 | 10 GB |
| P1 | 10 GB |
| P2 | 10 GB |
| P3 | **500 GB** |

P3 is a **hot partition**.

Why can this happen? Usually because of a **poor partition key**.

---

## 20. Example of Bad Partition Key

Suppose you have `Key = country` and your traffic is:

| Country | Traffic |
| --- | --- |
| India | 80% |
| USA | 10% |
| UK | 5% |
| Others | 5% |

Hashing may produce something like:

| Country | Partition |
| --- | --- |
| India | P2 |
| USA | P1 |
| UK | P3 |
| Others | P0 |

Now:

| Partition | Load |
| --- | --- |
| P2 | extremely busy |
| P0 | mostly idle |
| P1 | moderate |
| P3 | moderate |

This is **partition imbalance**.

---

## 21. Better Partition Key

Instead of `country`, you might use `userId` or `orderId`, depending on the ordering requirement.

For example:

```text
user101 → P1
user102 → P3
user103 → P0
user104 → P2
```

Traffic is likely to distribute more evenly.

---

## 22. Don't Blindly Choose Random Keys

Suppose you need ordering for a user's events.

You cannot simply use a **random UUID**, because:

```text
Event A → P1
Event B → P3
Event C → P0
```

Now the events for the same user are spread across partitions.

You lose the easy **per-user ordering** guarantee.

So partition-key selection is a trade-off:

```text
Ordering requirement
        +
Load distribution
        +
Parallelism
```

---

## 23. Real-World Example — Order Processing

Imagine:

- Topic: `orders`
- Partitions: `4`

Producer sends:

| Key | Value |
| --- | --- |
| `user101` | `OrderCreated` |
| `user101` | `PaymentCompleted` |
| `user101` | `OrderShipped` |

Kafka hashes:

```text
user101
   |
   v
partitioner
   |
   v
P2
```

So P2 contains:

```text
Offset 100 → OrderCreated
Offset 101 → PaymentCompleted
Offset 102 → OrderShipped
```

Consumer reads:

```text
OrderCreated
      ↓
PaymentCompleted
      ↓
OrderShipped
```

The ordering works because all these records use the **same key** and therefore go to the **same partition**.

---

## 24. Important Distinction: Topic vs Partition

| Topic | Partition |
| --- | --- |
| Logical category | Physical/logical ordered log within topic |
| Has a name | Identified by partition number |
| Contains partitions | Stores records |
| Producers write to topic | Records ultimately go to partitions |
| Consumers subscribe to topic | Consumers actually consume partitions |
| No single global ordering | Ordering within partition |

Think:

```text
Topic
 ├── Partition 0
 ├── Partition 1
 ├── Partition 2
 └── Partition 3
```

---

## 25. Interview Questions You Should Know

### Q1. Why does Kafka use partitions?

Partitions provide scalability, parallelism, load distribution, and per-partition ordering. Multiple consumers can process different partitions concurrently, and partitions can be distributed across brokers.

### Q2. Does Kafka guarantee ordering?

Kafka guarantees ordering **within a partition**, but **not across** multiple partitions.

### Q3. How do you guarantee ordering for a user?

Use the user ID as the message key so all events for that user are routed to the same partition.

```text
key = userId
```

### Q4. What happens if there is no key?

The producer's partitioner selects the partition. Modern Kafka clients use sticky partitioning behavior for keyless records to improve batching.

### Q5. What happens if you increase partitions?

Kafka can increase the partition count, but the key-to-partition mapping can change because the partition count is part of partition selection. Therefore, applications relying on stable key mapping and ordering need to consider this carefully.

### Q6. Can we decrease partition count?

Generally, **no**. Kafka does not support reducing a topic's partition count in place.

### Q7. Can two consumers in the same consumer group consume the same partition simultaneously?

Normally, **no**.

```text
P0 → C1     ✓

P0 → C1     ✗  (same group, same partition)
P0 → C2
```

### Q8. Can consumers from different consumer groups consume the same partition?

**Yes.**

```text
orders
       P0
      /  \
     /    \
 Group A  Group B
   C1       C5
```

Both groups maintain their own consumption progress.

---

## 26. The 30-Second Interview Answer

If the interviewer asks:

> **How does Kafka decide which partition receives a message?**

Give this answer:

> The producer determines the target partition. If the application explicitly specifies a partition, Kafka sends the record there. Otherwise, if a key is provided, the producer's partitioner hashes the key and deterministically maps it to a partition. This means records with the same key normally go to the same partition, which is useful for maintaining ordering for that key. If no key is provided, the partitioner selects a partition, with modern Kafka clients using sticky partitioning behavior to improve batching. Once the record reaches a partition, Kafka guarantees ordering within that partition, not across partitions.

That's a strong interview answer.

---

## 27. One Diagram to Memorize

```text
                         Producer
                            |
                            v
                       Kafka Producer
                            |
                    +-------+-------+
                    |               |
             Partition given?      No
                    |               |
                   Yes              v
                    |           Key given?
                    |            /      \
                    |          Yes       No
                    |           |         |
                    |           v         v
                    |        Hash key   Partitioner
                    |           |         |
                    +-----------+---------+
                                |
                                v
                       Selected Partition
                                |
              +-----------------+-----------------+
              |                 |                 |
              v                 v                 v
             P0                P1                P2
              |                 |                 |
              v                 v                 v
          Consumer           Consumer           Consumer
```

### Core mental model

```text
Topic
  ↓
Partitions
  ↓
Producer chooses partition
  ↓
Key determines stable partition mapping
  ↓
Same key → same partition
  ↓
Same partition → ordered records
  ↓
More partitions → more parallelism
```

### The one sentence to remember

> Partitioning gives Kafka scalability and parallelism, while partition keys allow us to control which related messages stay together and therefore preserve their ordering.
