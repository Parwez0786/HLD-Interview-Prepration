# Day 11 — Kafka Replication

Replication is one of the most important Kafka concepts for fault tolerance.

The basic idea is:

> Kafka keeps multiple copies of each partition on different brokers so that if one broker fails, another broker can continue serving the data.

---

## Table of Contents

- [1. Why Do We Need Replication?](#1-why-do-we-need-replication)
- [2. What is Replication Factor?](#2-what-is-replication-factor)
- [3. Leader and Followers](#3-leader-and-followers)
- [4. What Does the Leader Do?](#4-what-does-the-leader-do)
- [5. What Do Followers Do?](#5-what-do-followers-do)
- [6. How Does Replica Synchronization Work?](#6-how-does-replica-synchronization-work)
- [7. What is ISR?](#7-what-is-isr)
- [8. Example of ISR](#8-example-of-isr)
- [9. What is an Out-of-Sync Replica?](#9-what-is-an-out-of-sync-replica)
- [10. ISR vs Replicas](#10-isr-vs-replicas)
- [11. What Happens When the Leader Fails?](#11-what-happens-when-the-leader-fails)
- [12. Why Does Kafka Prefer ISR for Leader Election?](#12-why-does-kafka-prefer-isr-for-leader-election)
- [13. Example with Offsets](#13-example-with-offsets)
- [14. What Happens When a Follower Comes Back?](#14-what-happens-when-a-follower-comes-back)
- [15. What is Fault Tolerance?](#15-what-is-fault-tolerance)
- [16. RF = 1 vs RF = 3](#16-rf--1-vs-rf--3)
- [17. What Does RF = 3 NOT Mean?](#17-what-does-rf--3-not-mean)
- [18. Replication vs Partitioning](#18-replication-vs-partitioning)
- [19. How Replication Relates to acks=all](#19-how-replication-relates-to-acksall)
- [20. Real-World Example](#20-real-world-example)
- [21. Important Terms Together](#21-important-terms-together)
- [22. Interview Answer](#22-interview-answer--explain-kafka-replication)
- [23. Questions You Should Be Ready For](#23-questions-you-should-be-ready-for)

---

## 1. Why Do We Need Replication?

Imagine we have a Kafka topic `orders` with one partition `P0`.

Suppose P0 exists only on Broker 1:

```text
Broker 1
   |
   └── P0
```

Producer sends Order 101, Order 102, Order 103.

If Broker 1 crashes:

```text
Broker 1 ❌
   |
   └── P0 ❌
```

Now the partition is unavailable.

Potentially:

- Producers cannot write
- Consumers cannot read
- The system is down

Kafka solves this using **replication**.

---

## 2. What is Replication Factor?

Replication Factor (RF) tells us: **how many copies of each partition Kafka maintains**.

For example, Replication Factor = 3 means Kafka maintains 3 replicas of every partition.

For P0:

```text
P0

Broker 1 → Replica
Broker 2 → Replica
Broker 3 → Replica
```

These are three copies of the same partition.

**Important:** Replication factor is **per partition**, not per topic.

For example:

```text
Topic: orders

P0 → RF 3
P1 → RF 3
P2 → RF 3
```

Each partition has 3 replicas.

---

## 3. Leader and Followers

Kafka doesn't allow all replicas to accept writes independently.

For each partition:

- One replica = **Leader**
- Other replicas = **Followers**

Example:

```text
Partition P0

Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

So:

```text
              P0
               |
       ┌───────┼───────┐
       ↓       ↓       ↓
    Broker1 Broker2 Broker3
     Leader  Follower Follower
```

---

## 4. What Does the Leader Do?

The leader is responsible for handling client requests for that partition.

Producer:

```text
Producer
   |
   | Order 101
   ↓
Broker 1
Leader P0
```

The producer sends the record to the leader.

Similarly, consumers normally fetch records from the partition leader.

So:

```text
Producer
    |
    ↓
Leader
    |
    ↓
Followers replicate the data
```

---

## 5. What Do Followers Do?

Followers maintain copies of the leader's data.

Suppose:

```text
P0

Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Producer sends Order 101 to Broker 1.

Broker 1 writes it:

```text
Broker 1
P0:
Offset 0 → Order 101
```

Followers fetch the new data from the leader and replicate it:

```text
Broker 1 → Order 101
     ↓
Broker 2 → Order 101
Broker 3 → Order 101
```

Eventually:

```text
Broker 1: 101
Broker 2: 101
Broker 3: 101
```

All three replicas are synchronized.

---

## 6. How Does Replica Synchronization Work?

This is an important interview concept.

Followers don't usually receive data by the leader "pushing" every message to them.

Instead, followers **fetch** records from the leader.

Conceptually:

```text
Leader
P0:
0 → Order A
1 → Order B
2 → Order C
3 → Order D

Follower asks:

"Give me records after offset 2."

Leader responds:

3 → Order D
```

Follower writes it locally.

So:

```text
Follower
   |
   | Fetch records
   ↓
Leader
```

The follower continuously fetches new records.

---

## 7. What is ISR?

**ISR = In-Sync Replicas**

This is extremely important.

ISR is the set of replicas that are considered **sufficiently caught up** with the leader.

Example:

```text
P0

Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

If all three are synchronized:

```text
ISR = {Broker 1, Broker 2, Broker 3}
```

**Remember:** The leader itself is also part of ISR.

---

## 8. Example of ISR

Suppose:

```text
Leader:
Offset 0 1 2 3 4 5

Follower 1:
Offset 0 1 2 3 4 5

Follower 2:
Offset 0 1 2 3 4 5
```

Everything is caught up.

Therefore: ISR = Broker 1, Broker 2, Broker 3.

Now suppose Broker 3 becomes slow:

```text
Broker 1:
0 1 2 3 4 5 6 7 8

Broker 2:
0 1 2 3 4 5 6 7 8

Broker 3:
0 1 2 3
```

Broker 3 is far behind.

Kafka may remove Broker 3 from ISR.

Now:

```text
ISR = {Broker 1, Broker 2}
```

Broker 3 is an **out-of-sync replica**.

---

## 9. What is an Out-of-Sync Replica?

An out-of-sync replica is a replica that has fallen too far behind the leader.

Example:

```text
Leader P0
0 1 2 3 4 5 6 7 8 9

Follower 1
0 1 2 3 4 5 6 7 8 9

Follower 2
0 1 2 3
```

Follower 2 is behind.

Therefore: Broker 2 → Out of Sync.

It can happen because:

- Broker is overloaded
- Disk is slow
- Network problems
- Broker temporarily stops
- Kafka process pauses/restarts
- Hardware problems

---

## 10. ISR vs Replicas

This distinction is often asked in interviews.

Suppose RF = 3:

```text
Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Total replicas: 3.

But Broker 3 becomes too slow:

```text
Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Out of sync
```

Then:

```text
Total replicas = 3
ISR = 2
```

So:

> Replication factor tells us how many replicas exist. ISR tells us how many replicas are currently in sync.

---

## 11. What Happens When the Leader Fails?

This is where replication becomes extremely useful.

Initially:

```text
P0

Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Suppose Broker 1 crashes:

```text
Broker 1 ❌
```

Kafka needs a new leader.

It chooses an eligible replica, typically from the **ISR**.

For example:

```text
Broker 2 → New Leader
Broker 3 → Follower
```

Now:

```text
P0

Broker 2 → Leader
Broker 3 → Follower
Broker 1 → Down
```

Clients discover the new leader and continue operating.

This is called **leader election**.

---

## 12. Why Does Kafka Prefer ISR for Leader Election?

Imagine:

```text
Broker 1 → Leader
Broker 2 → ISR
Broker 3 → ISR

Broker 4 → Very far behind
```

If Broker 1 fails, we don't want Broker 4 to become leader because it might be missing recent records.

Instead Kafka prefers an in-sync replica.

So:

```text
Leader fails
      ↓
Choose eligible ISR replica
      ↓
New Leader
```

This reduces the chance of losing acknowledged data.

---

## 13. Example with Offsets

Suppose:

```text
Broker 1 → Leader

0 → A
1 → B
2 → C
3 → D
4 → E

Broker 2:

0 → A
1 → B
2 → C
3 → D
4 → E

Broker 3:

0 → A
1 → B
2 → C
```

Here:

- Broker 2 → In Sync
- Broker 3 → Behind

If Broker 1 crashes, Broker 2 is a good candidate for leader because it has all the records.

Broker 3 is not caught up.

---

## 14. What Happens When a Follower Comes Back?

Suppose:

```text
Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Broker 3 crashes: Broker 3 ❌.

Later it comes back.

It doesn't immediately become ISR.

It first needs to **catch up**.

For example:

```text
Leader:
0 1 2 3 4 5 6 7 8

Broker 3:
0 1 2
```

Broker 3 fetches 3 4 5 6 7 8.

Eventually:

```text
Leader:
0 1 2 3 4 5 6 7 8

Broker 3:
0 1 2 3 4 5 6 7 8
```

Now it can rejoin ISR.

```text
ISR = Broker 1, Broker 2, Broker 3
```

---

## 15. What is Fault Tolerance?

Fault tolerance means: the system can continue working even when some components fail.

Suppose RF = 3:

```text
Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Broker 1 crashes: Broker 1 ❌.

```text
Broker 2 → New Leader
Broker 3 → Follower
```

The partition can continue operating.

That's fault tolerance.

---

## 16. RF = 1 vs RF = 3

### RF = 1

```text
Broker 1
   |
   └── P0
```

No backup.

Broker fails: P0 ❌.

Bad for production.

### RF = 3

```text
Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Broker 1 fails: Broker 1 ❌.

```text
Broker 2 → New Leader
Broker 3 → Follower
```

Much more fault tolerant.

---

## 17. What Does RF = 3 NOT Mean?

A common misunderstanding:

**RF = 3 does NOT mean three leaders.**

It means:

```text
1 Leader
2 Followers
```

for that partition.

```text
P0
│
├── Broker 1 → Leader
├── Broker 2 → Follower
└── Broker 3 → Follower
```

Only **one leader** exists for a partition at a time.

---

## 18. Replication vs Partitioning

Don't confuse these two.

### Partitioning

Used mainly for: parallelism, scalability, higher throughput.

Example:

```text
Topic orders

P0
P1
P2
P3
```

### Replication

Used mainly for: fault tolerance, availability, data redundancy.

Example:

```text
P0

Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

You normally use **both**.

Example: Topic `orders`, Partitions: 4, Replication Factor: 3.

Conceptually:

```text
             Topic orders
                  |
       ┌──────────┼──────────┐
       ↓          ↓          ↓
      P0         P1         P2 ... P3
      │
      ├─ B1 Leader
      ├─ B2 Follower
      └─ B3 Follower
```

---

## 19. How Replication Relates to acks=all

This connects directly to what you learned on Day 4.

Suppose:

```text
P0

Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Producer sends Order 101 with `acks=all`.

Kafka waits according to the required ISR acknowledgment condition before considering the write successful.

The important configuration here is `min.insync.replicas`.

Suppose:

```text
RF = 3
min.insync.replicas = 2
acks = all
```

Then Kafka requires at least 2 ISR replicas for the write to succeed.

For example:

```text
Broker 1 → Leader ✓
Broker 2 → Follower ✓
Broker 3 → Out of sync

ISR:

Broker 1
Broker 2
```

There are 2 ISR replicas, so writes can continue.

But if:

```text
Broker 1 → Leader
Broker 2 → Out of sync
Broker 3 → Out of sync

then:

ISR = 1

while:

min.insync.replicas = 2
```

Kafka can **reject the write** rather than accepting a write when the required replication safety level isn't available.

This is one reason production systems commonly combine:

```text
RF = 3
acks = all
min.insync.replicas = 2
```

---

## 20. Real-World Example

Imagine your Paytm-style SIP notification system: `sip.pdn.debit.events`.

Suppose: Partitions = 12, Replication Factor = 3.

For one partition:

```text
P0

Broker 1 → Leader
Broker 2 → Follower
Broker 3 → Follower
```

Producer sends User 101, SIP 555, Amount ₹5,000 to P0.

Flow:

```text
Producer
   |
   ↓
Broker 1
Leader
   |
   ├────────→ Broker 2
   |
   └────────→ Broker 3
```

Broker 2 and Broker 3 replicate the record.

If Broker 1 crashes: Broker 1 ❌.

```text
Broker 2 → New Leader
Broker 3 → Follower
```

Consumers can continue consuming from the new leader.

That's the practical reason replication matters.

---

## 21. Important Terms Together

You should be able to explain this entire picture:

```text
                    Partition P0
                         |
        ┌────────────────┼────────────────┐
        ↓                ↓                ↓
     Broker 1         Broker 2         Broker 3
      Leader          Follower          Follower
        │                │                │
        └────────────────┼────────────────┘
                         │
                        ISR
```

If Broker 3 becomes slow:

```text
Broker 1 → Leader       ✓ ISR
Broker 2 → Follower     ✓ ISR
Broker 3 → Follower     ❌ Out of sync
```

If Broker 1 crashes:

```text
Broker 1 ❌

Broker 2 → New Leader
Broker 3 → Follower
```

Broker 1 comes back:

```text
Broker 1 → Catch up
         ↓
     Rejoin ISR
```

---

## 22. Interview Answer — "Explain Kafka Replication"

You can answer in simple English:

> Kafka replication means maintaining multiple copies of each partition on different brokers for fault tolerance. Each partition has one leader replica and one or more follower replicas. Producers write to the leader, while followers continuously fetch and replicate the leader's data. Kafka maintains an ISR, which is the set of replicas that are sufficiently caught up with the leader. If the leader fails, Kafka can elect an eligible in-sync replica as the new leader. Once the failed broker comes back, its replica catches up with the leader and can rejoin the ISR. For example, with replication factor 3, one partition can have one leader and two followers, so the system can tolerate broker failures without immediately losing availability.

---

## 23. Questions You Should Be Ready For

For Day 11, interviewers can ask:

- What is replication factor?
- Why does Kafka need replication?
- What is the difference between leader and follower?
- What is ISR?
- Can a follower become a leader?
- What happens when the leader broker crashes?
- How does Kafka select a new leader?
- What is an out-of-sync replica?
- How does a follower synchronize with the leader?
- What happens when a failed broker comes back?
- Difference between replication factor and ISR?
- What is `min.insync.replicas`?
- How does `acks=all` work with replication?
- RF=3 means how many leaders?
- How many broker failures can RF=3 tolerate?

### One important rule to remember

```text
Replication Factor
        ↓
How many copies exist?

ISR
        ↓
How many copies are currently in sync?

Leader
        ↓
Where writes go

Follower
        ↓
Copies the leader

Leader failure
        ↓
Leader election
        ↓
ISR replica becomes leader
```

### Day 11 mental model

```text
                 Kafka Partition
                       P0
                        |
             ┌──────────┼──────────┐
             ↓          ↓          ↓
           B1           B2         B3
         Leader       Follower   Follower
             │          ↑          ↑
             └──── data ───────────┘
                       replication

ISR = B1 + B2 + B3

B3 becomes slow
        ↓
ISR = B1 + B2

B1 crashes
        ↓
B2 becomes Leader

B1 returns
        ↓
B1 catches up
        ↓
B1 joins ISR again
```

That is the core of Kafka replication.
