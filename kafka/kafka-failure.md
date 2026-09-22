# Day 20 — Kafka Failure Scenarios

One of the most important interview topics, because interviewers often move from “How does Kafka work?” to “What happens when something fails?”

Each scenario is explained as:

> **Failure → What happens → Why → How to handle → Interview answer**

---

## Table of Contents

- [1. Producer Crashes](#1-producer-crashes)
- [2. Consumer Crashes](#2-consumer-crashes)
- [3. Broker Crashes](#3-broker-crashes)
- [4. Leader Crashes](#4-leader-crashes)
- [5. Network Failure](#5-network-failure)
- [6. Consumer Processing Failure](#6-consumer-processing-failure)
- [7. DB Failure](#7-db-failure)
- [8. Kafka Failure](#8-kafka-failure)
- [9. Duplicate Event](#9-duplicate-event)
- [10. Lost Event](#10-lost-event)
- [The Big Interview Scenario](#the-big-interview-scenario)
- [Failure Matrix](#failure-matrix--memorize-this)
- [5 Questions You MUST Be Able to Answer](#5-questions-you-must-be-able-to-answer)
- [Paytm-Style Interview Answer](#your-paytm-style-interview-answer)
- [Day 20 Mental Model](#day-20-mental-model)

---

## 1. Producer Crashes

### Scenario

Producer sends an event:

```text
Application
   ↓
Kafka Producer
   ↓
Broker
   ↓
Topic Partition
```

Suppose the producer crashes while sending. There are two possibilities.

### Case A — Message never reached Kafka

```text
Producer
   X
Kafka
```

The message is lost from the producer's perspective.

### Case B — Kafka received it, but producer didn't receive the acknowledgement

```text
Producer → Kafka
             ↓
         Message stored

Producer ← ACK
      X
   crashed
```

The message exists in Kafka, but the producer doesn't know that it succeeded.

When the application restarts, it might retry. That can create a **duplicate event**.

### How do we handle it?

Use:

- `acks=all`
- `enable.idempotence=true`
- `retries=...`
- Idempotent producer

Kafka assigns sequence numbers to producer requests. If the same request is accidentally retried, Kafka can detect the duplicate.

```text
Producer
   ↓
Send Event #100
   ↓
Kafka stores it
   ↓
ACK lost
   ↓
Producer retries Event #100
   ↓
Kafka detects duplicate
```

Result: Event #100 stored once.

### Interview answer

> If a producer crashes before Kafka receives the event, the event may be lost. If Kafka receives the event but the producer doesn't receive the ACK, retrying can create duplicates. I would use acks=all, retries, and idempotent producer to provide reliable delivery and avoid duplicate writes caused by producer retries.

---

## 2. Consumer Crashes

This is extremely important.

Suppose:

```text
Partition 0

Offset:
100
101
102
103
```

Consumer reads: 100 → process, 101 → process, 102 → process.

Now consumer crashes.

The important question is: **When was the offset committed?**

### Case A — Commit before processing

```text
poll()
 ↓
commit offset
 ↓
process
 ↓
CRASH
```

Suppose offset 102 was committed, but processing didn't finish.

After restart, consumer starts from 103. Event 102 is skipped.

**Result:** lost processing.

### Case B — Commit after processing

```text
poll()
 ↓
process
 ↓
DB update
 ↓
commit offset
```

Consumer crashes here:

```text
DB update
    ↓
  CRASH
    ↓
commit never happens
```

After restart, consumer reads same event again. Now the DB operation may happen twice.

**Result:** duplicate processing.

This is why we usually prefer:

```text
Process
   ↓
Successful side effect
   ↓
Commit offset
```

and make the processing **idempotent**.

---

## 3. Broker Crashes

Suppose:

```text
Topic: orders

Partition 0

Leader → Broker 1
Follower → Broker 2
Follower → Broker 3
```

Broker 1 crashes. Kafka detects the failure.

One of the replicas becomes the new leader:

```text
Broker 1 ❌

Broker 2
  ↓
New Leader
```

Consumers and producers eventually discover the new leader through Kafka metadata.

### What happens to data?

If replication is configured properly (`Replication Factor = 3`), the data exists on Broker 1, Broker 2, Broker 3.

Therefore a single broker failure shouldn't cause data loss.

---

## 4. Leader Crashes

This is slightly different from general broker failure.

Suppose:

```text
Partition 0

Leader
Broker 1
   ↓
Follower
Broker 2
   ↓
Follower
Broker 3
```

Leader crashes. Kafka elects another replica:

```text
Broker 1 ❌

Broker 2
   ↓
New Leader
```

This is called **leader election**.

### What is ISR?

ISR = **In-Sync Replicas**.

Example:

```text
Leader: Broker 1
ISR:
Broker 1
Broker 2
Broker 3
```

If Broker 1 dies, Kafka preferably elects a replica from the ISR.

That's important because ISR replicas are considered sufficiently caught up.

---

## 5. Network Failure

Suppose:

```text
Producer
   |
   X
   |
Kafka
```

Network connection breaks.

The producer doesn't know whether Kafka received the message.

This creates the classic problem:

```text
Did Kafka receive it?
       ↓
      ???
```

Producer may retry.

Therefore:

```text
Network failure
      ↓
Retry
      ↓
Possible duplicate
```

Again: `enable.idempotence=true` helps prevent duplicates from producer retries.

---

## 6. Consumer Processing Failure

Suppose consumer receives:

```json
{
  "orderId": 101,
  "amount": 5000
}
```

Processing fails:

```text
Consumer
   ↓
process()
   ↓
Exception ❌
```

What should happen? **Don't immediately commit the offset.**

Otherwise:

```text
process ❌
 ↓
commit
 ↓
event skipped
```

Instead:

```text
process
   ↓
failure
   ↓
retry
```

For repeated failures:

```text
Kafka
 ↓
Consumer
 ↓
Retry
 ↓
Retry
 ↓
Retry
 ↓
DLT
```

**DLT** = Dead Letter Topic.

Example:

```text
orders
   ↓
Consumer
   ↓
processing failure
   ↓
retry 3 times
   ↓
orders.DLT
```

The DLT can contain: original topic, partition, offset, message, exception, timestamp.

---

## 7. DB Failure

This is one of the most important real-world scenarios.

Suppose:

```text
Kafka
 ↓
Consumer
 ↓
DB
```

Kafka event: Payment = ₹1000.

Consumer tries:

```sql
UPDATE account
SET balance = balance - 1000;
```

But DB is down.

Should we commit the Kafka offset? **No.**

If we commit:

```text
DB update ❌
 ↓
Kafka offset committed
 ↓
event won't be processed again
```

That can cause a lost business operation.

Instead:

```text
DB failure
   ↓
Don't commit
   ↓
Retry
```

After DB comes back:

```text
Kafka
 ↓
Consumer
 ↓
DB succeeds
 ↓
Commit offset
```

---

## 8. Kafka Failure

Suppose the entire Kafka cluster becomes unavailable.

Your application may be unable to produce events or consume events.

What should the application do? Depends on business requirements.

For producer:

```text
Application
 ↓
Kafka unavailable
 ↓
Retry with backoff
```

For critical systems, you might also use the **Outbox Pattern**.

```text
Application
    |
    +------ DB
    |        |
    |     Outbox table
    |
    +------ Kafka
```

Application first stores the business transaction + event in the same DB transaction.

Then an outbox publisher sends the event to Kafka.

This reduces the risk of: DB succeeded, Kafka failed — creating inconsistency.

---

## 9. Duplicate Event

This is extremely common.

Suppose event: `paymentId = P123`, `amount = ₹1000`.

Consumer receives it twice: `P123`, `P123`.

If your consumer simply does `INSERT INTO payments ...`, you might process the payment twice.

### Solution: Idempotency

Give every event a unique ID: `eventId = ABC123`.

Create a unique constraint: `event_id UNIQUE`.

Processing:

```text
Receive event
     ↓
Check eventId
     ↓
Already processed?
   /       \
 Yes       No
 ↓          ↓
Skip      Process
            ↓
        Save eventId
```

This is extremely useful in payment systems.

---

## 10. Lost Event

This is the most important question: **How can Kafka lose an event?**

Several things can cause apparent loss.

### Producer didn't successfully send

```text
Producer
   ↓
Kafka
   X
```

### Consumer commits before processing

```text
poll()
 ↓
commit
 ↓
process
 ↓
CRASH
```

Event remains in Kafka, but your application effectively skipped it.

### Kafka-side protection

Use:

- Replication Factor > 1
- `acks=all`
- `min.insync.replicas`

For example:

```properties
replication.factor=3
min.insync.replicas=2
acks=all
```

This means the producer requires acknowledgement from enough in-sync replicas before considering the write successful.

---

## The Big Interview Scenario

An interviewer may give you this:

```text
Kafka
  ↓
Consumer
  ↓
DB
```

And ask: **What happens if the consumer updates DB successfully but crashes before committing the Kafka offset?**

Answer:

```text
Kafka event
    ↓
Consumer
    ↓
DB update SUCCESS
    ↓
Consumer CRASH
    ↓
Offset NOT committed
```

After restart:

```text
Consumer
    ↓
Reads same event again
    ↓
DB operation happens again
```

Therefore: **at-least-once processing can produce duplicates**, so the consumer-side DB operation should be idempotent.

For example: `eventId = 123` — store it with a unique constraint.

---

## Failure Matrix — Memorize This

| Failure | Main Problem | Typical Solution |
| --- | --- | --- |
| Producer crash | Lost/duplicate send | Idempotent producer + retries |
| Consumer crash | Duplicate/lost processing | Commit after processing + idempotency |
| Broker crash | Availability | Replication |
| Leader crash | Partition unavailable temporarily | Leader election |
| Network failure | Unknown ACK status | Retry + idempotence |
| Consumer processing failure | Repeated failure | Retry + DLT |
| DB failure | Business operation incomplete | Don't commit; retry |
| Kafka failure | Messaging unavailable | Retry / Outbox |
| Duplicate event | Double processing | Idempotency + unique event ID |
| Lost event | Data/processing loss | `acks=all` + replication + correct offset handling |

---

## 5 Questions You MUST Be Able to Answer

**Q1. Why commit offset after processing?**

Because committing before processing can cause an event to be skipped if the consumer crashes.

**Q2. Why can duplicate events happen?**

Because processing can succeed while the offset commit fails.

```text
Process SUCCESS
      ↓
Crash
      ↓
Commit ❌
      ↓
Same event processed again
```

**Q3. How do you prevent duplicate processing?**

Use an idempotency key/event ID:

```text
eventId
   ↓
DB unique constraint
```

**Q4. What happens if the Kafka leader dies?**

Kafka elects another replica, preferably from the ISR, as the new leader.

**Q5. What happens if DB succeeds but Kafka offset commit fails?**

The event will be consumed again. Therefore the DB operation must be idempotent.

---

## Your Paytm-Style Interview Answer

For your notification pipeline, imagine:

```text
SIP Event
   ↓
Kafka
   ↓
Notification Consumer
   ↓
Notification Service
   ↓
DB
```

If interviewer asks: **What if your notification consumer crashes after sending the notification but before committing the Kafka offset?**

You can answer:

> "The event would be consumed again because the offset wasn't committed. That could result in duplicate notifications. I would make the notification operation idempotent using a unique event or notification ID. Before sending, we can check whether that event has already been processed, or maintain an idempotency record with a unique constraint. I would commit the Kafka offset only after successful processing."

That's a very strong SDE-1 Kafka answer.

---

## Day 20 Mental Model

Remember this one rule:

```text
Kafka reliability
       ↓
Don't lose events
       +
Don't process events incorrectly
       +
Handle retries
       +
Make consumers idempotent
```

And the most important pattern:

```text
poll()
  ↓
process()
  ↓
DB / external operation
  ↓
SUCCESS
  ↓
commit offset
```

But: because `process()` and Kafka offset commit are usually **not one atomic transaction**, duplicates are still possible → therefore **idempotency is essential**.
