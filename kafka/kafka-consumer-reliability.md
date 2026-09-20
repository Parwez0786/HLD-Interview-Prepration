# Day 17 — Kafka Consumer Reliability

The main goal today is to understand what happens when a Kafka consumer crashes at different points of message processing.

The most important idea:

> Kafka offset commit tells Kafka: **“I have successfully processed everything up to this offset.”**
>
> It does **not** mean the database transaction was committed.

---

## Table of Contents

- [1. Basic Consumer Flow](#1-basic-consumer-flow)
- [2. What is Offset Commit?](#2-what-is-offset-commit)
- [3. Commit BEFORE Processing](#3-commit-before-processing)
- [4. Commit AFTER Processing](#4-commit-after-processing)
- [5. The Important Crash Scenario](#5-the-important-crash-scenario)
- [6. Is This Data Loss?](#6-is-this-data-loss)
- [7. Why Duplicate Processing Can Be Dangerous](#7-why-duplicate-processing-can-be-dangerous)
- [8. Idempotency](#8-idempotency)
- [9. Four Important Crash Scenarios](#9-four-important-crash-scenarios)
- [10. What if DB Update Fails?](#10-what-if-db-update-fails)
- [11. What if the Message Always Fails?](#11-what-if-the-message-always-fails)
- [12. Manual vs Automatic Offset Commit](#12-manual-vs-automatic-offset-commit)
- [13. Kafka Doesn't Know About Your DB](#13-important-concept-kafka-doesnt-know-about-your-db)
- [14. At-Most-Once vs At-Least-Once](#14-at-most-once-vs-at-least-once)
- [15. Exactly-Once](#15-exactly-once)
- [16. Interview Answer](#16-interview-answer-to-your-exact-question)
- [17. Remember This Diagram](#17-remember-this-diagram)
- [Day 17 Interview Checklist](#day-17-interview-checklist)

---

## 1. Basic Consumer Flow

Suppose Kafka has:

```text
Partition 0

Offset
  100 → Order A
  101 → Order B
  102 → Order C
```

Consumer does:

```text
poll()
   ↓
process message
   ↓
update DB
   ↓
commit Kafka offset
```

For Order A:

```text
poll offset 100
      ↓
process Order A
      ↓
DB update
      ↓
commit offset 101
```

Why 101?

Because Kafka commits the **next offset to read**.

So:

- Processed: `100`
- Committed: `101`
- Next message = `101`

---

## 2. What is Offset Commit?

Kafka keeps track of how far each consumer group has processed a partition.

Example:

```text
Partition 0:

100   101   102   103
 ↓     ↓     ↓     ↓
 A     B     C     D
```

Suppose the consumer successfully processes A and B.

It commits:

```text
offset = 102
```

This means:

- `100` → processed
- `101` → processed
- `102` → next message

If the consumer crashes and restarts:

```text
restart
   ↓
read committed offset
   ↓
start from 102
```

So A and B won't normally be read again.

---

## 3. Commit BEFORE Processing

Consider:

```text
poll()
 ↓
commit offset
 ↓
process()
 ↓
DB update
```

Suppose:

```text
Kafka message:
Order A

offset = 100
```

Consumer polls it, then immediately commits:

```text
commit(101)
```

Now Kafka thinks:

```text
Order A → successfully processed
```

But actually:

```text
process Order A
      ↓
DB update
      ↓
CRASH
```

The DB update never happened.

Consumer restarts:

```text
committed offset = 101
```

So Kafka starts from `102`. Order A is skipped.

### Result

```text
Kafka:
Order A = processed

Database:
Order A = NOT processed
```

This is **message loss**.

Therefore:

> Never commit the offset before successful processing if losing messages is unacceptable.

---

## 4. Commit AFTER Processing

Now use:

```text
poll()
 ↓
process()
 ↓
DB update
 ↓
commit offset
```

This is much safer.

Suppose offset = `100`. Consumer processes it:

```text
poll(100)
   ↓
process
   ↓
DB update SUCCESS
   ↓
commit(101)
```

Everything is fine.

---

## 5. The Important Crash Scenario

Now let's answer the exact interview question.

```text
poll()
 ↓
process()
 ↓
DB update
 ↓
CRASH
 ↓
commit offset ❌
```

Suppose offset = `100`.

Consumer successfully updates the DB:

```text
DB:
Order A = SUCCESS
```

But before `commit(101)`, the consumer crashes.

Kafka still remembers:

```text
committed offset = 100
```

After restart:

```text
consumer starts from offset 100
```

So it receives Order A again.

Now:

```text
Kafka:
Order A → process again

DB:
Order A → already processed
```

This creates **duplicate processing**.

---

## 6. Is This Data Loss?

Usually, **no**.

The sequence was:

```text
Kafka message
     ↓
DB update SUCCESS
     ↓
CRASH
     ↓
Kafka offset NOT committed
     ↓
restart
     ↓
same Kafka message again
```

The message is **reprocessed**, not lost.

This is why:

> Commit after processing gives **at-least-once** delivery semantics.

Meaning: every message will be processed **at least once**.

But it might be processed 1 time, 2 times, 3 times...

---

## 7. Why Duplicate Processing Can Be Dangerous

Imagine your Kafka event is:

```json
{
  "userId": 123,
  "amount": 1000
}
```

Consumer does:

```text
DB:
balance = balance - 1000
```

First processing: ₹10,000 → ₹9,000

Consumer crashes before committing Kafka offset.

Message comes again.

Second processing: ₹9,000 → ₹8,000

Now the customer lost ₹2,000 instead of ₹1,000.

So simply using `process → commit` isn't enough for every business operation.

We need **idempotency**.

---

## 8. Idempotency

Idempotency means:

> Processing the same event multiple times produces the same final result as processing it once.

Suppose event has:

```json
{
    "eventId": "PAYMENT-12345",
    "userId": 101,
    "amount": 1000
}
```

Database:

```text
processed_events

event_id
----------------
PAYMENT-12345
```

Consumer:

```text
receive event
      ↓
check eventId
      ↓
already processed?
   /       \
 yes        no
 ↓           ↓
skip       process
             ↓
        mark processed
```

So if Kafka sends `PAYMENT-12345` again:

```text
event already exists
        ↓
skip processing
```

---

## 9. Four Important Crash Scenarios

This is extremely important for interviews.

### Scenario 1 — Crash before processing

```text
poll()
 ↓
CRASH
```

Kafka offset wasn't committed.

After restart: message is consumed again.

**Result:** no data loss.

### Scenario 2 — Crash during processing

```text
poll()
 ↓
process()
 ↓
CRASH
```

Offset wasn't committed.

After restart: message processed again.

**Potential duplicate.**

### Scenario 3 — DB update succeeds, then crash

```text
poll()
 ↓
process()
 ↓
DB SUCCESS
 ↓
CRASH
 ↓
commit ❌
```

After restart: same message again.

**Potential duplicate DB operation.** This is the exact example interviewers ask.

### Scenario 4 — Commit succeeds, then crash

```text
poll()
 ↓
process()
 ↓
DB SUCCESS
 ↓
commit SUCCESS
 ↓
CRASH
```

After restart: Kafka starts from next offset.

Message won't normally be processed again. Everything is good.

---

## 10. What if DB Update Fails?

Consider:

```text
poll()
 ↓
process()
 ↓
DB update ❌
 ↓
commit ❌
```

Don't commit the offset.

Kafka will eventually deliver the message again.

```text
Kafka
  ↓
retry
  ↓
DB
```

This is useful for temporary failures.

For example, DB temporarily unavailable.

After DB recovers:

```text
message
 ↓
retry
 ↓
DB SUCCESS
 ↓
commit
```

---

## 11. What if the Message Always Fails?

Suppose malformed data:

```text
Order A
 ↓
process
 ↓
ERROR

retry
 ↓
ERROR

retry
 ↓
ERROR
```

You don't want to retry forever.

Use:

```text
Kafka Topic
     ↓
Consumer
     ↓
processing failed
     ↓
retry
     ↓
retry
     ↓
retry
     ↓
DLT
```

**DLT** = Dead Letter Topic.

Example:

```text
orders
   ↓
consumer
   ↓
processing failure
   ↓
retry 3 times
   ↓
orders.DLT
```

The DLT event can contain:

- original topic
- partition
- offset
- event
- exception
- failure timestamp
- retry count

---

## 12. Manual vs Automatic Offset Commit

Kafka consumers commonly use:

```properties
enable.auto.commit=true
```

With automatic commit, Kafka periodically commits offsets.

The problem is that your application might not have finished processing when the offset gets committed.

Example:

```text
poll()
 ↓
processing takes 10 seconds
 ↓
auto commit happens
 ↓
CRASH
```

Kafka may believe the message was processed even though your application didn't finish.

That can cause **message loss**.

For critical processing, a common approach is:

```properties
enable.auto.commit=false
```

Then:

```text
poll()
 ↓
process
 ↓
DB success
 ↓
commit offset
```

---

## 13. Important Concept: Kafka Doesn't Know About Your DB

This is a very common interview question.

Suppose:

```text
Kafka
   ↓
Consumer
   ↓
MySQL
```

Kafka knows: `offset = 101`

MySQL knows: transaction committed

These are **two separate systems**.

Kafka doesn't automatically know: *"Did MySQL commit?"*

And MySQL doesn't automatically know: *"Did Kafka commit?"*

Therefore you can get this situation:

```text
MySQL COMMIT
     ↓
Kafka COMMIT fails
```

Result: DB = updated, Kafka = message not committed. Message gets processed again.

Or:

```text
Kafka COMMIT
     ↓
MySQL COMMIT fails
```

Result: Kafka = message considered processed, DB = update failed. **Potential message loss.**

This is the core reliability problem.

---

## 14. At-Most-Once vs At-Least-Once

### At-most-once

```text
commit
 ↓
process
```

Message is processed **0 or 1 times**.

Possible: message loss.

But generally no duplicate processing.

### At-least-once

```text
process
 ↓
commit
```

Message is processed **1 or more times**.

Possible: duplicates.

But much less risk of losing a message due to the consumer crash between processing and commit.

---

## 15. Exactly-Once

People often say:

> "Kafka exactly once means the message will never be processed twice."

That's an oversimplification.

Exactly-once semantics are more complicated, especially when your processing involves an **external database**.

For example:

```text
Kafka
 ↓
Consumer
 ↓
MySQL
```

Kafka's transactional mechanisms don't automatically make an arbitrary MySQL side effect exactly-once.

For DB + Kafka workflows, you may need patterns such as:

- Idempotency
- Unique event ID
- Database transaction
- Inbox/outbox pattern

depending on the architecture.

---

## 16. Interview Answer to Your Exact Question

If interviewer asks:

> What happens if the consumer crashes between DB update and offset commit?

Answer:

> "The DB update succeeds, but the Kafka offset is not committed. When the consumer restarts, Kafka gives the same message again because the committed offset still points to that message. Therefore the message can be processed twice. To handle this safely, I would use idempotent processing, typically with a unique event ID and a database constraint or processed-event table, so reprocessing the same event doesn't create duplicate business effects."

That's a very strong interview answer.

---

## 17. Remember This Diagram

```text
             Kafka
               │
               │ poll()
               ↓
          Consumer
               │
               ↓
          Process Event
               │
               ↓
          Update DB
               │
        ┌──────┴──────┐
        │             │
      SUCCESS       FAILURE
        │             │
        ↓             ↓
  Commit Offset     Don't Commit
        │             │
        ↓             ↓
   Next Message      Retry
```

And the dangerous point:

```text
DB SUCCESS
    │
    │
    X  ← CRASH
    │
Kafka offset NOT committed
    │
    ↓
Message comes again
```

Therefore:

> `process → DB → commit` gives **at-least-once** delivery, and you usually combine it with **idempotency** to safely handle duplicates.

---

## Day 17 Interview Checklist

You should be able to explain:

- What is a Kafka offset?
- Why does Kafka commit the next offset?
- Auto vs manual commit
- Commit before processing
- Commit after processing
- Crash before processing
- Crash during processing
- Crash after DB update but before offset commit
- Crash after offset commit
- At-most-once
- At-least-once
- Duplicate processing
- Idempotency
- Retry
- DLT
- Why Kafka offset and DB transaction are separate
- Why exactly-once is harder with external DBs
