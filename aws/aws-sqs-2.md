# Amazon SQS — Complete Tutorial for SDE-1 / HLD Interviews

Amazon Web Services SQS (Simple Queue Service) is a managed message queue used to decouple services.

The easiest way to understand SQS is:

Producer puts a message into SQS → Consumer processes it asynchronously → Consumer deletes it after successful processing.

---

## Table of Contents

- [1. Why do we need SQS?](#1-why-do-we-need-sqs)
- [2. Basic SQS architecture](#2-basic-sqs-architecture)
- [3. Producer](#3-producer)
- [4. Consumer](#4-consumer)
- [5. What happens when consumer receives a message?](#5-what-happens-when-consumer-receives-a-message)
- [6. Visibility Timeout](#6-visibility-timeout)
- [7. Very important interview question](#7-very-important-interview-question)
- [8. Standard Queue vs FIFO Queue](#8-standard-queue-vs-fifo-queue)
- [9. Standard Queue](#9-standard-queue)
- [10. FIFO Queue](#10-fifo-queue)
- [11. MessageGroupId](#11-messagegroupid)
- [12. SQS vs Kafka](#12-sqs-vs-kafka)
- [13. SQS Message Lifecycle](#13-sqs-message-lifecycle)
- [14. DeleteMessage](#14-deletemessage)
- [15. What if processing takes longer than visibility timeout?](#15-what-if-processing-takes-longer-than-visibility-timeout)
- [16. Long Polling](#16-long-polling)
- [17. Dead Letter Queue — DLQ](#17-dead-letter-queue--dlq)
- [18. maxReceiveCount](#18-maxreceivecount)
- [19. Poison Message](#19-poison-message)
- [20. Idempotency](#20-idempotency)
- [21. How to implement idempotency with Redis](#21-how-to-implement-idempotency-with-redis)
- [22. SQS and Database Transaction Problem](#22-sqs-and-database-transaction-problem)
- [23. Batch Processing](#23-batch-processing)
- [24. Delay Queue](#24-delay-queue)
- [25. Message Retention](#25-message-retention)
- [26. Message Size](#26-message-size)
- [27. SQS Encryption](#27-sqs-encryption)
- [28. IAM](#28-iam)
- [29. SQS + Auto Scaling](#29-sqs--auto-scaling)
- [30. SQS doesn't push messages to consumers](#30-sqs-doesnt-push-messages-to-consumers)
- [31. SQS Standard Queue Architecture](#31-sqs-standard-queue-architecture)
- [32. What happens if all consumers go down?](#32-what-happens-if-all-consumers-go-down)
- [33. SQS vs REST](#33-sqs-vs-rest)
- [34. SQS vs SNS](#34-sqs-vs-sns)
- [35. SNS + SQS architecture](#35-sns--sqs-architecture)
- [36. SQS + Lambda](#36-sqs--lambda)
- [37. Important SQS metrics](#37-important-sqs-metrics)
- [38. In-flight messages](#38-in-flight-messages)
- [39. Important problem: Visibility timeout too small](#39-important-problem-visibility-timeout-too-small)
- [40. Important problem: Visibility timeout too large](#40-important-problem-visibility-timeout-too-large)
- [41. At-least-once delivery](#41-at-least-once-delivery)
- [42. Exactly-once misconception](#42-exactly-once-misconception)
- [43. SQS FIFO deduplication](#43-sqs-fifo-deduplication)
- [44. SQS FIFO ordering](#44-sqs-fifo-ordering)
- [45. How many consumers can you have?](#45-how-many-consumers-can-you-have)
- [46. Backpressure](#46-backpressure)
- [47. SQS Queue Design Example](#47-sqs-queue-design-example)
- [48. How would you prevent duplicate processing?](#48-sqs-interview-question-how-would-you-prevent-duplicate-processing)
- [49. What happens if consumer crashes?](#49-interview-question-what-happens-if-consumer-crashes)
- [50. Why use SQS instead of direct HTTP?](#50-interview-question-why-use-sqs-instead-of-direct-http)
- [51. How do you handle ordering?](#51-interview-question-how-do-you-handle-ordering)
- [52. How do you handle poison messages?](#52-interview-question-how-do-you-handle-poison-messages)
- [53. How do you scale consumers?](#53-interview-question-how-do-you-scale-consumers)
- [54. What is long polling?](#54-interview-question-what-is-long-polling)
- [55. Visibility timeout vs retention](#55-interview-question-what-is-the-difference-between-visibility-timeout-and-retention)
- [56. Important SQS numbers to memorize](#56-important-sqs-numbers-to-memorize)
- [57. SQS pricing concept](#57-sqs-pricing-concept)
- [58. SQS complete mental model](#58-sqs-complete-mental-model)
- [59. SQS vs Kafka — interview cheat sheet](#59-sqs-vs-kafka--interview-cheat-sheet)
- [60. One complete HLD example](#60-one-complete-hld-example)
- [Your SQS learning order](#your-sqs-learning-order)

---

## 1. Why do we need SQS?

Suppose you have:

```text
User
  |
  v
Order Service
  |
  v
Payment Service
```

When a user places an order, Order Service directly calls Payment Service.

If Payment Service is slow/down:

```text
Order Service
     |
     | HTTP
     v
Payment Service ❌
```

The request may fail.

Instead:

```text
                 SQS
                  |
Order Service --> Queue --> Payment Service
```

Now Order Service doesn't need Payment Service to be available immediately.

It puts:

```json
{
  "orderId": "ORD123",
  "amount": 1500,
  "userId": "U100"
}
```

into SQS.

Payment Service processes it later.

### Main benefit

Decoupling.

### Other benefits

- asynchronous processing
- buffering traffic spikes
- retry mechanism
- horizontal scaling
- fault isolation
- load leveling

---

## 2. Basic SQS architecture

There are three important components:

```text
Producer
   |
   | SendMessage
   v
+----------------+
|      SQS       |
|     Queue      |
+----------------+
   |
   | ReceiveMessage
   v
Consumer
   |
   | Process
   v
Database
```

### Example

```text
Order Service
     |
     | Send order event
     v
Order Queue
     |
     | Receive
     v
Payment Worker
     |
     v
Payment DB
```

---

## 3. Producer

Producer creates and sends messages.

Example:

```java
sqsClient.sendMessage(
    SendMessageRequest.builder()
        .queueUrl(queueUrl)
        .messageBody(message)
        .build()
);
```

The producer does not need to wait for the consumer.

---

## 4. Consumer

Consumer continuously polls SQS.

```text
Consumer
   |
   | ReceiveMessage
   v
 SQS
   |
   v
Message
   |
   v
Process
   |
   v
DeleteMessage
```

**Important:**

Receiving a message does not remove it from SQS.

This is one of the most important SQS interview concepts.

---

## 5. What happens when consumer receives a message?

Suppose queue contains:

```text
M1
M2
M3
```

Consumer asks:

```text
ReceiveMessage()
```

SQS gives:

```text
M1
```

But SQS does not immediately delete M1.

Instead, M1 becomes temporarily invisible.

This is called:

**Visibility Timeout**

---

## 6. Visibility Timeout

Suppose:

Visibility Timeout = 30 seconds

Consumer receives:

```text
M1
```

For the next 30 seconds:

M1 → invisible to other consumers

Consumer processes:

```text
M1
```

If processing succeeds:

```text
DeleteMessage(M1)
```

Done.

### What if consumer crashes?

Suppose:

- 10:00:00 → receive M1
- 10:00:00 → visibility timeout starts
- 10:00:10 → consumer crashes

M1 has not been deleted.

After 30 seconds:

M1 becomes visible again

Another consumer can process it.

```text
Consumer 1
   |
   | M1
   v
 SQS
   X
 crash

after timeout

 SQS
   |
   v
Consumer 2
   |
   v
 M1
```

AWS documents the visibility timeout as the period during which a received message is hidden from other consumers; the default is 30 seconds and the maximum is 12 hours.

---

## 7. Very important interview question

**Why doesn't SQS delete the message immediately after receiving it?**

Because the consumer might fail while processing.

Example:

```text
Receive
   ↓
Process payment
   ↓
Update DB
   ↓
Delete message
```

If SQS deleted immediately:

```text
Receive
   ↓
SQS deletes
   ↓
Consumer crashes
   ↓
Payment never processed
```

Message would be lost.

Therefore:

Receive first, process, then explicitly delete after successful processing.

---

## 8. Standard Queue vs FIFO Queue

SQS has two major queue types:

| Feature | Standard | FIFO |
| --- | --- | --- |
| Throughput | Very high | Lower / controlled |
| Ordering | Best effort | Ordered within message group |
| Duplicate delivery | Possible | Deduplication supported |
| Use case | Most async workloads | Ordering-sensitive workflows |
| Message group | Not required | Required |
| Complexity | Lower | Higher |

AWS describes Standard queues as providing at-least-once delivery and best-effort ordering, while FIFO queues provide ordered processing within message groups and deduplication features.

---

## 9. Standard Queue

Example:

```text
M1
M2
M3
M4
```

Consumers:

```text
Consumer 1 → M1
Consumer 2 → M2
Consumer 3 → M3
Consumer 4 → M4
```

Very scalable.

But don't assume:

```text
M1 → M2 → M3
```

will always be processed in exactly that order.

Also, duplicate delivery can happen.

Therefore consumers should generally be idempotent.

---

## 10. FIFO Queue

FIFO = First In First Out

Example:

```text
M1
M2
M3
```

Processing maintains ordering within a message group:

```text
M1 → M2 → M3
```

Useful for:

- financial transactions
- order state changes
- inventory updates
- sequential workflows

FIFO queue names end with:

`.fifo`

and messages require a `MessageGroupId`.

---

## 11. MessageGroupId

This is extremely important.

Suppose:

- Order 1
- Order 2
- Order 3

You could use:

`MessageGroupId = customerId`

For customer A:

```text
Customer A

M1
M2
M3
```

These maintain ordering.

But customer B can be processed independently:

```text
Customer A → M1 → M2 → M3
Customer B → X1 → X2 → X3
```

So you get:

**ordering + parallelism**

This is similar to partition-key thinking in Kafka.

---

## 12. SQS vs Kafka

This is a very common interview question.

| SQS | Kafka |
| --- | --- |
| Managed queue | Distributed event streaming platform |
| Consumer deletes message | Consumers track offsets |
| Message disappears after deletion | Records remain for retention period |
| Simpler | More complex |
| Great for task queues | Great for event streaming |
| AWS-native | Multi-platform |
| No partition management for Standard | Partition-based |
| Replay is not Kafka-style | Replay is fundamental |

Simple mental model

### SQS

Task queue

```text
Producer
   ↓
SQS
   ↓
Worker
   ↓
Delete
```

### Kafka

Event log

```text
Producer
   ↓
Kafka
   ↓
Partition
   ↓
Consumer
   ↓
Offset
```

---

## 13. SQS Message Lifecycle

Remember this flow:

```text
             Send
Producer ──────────────> SQS
                           |
                           | Receive
                           v
                       Consumer
                           |
                     Processing
                           |
               +-----------+-----------+
               |                       |
            Success                  Failure
               |                       |
               v                       v
          DeleteMessage          Visibility expires
                                       |
                                       v
                                  Retry message
```

This is probably the most important SQS diagram to remember.

---

## 14. DeleteMessage

After successful processing:

```java
deleteMessage(receiptHandle);
```

**Important:**

You delete using the receipt handle, not simply the message ID.

Conceptually:

```text
ReceiveMessage
     ↓
Message + ReceiptHandle
     ↓
Process
     ↓
DeleteMessage(receiptHandle)
```

---

## 15. What if processing takes longer than visibility timeout?

Suppose:

- Visibility timeout = 30 sec
- Processing = 2 minutes

### Problem

```text
0 sec → receive
30 sec → message becomes visible
40 sec → another consumer receives it
```

Now two consumers may process the same message.

### Solution

`ChangeMessageVisibility`

Extend the timeout.

```text
Receive
  |
  | 30 sec
  |
processing
  |
  | ChangeMessageVisibility
  v
another 60 sec
```

AWS specifically recommends adjusting visibility timeout to processing time and using `ChangeMessageVisibility` when processing needs more time.

---

## 16. Long Polling

There are two ways to receive messages.

### Short polling

Consumer asks:

Any messages?

SQS responds immediately.

If empty:

No messages

Consumer asks again.

This can generate many unnecessary API calls.

### Long polling

Consumer says:

Wait up to 20 seconds for messages.

If a message arrives:

SQS → return message

Otherwise:

after 20 sec → empty response

SQS supports receive wait time from 0–20 seconds; non-zero wait time enables long polling.

### Interview answer

> Long polling reduces empty ReceiveMessage calls and therefore reduces unnecessary API calls and cost.

---

## 17. Dead Letter Queue — DLQ

Suppose message keeps failing:

```text
M1
 ↓
Consumer
 ↓
FAIL
 ↓
retry
 ↓
FAIL
 ↓
retry
 ↓
FAIL
```

This could continue forever.

Instead:

```text
Main Queue
    |
    | failed multiple times
    v
  DLQ
```

DLQ = Dead Letter Queue

Example:

```text
Order Queue
     |
     | 5 failures
     v
Order DLQ
```

Then engineers investigate the failed message.

---

## 18. maxReceiveCount

You can configure something like:

`maxReceiveCount = 5`

Meaning:

```text
Attempt 1 → failure
Attempt 2 → failure
Attempt 3 → failure
Attempt 4 → failure
Attempt 5 → failure
             ↓
            DLQ
```

This prevents poison messages from continuously consuming workers.

---

## 19. Poison Message

Example:

```json
{
  "orderId": null
}
```

Your application cannot process it.

Every time:

```text
Receive
 ↓
Fail
 ↓
Retry
 ↓
Fail
```

This is a poison message.

DLQ is a standard way to isolate it.

---

## 20. Idempotency

This is extremely important for SQS interviews.

Suppose:

Payment message

`transactionId = TX100`

Consumer processes:

Charge ₹1000

But before deleting the message:

Consumer crashes

Visibility timeout expires.

Message comes again.

Now:

Charge ₹1000

again.

Customer could be charged twice.

Therefore:

Consumer processing should be idempotent.

---

## 21. How to implement idempotency with Redis

Suppose:

`transactionId = TX100`

Before processing:

```redis
SETNX processed:TX100 1
```

If successful:

process payment

If key already exists:

duplicate → skip

Conceptually:

```text
SQS Message
     |
     v
transactionId
     |
     v
Redis SETNX
   /     \
new      exists
 |          |
process    skip
```

But there is an important caveat:

Do not blindly mark a message processed before the business operation succeeds, otherwise a crash between the idempotency write and the business transaction can cause the work to be skipped.

For critical workflows, use a database idempotency record / unique constraint or an atomic transactional design appropriate to the business operation.

---

## 22. SQS and Database Transaction Problem

Classic interview question:

```text
Receive SQS message
       ↓
Update DB
       ↓
Delete SQS message
```

What if:

```text
DB update succeeds
       ↓
application crashes
       ↓
DeleteMessage never happens
```

Message comes again.

Now DB operation happens twice.

Therefore:

SQS + DB

does not automatically give you exactly-once business processing.

Use:

- idempotency
- unique constraints
- transactional database operations
- idempotency keys
- carefully designed retry handling

---

## 23. Batch Processing

SQS supports batching.

Instead of:

```text
Receive
Receive
Receive
Receive
Receive
```

you can receive multiple messages.

A batch can contain up to 10 messages.

Conceptually:

```text
SQS
 |
 +---- M1
 +---- M2
 +---- M3
 +---- M4
 +---- M5
```

Consumer processes them together.

### Benefits

- fewer API calls
- better throughput
- lower overhead

---

## 24. Delay Queue

Sometimes you don't want a message processed immediately.

Example:

```text
Send message
     ↓
wait 5 minutes
     ↓
process
```

SQS supports delivery delay up to 15 minutes.

Example:

```text
Order created
     ↓
SQS
     ↓
delay 60 sec
     ↓
Worker
```

Useful for:

- delayed jobs
- retry scheduling
- reminder workflows

---

## 25. Message Retention

How long does SQS keep a message?

**Default:**

4 days

**Configurable:**

1 minute → 14 days

After retention expires, the message is automatically deleted.

---

## 26. Message Size

Current SQS message size limit:

**1 MiB**

If you need larger payloads, AWS provides an Extended Client approach that stores the large payload in S3 and puts a reference in SQS; the documented S3 payload size can reach 2 GB.

Don't do:

```json
{
   "hugeData": "....many MB..."
}
```

Prefer:

```json
{
   "fileId": "123",
   "s3Key": "orders/123.json"
}
```

---

## 27. SQS Encryption

SQS supports server-side encryption.

You can use AWS-managed encryption or KMS-backed encryption depending on requirements.

Think:

```text
Producer
   |
 encrypted
   v
 SQS
```

Useful when messages contain sensitive business information.

AWS states that server-side encryption is applied by default to SQS queues, with SQS-managed or KMS-based options available.

---

## 28. IAM

Who can send/read/delete?

AWS IAM controls permissions.

Example producer:

```text
OrderServiceRole
     |
     +-- sqs:SendMessage
```

Consumer:

```text
PaymentServiceRole
     |
     +-- sqs:ReceiveMessage
     +-- sqs:DeleteMessage
     +-- sqs:ChangeMessageVisibility
```

Follow least privilege.

Don't give:

`*`

if the service only needs access to one queue.

---

## 29. SQS + Auto Scaling

This is a very important HLD pattern.

Suppose:

```text
Producer
   |
   v
SQS
```

Messages increase:

- Queue depth = 10
- Queue depth = 10,000
- Queue depth = 100,000

We can increase consumers:

```text
             SQS
              |
       +------+------+------+
       |      |      |      |
      C1     C2     C3     C4
```

If queue backlog becomes large:

```text
2 workers
   ↓
10 workers
```

This is horizontal scaling.

A common metric is:

`ApproximateNumberOfMessagesVisible`

You can scale workers based on queue depth and processing latency.

---

## 30. SQS doesn't push messages to consumers

This is another interview question.

Many beginners think:

SQS → automatically sends message → Consumer

Instead, consumers normally poll SQS:

```text
Consumer
   |
   | ReceiveMessage
   v
 SQS
```

Long polling makes this efficient.

---

## 31. SQS Standard Queue Architecture

For an order-processing system:

```text
                  +----------------+
                  |   Order API    |
                  +-------+--------+
                          |
                          | SendMessage
                          v
                    +-----------+
                    |    SQS    |
                    |   Queue   |
                    +-----+-----+
                          |
             +------------+------------+
             |            |            |
             v            v            v
          Worker 1     Worker 2     Worker 3
             |            |            |
             +------------+------------+
                          |
                          v
                       Database
```

This gives:

- decoupling
- buffering
- parallel processing
- fault tolerance
- horizontal scaling

---

## 32. What happens if all consumers go down?

Suppose:

```text
Producer
   |
   v
 SQS
   |
   X
Consumers DOWN
```

Messages remain in SQS until they are processed or retention expires.

When consumers come back:

```text
SQS
 |
 +--> Worker 1
 +--> Worker 2
 +--> Worker 3
```

They process the backlog.

This is why queues are useful for handling temporary consumer outages.

---

## 33. SQS vs REST

### REST

```text
Service A
   |
   | HTTP request
   v
Service B
```

Synchronous.

Service A waits.

### SQS

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

Asynchronous.

Service A doesn't need to wait for Service B to finish processing.

---

## 34. SQS vs SNS

Very important AWS interview question.

### SQS

One queue → workers consume tasks.

```text
Producer
   |
   v
 SQS
   |
   +--> Consumer
```

### SNS

One publisher → multiple subscribers.

```text
             SNS
              |
       +------+------+
       |      |      |
      SQS    SQS   Lambda
```

SNS is commonly used for fan-out.

SQS is commonly used for durable asynchronous consumption.

---

## 35. SNS + SQS architecture

This is a powerful AWS pattern.

Suppose Order Service generates:

`OrderCreated`

Three systems need it:

```text
                 SNS
                  |
        +---------+---------+
        |         |         |
        v         v         v
      SQS       SQS       SQS
       |          |         |
    Payment    Shipping   Email
```

### Advantages

- loose coupling
- independent consumers
- each consumer can retry independently
- each consumer has its own queue
- consumers can scale independently

---

## 36. SQS + Lambda

AWS Lambda can consume SQS messages.

Architecture:

```text
SQS
 |
 v
Lambda
 |
 v
Database
```

AWS manages the polling infrastructure.

Useful for:

- background processing
- serverless applications
- lightweight workers

---

## 37. Important SQS metrics

For production/HLD interviews, know these:

### Queue depth

`ApproximateNumberOfMessagesVisible`

How many messages are waiting.

### In-flight messages

Messages that have been received but not deleted.

### Age of oldest message

Very useful for detecting processing delays.

Example:

Oldest message age = 5 minutes

Maybe consumers are falling behind.

### DLQ message count

DLQ = 500 messages

Indicates repeated failures.

---

## 38. In-flight messages

Suppose:

```text
SQS
1000 messages
```

Consumer receives:

100

but hasn't deleted them.

Those are in-flight.

AWS documents an in-flight limit of 120,000 messages for both Standard and FIFO queues in supported regions.

---

## 39. Important problem: Visibility timeout too small

Suppose:

- Processing time = 2 minutes
- Visibility timeout = 30 sec

Then:

```text
0 sec → Consumer 1 receives
30 sec → visible again
40 sec → Consumer 2 receives
```

Duplicate processing.

### Solution

Set visibility timeout appropriately:

Visibility timeout > normal processing time

And extend it dynamically when required.

---

## 40. Important problem: Visibility timeout too large

Suppose:

Visibility timeout = 2 hours

Consumer crashes immediately.

Message remains invisible for 2 hours.

That delays retry.

Therefore:

Visibility timeout should be long enough to normally complete processing, but not unnecessarily long.

---

## 41. At-least-once delivery

Standard SQS should be treated as:

**at least once**

Meaning:

Message may be delivered:

- 1 time
- or
- more than 1 time

Therefore:

**Consumer must handle duplicates**

This is a critical interview statement.

---

## 42. Exactly-once misconception

Be careful saying:

> "SQS provides exactly-once processing."

That's too broad.

FIFO provides deduplication capabilities for messages, but your entire business operation is not automatically exactly once.

Example:

```text
SQS
 ↓
Payment service
 ↓
Bank
```

Even if SQS prevents a duplicate message from being introduced during its deduplication window, the external payment operation still needs its own idempotency mechanism.

AWS's FIFO deduplication window is 5 minutes.

---

## 43. SQS FIFO deduplication

Suppose producer sends:

`TX100`

Then network fails and producer retries.

With FIFO deduplication:

```text
TX100
TX100
```

SQS can recognize the duplicate within the deduplication window.

You can provide:

`MessageDeduplicationId`

or enable content-based deduplication.

---

## 44. SQS FIFO ordering

Suppose:

`MessageGroupId = USER123`

Messages:

```text
M1
M2
M3
```

Processing order:

```text
M1 → M2 → M3
```

But another group:

`USER456`

can be processed independently.

So:

```text
USER123 → M1 → M2 → M3
                 ↘
USER456 → X1 → X2 → X3
```

This provides ordered processing within each group while allowing parallelism across groups.

---

## 45. How many consumers can you have?

For Standard SQS:

```text
Queue
 |
 +--- Consumer 1
 +--- Consumer 2
 +--- Consumer 3
 +--- Consumer 4
 ...
```

You can horizontally scale workers.

But scaling consumers doesn't magically create more useful parallelism if your downstream database is the bottleneck.

Example:

```text
100 consumers
     |
     v
Database
     X
 max connections
```

So always identify the bottleneck.

---

## 46. Backpressure

Suppose producer generates:

10,000 msg/sec

but consumers process:

5,000 msg/sec

Then:

Queue depth increases

SQS acts as a buffer:

```text
Producer
  |
  | 10k/sec
  v
 SQS
  |
  | 5k/sec
  v
Consumers
```

Backlog:

+5k/sec

You can increase consumers if the downstream systems can handle the additional load.

---

## 47. SQS Queue Design Example

Let's design:

Process video uploads asynchronously.

User uploads video.

We don't want HTTP request to wait for video processing.

### Architecture

```text
              User
                |
                v
          Upload Service
                |
                v
               S3
                |
                | event
                v
              SQS
                |
       +--------+--------+
       |        |        |
       v        v        v
    Worker1   Worker2   Worker3
       |        |        |
       +--------+--------+
                |
                v
          Video Processor
```

### Message

```json
{
  "videoId": "V123",
  "bucket": "videos",
  "key": "uploads/V123.mp4"
}
```

### Worker

```text
Receive
   ↓
Download/read S3 object
   ↓
Process video
   ↓
Update DB
   ↓
Delete SQS message
```

If processing fails:

```text
Visibility timeout
       ↓
Retry
       ↓
Retry
       ↓
DLQ
```

---

## 48. SQS Interview Question: "How would you prevent duplicate processing?"

> SQS Standard provides at-least-once delivery, so duplicate processing is possible. I would make the consumer idempotent using a unique business identifier such as transactionId/orderId, backed by a database unique constraint or an idempotency record. If appropriate, Redis can also be used for fast duplicate detection, but the business operation and idempotency state need careful atomicity.

---

## 49. Interview Question: "What happens if consumer crashes?"

> The message is not deleted. Once the visibility timeout expires, the message becomes visible again and another consumer can process it. If failures continue, I would configure a DLQ with an appropriate maxReceiveCount.

---

## 50. Interview Question: "Why use SQS instead of direct HTTP?"

> SQS decouples producer and consumer. The producer doesn't need the consumer to be available immediately. It also provides buffering during traffic spikes, retry behavior through visibility timeout, and allows consumers to scale horizontally.

---

## 51. Interview Question: "How do you handle ordering?"

> If ordering isn't important, I would use Standard SQS. If strict ordering is required, I would use FIFO and choose an appropriate MessageGroupId. Messages within the same group maintain ordering, while different groups can be processed independently.

---

## 52. Interview Question: "How do you handle poison messages?"

```text
Main Queue
    |
    | repeated failure
    v
   DLQ
```

Configure:

`maxReceiveCount`

Then monitor DLQ and investigate failed messages.

---

## 53. Interview Question: "How do you scale consumers?"

Monitor:

- Queue depth
- Age of oldest message
- Consumer processing latency
- Downstream capacity

Then:

```text
queue depth ↑
      ↓
increase workers
      ↓
queue depth ↓
```

But don't scale beyond the capacity of the database/downstream services.

---

## 54. Interview Question: "What is long polling?"

Simple answer:

> Long polling allows the consumer to wait for messages instead of immediately receiving an empty response. SQS supports waiting up to 20 seconds. It reduces unnecessary ReceiveMessage calls and improves efficiency.

---

## 55. Interview Question: "What is the difference between visibility timeout and retention?"

Very important:

### Visibility timeout

Controls:

How long a received message stays hidden

Example:

30 sec

### Retention period

Controls:

How long SQS keeps the message

Example:

4 days

So:

- **Retention** = lifetime in queue
- **Visibility** = temporary invisibility after receive

---

## 56. Important SQS numbers to memorize

| Concept | Value |
| --- | --- |
| Default visibility timeout | 30 sec |
| Maximum visibility timeout | 12 hours |
| Default retention | 4 days |
| Maximum retention | 14 days |
| Maximum message size | 1 MiB |
| Long polling maximum | 20 sec |
| Maximum delay | 15 min |
| Messages per batch | 10 |
| In-flight messages | 120,000 |
| FIFO deduplication interval | 5 min |

These values are from current AWS documentation.

---

## 57. SQS pricing concept

SQS is usage-based.

AWS currently states that customers get 1 million SQS requests/month in the Free Tier, subject to the applicable Free Tier terms. Requests are metered by API actions, and batching can reduce API-call overhead.

For HLD:

Use long polling and batching to reduce unnecessary API calls and improve throughput/cost efficiency.

---

## 58. SQS complete mental model

If you remember only this:

```text
                 PRODUCER
                    |
                    |
              SendMessage
                    |
                    v
          +------------------+
          |       SQS        |
          |      Queue       |
          +--------+---------+
                   |
            ReceiveMessage
                   |
                   v
              CONSUMER
                   |
              Process
             /       \
         Success     Failure
            |           |
            v           v
      DeleteMessage   Visibility
                       expires
                          |
                          v
                       Retry
                          |
                    repeated failures
                          |
                          v
                         DLQ
```

And for scaling:

```text
                 Producer
                    |
                    v
                  SQS
                    |
        +-----------+-----------+
        |           |           |
       C1          C2          C3
        |           |           |
        +-----------+-----------+
                    |
                    v
                 Database
```

---

## 59. SQS vs Kafka — interview cheat sheet

Since you're preparing HLD, this distinction is especially important.

### Use SQS when

"I have background jobs/tasks."

Examples:

- send email
- process image
- generate report
- process payment
- resize video
- asynchronous API work

### Use Kafka when

"I have a stream of events that multiple consumers may independently consume and replay."

Examples:

- transaction events
- analytics
- event-driven microservices
- clickstream
- audit events
- large-scale event pipelines

---

## 60. One complete HLD example

### Requirement

Design an asynchronous payment processing system.

```text
                Client
                  |
                  v
             Order API
                  |
             Create Order
                  |
                  v
              Database
                  |
                  |
             SendMessage
                  |
                  v
          +---------------+
          |     SQS       |
          | Payment Queue |
          +-------+-------+
                  |
       +----------+----------+
       |          |          |
       v          v          v
    Worker1    Worker2    Worker3
       |          |          |
       +----------+----------+
                  |
                  v
             Payment DB
                  |
                  v
            Payment Gateway
```

### Failure handling

```text
Payment Worker
      |
      v
Payment Gateway
   /       \
success    failure
  |          |
  v          v
delete     retry
message      |
             v
         max attempts
             |
             v
            DLQ
```

### Duplicate handling

```text
transactionId
      |
      v
Idempotency record
      |
  +---+---+
  |       |
new     exists
 |         |
process   skip
```

### Scaling

```text
Queue depth ↑
     |
     v
Increase workers
     |
     v
Queue depth ↓
```

### Monitoring

Track:

- Queue depth
- Oldest message age
- Processing latency
- Error rate
- DLQ count
- Consumer count

This is the kind of explanation I'd give in an SDE-1 HLD interview.

---

## Your SQS learning order

For interviews, learn it in this order:

```text
1. Why message queues?
        ↓
2. SQS architecture
        ↓
3. Send / Receive / Delete
        ↓
4. Visibility Timeout ⭐
        ↓
5. At-least-once delivery ⭐
        ↓
6. Idempotency ⭐
        ↓
7. Standard vs FIFO ⭐
        ↓
8. MessageGroupId
        ↓
9. Long Polling
        ↓
10. DLQ ⭐
        ↓
11. Retry
        ↓
12. Batch processing
        ↓
13. Scaling consumers
        ↓
14. SNS + SQS
        ↓
15. Lambda + SQS
        ↓
16. SQS vs Kafka ⭐
        ↓
17. HLD architecture
```

The five concepts I would make sure you can explain without notes are:

Visibility Timeout → At-least-once delivery → Idempotency → DLQ → Standard vs FIFO.
