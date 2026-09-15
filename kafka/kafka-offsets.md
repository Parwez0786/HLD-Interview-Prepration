# Day 10 — Kafka Offsets

Offsets are one of the most important Kafka concepts for interviews. If you understand offsets properly, consumer groups, lag, retries, replay, and failure handling become much easier.

---

## Table of Contents

- [1. What is an Offset?](#1-what-is-an-offset)
- [2. Offset Per Partition](#2-offset-per-partition)
- [3. Why Doesn't Kafka Use a Global Offset?](#3-why-doesnt-kafka-use-a-global-offset)
- [4. Consumer Position](#4-consumer-position)
- [5. Committed Offset](#5-committed-offset)
- [6. Consumer Position vs Committed Offset](#6-consumer-position-vs-committed-offset)
- [7. Where Is the Committed Offset Stored?](#7-where-is-the-committed-offset-stored)
- [8. Same Topic, Different Consumer Groups](#8-same-topic-different-consumer-groups)
- [9. Log End Offset](#9-log-end-offset)
- [10. Consumer Lag](#10-consumer-lag)
- [11. Visualizing Lag](#11-visualizing-lag)
- [12. Why Does Consumer Lag Increase?](#12-why-does-consumer-lag-increase)
- [13. Offset Reset](#13-offset-reset)
- [14. earliest](#14-earliest)
- [15. latest](#15-latest)
- [16. none](#16-none)
- [17. earliest Doesn't Mean Always Start From 0](#17-very-important-earliest-doesnt-mean-always-start-from-0)
- [18. What If the Offset Was Deleted?](#18-what-if-the-offset-was-deleted)
- [19. Complete Example](#19-complete-example)
- [20. Offset Flow During Consumption](#20-offset-flow-during-consumption)
- [21. SIP Notification Example](#21-real-world-example--your-sip-notification-system)
- [22. Interview Questions You Must Know](#22-interview-questions-you-must-know)
- [23. The One Diagram to Remember](#23-the-one-diagram-to-remember)

---

## 1. What is an Offset?

An offset is the **position/number of a record inside a Kafka partition**.

For example:

```text
Partition 0

Offset:
  0   1   2   3   4   5   6   7
  ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓
 [A] [B] [C] [D] [E] [F] [G] [H]
                      ↑
                   Consumer
```

Here:

```text
A → offset 0
B → offset 1
C → offset 2
D → offset 3
E → offset 4
F → offset 5
G → offset 6
H → offset 7
```

Kafka uses this offset to identify where a consumer is in a partition.

### Important

An offset is **not globally unique**.

It is unique only within a partition.

---

## 2. Offset Per Partition

Suppose we have:

```text
Topic: orders

Partition 0:
0  1  2  3  4

Partition 1:
0  1  2  3  4

Partition 2:
0  1  2  3
```

Notice that offset 0 exists in all three partitions.

So `offset = 0` doesn't uniquely identify a Kafka record.

You need: **partition + offset**.

For example: Partition 1 + Offset 3 uniquely identifies a position.

### Interview answer

> Kafka offsets are unique within a partition, not across the entire topic.

---

## 3. Why Doesn't Kafka Use a Global Offset?

Because Kafka partitions are **independent logs**.

Think of them as separate files:

```text
Partition 0 → 0 1 2 3 4
Partition 1 → 0 1 2 3 4
Partition 2 → 0 1 2 3 4
```

Each partition maintains its own sequence.

This allows Kafka to scale horizontally.

---

## 4. Consumer Position

The consumer position is the **next offset that the consumer will read**.

Suppose:

```text
Partition 0

0  1  2  3  4  5  6  7
A  B  C  D  E  F  G  H
         ↑
      Consumer
```

If the consumer has already processed 0, 1, 2, then its current position can be **3**.

Meaning: "The next record I want to read is offset 3."

So don't confuse:

```text
processed offset = 2

with:

consumer position = 3
```

---

## 5. Committed Offset

Now comes a very important concept.

Kafka consumers need to remember their progress.

Suppose:

```text
Partition 0

0  1  2  3  4  5  6
A  B  C  D  E  F  G

         ↑
      Consumer
```

Consumer has processed 0, 1, 2.

It can commit offset **3**.

**Why 3?** Because: "I have successfully processed everything before offset 3. Start from 3 if I restart."

So:

```text
Committed offset = 3
```

means the consumer should resume from 3.

---

## 6. Consumer Position vs Committed Offset

This is a very common interview question.

Suppose:

```text
Records:

0  1  2  3  4  5  6
         ↑        ↑
       commit   position
         3         5
```

The consumer may have fetched records up to 5, but only committed up to 3.

So:

```text
Consumer Position = 5
Committed Offset = 3
```

If the consumer crashes:

- Consumer Position → **lost**
- Committed Offset → **stored in Kafka**

After restart, it can resume from offset 3.

This can cause records 3 and 4 to be processed again if they were fetched/processed but not committed.

That's why Kafka consumers can provide **at-least-once** processing.

---

## 7. Where Is the Committed Offset Stored?

Kafka stores consumer group offsets in an internal Kafka topic: `__consumer_offsets`.

Conceptually:

```text
Consumer
   |
   | commit offset
   ↓
Kafka
   |
   ↓
__consumer_offsets
```

The committed offset is associated with:

```text
Consumer Group
        +
Topic
        +
Partition
```

For example:

```text
group = payment-service
topic = orders
partition = 2
committed offset = 1050
```

Another consumer group can have a completely different offset:

```text
group = notification-service
topic = orders
partition = 2
committed offset = 800
```

That's extremely important.

---

## 8. Same Topic, Different Consumer Groups

Suppose:

```text
orders topic

P0:
0 1 2 3 4 5 6 7 8 9

Consumer Group A:

payment-service
              ↑
              6

Consumer Group B:

notification-service
                    ↑
                    3
```

Both groups can consume the same messages independently.

Their offsets are independent.

```text
Payment group       → offset 6
Notification group  → offset 3
```

This is one of Kafka's biggest advantages.

---

## 9. Log End Offset

The Log End Offset (LEO) is essentially the offset of the **next record that would be appended** to a partition.

Example:

```text
Partition:

0  1  2  3  4  5  6
A  B  C  D  E  F  G
```

The next record will receive offset 7.

Therefore: **LEO = 7**.

There are currently 7 records with offsets 0 → 6.

### Remember

LEO points to the **next available offset**, not the last existing offset.

---

## 10. Consumer Lag

Consumer lag tells us: how far behind the consumer is from the latest available data.

Simplified formula:

```text
Consumer Lag = Log End Offset - Consumer Position
```

Example:

```text
Log End Offset = 1000
Consumer Position = 850

Therefore:

Lag = 1000 - 850
    = 150
```

The consumer is behind by approximately 150 records.

---

## 11. Visualizing Lag

Imagine:

```text
Partition 0

0 1 2 3 4 5 6 7 8 9 10 11 12
            ↑                    ↑
         Consumer               LEO
```

Suppose:

```text
Consumer position = 5
LEO = 13

Then:

Lag = 13 - 5
    = 8
```

So there are approximately 8 records waiting to be consumed.

---

## 12. Why Does Consumer Lag Increase?

Imagine producers are producing 1000 messages/sec, but your consumer processes 500 messages/sec.

Then:

```text
Incoming:
████████████████████ 1000/sec

Processing:
██████████           500/sec
```

The backlog keeps increasing. Therefore: **Consumer Lag ↑**.

Possible solutions:

### Option 1 — Add consumers

```text
Consumer 1 → P0
Consumer 2 → P1
Consumer 3 → P2
Consumer 4 → P3
```

### Option 2 — Increase partitions

If you don't have enough partitions, adding consumers won't help.

For example: 4 partitions, 10 consumers.

Only 4 consumers can actively consume partitions in that group.

```text
C1 → P0
C2 → P1
C3 → P2
C4 → P3

C5 → idle
C6 → idle
...
```

So partition count limits consumer parallelism.

---

## 13. Offset Reset

What happens if Kafka doesn't have a valid committed offset for a consumer group?

Kafka needs to decide: "Where should I start consuming?"

This is controlled by `auto.offset.reset`.

Common values: `earliest`, `latest`, `none`.

---

## 14. earliest

Start from the earliest available offset.

Example:

```text
Partition

0 1 2 3 4 5 6 7
↑
Consumer
```

If there is no committed offset, `auto.offset.reset=earliest` consumer starts from 0.

Useful when you want to process existing data.

Example: New analytics service — you might want all historical events.

---

## 15. latest

Start from the latest offset.

Example:

```text
Existing:

0 1 2 3 4 5 6 7
              ↑
             LEO
```

Consumer starts from the end and generally waits for newly arriving records.

Useful when you don't care about historical records and only want new events.

---

## 16. none

If there is no valid committed offset, `auto.offset.reset=none` — Kafka throws an error instead of automatically choosing a starting position.

This is useful when silently skipping or replaying data would be unacceptable.

---

## 17. Very Important: earliest Doesn't Mean "Always Start From 0"

This is a common misconception.

Suppose:

```text
Committed offset = 500
auto.offset.reset = earliest
```

Kafka will **not** start from 0.

It starts from 500, because a valid committed offset exists.

`auto.offset.reset` matters when Kafka doesn't have a valid committed offset, or when the committed offset is no longer available because of retention.

---

## 18. What If the Offset Was Deleted?

Kafka has retention.

Suppose consumer committed offset = 100, but Kafka's retention deleted records 0 → 499.

Now the earliest available offset might be 500.

The consumer asks for offset 100, but Kafka no longer has it.

Then `auto.offset.reset=earliest` means: start from **500**, not 100.

This is an important real-world scenario.

---

## 19. Complete Example

Let's combine everything.

```text
Topic: orders

Partition 0

0  1  2  3  4  5  6  7  8  9
            ↑              ↑
       Consumer Position   LEO
```

Suppose:

```text
Consumer position = 3
LEO = 10

Then:

Lag = 10 - 3
    = 7
```

Suppose consumer processes 3, 4, 5, 6 and commits 7.

Now:

```text
Consumer Position = 7
Committed Offset  = 7
LEO                = 10
Lag                = 3
```

Then consumer crashes.

After restart: start from offset 7, because committed offset = 7.

---

## 20. Offset Flow During Consumption

Think about the lifecycle like this:

```text
Kafka Partition
      |
      | records
      ↓
Consumer poll()
      |
      ↓
Consumer Position
      |
      ↓
Business Processing
      |
      ↓
Successful processing
      |
      ↓
Commit Offset
      |
      ↓
__consumer_offsets
```

If crash happens **before** commit: record may be processed again.

If crash happens **after** commit: record normally won't be consumed again.

This is why the relationship between processing → commit is extremely important.

---

## 21. Real-World Example — Your SIP Notification System

Imagine your Kafka topic `sip.pdn.debit.events` with 12 partitions.

Consumer group: `notification-group`.

Suppose:

```text
Partition 3

Offset:
100 101 102 103 104 105 106
                ↑
             Consumer
```

```text
Consumer position: 103
LEO: 106

Therefore:

Lag = 106 - 103
    = 3
```

There are records waiting behind the consumer.

If the consumer successfully processes 103, 104, 105, it can commit 106.

Now:

```text
Consumer Position = 106
Committed Offset  = 106
LEO               = 106
Lag               = 0
```

Your notification consumer has caught up.

---

## 22. Interview Questions You Must Know

### Q1. Is Kafka offset globally unique?

No. It is unique only within a partition.

### Q2. What uniquely identifies a Kafka record?

Conceptually: **Topic + Partition + Offset**.

### Q3. What is consumer position?

The next offset the consumer intends to read.

### Q4. What is committed offset?

The offset stored for a consumer group indicating where the consumer should resume after restart.

### Q5. Where are committed offsets stored?

`__consumer_offsets`

### Q6. What is consumer lag?

Difference between the latest available position and the consumer's current position.

Simplified: `Lag = LEO - Consumer Position`

### Q7. What is LEO?

The next offset that will be assigned when a new record is appended.

### Q8. What happens when a consumer crashes before committing?

Previously processed but uncommitted records may be processed again.

This is why duplicate processing is possible with at-least-once processing.

### Q9. What does auto.offset.reset=earliest do?

If there is no valid committed offset, start from the earliest available offset.

### Q10. Does earliest always mean offset 0?

No.

Kafka may have already deleted older records because of retention.

It means the **earliest currently available offset**.

---

## 23. The One Diagram to Remember

```text
                    Kafka Partition

     0   1   2   3   4   5   6   7   8   9
     |   |   |   |   |   |   |   |   |   |
     └────────────────┘
         Processed

                 ↑
          Committed Offset
                 5

                     ↑
              Consumer Position
                     7

                             ↑
                       Log End Offset
                             10
```

Think of the three numbers separately:

```text
Committed Offset
      ↓
Where I can safely resume

Consumer Position
      ↓
Where I am currently reading

Log End Offset
      ↓
Where Kafka currently ends
```

And:

```text
Lag = Log End Offset - Consumer Position
```

### Mental model

| Term | Meaning |
| --- | --- |
| Offset | Address inside a partition |
| Position | Where consumer currently is |
| Committed offset | Where consumer can safely resume |
| LEO | Where Kafka currently ends |
| Lag | How far consumer is behind |
| Offset reset | Where to start when no valid position exists |
