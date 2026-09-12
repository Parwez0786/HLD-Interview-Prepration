# Day 5 — Kafka Consumers

Today the main thing you should understand is:

> Kafka Consumer reads messages using `poll()`, processes them, and commits the offset so Kafka knows how much has been successfully consumed.

The complete flow is:

```text
Kafka Broker
     ↓
Consumer Group
     ↓
Consumer
     ↓
poll()
     ↓
ConsumerRecords
     ↓
Business Logic
     ↓
Commit Offset
```

---

## Table of Contents

- [1. Consumer Architecture](#1-consumer-architecture)
- [2. Consumer Group](#2-consumer-group)
- [3. Important Consumer Rule](#3-important-consumer-rule)
- [4. poll()](#4-poll)
- [5. What Does poll() Return?](#5-what-does-poll-return)
- [6. Fetching Records](#6-fetching-records)
- [7. Offset](#7-offset)
- [8. Offset Is Per Partition](#8-offset-is-per-partition)
- [9. Committed Offset vs Processed Record](#9-very-important-committed-offset-vs-processed-record)
- [10. Commit Offset](#10-commit-offset)
- [11. Auto Commit](#11-auto-commit)
- [12. Manual Commit](#12-manual-commit)
- [13. commitSync()](#13-commitsync)
- [14. commitAsync()](#14-commitasync)
- [15. Sync vs Async](#15-sync-vs-async)
- [16. Complete Consumer Flow](#16-complete-consumer-flow)
- [17. SIP Notification Example](#17-real-example--your-sip-notification-system)
- [18. What Happens If Consumer Crashes?](#18-what-happens-if-consumer-crashes)
- [19. Why Duplicate Processing Can Happen](#19-why-duplicate-processing-can-happen)
- [20. Consumer Lag](#20-consumer-lag)
- [21. Consumer Rebalancing](#21-consumer-rebalancing)
- [22. Same Group, Same Partition](#22-interview-question-why-cant-two-consumers-in-same-group-consume-same-partition)
- [23. Most Important Interview Questions](#23-most-important-interview-questions)
- [24. One Line to Remember](#24-one-line-to-remember)

---

## 1. Consumer Architecture

A Kafka consumer is an application that reads messages from Kafka topics.

For example, suppose your Paytm Money notification system has:

**Topic:** `sip.pdn.debit.events`

```text
Partition 0 → Event 1, Event 2, Event 3
Partition 1 → Event 4, Event 5
Partition 2 → Event 6, Event 7
```

Your Java application starts Kafka consumers:

```text
Kafka Cluster
     |
     +---- Partition 0
     +---- Partition 1
     +---- Partition 2
              |
         Consumer Group
              |
       +------+------+
       |             |
   Consumer 1    Consumer 2
```

Each consumer continuously calls `consumer.poll()` to fetch available records.

---

## 2. Consumer Group

A consumer group is a collection of consumers working together to consume a topic.

Example:

```text
Topic: sip.pdn.debit.events

P0
P1
P2
P3

Consumer group: notification-group

Consumer 1 → P0
Consumer 2 → P1
Consumer 3 → P2
Consumer 4 → P3
```

The important rule is:

> One partition can be assigned to only one consumer within the same consumer group at a time.

### Why consumer groups?

They provide **parallel processing** and **scalability**.

Suppose 12 partitions, 6 consumers. Kafka can distribute approximately:

```text
Consumer 1 → P0, P1
Consumer 2 → P2, P3
Consumer 3 → P4, P5
Consumer 4 → P6, P7
Consumer 5 → P8, P9
Consumer 6 → P10, P11
```

This is exactly the type of setup you can discuss for your SIP notification pipeline.

---

## 3. Important Consumer Rule

Remember this for interviews:

> Number of consumers > Number of partitions → some consumers will remain idle.

Example:

```text
3 partitions
5 consumers

Only 3 consumers can actively consume:

C1 → P0
C2 → P1
C3 → P2
C4 → idle
C5 → idle
```

Therefore, **partitions determine the maximum parallelism** within a consumer group.

---

## 4. poll()

This is one of the most important concepts.

The consumer doesn't receive messages automatically.

It asks Kafka: "Give me the records that I need to consume." using:

```java
consumer.poll(Duration.ofMillis(100));
```

Example:

```java
while (true) {

    ConsumerRecords<String, String> records =
        consumer.poll(Duration.ofMillis(100));

    for (ConsumerRecord<String, String> record : records) {
        process(record);
    }
}
```

Think of `poll()` as the consumer saying: **"Kafka, give me the next batch of messages."**

---

## 5. What Does poll() Return?

It returns `ConsumerRecords<K, V>`, which contains multiple `ConsumerRecord<K, V>`.

For example:

```text
poll()
   ↓
Record 1
Record 2
Record 3
Record 4
```

Each record contains information such as: topic, partition, offset, key, value, timestamp.

For example:

```text
Topic     = sip.pdn.debit.events
Partition = 3
Offset    = 10542
Key       = SIP123
Value     = debit event
```

---

## 6. Fetching Records

Kafka consumers don't normally fetch one message at a time.

They fetch records in **batches**.

For example:

```text
poll()
 ↓
100 records

Then:

process Record 1
process Record 2
...
process Record 100
```

Batching improves throughput because the consumer doesn't need a network request for every individual message.

---

## 7. Offset

This is extremely important for Kafka interviews.

An offset is basically the **position of a message inside a partition**.

Example:

```text
Partition 0

Offset
  0 → Message A
  1 → Message B
  2 → Message C
  3 → Message D
  4 → Message E
```

Suppose the consumer has successfully processed 0, 1, 2.

The consumer needs to remember where it should continue.

Kafka stores this progress using the **committed offset**.

---

## 8. Offset Is Per Partition

Offsets are **not global**.

They are maintained separately for every partition.

Example:

```text
Partition 0 → offset 100
Partition 1 → offset 250
Partition 2 → offset 73
```

So you can have:

```text
P0 → consumed till 100
P1 → consumed till 250
P2 → consumed till 73
```

---

## 9. Very Important: Committed Offset vs Processed Record

Suppose:

```text
Offset 100 → Message A
Offset 101 → Message B
Offset 102 → Message C
```

Consumer polls all three:

```text
poll()
 ↓
100, 101, 102
```

It processes:

```text
100 ✅
101 ✅
102 ❌
```

If it commits offset 102 incorrectly, after restart it may **skip** message 102.

That's why **when you commit matters**.

---

## 10. Commit Offset

Committing means: **"Kafka, I have successfully processed records up to this point."**

Kafka stores committed offsets for the consumer group.

Conceptually:

```text
Consumer
   |
   | processed messages
   ↓
commit offset
   |
   ↓
Kafka
```

If the consumer crashes, it can restart from the committed offset.

---

## 11. Auto Commit

Kafka can automatically commit offsets.

Configuration: `enable.auto.commit=true`

Kafka periodically commits the offsets.

For example: `auto.commit.interval.ms=5000` means offsets are periodically committed.

### Problem

Suppose:

```text
poll()
 ↓
100 records
 ↓
processing
```

Auto commit happens while processing is still happening.

Then:

```text
Offset committed ✅
Processing fails ❌
Consumer crashes
```

After restart, Kafka may believe those messages were already consumed.

**Result:** messages can be **lost** from processing.

So for important business flows, we often prefer controlled / **manual commits**.

---

## 12. Manual Commit

Disable auto commit: `enable.auto.commit=false`

Then your application decides when to commit.

Example:

```java
while (true) {

    ConsumerRecords<String, String> records =
        consumer.poll(Duration.ofMillis(100));

    for (ConsumerRecord<String, String> record : records) {
        process(record);
    }

    consumer.commitSync();
}
```

Flow:

```text
poll()
 ↓
process
 ↓
successful
 ↓
commit
```

This gives you more control.

---

## 13. commitSync()

```java
consumer.commitSync();
```

means: commit the offsets **synchronously** and wait until Kafka confirms the commit.

Flow:

```text
Consumer
   |
   | commitSync()
   ↓
Kafka
   |
   | confirmation
   ↓
Consumer continues
```

**Advantage:** reliable and simple.

**Disadvantage:** it blocks while waiting for the commit response. Therefore, it can reduce throughput.

---

## 14. commitAsync()

```java
consumer.commitAsync();
```

means: send the commit request **asynchronously** and don't wait for the response before continuing.

Flow:

```text
Consumer
   |
   | commitAsync()
   +----------------→ Kafka
   |
   ↓
continue processing
```

**Advantage:** better performance because it doesn't block.

**Disadvantage:** failure handling is more complicated.

For example:

```text
commitAsync()
     ↓
commit fails
```

You need to handle the failure appropriately.

---

## 15. Sync vs Async

| | `commitSync()` | `commitAsync()` |
| --- | --- | --- |
| Blocking | Yes | No |
| Waits for response | Yes | No |
| Performance | Lower | Better |
| Simplicity | Easier | More complex |
| Failure handling | Simpler | More complex |

A common practical pattern is:

```text
During normal processing
        ↓
commitAsync()

During shutdown/rebalance
        ↓
commitSync()
```

The idea is to get async performance during normal operation while using a final synchronous commit when you need stronger assurance.

---

## 16. Complete Consumer Flow

Now combine everything.

```text
             Kafka Broker
                  |
                  ↓
          Consumer Group
                  |
                  ↓
             Consumer
                  |
                  ↓
               poll()
                  |
                  ↓
          ConsumerRecords
                  |
                  ↓
          Business Logic
                  |
            ┌─────┴─────┐
            ↓           ↓
         Success      Failure
            ↓           ↓
        Commit       Retry/DLQ
         Offset
```

---

## 17. Real Example — Your SIP Notification System

Suppose:

**Topic:** `sip.pdn.debit.events`

Event:

```json
{
  "userId": 123,
  "sipId": 456,
  "amount": 5000,
  "triggerType": "PDN_SUCCESS"
}
```

Consumer:

```java
while (true) {

    ConsumerRecords<String, SipEvent> records =
        consumer.poll(Duration.ofMillis(100));

    for (ConsumerRecord<String, SipEvent> record : records) {

        SipEvent event = record.value();

        sendNotification(event);
    }

    consumer.commitSync();
}
```

The actual flow:

```text
Kafka
  ↓
sip.pdn.debit.events
  ↓
poll()
  ↓
ConsumerRecords
  ↓
SipEvent
  ↓
sendNotification()
  ↓
success
  ↓
commit offset
```

---

## 18. What Happens If Consumer Crashes?

Suppose:

```text
Offset 100 → processed ✅
Offset 101 → processed ✅
Offset 102 → processing
```

Consumer crashes before committing 102.

After restart:

```text
Kafka
 ↓
last committed offset
 ↓
Consumer starts again
 ↓
Offset 102 processed again
```

So Kafka may deliver **102 again**.

This is why Kafka consumers are commonly designed to handle **duplicate processing / idempotency**.

---

## 19. Why Duplicate Processing Can Happen

Imagine:

```text
process message
     ↓
success
     ↓
consumer crashes
     ↓
commit didn't happen
```

After restart:

```text
same message
     ↓
processed again
```

For a notification system, this could potentially send **two notifications**.

Therefore, we can use an idempotency mechanism such as:

```text
eventId
   ↓
Redis / DB
   ↓
already processed?
```

Example:

```text
eventId = ABC123

if alreadyProcessed(ABC123):
       skip
else:
       process
       markProcessed(ABC123)
```

---

## 20. Consumer Lag

Another very important interview concept.

Suppose Kafka has latest offset = 1000, but consumer has processed offset = 800.

Then:

```text
Consumer Lag = 1000 - 800
             = 200
```

Conceptually:

```text
Kafka
Latest
  |
  | 200 messages
  |
Consumer
```

High lag means the consumer is **falling behind**.

Possible reasons:

- Business logic is slow
- Too few consumers
- Too few partitions
- Database is slow
- External API is slow
- Consumer has crashed
- Traffic suddenly increased

---

## 21. Consumer Rebalancing

Suppose you have:

```text
P0 P1 P2 P3

C1 C2
```

Maybe:

```text
C1 → P0 P1
C2 → P2 P3
```

Now C2 crashes.

Kafka detects that the consumer has left the group and redistributes partitions:

```text
C1 → P0 P1 P2 P3
```

This is called **rebalance**.

When a new consumer joins, partitions can also be redistributed.

---

## 22. Interview Question: Why Can't Two Consumers in Same Group Consume Same Partition?

Because Kafka uses consumer groups for parallelism.

Within a group:

```text
Partition → one active consumer
```

This prevents two consumers from normally processing the same partition simultaneously and allows Kafka to distribute work.

But:

```text
Group A → P0
Group B → P0
```

is perfectly valid.

Both groups can independently consume the same partition.

This is how Kafka supports multiple independent applications consuming the same events.

---

## 23. Most Important Interview Questions

### Q1. What is poll()?

> `poll()` is the API through which a Kafka consumer fetches records from the broker. It returns a batch of records that the consumer can process.

### Q2. What is an offset?

An offset is the position of a record within a Kafka partition. Consumers use offsets to track their progress.

### Q3. Why do we commit offsets?

We commit offsets so Kafka knows the consumer's progress and can resume from the appropriate position after a restart or failure.

### Q4. Auto commit vs manual commit?

With auto commit, Kafka periodically commits offsets automatically. With manual commit, the application decides when to commit, giving better control over when a message is considered successfully processed.

### Q5. commitSync() vs commitAsync()?

`commitSync()` waits for the broker's response, so it is simpler but blocking. `commitAsync()` doesn't block, so it can provide better throughput, but failure handling is more complex.

### Q6. What happens if processing succeeds but offset commit fails?

Suppose:

```text
process ✅
commit ❌
```

After restart, Kafka may deliver the message again.

Therefore: **duplicate processing can happen**.

We handle this using **idempotent business logic**.

### Q7. What happens if offset is committed before processing?

```text
commit ✅
process ❌
```

After restart, Kafka may start after that committed offset.

The message might therefore not be processed again, which can lead to **message loss** from the application's processing perspective.

So generally:

```text
process successfully
       ↓
commit offset
```

is the safer basic pattern when using **at-least-once** processing.

---

## 24. One Line to Remember

For your interview, remember this exact mental model:

```text
poll()
  ↓
fetch records
  ↓
process records
  ↓
processing successful?
  ↓
YES
  ↓
commit offset
```

And the failure case:

```text
poll()
  ↓
process
  ↓
crash before commit
  ↓
restart
  ↓
same record may be processed again
```

That is the core of Kafka consumer behavior.

### Day 5 interview summary

```text
Consumer
   ↓
Consumer Group
   ↓
Partition assignment
   ↓
poll()
   ↓
Records
   ↓
Business Logic
   ↓
Commit Offset
   ↓
Kafka remembers progress
```

The 4 concepts you absolutely must be able to explain without notes are:

1. Consumer group + partition assignment
2. `poll()` and fetching batches
3. Offset + commit
4. Why failure before commit causes duplicate processing
