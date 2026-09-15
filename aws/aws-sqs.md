# AWS SQS — 7-Day Detailed Tutorial

By the end, you should confidently explain SQS architecture, message lifecycle, Standard vs FIFO, retries, DLQ, scaling, and production failure scenarios.

---

## Table of Contents

- [What You Will Be Able to Answer](#what-you-will-be-able-to-answer-after-7-days)
- [7-Day Roadmap](#7-day-roadmap)
- [Day 1 — SQS Fundamentals](#day-1--sqs-fundamentals)
- [Day 1 Interview Questions](#day-1--interview-questions)
- [Day 2 — Message Lifecycle + Visibility Timeout](#day-2--message-lifecycle--visibility-timeout)
- [Day 2 Interview Questions](#day-2-interview-questions)
- [Day 3 — Standard vs FIFO](#day-3--standard-vs-fifo)
- [Day 4 — Retries, DLQ and Poison Messages](#day-4--retries-dlq-and-poison-messages)
- [Day 4 — Idempotency](#day-4--idempotency)
- [Day 5 — Scaling and Performance](#day-5--scaling-and-performance)
- [Day 6 — SQS in Production](#day-6--sqs-in-production)
- [Day 7 — Interview Preparation](#day-7--interview-preparation)
- [Most Important SQS Architecture](#most-important-sqs-architecture)
- [SQS Production Checklist](#sqs-production-checklist)
- [Your 7-Day Study Schedule](#your-7-day-study-schedule)

---

## What You Will Be Able to Answer After 7 Days

- What SQS is and why it is used
- Producer → Queue → Consumer architecture
- Standard vs FIFO
- Visibility Timeout
- Message retention
- Long polling / Short polling
- At-least-once delivery
- Duplicate messages
- Message deletion / acknowledgement
- Consumer crashes
- DLQ / Poison messages / Retries
- Scaling consumers
- Message ordering
- Idempotency / Deduplication
- Delay queues
- SQS + Lambda / ECS / Kubernetes / Redis
- SQS in microservices
- High-throughput architecture
- Real production failure scenarios
- SQS interview questions

---

## 7-Day Roadmap

| Day | Topic | Goal |
| --- | --- | --- |
| Day 1 | SQS Fundamentals | Understand queue architecture |
| Day 2 | Message Lifecycle | Visibility timeout, delete, retry |
| Day 3 | Standard vs FIFO | Ordering, duplicates, deduplication |
| Day 4 | Reliability | DLQ, retries, poison messages, idempotency |
| Day 5 | Scaling & Performance | Long polling, batching, consumers |
| Day 6 | Production Architecture | SQS + Redis + microservices |
| Day 7 | Interview Preparation | Scenarios + system design + cross questions |

---

# DAY 1 — SQS FUNDAMENTALS

## 1. What is AWS SQS?

SQS = **Simple Queue Service**.

It is a managed message queue service provided by AWS that allows different components/services to communicate asynchronously.

Simple architecture:

```text
Producer
   |
   | Send Message
   v
+----------------+
|      SQS       |
|     Queue      |
+----------------+
   |
   | Receive Message
   v
Consumer
```

For example, suppose you have Order Service, Payment Service, Notification Service.

**Without SQS:**

```text
Order Service
     |
     v
Payment Service
     |
     v
Notification Service
```

The Order Service may have to wait for downstream services.

**With SQS:**

```text
Order Service
     |
     | message
     v
   SQS Queue
     |
     v
Payment Service
```

Now the producer doesn't need to wait for the consumer to finish processing.

---

## 2. Why Do We Need SQS?

Suppose 1000 requests/sec come into your application, but your downstream service can process only 300 requests/sec.

**Without a queue:**

```text
1000 req/sec
      |
      v
Downstream
300 req/sec
      |
      v
Overload
```

**With SQS:**

```text
1000 req/sec
      |
      v
+-------------+
|     SQS     |
|             |
|  700 msgs   |
+-------------+
      |
      v
300/sec consumer
```

The queue acts as a **buffer**.

---

## 3. Main Advantages of SQS

1. **Asynchronous processing** — Producer doesn't have to wait for consumer.
2. **Decoupling** — Producer and consumer don't need to directly communicate.
3. **Buffering** — SQS absorbs temporary traffic spikes.
4. **Retry** — Messages can become available again if processing fails.
5. **Scaling** — You can increase/decrease consumers based on queue load.
6. **Fault tolerance** — If consumers temporarily go down, messages can remain in the queue.

---

## 4. Basic Terminology

You need to know these terms extremely well.

### Producer

The application that sends messages.

```text
Producer → SQS
```

Example: Order Service.

### Queue

Stores messages until consumers process them.

```text
Producer
   |
   v
 Queue
```

### Consumer

Reads and processes messages.

```text
Queue
  |
  v
Consumer
```

### Message

Data stored inside SQS.

```json
{
  "orderId": "12345",
  "userId": "987",
  "amount": 500
}
```

---

## 5. Basic SQS Flow

Suppose Order Service creates an order and sends:

```json
{
  "orderId": "ORD123",
  "userId": "U100"
}
```

to SQS.

Flow:

```text
                SEND
Order Service -----------> SQS
                             |
                             |
                          RECEIVE
                             |
                             v
                       Payment Service
                             |
                             |
                          PROCESS
                             |
                             v
                          DELETE
```

This lifecycle is extremely important.

---

## 6. SQS APIs You Should Know

You don't need to memorize every API. Know these:

### SendMessage

Producer sends a message.

```text
Producer
   |
   | SendMessage
   v
 SQS
```

### ReceiveMessage

Consumer asks SQS for messages.

```text
Consumer
   |
   | ReceiveMessage
   v
 SQS
```

### DeleteMessage

After successful processing:

```text
Consumer
   |
   | DeleteMessage
   v
 SQS
```

This tells SQS: **"I successfully processed this message."**

---

## 7. Important Interview Question

**Does receiving a message automatically delete it?**

**No.**

This is one of the most important SQS concepts.

When the consumer receives a message:

```text
SQS
 |
 | Receive
 v
Consumer
```

the message is temporarily **hidden** from other consumers.

It is **not deleted**.

The consumer must explicitly delete it after successful processing.

```text
Receive
   ↓
Process
   ↓
Success
   ↓
Delete
```

---

## 8. What Happens If Consumer Fails?

This leads to Visibility Timeout, which we'll study deeply on Day 2.

Simplified:

```text
SQS
 |
 | Receive
 v
Consumer
 |
 | processing
 X
CRASH
```

The message isn't deleted.

After visibility timeout: **message becomes visible again**.

Then another consumer can process it.

---

## 9. Why Not Simply Use Database?

You might ask: Why don't we put pending jobs in MySQL?

You can build a queue using a database, but it has disadvantages.

For example:

```sql
SELECT * FROM jobs
WHERE status = 'PENDING'
LIMIT 100;
```

Under high load you need to deal with: locking, concurrent consumers, retries, visibility, polling, scaling, failed workers, duplicate processing.

SQS provides these queue primitives as a **managed service**.

---

## 10. SQS vs Synchronous API

### Synchronous

```text
Service A
   |
   | HTTP
   v
Service B
   |
   | response
   v
Service A
```

A waits for B.

### Asynchronous

```text
Service A
   |
   | message
   v
 SQS
   |
   v
Service B
```

A doesn't have to wait for B.

---

## DAY 1 — INTERVIEW QUESTIONS

### Q1. What is SQS?

> AWS SQS is a managed message queuing service used for asynchronous communication between distributed components. It decouples producers and consumers, provides buffering during traffic spikes, and supports reliable message processing through visibility timeouts, retries and dead-letter queues.

### Q2. Why use SQS?

We use SQS to decouple services, process tasks asynchronously, absorb traffic spikes, provide retry behavior when consumers fail, and independently scale consumers.

### Q3. Is SQS synchronous or asynchronous?

Asynchronous.

### Q4. Is SQS push or pull?

Traditional SQS consumers use a **pull** model.

```text
Consumer → SQS
```

The consumer asks SQS for messages.

---

# DAY 2 — MESSAGE LIFECYCLE + VISIBILITY TIMEOUT

This is probably the most important SQS day.

## 1. Message Lifecycle

A message generally goes through:

```text
          Send
Producer -------> SQS
                   |
                   | Receive
                   v
              In-flight
                   |
              +----+----+
              |         |
           Success    Failure
              |         |
              v         v
           Delete    Visible again
```

---

## 2. Visibility Timeout

Suppose a consumer receives Message A.

If SQS immediately allowed every consumer to see it:

```text
Consumer 1 → Message A
Consumer 2 → Message A
Consumer 3 → Message A
```

the same message could be processed concurrently.

So SQS temporarily hides the message.

```text
SQS
 |
 | receive
 v
Consumer 1
 |
 +---- Message becomes invisible
```

This period is called **Visibility Timeout**.

---

## 3. Example

Suppose Visibility Timeout = 30 seconds.

At `10:00:00` consumer receives the message.

For the next 30 seconds: **message invisible**.

If consumer successfully processes it at `10:00:10` → **DELETE**.

Message is permanently removed.

---

## 4. What If Consumer Crashes?

Suppose visibility timeout = 30 sec.

Consumer receives message at `10:00:00`.

Consumer crashes at `10:00:10`. No delete happens.

At `10:00:30` message becomes visible again.

```text
SQS
 |
 v
Consumer 2
```

Consumer 2 can process it.

---

## 5. Why Visibility Timeout Matters

Suppose processing takes 2 minutes, but visibility timeout is 30 seconds.

Then:

```text
0 sec → receive
30 sec → visible again
```

while Consumer 1 may still be processing.

Another consumer could receive the same message.

Therefore:

> Visibility timeout should generally be long enough to cover normal processing time, with appropriate handling for unusually long jobs.

---

## 6. Can Visibility Timeout Be Changed?

Yes.

A consumer can extend visibility timeout while processing a long-running message using the appropriate SQS visibility-timeout operation.

Conceptually:

```text
Receive
   |
   v
Processing
   |
   | Extend timeout
   v
Processing
   |
   v
Delete
```

---

## 7. At-Least-Once Delivery

SQS Standard provides **at-least-once delivery**.

This means: a message should be delivered at least once, but under some circumstances it can be delivered more than once.

Therefore Message A can go to Consumer 1, then Consumer 1 again.

This is why you need **idempotency**.

---

## 8. What is Idempotency?

An operation is idempotent if performing it multiple times produces the same final result as performing it once.

Example: Payment ₹500.

**Bad:** Message processed → charge ₹500. Duplicate → charge another ₹500. This is dangerous.

Instead:

```text
eventId = ABC123

Store:

ABC123 → PROCESSED

When duplicate arrives:

if eventId already processed:
       skip
else:
       process
       mark processed
```

---

## 9. Example with Database

Suppose `payment_event` table:

```text
event_id | status
---------|---------
ABC123   | SUCCESS
```

Consumer receives `ABC123`. Checks DB. If exists: already processed → skip business operation.

---

## 10. Important Distinction

**Visibility timeout** controls: how long a received message remains hidden before becoming available again.

**Message retention** controls: how long an un-deleted message can remain in the queue.

These are completely different.

---

## 11. Message Retention

SQS retains messages for a configurable period within AWS's supported limits.

Think:

```text
Retention
|
+-------------------------------+
| message can remain in queue   |
+-------------------------------+
```

If message isn't deleted before retention expires, it is removed.

---

## 12. Visibility Timeout vs Retention

Remember:

```text
Retention = How long SQS keeps the message

Visibility timeout = How long message is hidden after receive
```

**Interview question:** Is visibility timeout the same as retention period?

**No.**

---

## DAY 2 INTERVIEW QUESTIONS

### Q1. What happens when consumer receives a message?

SQS makes the message temporarily invisible to other consumers for the configured visibility timeout. The consumer processes it and should explicitly delete it after successful processing.

### Q2. What happens if consumer crashes?

If the message hasn't been deleted and its visibility timeout expires, it becomes visible again and can be received by another consumer.

### Q3. Why do duplicates happen?

Standard SQS provides at-least-once delivery, so duplicate delivery can occur. Applications should therefore implement idempotent processing.

---

# DAY 3 — STANDARD VS FIFO

This is another very common interview topic.

SQS has two important queue types: **Standard** and **FIFO**.

## 1. Standard Queue

Standard queue is designed for: very high throughput, distributed processing, scalability.

But you should assume: **duplicates can happen**, **strict ordering is not guaranteed**.

Example: Producer sends A, B, C, D.

Consumer might receive B, A, D, C — or A, B, B, C, D.

So application logic should not depend on strict ordering.

---

## 2. FIFO Queue

FIFO means **First In, First Out**.

Useful when order matters.

Example: A, B, C, D.

Consumer should process them in FIFO order within the relevant **message group**.

---

## 3. When Would You Use FIFO?

Suppose you process bank-account transactions: Deposit ₹100, Withdraw ₹50, Withdraw ₹20.

Order matters.

If processing becomes Withdraw ₹50 then Deposit ₹100, the result can be different.

FIFO can help maintain ordering requirements.

---

## 4. Message Group ID

FIFO queues support `MessageGroupId`.

Suppose User A → group A, User B → group B.

Messages: A1, A2, A3 and B1, B2, B3.

SQS can process different groups independently while maintaining ordering within each group.

Conceptually:

```text
Group A
A1 → A2 → A3

Group B
B1 → B2 → B3
```

This allows more parallelism than putting every message into one single group.

---

## 5. Deduplication

FIFO also supports deduplication mechanisms.

Suppose producer accidentally sends Payment ABC, Payment ABC within the applicable deduplication window.

FIFO can identify duplicates based on the configured deduplication mechanism.

But don't use this as your only business-level duplicate protection.

You should still design consumers to be **idempotent**.

---

## 6. Standard vs FIFO

| Feature | Standard | FIFO |
| --- | --- | --- |
| Throughput | Very high | High, subject to FIFO configuration/limits |
| Ordering | Not guaranteed | Ordered within message group |
| Duplicate delivery | Possible | Deduplication features available |
| Use case | General async workloads | Ordering-sensitive workflows |
| Scaling | Excellent | Group-based parallelism |

---

## 7. Interview Answer

**"Why did you choose Standard SQS?"**

> We chose Standard SQS because strict ordering wasn't required for our use case. We primarily needed asynchronous processing, buffering and retry behavior with high throughput. We handled possible duplicate processing using idempotency at the consumer.

---

# DAY 4 — RETRIES, DLQ AND POISON MESSAGES

Now we'll move into production reliability.

## 1. What is a Retry?

Suppose:

```text
SQS
 |
 v
Consumer
 |
 v
External API
 |
 X
500 Internal Server Error
```

Processing fails. The message isn't deleted.

After visibility timeout, consumer gets another opportunity to process it.

That's a retry.

---

## 2. Why Retries Are Useful

Temporary failures happen: database unavailable, external API timeout, network issue, pod restart, service deployment.

Retrying can recover from transient failures.

---

## 3. But Retries Can Become Dangerous

Suppose Message A always fails.

Then: Attempt 1 → fail, Attempt 2 → fail, Attempt 3 → fail, ...

This message is called a **Poison Message**.

---

## 4. What is a Poison Message?

A poison message is a message that repeatedly fails processing because of a persistent issue.

Example:

```json
{
   "userId": null
}
```

Consumer requires `userId != null`. So every attempt fails.

Without protection:

```text
Queue
 |
 v
Consumer
 |
 fail
 |
retry
 |
 fail
 |
retry
 |
 fail
```

This wastes resources.

---

## 5. Dead Letter Queue

The solution is **DLQ** — Dead Letter Queue.

Architecture:

```text
              Main Queue
                  |
                  v
              Consumer
                  |
          +-------+-------+
          |               |
       success          failure
          |               |
        delete        retry count
                          |
                          v
                         DLQ
```

---

## 6. Maximum Receive Count

You can configure a redrive policy with a maximum receive count.

Example: `maxReceiveCount = 5`

Meaning conceptually:

```text
Attempt 1 → fail
Attempt 2 → fail
Attempt 3 → fail
Attempt 4 → fail
Attempt 5 → fail

        ↓

       DLQ
```

The exact behavior depends on the queue/redrive configuration, but the interview concept is:

> After a configured number of unsuccessful receives, the message is moved to the DLQ.

---

## 7. Why DLQ?

**Without DLQ:** Poison message → retry forever → consumer resources wasted.

**With DLQ:** Poison message → limited retries → DLQ.

Now normal traffic can continue.

---

## 8. What Do You Do with DLQ?

Don't simply ignore it.

Production architecture should include:

```text
CloudWatch Alarm
       |
       v
DLQ > 0 messages
       |
       v
Alert
       |
       v
Engineer investigates
```

You can inspect: message body, error reason, correlation ID, event ID, failure count, timestamps.

Then decide whether to: fix data, or fix code, or replay message, or discard message.

---

## 9. Retry Strategy

There are two broad failure categories.

**Transient:** HTTP 503, network timeout, database temporarily unavailable. Retry makes sense.

**Permanent:** invalid user ID, malformed JSON, missing required field. Retrying 100 times doesn't help.

Therefore:

```text
Transient → retry
Permanent → DLQ
```

---

## 10. Exponential Backoff

Instead of retry every 1 second, use increasing delays: 1 sec, 2 sec, 4 sec, 8 sec, 16 sec.

This is called **Exponential Backoff**.

It prevents overwhelming a failing downstream service.

---

## 11. SQS and Delay

SQS supports mechanisms for delaying message delivery.

Useful for scenarios such as: retry after 30 sec, retry after 2 min.

For more sophisticated retry scheduling, teams may combine SQS with application logic, schedulers, or other AWS services.

---

# DAY 4 — IDEMPOTENCY

This is extremely important for backend interviews.

Suppose OrderCreated event is processed. Consumer: create order.

But after processing, consumer crashes before `DeleteMessage`.

SQS later delivers it again. Now `create order` runs twice.

### Solution

Use an idempotency key: `eventId = EVENT123`.

Database:

```text
event_id
---------
EVENT123
```

Before processing:

```text
if EVENT123 exists:
    skip
else:
    process
    insert EVENT123
```

### Important interview question

**Does SQS guarantee exactly-once processing?**

For Standard SQS: **No.**

You should design the application to tolerate duplicate deliveries.

A strong answer:

> SQS Standard provides at-least-once delivery, so exactly-once business effects should be achieved through idempotent consumer logic rather than assuming the queue itself guarantees exactly-once processing.

---

# DAY 5 — SCALING AND PERFORMANCE

Now learn how SQS handles large traffic.

## 1. Multiple Consumers

Suppose queue has 100,000 messages. One consumer might be too slow.

Add consumers:

```text
             +--> Consumer 1
             |
SQS ---------+--> Consumer 2
             |
             +--> Consumer 3
             |
             +--> Consumer 4
```

Messages can be processed concurrently.

---

## 2. Consumer Scaling

Suppose queue depth = 100 → you may need 5 consumers.

During traffic spike, queue depth = 100,000 → scale to 50 consumers.

After traffic falls to 100 messages → scale back down.

This is **horizontal scaling**.

---

## 3. What Metric Can Be Used?

One important metric: `ApproximateNumberOfMessagesVisible`.

This indicates queue backlog.

Conceptually:

```text
Queue depth ↑
     |
     v
Need more consumers
```

You can also monitor: message age, processing latency, consumer errors, DLQ count, CPU, memory.

---

## 4. Long Polling

This is another frequently asked question.

### Short polling

Consumer asks: "Any message?"

SQS immediately responds. If no message: no message. Consumer asks again.

```text
poll
poll
poll
poll
```

This creates unnecessary requests.

---

## 5. Long Polling

With long polling:

```text
Consumer
   |
   | "Give me a message"
   v
 SQS
   |
   | waits for messages
   v
Consumer
```

If a message becomes available during the wait, SQS returns it.

**Benefits:** fewer empty responses, fewer API requests, lower unnecessary polling overhead, often better efficiency.

---

## 6. Batch Processing

Instead of Receive 1, Receive 1, Receive 1, you can receive multiple messages in one request where supported.

Conceptually:

```text
ReceiveBatch
   |
   +-- Message A
   +-- Message B
   +-- Message C
   +-- Message D
```

This can improve efficiency.

---

## 7. Batch Deletion

Similarly, after successful processing, Delete A/B/C/D can be batched rather than making individual requests when appropriate.

---

## 8. Consumer Concurrency

Suppose you have 100 messages and 10 consumers.

Each consumer processes approximately 10 messages, assuming similar processing times and sufficient queue parallelism.

More consumers generally increase throughput until you hit another bottleneck.

For example:

```text
SQS
 |
 v
Consumers
 |
 v
Database
```

If DB can handle only 500 writes/sec, adding 100 consumers may just overload the database.

So: **scaling consumers doesn't mean unlimited throughput.**

---

## 9. Backpressure

Suppose Producer = 10,000 msg/sec, Consumer = 2,000 msg/sec.

Then queue backlog increases.

This is actually useful because SQS absorbs the difference temporarily.

But you must monitor **message age** and **backlog**.

---

# DAY 6 — SQS IN PRODUCTION

Now let's connect everything.

Suppose you have Payment Service and Notification Service. Payment service generates events.

Architecture:

```text
                  +------------------+
                  | Payment Service  |
                  +--------+---------+
                           |
                           | SendMessage
                           v
                    +-------------+
                    | SQS Queue   |
                    +------+------+
                           |
                           | Receive
                           v
                +----------------------+
                | Notification Worker  |
                +----------+-----------+
                           |
                           v
                    Notification API
```

---

## 1. Failure Scenario

Notification API returns 500.

Worker doesn't delete the message.

Visibility timeout expires.

Message becomes available again.

```text
SQS
 |
 v
Worker
 |
 v
Notification API
 |
 X 500
 |
 retry
```

---

## 2. Repeated Failure

Suppose `maxReceiveCount = 5`.

After repeated failures:

```text
Main Queue
     |
     v
Consumer
     |
     X
     |
     v
    DLQ
```

---

## 3. Monitoring

You should monitor: queue depth, message age, processing latency, consumer errors, DLQ count, consumer CPU, consumer memory.

---

## 4. SQS + Redis

This is especially useful for your interview preparation because you've worked with Redis-based retry/recovery concepts.

One possible architecture:

```text
              SQS
               |
               v
            Consumer
               |
               v
             Redis
               |
               v
        Retry / state / lock
               |
               v
        External Service
```

But be careful: **don't claim Redis is required for SQS retries.**

SQS itself supports message redelivery after visibility timeout.

Redis may be introduced for application-specific purposes such as: idempotency state, retry metadata, distributed locks, deduplication, temporary state, rate limiting — depending on architecture.

---

## 5. SQS + Kubernetes

Suppose consumers run in Kubernetes:

```text
                 SQS
                  |
       +----------+----------+
       |          |          |
       v          v          v
     Pod 1      Pod 2      Pod 3
       |          |          |
       +----------+----------+
                  |
                  v
              Database
```

If traffic increases: queue depth ↑ → pod count ↑.

When traffic decreases: queue depth ↓ → pod count ↓.

This is a common architecture.

---

## 6. Consumer Crash Scenario

Suppose Pod 1 receives message, then Pod 1 crashes.

No `DeleteMessage` happens.

Visibility timeout expires:

```text
SQS
 |
 v
Pod 2
```

Pod 2 processes it.

This is one of the major reliability benefits of queues.

---

## 7. What If the Pod Crashes After DB Update but Before Deleting SQS?

This is a very important interview scenario.

Suppose:

1. Receive message
2. Update DB
3. Pod crashes
4. `DeleteMessage` never happens

After visibility timeout: message delivered again. Then DB update happens again. Potential duplicate effect.

**Solution: Idempotency.**

For example `eventId = ABC123`.

DB has unique constraint `UNIQUE(event_id)`.

Consumer:

```text
if event already processed:
    don't repeat business operation
else:
    process
```

---

## 8. Exactly-Once Business Effect

This is an important distinction.

You may have **at-least-once message delivery**, but design **exactly-once business effect** using:

- idempotency key
- unique DB constraint
- transactional processing
- deduplication

---

# DAY 7 — INTERVIEW PREPARATION

Now let's convert everything into interview answers.

### Question 1 — "Explain SQS."

> SQS is AWS's managed message queue service used for asynchronous communication between distributed services. A producer sends messages to a queue and consumers pull and process them independently. SQS helps decouple services, absorb traffic spikes, support retries when consumers fail, and independently scale consumers. For Standard queues, delivery is at least once, so consumers should be idempotent.

### Question 2 — "Explain the complete SQS message lifecycle."

```text
Producer
   |
   | SendMessage
   v
SQS
   |
   | ReceiveMessage
   v
Consumer
   |
   | Message becomes invisible
   v
Processing
   |
   +------ Success ------> DeleteMessage
   |
   +------ Failure ------> Visibility expires
                              |
                              v
                         Message visible
                              |
                              v
                            Retry
```

### Question 3 — "What is visibility timeout?"

Visibility timeout is the period for which a message becomes invisible to other consumers after being received. It prevents multiple consumers from immediately processing the same message. If the consumer successfully processes the message, it deletes it. If the consumer crashes or doesn't delete it before the timeout expires, the message becomes visible again.

### Question 4 — "What happens if consumer crashes?"

```text
Receive
   ↓
Visibility Timeout
   ↓
Consumer crashes
   ↓
No DeleteMessage
   ↓
Timeout expires
   ↓
Message visible again
   ↓
Another consumer processes it
```

### Question 5 — "Why can duplicate messages occur?"

Because Standard SQS uses at-least-once delivery. A message can be delivered more than once, particularly around failures and visibility-timeout situations. Therefore the consumer should use an idempotency mechanism.

### Question 6 — "How do you handle duplicate messages?"

> I would assign a unique event or idempotency ID to each business event. Before applying the business operation, the consumer checks whether that ID has already been processed. A database unique constraint or idempotency table can provide stronger protection. If it was already processed, the consumer skips the operation and acknowledges/deletes the duplicate message.

### Question 7 — "What is DLQ?"

A Dead Letter Queue is a separate queue used to isolate messages that repeatedly fail processing. We configure a maximum receive count for the source queue. Once a message exceeds the configured retry threshold, it is moved to the DLQ, preventing poison messages from continuously consuming consumer resources.

### Question 8 — "What is a poison message?"

A poison message is a message that repeatedly fails processing because of a persistent issue, such as invalid data or an unsupported business state. Without a DLQ, it can repeatedly consume consumer capacity. We typically use retry limits and a DLQ to isolate it.

### Question 9 — "Standard vs FIFO?"

> Standard SQS is preferred when we need high throughput and don't require strict ordering. It provides at-least-once delivery, so duplicates are possible. FIFO is used when ordering and deduplication requirements are important. FIFO maintains ordering within a message group, allowing independent groups to be processed concurrently.

### Question 10 — "How would you scale SQS consumers?"

```text
Monitor:
    Queue Depth
    Message Age
    Processing Latency

Queue backlog increases
        ↓
Increase consumer count
        ↓
Process messages faster
        ↓
Backlog decreases
        ↓
Scale consumers down
```

But mention: I would also consider downstream capacity because blindly increasing consumers could overload the database or external services.

### Question 11 — "What is long polling?"

Long polling allows the consumer to wait for messages for a configured period instead of immediately returning an empty response when the queue has no available messages. It reduces unnecessary empty polling requests and improves efficiency.

### Question 12 — "What is message retention?"

Message retention is how long SQS keeps a message available in the queue before automatically removing it if it hasn't been deleted.

### Question 13 — "Visibility timeout vs retention?"

```text
Retention
    ↓
How long SQS keeps message

Visibility Timeout
    ↓
How long received message stays hidden
```

### Question 14 — "Does SQS provide exactly-once processing?"

> Standard SQS does not provide exactly-once processing. It uses at-least-once delivery, so duplicates are possible. If exactly-once business effects are required, I would implement idempotency using an event ID, unique database constraint, or idempotency store.

### Question 15 — "What happens if processing takes longer than visibility timeout?"

Example: Processing = 5 min, Visibility Timeout = 1 min.

Then: 0 min → receive, 1 min → message visible again. Another consumer could receive it.

Therefore: we should configure the visibility timeout based on expected processing duration and extend it when processing can legitimately take longer.

---

## Most Important SQS Architecture

Memorize this:

```text
                         PRODUCER
                            |
                            |
                       SendMessage
                            |
                            v
                  +-------------------+
                  |                   |
                  |    SQS QUEUE      |
                  |                   |
                  +---------+---------+
                            |
                       ReceiveMessage
                            |
             +--------------+--------------+
             |              |              |
             v              v              v
          Worker 1       Worker 2       Worker 3
             |              |              |
             +--------------+--------------+
                            |
                       Business Logic
                            |
                +-----------+-----------+
                |                       |
              Success                 Failure
                |                       |
                v                       v
             Delete                 Retry
                                        |
                                        v
                              Max Receive Count
                                        |
                                        v
                                      DLQ
```

---

## SQS Production Checklist

When designing an SQS-based system, think about these:

1. Standard or FIFO?
2. Message format?
3. Visibility timeout?
4. Retention period?
5. Long polling?
6. Batch receive/delete?
7. Consumer concurrency?
8. Retry strategy?
9. DLQ?
10. Idempotency?

Then add:

11. Monitoring
12. Alerting
13. Autoscaling
14. Downstream capacity
15. Security/IAM

---

## Your 7-Day Study Schedule

Since you want this daywise, don't try to consume everything at once.

### Day 1 — 1.5–2 hours

Learn: What is SQS? Why SQS? Producer, Consumer, Queue, Message, SendMessage, ReceiveMessage, DeleteMessage, Async communication, Decoupling, Buffering.

Practice explaining: "Explain SQS to an interviewer in 60 seconds."

### Day 2 — 2 hours

Learn deeply: Message lifecycle, Visibility Timeout, Message retention, Consumer crash, Message redelivery, At-least-once delivery, Duplicate messages, Idempotency.

Practice this scenario: Consumer updates DB successfully but crashes before deleting SQS message. What happens?

### Day 3 — 1.5–2 hours

Learn: Standard, FIFO, Ordering, MessageGroupId, Deduplication, Throughput.

Practice: Why would you choose Standard instead of FIFO?

### Day 4 — 2 hours

Learn: Retries, DLQ, Poison messages, Max receive count, Transient vs permanent errors, Exponential backoff, Idempotency.

Practice: How would you prevent one bad message from continuously blocking your consumers?

### Day 5 — 2 hours

Learn: Long polling, Short polling, Batching, Consumer scaling, Queue depth, Message age, Backpressure, Autoscaling.

Practice: How would you process 1 million SQS messages efficiently?

### Day 6 — 2–3 hours

Build architecture mentally: SQS + Spring Boot, SQS + Redis, SQS + MySQL, SQS + Kubernetes, SQS + Lambda, SQS + external API.

Practice failure scenarios: Consumer crash, DB crash, External API timeout, Duplicate message, Poison message, Queue backlog, Consumer overload.

### Day 7 — 2–3 hours

Do only interview practice. You should be able to answer without notes:

What is SQS? Why SQS? Standard vs FIFO? Visibility timeout? Retention? Long polling? At-least-once? Duplicate messages? Idempotency? DLQ? Poison message? Retry? Consumer crash? Scaling consumers? Queue backlog? Batch processing? SQS vs Kafka? SQS vs RabbitMQ? SQS + Kubernetes? SQS + Redis?

---

## One Important Thing for Your Interviews

Given your backend experience, don't stop at definitions.

Interviewers often ask:

> Okay, but what happens if the consumer crashes?

Then: What if it crashes after updating DB? What if the message is delivered twice? How do you prevent duplicate payment? What if the downstream API is down for 30 minutes? Would you retry forever? What happens to the poison message? How do you monitor it?

That chain is where you demonstrate real backend understanding.

The core mental model to remember is:

```text
                SQS
                 |
           Receive Message
                 |
                 v
        Visibility Timeout
                 |
          +------+------+
          |             |
       Success        Failure
          |             |
       Delete       Timeout expires
                        |
                        v
                      Retry
                        |
                  Retry limit
                        |
                        v
                       DLQ
```

And underneath all of this:

```text
At-least-once delivery
          ↓
Duplicates possible
          ↓
Idempotent consumer required
```

Next, I recommend doing Day 1 practically with a Spring Boot producer + consumer and AWS SQS, including AWS Console setup, IAM, queue creation, Java code, message sending/receiving/deleting, and a hands-on failure experiment.
