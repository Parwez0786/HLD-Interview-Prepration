# Day 18 — Idempotency in Kafka

Idempotency is one of the most important concepts in Kafka consumer reliability.

The basic idea is:

> If the same event is processed multiple times, the final result should be the same as if it were processed only once.

This is especially important because Kafka consumers can receive the same event more than once.

---

## Table of Contents

- [1. Why Do Duplicate Events Happen?](#1-why-do-duplicate-events-happen)
- [2. Why Kafka Can Produce Duplicates](#2-why-kafka-can-produce-duplicates)
- [3. What Is an Idempotent Operation?](#3-what-is-an-idempotent-operation)
- [4. Non-Idempotent Operation](#4-non-idempotent-operation)
- [5. Making an Operation Idempotent](#5-making-an-operation-idempotent)
- [6. Idempotency Key](#6-idempotency-key)
- [7. Kafka Event Structure](#7-kafka-event-structure)
- [8. Deduplication Using a Database](#8-deduplication-using-a-database)
- [9. processed_events Table](#9-processed_events-table)
- [10. Consumer Flow](#10-consumer-flow)
- [11. What Happens When Duplicate Arrives?](#11-what-happens-when-duplicate-arrives)
- [12. Very Important: Race Condition](#12-very-important-race-condition)
- [13. Database Constraint Solves This](#13-database-constraint-solves-this)
- [14. Better Implementation Pattern](#14-better-implementation-pattern)
- [15. But There Is a BIG Problem](#15-but-there-is-a-big-problem)
- [16. Therefore, Order Matters](#16-therefore-order-matters)
- [17. Example](#17-example)
- [18. Crash Scenarios](#18-crash-scenarios)
- [19. Database Transaction + Kafka Offset](#19-database-transaction--kafka-offset)
- [20. Redis-Based Deduplication](#20-redis-based-deduplication)
- [21. Why Use Redis?](#21-why-use-redis)
- [22. Redis Has an Important Problem](#22-redis-has-an-important-problem)
- [23. Redis TTL](#23-redis-ttl)
- [24. Database vs Redis](#24-database-vs-redis)
- [25. Unique Business ID](#25-another-approach--unique-business-id)
- [26. Idempotency vs Exactly-Once](#26-idempotency-vs-exactly-once)
- [27. Idempotency Does NOT Mean Kafka Delivers Only Once](#27-idempotency-does-not-mean-kafka-delivers-only-once)
- [28. Practical Paytm-Style Example](#28-practical-paytm-style-example)
- [29. What Should the Event ID Be?](#29-what-should-the-event-id-be)
- [30. What If Producer Itself Sends Duplicates?](#30-what-if-producer-itself-sends-duplicates)
- [31. Partitioning Does Not Solve Duplicates](#31-important-partitioning-does-not-solve-duplicates)
- [32. Recommended Architecture](#32-recommended-architecture)
- [33. Interview Answer](#33-interview-answer)
- [34. The Most Important Failure Scenario](#34-the-most-important-failure-scenario-to-remember)
- [Day 17 → Day 18 Connection](#day-17--day-18-connection)

---

## 1. Why Do Duplicate Events Happen?

Suppose Kafka has:

- Topic: `payment-events`
- Event: `eventId = E101`, `userId = U1`, `amount = ₹500`

Consumer receives it:

```text
poll()
   ↓
process payment
   ↓
DB update
   ↓
commit Kafka offset
```

Now imagine:

```text
DB update succeeds
       ↓
Consumer crashes
       ↓
Kafka offset was NOT committed
```

When the consumer restarts:

```text
Kafka sends E101 again
       ↓
Consumer processes E101 again
```

So the same event gets processed twice.

### Example

First processing:

- Balance = ₹1000
- Payment = ₹500
- Balance = ₹500

Consumer crashes before committing offset.

Second processing:

- Payment = ₹500
- Balance = ₹0

That's a serious problem.

---

## 2. Why Kafka Can Produce Duplicates

Kafka generally gives you **at-least-once** processing when you commit offsets after successful processing.

The sequence is:

```text
Kafka
  ↓
Consumer reads event
  ↓
Process event
  ↓
Database update
  ↓
Commit offset
```

There is a small failure window:

```text
DB update
   ↓
💥 CRASH
   ↓
Offset not committed
```

Kafka doesn't know that your DB operation already happened.

Therefore: same event → processed again.

This is why idempotency is necessary.

---

## 3. What Is an Idempotent Operation?

An operation is idempotent if performing it multiple times produces the same final result.

Mathematically:

```text
f(f(x)) = f(x)
```

### Simple example

Suppose: Set user status = `ACTIVE`

First execution: `status = ACTIVE`

Second execution: `status = ACTIVE`

No problem. So this is idempotent.

---

## 4. Non-Idempotent Operation

Consider: `balance = balance - ₹500`

Initial: `balance = ₹1000`

First execution: ₹1000 - ₹500 = ₹500

Second execution: ₹500 - ₹500 = ₹0

Different result.

Therefore `balance = balance - amount` is **not** idempotent.

---

## 5. Making an Operation Idempotent

Instead of blindly processing an event, give every event a unique ID.

For example:

```json
{
    "eventId": "PAYMENT-12345",
    "userId": "U101",
    "amount": 500
}
```

Now the consumer can remember:

```text
PAYMENT-12345 → already processed
```

If the event comes again (`PAYMENT-12345`), we skip it.

---

## 6. Idempotency Key

An idempotency key is a unique identifier used to recognize duplicate requests/events.

Example:

- `eventId = "PAYMENT-12345"`
- or `transactionId = "TXN-98765"`
- or `orderId = "ORDER-123"`

The important thing is:

> The same logical operation must have the same key whenever it is retried.

---

## 7. Kafka Event Structure

A good Kafka event might look like:

```json
{
    "eventId": "E12345",
    "eventType": "PAYMENT_SUCCESS",
    "userId": "U1001",
    "amount": 500,
    "timestamp": "2026-09-20T10:00:00"
}
```

Here `eventId = E12345` is our idempotency key.

---

## 8. Deduplication Using a Database

Your design is:

```text
              Kafka Event
                   ↓
                eventId
                   ↓
          Check processed_events
                   ↓
            Already processed?
              /          \
            Yes           No
             ↓             ↓
           Skip         Process
                           ↓
                  Mark processed
```

Let's understand every step.

---

## 9. processed_events Table

We create a table:

```sql
CREATE TABLE processed_events (
    event_id VARCHAR(100) PRIMARY KEY,
    processed_at TIMESTAMP
);
```

Example:

| event_id | processed_at |
| --- | --- |
| E101 | 10:01 |
| E102 | 10:02 |
| E103 | 10:05 |

The **PRIMARY KEY** is extremely important.

It guarantees that `E101` cannot be inserted twice.

---

## 10. Consumer Flow

Suppose Kafka sends `E101`.

Consumer checks:

```sql
SELECT event_id
FROM processed_events
WHERE event_id = 'E101';
```

If no record exists:

```text
E101 doesn't exist
       ↓
Process event
       ↓
Mark E101 as processed
```

Then:

```sql
INSERT INTO processed_events
(event_id, processed_at)
VALUES
('E101', NOW());
```

---

## 11. What Happens When Duplicate Arrives?

Kafka sends `E101` again.

Consumer checks `processed_events` and finds `E101`.

Therefore:

```text
Already processed
       ↓
Skip
       ↓
Commit Kafka offset
```

So the payment isn't processed twice.

---

## 12. Very Important: Race Condition

A common mistake is:

```text
Check event
     ↓
Not found
     ↓
Process
     ↓
Insert event
```

This can have a race condition.

Imagine two consumers/threads process the same event:

```text
Consumer A                 Consumer B

Check E101                 Check E101
   ↓                          ↓
Not found                   Not found
   ↓                          ↓
Process                     Process
```

Both think the event is new.

So simply doing `SELECT` is not enough.

---

## 13. Database Constraint Solves This

Use `PRIMARY KEY(event_id)`.

Then:

```text
Consumer A
    ↓
INSERT E101
    ↓
SUCCESS

while:

Consumer B
    ↓
INSERT E101
    ↓
PRIMARY KEY violation
```

Only one consumer can successfully claim the event.

---

## 14. Better Implementation Pattern

A common approach is:

```text
Kafka Event
    ↓
Try to insert eventId
    ↓
Was insert successful?
   /       \
 No         Yes
 ↓           ↓
Skip       Process
             ↓
          Commit
```

For example:

```sql
INSERT INTO processed_events(event_id, processed_at)
VALUES ('E101', NOW());
```

If successful: continue processing.

If duplicate:

```text
Duplicate key
    ↓
Already processed
    ↓
Skip
```

This is safer than `SELECT → INSERT` because the database constraint handles concurrency.

---

## 15. But There Is a BIG Problem

Consider:

```text
INSERT eventId
      ↓
Process payment
      ↓
💥 crash
```

Suppose `processed_events` already has `E101` inserted, but payment processing failed.

When Kafka retries `E101`, our consumer sees `E101 already exists` and skips it.

Now the payment was **never successfully processed**.

This is a critical issue.

---

## 16. Therefore, Order Matters

You need to think carefully about:

- Deduplication
- Business operation
- Kafka offset

Ideally, the business update and idempotency record should be handled **atomically** when they use the same database.

For example:

```text
BEGIN TRANSACTION

    Insert processed_events
          ↓
    Update business table

COMMIT
```

If anything fails: `ROLLBACK`.

Then Kafka does not commit the offset.

Kafka retries the event.

---

## 17. Example

Suppose we have `payments` and `processed_events`.

Consumer receives `E101`.

Transaction:

```sql
BEGIN

INSERT INTO processed_events
VALUES ('E101', NOW());

UPDATE payments
SET status = 'SUCCESS'
WHERE payment_id = 'P101';

COMMIT
```

Then:

```text
DB transaction successful
        ↓
Kafka offset commit
```

Everything is consistent.

---

## 18. Crash Scenarios

This is very important for interviews.

### Scenario 1 — Crash before DB transaction

```text
Kafka event
   ↓
💥 crash
```

Kafka offset isn't committed. Event is retried. No problem.

### Scenario 2 — Crash during DB transaction

```text
Kafka event
   ↓
BEGIN
   ↓
DB operation
   ↓
💥 crash
```

Database transaction rolls back. Kafka retries. Event gets processed again. Good.

### Scenario 3 — DB transaction succeeds, then consumer crashes

```text
Kafka event
   ↓
DB transaction
   ↓
COMMIT
   ↓
💥 crash
   ↓
Kafka offset NOT committed
```

Kafka sends event again.

Consumer checks: `E101 already exists` → skip business processing → commit offset.

This is exactly where idempotency protects you.

---

## 19. Database Transaction + Kafka Offset

The complete flow becomes:

```text
             Kafka
               │
               ↓
            Event E101
               │
               ↓
       Start DB transaction
               │
               ↓
    Insert eventId into table
               │
        ┌──────┴──────┐
        │             │
     Duplicate       New
        │             │
        ↓             ↓
      Skip       Business update
        │             │
        │             ↓
        │         COMMIT
        │             │
        └──────┬──────┘
               ↓
        Commit Kafka offset
```

Database:

```text
processed_events
----------------
event_id PK
processed_at
```

---

## 20. Redis-Based Deduplication

Instead of a database table, you can use Redis.

Example:

```text
SET event:E101 1 NX EX 86400
```

`NX` means: set only if the key doesn't already exist.

So:

```text
First E101
    ↓
SET NX
    ↓
SUCCESS
    ↓
Process

Second E101:
SET NX
    ↓
FAIL
    ↓
Already processed
    ↓
Skip
```

---

## 21. Why Use Redis?

Redis is very fast.

```text
Database:

Application
    ↓
Database

Redis:

Application
    ↓
Redis
```

For very high event volumes, Redis can reduce database load.

Example: 3 million events/day.

You don't necessarily want every duplicate-check query hitting your main database.

---

## 22. Redis Has an Important Problem

Suppose:

```text
SET E101
   ↓
Process payment
   ↓
💥 crash
```

Redis already says `E101 = processed`, but the business operation may not have completed.

Then retry:

```text
E101
   ↓
Redis says exists
   ↓
Skip
```

Again, the event could be lost logically.

Therefore: Redis-based deduplication needs careful failure handling.

For critical financial operations, a database transaction or another durable coordination mechanism is often preferable.

---

## 23. Redis TTL

Usually we don't want deduplication keys forever.

For example:

```text
SET event:E101 1 NX EX 86400
```

means `event:E101` expires after 24 hours.

After 24 hours, Redis key disappears. Then the same event could potentially be processed again.

Therefore, TTL must be chosen based on your business requirement.

For example:

- Payment event → maybe longer retention
- Notification event → perhaps shorter retention

The correct duration depends on how long duplicates can realistically appear and what the business impact is.

---

## 24. Database vs Redis

| Feature | Database | Redis |
| --- | --- | --- |
| Speed | Good | Very fast |
| Durability | High | Depends on configuration |
| Persistence | Strong | Configurable |
| TTL | Possible | Very easy |
| Large volume | Can become expensive | Good for high-throughput checks |
| Transactions | Strong | Limited compared with relational DB transactions |
| Best for | Critical business operations | Fast dedup/cache-like workloads |

---

## 25. Another Approach — Unique Business ID

Sometimes you don't even need a separate `processed_events` table.

Suppose `payment_id = P101` is globally unique.

You can have:

```sql
CREATE TABLE payments (
    payment_id VARCHAR(100) PRIMARY KEY,
    user_id VARCHAR(100),
    amount DECIMAL(10,2),
    status VARCHAR(20)
);
```

When processing:

```sql
INSERT INTO payments(...)
VALUES ('P101', ...);
```

If Kafka sends `P101` again: **PRIMARY KEY violation**.

Therefore, the database itself provides idempotency.

This is often a very clean solution.

---

## 26. Idempotency vs Exactly-Once

This is an important interview distinction.

### Idempotency

Means:

```text
Duplicate execution
       ↓
Same final business result
```

### Exactly-once

Means: business effect happens exactly once.

They are related but not identical.

For example, your consumer might receive `E101` three times.

With idempotency:

```text
received 3 times
        ↓
business effect only once
```

That's what you usually want.

---

## 27. Idempotency Does NOT Mean Kafka Delivers Only Once

This is a common interview trap.

Don't say: *"We use idempotency, so Kafka won't send duplicates."*

Incorrect.

Kafka may still deliver:

```text
E101
E101
```

Idempotency means:

```text
Kafka duplicates
      ↓
Consumer detects duplicates
      ↓
Business operation remains safe
```

---

## 28. Practical Paytm-Style Example

Let's use your SIP notification pipeline.

Suppose Kafka event:

```json
{
    "eventId": "SIP-98765",
    "userId": "U123",
    "sipId": "SIP001",
    "amount": 5000,
    "triggerType": "PDN_SUCCESS"
}
```

Consumer:

```text
Kafka
  ↓
SIP-98765
  ↓
Check processed_events
  ↓
Already processed?
   /       \
 Yes       No
  ↓         ↓
Skip      Send notification
            ↓
       Mark processed
            ↓
       Commit offset
```

Without idempotency:

```text
Kafka retry
   ↓
same notification
   ↓
user receives notification twice
```

With idempotency:

```text
Kafka retry
   ↓
SIP-98765 already processed
   ↓
Skip
```

---

## 29. What Should the Event ID Be?

This is a very important design question.

**Bad:** timestamp — because two events can have the same timestamp.

**Better:** UUID. Example: `550e8400-e29b-41d4-a716-446655440000`

Or a business-generated unique ID: `paymentId`, `orderId`, `sipTransactionId`, `mandateId`.

The key requirement is:

> Same logical operation → same ID
> Different logical operation → different ID

---

## 30. What If Producer Itself Sends Duplicates?

Suppose producer sends:

```text
E101
E101
```

Kafka stores both:

```text
Partition 0

offset 100 → E101
offset 101 → E101
```

Consumer receives both.

Idempotency handles it:

```text
offset 100 → process
offset 101 → skip
```

So idempotency protects against producer duplicates as well as consumer retries.

---

## 31. Important: Partitioning Does Not Solve Duplicates

You might think:

```text
Same event
   ↓
same partition
   ↓
therefore no duplicate
```

No.

Kafka partitioning provides **ordering within a partition**, but it does not automatically guarantee that your business operation happens only once.

You still need idempotency.

---

## 32. Recommended Architecture

For a critical backend system, I'd explain it in an interview like this:

```text
                         Kafka
                           │
                           ↓
                    Consumer Group
                           │
                           ↓
                       eventId
                           │
                           ↓
                ┌─────────────────────┐
                │   DB Transaction    │
                │                     │
                │ Insert eventId      │
                │        ↓            │
                │ Business update     │
                │        ↓            │
                │      COMMIT         │
                └─────────────────────┘
                           │
                           ↓
                    Commit Kafka
                       offset
```

Database:

```text
processed_events
----------------
event_id PK
processed_at
```

---

## 33. Interview Answer

If interviewer asks:

> How will you handle duplicate Kafka events?

You can say:

> "Kafka consumers can receive duplicate events, especially when the consumer successfully updates the database but crashes before committing the Kafka offset. I would make the consumer idempotent by assigning every event a unique event ID. I can maintain a processed_events table with event_id as a unique or primary key. During processing, I would atomically record the event and perform the business update in a database transaction. If the same event is delivered again, the unique constraint tells us that it has already been processed, so we skip the business operation and commit the Kafka offset. For high-throughput non-critical deduplication, Redis with SET NX and an appropriate TTL can also be used."

That's a strong SDE-1 Kafka interview answer.

---

## 34. The Most Important Failure Scenario to Remember

Memorize this:

```text
Kafka
  ↓
Process event
  ↓
DB update SUCCESS
  ↓
💥 Consumer crashes
  ↓
Kafka offset NOT committed
  ↓
Kafka sends event again
  ↓
Idempotency check
  ↓
Already processed
  ↓
Skip
  ↓
Commit offset
```

This is the core reason idempotency exists in Kafka consumers.

---

## Day 17 → Day 18 Connection

### Day 17: Consumer Reliability

```text
Kafka
  ↓
process
  ↓
DB
  ↓
commit offset

Problem:
DB success + crash before commit
        ↓
duplicate event
```

### Day 18: Idempotency

```text
Kafka
  ↓
eventId
  ↓
deduplication
  ↓
DB
  ↓
commit offset

Result:
duplicate delivery
        ↓
safe business result
```

**Key takeaway:** Kafka can deliver an event more than once. You don't try to prevent every duplicate delivery; instead, you design the consumer's business operation to be safe when duplicates occur.
