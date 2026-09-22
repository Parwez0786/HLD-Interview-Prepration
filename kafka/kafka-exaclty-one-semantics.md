# Day 19 — Kafka Exactly-Once Semantics (EOS)

Today’s topic is important for Kafka interviews and distributed-system design, especially when they ask:

> “How do you guarantee exactly-once processing in Kafka?”

The key idea is:

> Kafka Exactly-Once Semantics (EOS) prevents duplicate effects when a Kafka consumer reads a message, processes it, and writes the result back to Kafka.
>
> But Kafka EOS does **NOT** automatically mean your entire business operation is exactly once.

---

## Table of Contents

- [1. What is Exactly-Once Semantics?](#1-what-is-exactly-once-semantics)
- [2. What Does Kafka EOS Solve?](#2-what-does-kafka-eos-solve)
- [3. Kafka Transactions](#3-kafka-transactions)
- [4. Transactional Producer](#4-transactional-producer)
- [5. Why transactional.id?](#5-why-transactionalid)
- [6. What is Producer Fencing?](#6-what-is-producer-fencing)
- [7. Transactional Consumer](#7-transactional-consumer)
- [8. Read → Process → Write](#8-read--process--write)
- [9. Why Offset Commit Must Be Part of the Transaction](#9-why-offset-commit-must-be-part-of-the-transaction)
- [10. Example](#10-example)
- [11. What If the Consumer Crashes?](#11-what-if-the-consumer-crashes)
- [12. isolation.level](#12-isolationlevel)
- [13. Complete EOS Pipeline](#13-complete-eos-pipeline)
- [14. Kafka Exactly-Once vs At-Least-Once](#14-kafka-exactly-once-vs-at-least-once)
- [15. Kafka EOS ≠ End-to-End Exactly Once](#15-kafka-eos--end-to-end-exactly-once)
- [16. Example of the Problem](#16-example-of-the-problem)
- [17. So What Is End-to-End Exactly Once?](#17-so-what-is-end-to-end-exactly-once)
- [18. How Do We Handle This?](#18-how-do-we-handle-this)
- [19. Kafka EOS + Idempotent Database](#19-kafka-eos--idempotent-database)
- [20. Outbox Pattern](#20-outbox-pattern)
- [21. EOS vs Idempotency](#21-eos-vs-idempotency)
- [22. Does Kafka EOS Guarantee Exactly-Once Execution?](#22-important-interview-question)
- [23. Crash After Processing Before Commit](#23-another-important-question)
- [24. enable.idempotence vs EOS](#24-enableidempotence-vs-eos)
- [25. Interview Comparison](#25-interview-comparison)
- [26. Your Mental Model](#26-your-mental-model)
- [27. Relate This to Your Paytm Kafka Work](#27-relate-this-to-your-paytm-kafka-work)
- [28. Questions You Should Be Able to Answer](#28-questions-you-should-be-able-to-answer)

---

## 1. What is Exactly-Once Semantics?

Suppose Kafka has:

```text
Topic A
   |
   v
Consumer
   |
   | process
   v
Topic B
```

Input: `OrderCreated(orderId=101)`

Consumer calculates `OrderAmount = ₹1000`, `Tax = ₹180` and produces `OrderProcessed(orderId=101, amount=1180)`.

The problem is failure.

### Without EOS

Consumer:

1. Read `OrderCreated`
2. Process
3. Produce `OrderProcessed`
4. Consumer crashes before committing offset

Kafka thinks `OrderCreated` was never successfully processed.

So after restart:

5. Read `OrderCreated` again
6. Process
7. Produce `OrderProcessed` again

Output:

```text
OrderProcessed(101)
OrderProcessed(101)
```

**Duplicate.**

---

## 2. What Does Kafka EOS Solve?

Kafka provides a mechanism where consume + process + produce + offset commit can be coordinated using a **Kafka transaction**.

Conceptually:

```text
             Kafka Transaction
        ┌─────────────────────────┐
        │                         │
Input → │ Read                    │
        │   ↓                     │
        │ Process                 │
        │   ↓                     │
        │ Produce Output          │
        │   ↓                     │
        │ Commit Offset           │
        │                         │
        └──────────┬──────────────┘
                   │
              COMMIT / ABORT
```

Either the transaction succeeds:

```text
Output + offset
      ↓
   committed
```

or fails:

```text
Output + offset
      ↓
   aborted
```

---

## 3. Kafka Transactions

Kafka transactions allow a producer to atomically write records to multiple Kafka partitions/topics and commit consumed offsets as part of the same transaction.

For example:

```text
Topic A
   |
   v
Consumer
   |
   | process
   v
Topic B
Topic C
```

You can make Topic B write, Topic C write, and offset commit part of one transaction.

**Transaction succeeds:** Topic B committed, Topic C committed, Offset committed.

**Transaction fails:** Topic B aborted, Topic C aborted, Offset not committed.

This is the foundation of Kafka EOS.

---

## 4. Transactional Producer

A normal producer (`KafkaProducer`) can send messages.

A transactional producer additionally participates in Kafka transactions.

Conceptually:

```java
producer.initTransactions();

producer.beginTransaction();

producer.send(record1);
producer.send(record2);

producer.commitTransaction();
```

If something goes wrong:

```java
producer.abortTransaction();
```

---

## 5. Why transactional.id?

This is a very important interview question.

A transactional producer needs a unique identity: `transactional.id`.

Example:

```java
props.put(
    ProducerConfig.TRANSACTIONAL_ID_CONFIG,
    "payment-processor-1"
);
```

Kafka uses this identity to associate the producer with its transactional state.

It also enables **producer fencing**.

---

## 6. What is Producer Fencing?

Imagine producer instance A has `transactional.id = payment-processor-1`.

Then due to a restart/network problem, another instance B starts using the same transactional ID.

Now two producers appear to be the same logical producer.

Kafka must prevent the old producer from continuing to write.

So Kafka gives the newer producer a newer **producer epoch** and fences the old producer.

Conceptually:

```text
Producer A
transactional.id = payment-processor-1
epoch = 5

       ↓ restart

Producer B
transactional.id = payment-processor-1
epoch = 6

Kafka:

epoch 5 → fenced
epoch 6 → valid
```

Producer A can no longer successfully participate in the transaction.

**Interview answer:**

> `transactional.id` uniquely identifies a transactional producer and enables Kafka to maintain transactional state and fence older producer instances using the same identity.

---

## 7. Transactional Consumer

This terminology can be confusing.

Kafka consumers don't themselves perform transactions in the same way producers do.

Instead, a consumer participates in EOS through the producer transaction.

The common pattern is:

```text
Consumer
   |
   | read
   ↓
Process
   |
   | produce
   ↓
Transactional Producer
   |
   ├── output records
   └── consumed offsets
```

The producer commits both output records + consumed offsets inside the transaction.

---

## 8. Read → Process → Write

This is the most important EOS pattern.

Suppose:

```text
Input Topic
    |
    v
Consumer
    |
    v
Business Logic
    |
    v
Output Topic
```

Example:

```text
orders
   |
   v
OrderProcessor
   |
   v
payments
```

Consumer reads `OrderCreated(101)`, process `amount = 1000`, produce `PaymentRequested(101, 1000)`.

Then commit `PaymentRequested` + offset of `OrderCreated` as one Kafka transaction.

---

## 9. Why Offset Commit Must Be Part of the Transaction

Consider this:

1. Read message
2. Produce output
3. Commit output
4. Crash
5. Offset wasn't committed

After restart:

6. Read same input
7. Produce output again

**Duplicate output.**

So we want output write + offset commit to happen **atomically**.

That's exactly what Kafka transactions provide for Kafka-to-Kafka processing.

---

## 10. Example

Input:

```text
orders
partition 0

offset 100:
OrderCreated(101)
```

Consumer reads offset 100.

It produces:

```text
payments
partition 2

PaymentRequested(101)
```

Transaction contains:

```text
BEGIN TRANSACTION

Produce:
    PaymentRequested(101)

Commit consumed offset:
    orders partition 0 → offset 101

COMMIT TRANSACTION
```

Now Kafka sees both as committed.

---

## 11. What If the Consumer Crashes?

Suppose:

```text
Read offset 100
     ↓
Process
     ↓
Produce output
     ↓
CRASH
```

If the transaction was not committed:

```text
Output → ABORTED
Offset → NOT COMMITTED
```

After restart:

```text
Read offset 100 again
     ↓
Process
     ↓
Produce output
     ↓
Commit transaction
```

Only the successful transaction's output is visible to a properly configured transactional consumer.

---

## 12. isolation.level

Kafka consumers can use `read_uncommitted` or `read_committed`.

### read_uncommitted

Consumer can read committed records + aborted transactional records.

Example:

```text
Transaction 1
   ↓
Output A
   ↓
ABORT
```

A `read_uncommitted` consumer may still see Output A.

### read_committed

Consumer sees only committed records. Aborted transactional records are hidden.

Configuration:

```properties
isolation.level=read_committed
```

For an EOS pipeline, downstream consumers generally need `read_committed` if they should not observe aborted transactional records.

---

## 13. Complete EOS Pipeline

```text
             Topic A
                |
                v
        ┌───────────────┐
        │   Consumer    │
        └───────┬───────┘
                |
                v
          Business Logic
                |
                v
      Transactional Producer
                |
        ┌───────┴────────┐
        ↓                ↓
    Topic B          Topic C
        |
        +
   Offset Commit
```

Transaction:

```text
BEGIN
   |
   +-- Produce Topic B
   |
   +-- Produce Topic C
   |
   +-- Commit consumed offsets
   |
COMMIT
```

Everything succeeds or the transaction is aborted.

---

## 14. Kafka Exactly-Once vs At-Least-Once

### At-least-once

Guarantee: message won't be intentionally lost, but it may be processed more than once.

```text
Input
 ↓
Process
 ↓
Output
 ↓
Crash before offset commit
 ↓
Retry
 ↓
Output again
```

Result: Output, Output.

### Exactly-once

For a Kafka transactional read-process-write pipeline:

```text
Input
 ↓
Process
 ↓
Output
 +
Offset
 ↓
Atomic transaction
```

The aborted attempt's output is not exposed to `read_committed` consumers.

---

## 15. Kafka EOS ≠ End-to-End Exactly Once

This is probably the most important concept of today's lesson.

Suppose:

```text
Kafka
  ↓
Consumer
  ↓
Payment Service
  ↓
MySQL
```

Kafka EOS can guarantee transactional behavior **inside Kafka**.

But what about MySQL?

Kafka transaction cannot automatically make Kafka transaction + MySQL transaction one atomic transaction.

---

## 16. Example of the Problem

Suppose:

```text
Kafka
  ↓
Consumer
  ↓
Update MySQL
```

Consumer receives `PaymentCreated(101)`.

Then:

```sql
UPDATE payments
SET status = 'SUCCESS'
WHERE id = 101;
```

Database commits.

Then the application crashes before Kafka offset is committed.

After restart, Kafka sends `PaymentCreated(101)` again.

Now the DB operation happens again.

Kafka EOS alone doesn't magically undo or coordinate the MySQL transaction.

---

## 17. So What Is End-to-End Exactly Once?

End-to-end exactly once means the entire business operation, across all involved systems, has exactly-once externally observable effects.

For example:

```text
Kafka
  ↓
Consumer
  ↓
Business Logic
  ↓
MySQL
  ↓
External Payment Gateway
```

Kafka EOS alone doesn't guarantee exactly-once across Kafka + MySQL + Payment Gateway, because they don't share Kafka's transaction protocol.

---

## 18. How Do We Handle This?

Several techniques are commonly used.

**Idempotency** — give every operation a unique ID: `transactionId = TXN123`.

Database: `transaction_id UNIQUE`.

First request: `TXN123 → processed`.

Retry: `TXN123 already exists` → don't process again.

This gives you effectively-once business behavior for that operation.

---

## 19. Kafka EOS + Idempotent Database

A common architecture:

```text
Kafka
  |
  v
Consumer
  |
  v
Process
  |
  v
MySQL
  |
  +-- UNIQUE transaction_id
  |
  v
Commit
```

If Kafka retries:

```text
Same transaction_id
       ↓
DB detects duplicate
       ↓
No duplicate business effect
```

---

## 20. Outbox Pattern

Another very important pattern.

Suppose you need MySQL + Kafka to stay consistent.

Instead of DB update + Kafka publish directly, use an **outbox table**.

```text
Application
     |
     v
MySQL Transaction
 ┌─────────────────────┐
 │ Business update     │
 │                     │
 │ Outbox event        │
 └─────────────────────┘
     |
     v
Outbox Publisher
     |
     v
Kafka
```

Business update and outbox event are committed in the same DB transaction.

Then a publisher sends the outbox event to Kafka.

This solves the classic: DB succeeded, Kafka failed — consistency problem.

---

## 21. EOS vs Idempotency

Don't confuse them.

**EOS** — Kafka-level mechanism: consume + process + produce + offset within Kafka transactions.

**Idempotency** — application-level mechanism: same request → same effect.

Example: `TXN123 processed`, `TXN123 retry` → ignore duplicate.

In real distributed systems, EOS and idempotency are often used together.

---

## 22. Important Interview Question

**Q: Does Kafka EOS guarantee exactly-once execution?**

Answer:

> No. Kafka EOS provides exactly-once processing semantics for Kafka transactional workflows, particularly read-process-write pipelines. It does not guarantee that application code executes only once. The application may execute the processing logic multiple times after failures, but aborted Kafka outputs are not exposed to read_committed consumers. For external systems such as MySQL or payment gateways, we generally need idempotency or patterns such as the transactional outbox.

This is a very strong interview answer.

---

## 23. Another Important Question

**Q: What happens if the consumer crashes after processing but before committing?**

With EOS:

```text
BEGIN TRANSACTION
      ↓
Read
      ↓
Process
      ↓
Produce
      ↓
CRASH
```

Transaction doesn't successfully commit.

Therefore:

```text
Produced records → aborted
Offset → not committed
```

After restart:

```text
Read same input
      ↓
Process again
      ↓
Successful transaction
      ↓
COMMIT
```

The processing code may run twice, but the aborted output is not visible to `read_committed` consumers.

---

## 24. enable.idempotence vs EOS

Another common interview question.

### Idempotent producer

```properties
enable.idempotence=true
```

Protects against duplicate records caused by producer retries.

```text
Producer
   |
   | send
   v
Broker
   X
network failure
   |
   v
Producer retries
```

Kafka can prevent duplicate writes from the retry.

But idempotence alone doesn't provide the complete consume + process + produce + offset transaction.

### EOS

Uses: idempotent producer + transactions + transactional offset handling for Kafka-to-Kafka processing.

---

## 25. Interview Comparison

| Concept | Purpose |
| --- | --- |
| `enable.idempotence=true` | Prevent duplicate producer writes from retries |
| Kafka transaction | Atomically commit multiple Kafka writes/offsets |
| `transactional.id` | Identify transactional producer |
| `read_committed` | Hide aborted transactional records |
| `read_uncommitted` | Can read aborted records |
| Kafka EOS | Exactly-once semantics within Kafka transactional workflow |
| Idempotency | Prevent duplicate business effects |
| Outbox | Keep DB changes and emitted events consistent |
| End-to-end exactly once | Exactly-once external/business effect across the whole workflow |

---

## 26. Your Mental Model

Remember this:

```text
                 KAFKA EOS
                    |
       ┌────────────┴────────────┐
       ↓                         ↓
    Consume                   Produce
       |                         |
       └──────────┬──────────────┘
                  ↓
            Commit Offset
                  |
                  ↓
             TRANSACTION
```

The transaction gives:

```text
OUTPUT + OFFSET
     ↓
   ATOMIC
```

But:

```text
Kafka EOS
    ❌
Kafka + MySQL + External API
```

doesn't automatically become exactly once.

For that:

```text
Kafka EOS
    +
Idempotency
    +
Outbox / appropriate distributed transaction strategy
```

may be required depending on the architecture.

---

## 27. Relate This to Your Paytm Kafka Work

For your Kafka notification pipeline:

```text
Mutual Fund Service
       |
       v
sip.pdn.debit.events
       |
       v
Payments Consumer
       |
       v
Notification Service
```

If you ask: *"Can we claim exactly-once delivery because we use Kafka?"*

**No.**

You should say:

> "Kafka can provide exactly-once semantics for a transactional Kafka read-process-write workflow, but our downstream notification service is an external system. Therefore, end-to-end exactly-once notification would require additional idempotency or deduplication at the notification layer."

For example: `eventId = SIP123-PDN456`.

Notification service maintains `processed_event_id` with a unique constraint.

If Kafka retries:

```text
SIP123-PDN456
       ↓
already processed
       ↓
don't send duplicate notification
```

That is a much safer production design.

---

## 28. Questions You Should Be Able to Answer

For your SDE-1 interview, make sure you can explain these without notes:

- What is Kafka EOS?
- Why do we need Kafka transactions?
- What is a transactional producer?
- What is `transactional.id`?
- What is producer fencing?
- What is `read_committed`?
- Difference between `read_committed` and `read_uncommitted`
- Explain read-process-write
- Why should offset commit be part of the transaction?
- What happens when consumer crashes during a transaction?
- `enable.idempotence` vs transactions?
- Kafka EOS vs end-to-end exactly once?
- Does Kafka EOS guarantee exactly-once execution?
- How would you achieve exactly-once effects in MySQL?
- How would you prevent duplicate payment/notification processing?
- What is the transactional outbox pattern?
- Can Kafka transaction include a MySQL transaction?
- Why do we need idempotency even when using Kafka EOS?

**One-line takeaway:**

> Kafka EOS makes Kafka's consume-process-produce workflow transactionally consistent; it does not make the entire distributed business operation exactly once. For external side effects, idempotency and patterns such as the outbox are typically required.
