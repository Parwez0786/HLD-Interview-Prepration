# Day 6 — Kafka Consumer Groups

The most important idea is:

> A consumer group allows multiple consumers to work together to process a topic in parallel.

---

## Table of Contents

- [1. Why Do We Need Consumer Groups?](#1-why-do-we-need-consumer-groups)
- [2. What Exactly Is a Consumer Group?](#2-what-exactly-is-a-consumer-group)
- [3. One Partition → One Consumer Within a Group](#3-one-partition--one-consumer-within-a-group)
- [4. Why Partition Count Matters for Scaling](#4-why-partition-count-matters-for-scaling)
- [5. 3 Consumers and 5 Partitions](#5-what-if-we-have-3-consumers-and-5-partitions)
- [6. Consumer Scaling](#6-consumer-scaling)
- [7. What Is Rebalancing?](#7-what-is-rebalancing)
- [8. What Happens When a Consumer Crashes?](#8-what-happens-when-a-consumer-crashes)
- [9. Consumer Lag](#9-consumer-lag)
- [10. How Do We Reduce Consumer Lag?](#10-how-do-we-reduce-consumer-lag)
- [11. Consumer Group vs Multiple Consumer Groups](#11-consumer-group-vs-multiple-consumer-groups)
- [12. Real Paytm Money Example](#12-real-paytm-money-example)
- [13. Interview Cheat Sheet](#13-interview-cheat-sheet)

---

## 1. Why Do We Need Consumer Groups?

Suppose we have:

```text
Topic: sip.pdn.debit.events

P0
P1
P2
P3
```

If one consumer reads all 4 partitions:

```text
          Consumer C1
        /   |   |   \
       P0  P1  P2   P3
```

It becomes a bottleneck when traffic increases.

So we create multiple consumers:

```text
Consumer Group: notification-group

C1 → P0
C2 → P1
C3 → P2
C4 → P3
```

Now processing happens in **parallel**.

---

## 2. What Exactly Is a Consumer Group?

A consumer group is a collection of consumers having the same `group.id`.

Example:

```text
group.id=notification-group
C1 ─┐
C2 ─┤
C3 ─┤ → notification-group
C4 ─┘
```

Kafka treats them as **one logical subscriber**.

This is important because Kafka distributes partitions among consumers inside the same group.

---

## 3. One Partition → One Consumer Within a Group

Suppose:

```text
Topic
├── P0
├── P1
├── P2
└── P3

Consumer Group
├── C1
├── C2
├── C3
└── C4
```

Kafka can assign:

```text
C1 → P0
C2 → P1
C3 → P2
C4 → P3
```

At a given time:

> A partition is assigned to only one consumer within the same consumer group.

This prevents two consumers in the same group from normally processing the same partition simultaneously.

---

## 4. Why Partition Count Matters for Scaling

This is a very important interview concept.

Suppose 3 partitions, 3 consumers:

```text
P0 → C1
P1 → C2
P2 → C3
```

All 3 consumers are doing useful work.

Now increase consumers: 3 partitions, 5 consumers.

There aren't enough partitions for every consumer.

Therefore:

```text
P0 → C1
P1 → C2
P2 → C3

C4 → IDLE
C5 → IDLE
```

### Answer to your important question

**What happens if there are 5 consumers but only 3 partitions?**

Only 3 consumers will actively consume. The remaining 2 consumers will be idle because Kafka cannot assign the same partition to multiple consumers within the same consumer group.

So:

```text
Partitions = 3
Consumers  = 5

Active consumers = 3
Idle consumers   = 2
```

This is why:

> Maximum useful consumer parallelism within one consumer group is limited by the number of partitions.

---

## 5. What If We Have 3 Consumers and 5 Partitions?

Now:

```text
Partitions = 5
Consumers  = 3
```

Kafka distributes multiple partitions to some consumers:

```text
C1 → P0, P1
C2 → P2, P3
C3 → P4
```

So one consumer can consume **multiple partitions**.

Remember:

- **One partition → max one consumer** within a group
- **One consumer → can consume multiple partitions**

This distinction is extremely important.

---

## 6. Consumer Scaling

Suppose initially: 4 partitions, 2 consumers.

Kafka may assign:

```text
C1 → P0, P1
C2 → P2, P3
```

Traffic increases. We add two consumers: 4 partitions, 4 consumers.

Kafka rebalances:

```text
C1 → P0
C2 → P1
C3 → P2
C4 → P3
```

Now processing is more parallel.

But adding consumers **beyond partition count doesn't help**.

```text
4 partitions
6 consumers

Maximum active consumers: 4
Two remain idle.
```

---

## 7. What Is Rebalancing?

Suppose we have:

```text
P0 → C1
P1 → C2
P2 → C3
P3 → C4
```

Now C2 crashes.

Kafka detects that C2 has left the group.

It performs a **rebalance**.

For example:

```text
P0 → C1
P1 → C3
P2 → C4
P3 → C1
```

Now all partitions have consumers again.

Rebalancing means:

> Kafka redistributes partitions among the consumers currently present in the consumer group.

Rebalancing can happen when:

- Consumer joins
- Consumer leaves
- Consumer crashes
- Consumer is considered dead
- Topic partition count changes

---

## 8. What Happens When a Consumer Crashes?

Example:

```text
P0 → C1
P1 → C2
P2 → C3
```

C2 crashes.

Kafka eventually detects it.

**Before:**

```text
C1 → P0
C2 → P1
C3 → P2
```

**After rebalance:**

```text
C1 → P0
C3 → P1, P2
```

The important thing is that the partition is **reassigned**, so the group can continue processing.

Kafka uses the consumer's **committed offset** to determine where the new consumer should resume.

---

## 9. Consumer Lag

This is another very common interview question.

Suppose partition P0 has:

```text
Latest Kafka offset = 1000
Consumer committed offset = 900
```

Then:

```text
Consumer Lag = 1000 - 900
             = 100
```

Meaning: there are approximately 100 records that the consumer has not caught up with.

Visualize it:

```text
Kafka
P0:
0 1 2 3 ... 900 ... 999 1000
              ↑         ↑
         Consumer      Latest
         position
```

If producers are producing faster than consumers process:

```text
Producer rate > Consumer processing rate
then:
Lag ↑
```

If consumers process faster:

```text
Consumer processing rate > Producer rate
then:
Lag ↓
```

---

## 10. How Do We Reduce Consumer Lag?

Common approaches:

### Option 1 — Add consumers

If you have 12 partitions, 3 consumers, you could increase to 12 consumers to increase parallelism.

But 12 partitions, 20 consumers doesn't give 20x processing power.

Only 12 can actively consume.

### Option 2 — Increase partitions

If you have 3 partitions, 10 consumers, only 3 consumers can work.

If the application needs more parallelism, you may increase partitions: 10 partitions, 10 consumers.

Now all 10 consumers can potentially work.

**Important:** Increasing partitions is a topic-level design decision and has consequences for **ordering** and operational management.

---

## 11. Consumer Group vs Multiple Consumer Groups

This is VERY important.

Suppose:

```text
Topic: orders
P0 P1 P2 P3
```

### Same group

```text
Group A

C1 → P0
C2 → P1
C3 → P2
C4 → P3
```

Each message is processed by **one consumer** in that group.

### Different groups

Suppose we have:

- Group A → Notification Service
- Group B → Analytics Service

Both groups independently consume the topic:

```text
                 Topic
              /         \
             /           \
      Group A             Group B
   Notification          Analytics
```

So the same Kafka record can be consumed **once by Group A** and **once by Group B**.

This gives Kafka its **publish-subscribe** behavior.

---

## 12. Real Paytm Money Example

For your SIP notification system, you can explain it like this:

> We publish SIP debit events to a Kafka topic. We use a consumer group for the notification service so that multiple instances of the notification service can process events in parallel. Kafka distributes partitions among the instances. If one instance goes down, Kafka rebalances its partitions among the remaining consumers. We monitor consumer lag to identify whether consumers are falling behind producers.

If interviewer asks:

> Why not simply create 10 consumers?

Say:

> Consumer parallelism is limited by partition count. If the topic has only 3 partitions and I create 10 consumers in the same group, only 3 consumers can actively consume and the remaining 7 will be idle. So partition count should be designed according to the required processing parallelism.

---

## 13. Interview Cheat Sheet

| Concept | Meaning |
| --- | --- |
| Consumer Group | Logical group of consumers |
| `group.id` | Identifies the consumer group |
| Partition assignment | Kafka distributes partitions among group members |
| One partition | Max one consumer within a group |
| One consumer | Can consume multiple partitions |
| Consumers > partitions | Extra consumers are idle |
| Consumers < partitions | Consumers get multiple partitions |
| Rebalancing | Redistributing partitions |
| Consumer Lag | Records consumer is behind |
| Add consumers | Increases parallelism up to partition count |
| Add partitions | Allows greater potential parallelism |
| Different groups | Each group independently consumes messages |

### 5 lines to remember for the interview

1. Consumer groups provide parallel processing and scalability.
2. Consumers with the same `group.id` share partitions.
3. One partition can be assigned to only one consumer within a group.
4. If consumers > partitions, extra consumers remain idle.
5. If a consumer joins/leaves/crashes, Kafka rebalances partitions.

Next Day 6 follow-up topics worth mastering: partition assignment strategies (Range, RoundRobin, Sticky, CooperativeSticky), group coordinator, heartbeats/session timeout, and how Kafka detects a dead consumer.
