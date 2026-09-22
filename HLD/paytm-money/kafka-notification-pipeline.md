# Kafka Notification Pipeline — Paytm Money PDN/DN

## Resume bullet

> Sustained a 100–150 TPS traffic increase by designing an asynchronous, Kafka-driven pipeline delivering Pre-Debit (PDN) and Debit Notifications (DNs) for ~80K daily SIPs.

---

## Table of Contents

- [Part 1 — Architecture](#part-1--architecture)
  - [1. First Understand the Problem](#1-first-understand-the-problem)
  - [2. What Exactly Is PDN and DN?](#2-what-exactly-is-pdn-and-dn)
  - [3. Why Did the Old Synchronous Architecture Create a Problem?](#3-why-did-the-old-synchronous-architecture-create-a-problem)
  - [4. Why Kafka Solves This](#4-why-kafka-solves-this)
  - [5. What Happens When a SIP Event Occurs?](#5-what-exactly-happens-when-a-sip-event-occurs)
  - [6. Why 20 Partitions?](#6-why-20-partitions)
  - [7. Why 8 Consumers If There Are 20 Partitions?](#7-why-8-consumers-if-there-are-20-partitions)
  - [8. Why Not Just Use 20 Consumers?](#8-why-not-just-use-20-consumers)
  - [9. Why Kafka Instead of RabbitMQ?](#9-why-kafka-instead-of-rabbitmq)
  - [10. Why Kafka Instead of Redis?](#10-why-kafka-instead-of-redis)
  - [11. Why Asynchronous Architecture?](#11-why-asynchronous-architecture)
  - [12. What If Notification Service Is Down?](#12-what-happens-if-notification-service-is-down)
  - [13. What If Consumer Crashes?](#13-what-happens-if-consumer-crashes)
  - [14. What Is an Offset?](#14-what-is-an-offset)
  - [15. Duplicate Notification Processing](#15-what-if-the-same-notification-is-processed-twice)
  - [16. Partition Key](#16-what-key-did-you-use-for-kafka-partitioning)
  - [17–24. Ordering, Brokers, Producer, Consumer](#17-why-is-partition-key-important)
  - [25. Complete Flow Diagram](#25-what-does-the-complete-flow-look-like)
  - [26–40. CPU, Metrics, Lag, Dual-Write, Schema](#26-how-did-kafka-reduce-cpu)
  - [41–52. Delivery, DLQ, Testing, Grafana, Contribution](#41-what-delivery-guarantee-did-you-have)
  - [53. 2-Minute Interview Answer](#53-how-would-you-answer-explain-this-project-in-2-minutes)
  - [54. Complete Cross-Question Bank](#54-your-complete-interviewer-cross-question-bank)
  - [55. The 10 Questions I Expect First](#55-the-10-questions-i-expect-to-be-asked-first)
- [Part 2 — Interview-Ready Cross-Question Answers](#part-2--interview-ready-cross-question-answers)
- [15 Answers You Should Memorize](#o-15-answers-you-should-memorize)
- [Facts You Can Confidently Anchor On](#one-final-warning-for-your-interview)

---

# Part 1 — Architecture

## 1. First Understand the Problem

The easiest way to explain this is:

### Before Kafka

The flow was approximately:

```text
SIP Scheduler
      |
      v
Mutual Fund Service
      |
      |  PDN/DN event
      v
Notification Service
      |
      +----> Push
      |
      +----> Email
      |
      +----> Notification
```

The important problem was that the Mutual Fund service was doing the notification-related work **synchronously**.

```text
Process SIP
   |
   |---- generate PDN/DN
   |
   |---- call Notification Service
   |        |
   |        |---- Push
   |        |---- Email
   |        |---- Notification
   |
   |---- continue
```

So if the Notification Service became slow, the Mutual Fund service could also become slow.

---

## 2. What Exactly Is PDN and DN?

For a SIP, there is generally a scheduled debit/investment process.

### PDN — Pre-Debit Notification

A notification sent **before** the money is debited.

Example: *"₹5,000 will be debited from your bank account tomorrow for your SIP."*

### DN — Debit Notification

A notification related to the **actual debit**.

Example: *"₹5,000 has been debited for your SIP."*

Your system needed to generate these events for a large number of SIPs.

Your resume mentions approximately **80K daily SIPs**.

---

## 3. Why Did the Old Synchronous Architecture Create a Problem?

Suppose there are **80,000 SIPs/day**.

During SIP processing hours, many events can arrive within a short period.

```text
Mutual Fund Service
       |
       | 10,000 notification requests
       v
Notification Service
```

If the Mutual Fund service makes synchronous calls (`notificationService.send(notification)`), the request/thread has to wait.

```text
Request
  |
  v
Generate PDN
  |
  v
Call Notification Service
  |
  |---- network latency
  |---- notification processing
  |---- DB operation
  |---- push/email processing
  |
  v
Response
  |
  v
Continue
```

This causes several problems.

### Problem 1 — Thread blocking

If 500 requests are simultaneously waiting for Notification Service:

```text
500 application threads
       |
       +---- waiting for downstream
```

CPU may not necessarily be the only problem. You can get:

- Thread pool exhaustion
- Increased latency
- Connection pool pressure
- Increased memory
- Request queues
- Timeouts

### Problem 2 — Downstream overload

Suppose Mutual Fund sends 500 requests/sec, but Notification Service capacity is 200 requests/sec.

```text
Incoming = 500 TPS
Processing = 200 TPS
Backlog = 300 TPS
```

The Notification Service becomes a bottleneck.

### Problem 3 — Failure coupling

Suppose Notification Service is down.

With synchronous communication, the Mutual Fund workflow can experience timeout, retries, increased latency, and cascading failures.

This is called **tight coupling**.

---

## 4. Why Kafka Solves This

The architecture becomes:

```text
                 Kafka
              +---------+
              | PDN/DN  |
              | Topics  |
              +----+----+
                   ^
                   |
Mutual Fund Service
     Producer
                   |
                   v
             Payments Service
               Consumer
                   |
                   v
          Notification Service
             /      |      \
           Push   In-App   Email
```

The key change is:

| Before | After |
| --- | --- |
| Mutual Fund → (synchronous) → Notification | Mutual Fund → publish event → Kafka → consume asynchronously → Payments → Notification |

Now Mutual Fund Service **doesn't need to wait** for the entire notification workflow.

---

## 5. What Exactly Happens When a SIP Event Occurs?

Let's construct a realistic interview explanation.

Suppose: SIP = ₹5,000, Customer = C123, SIP ID = SIP789.

A PDN event is generated.

### Step 1 — Mutual Fund Service generates event

```json
{
  "eventId": "EVT123",
  "eventType": "PDN",
  "sipId": "SIP789",
  "userId": "C123",
  "amount": 5000,
  "timestamp": "..."
}
```

### Step 2 — Producer publishes to Kafka

Mutual Fund Service acts as the producer. It doesn't call the Notification Service directly.

```text
Mutual Fund Service
        |
        | publish
        v
Kafka PDN/DN topic
```

### Step 3 — Kafka stores the event

Kafka has: **Broker = 3**, **Partitions = 20**.

```text
Kafka Cluster

Broker 1
 ├── P0
 ├── P1
 ├── P2
 ...

Broker 2
 ├── P7
 ├── P8
 ...

Broker 3
 ├── P14
 ├── P15
 ...
```

**Important:** 20 partitions does **NOT** mean 20 brokers.

You have 3 brokers and 20 partitions. Partitions are distributed across the brokers.

---

## 6. Why 20 Partitions?

This is one of the questions an interviewer will almost certainly ask.

**Answer:** We used partitions primarily to enable **parallel consumption** and **horizontal scalability**.

Suppose we have only one partition:

```text
Topic
 |
 P0
 |
 Consumer
```

Only one consumer in a consumer group can actively consume that partition at a time.

With 20 partitions:

```text
P0  ---> Consumer 1
P1  ---> Consumer 2
P2  ---> Consumer 3
...
P19 ---> Consumer 20
```

This allows multiple consumers to process messages concurrently.

But you had **20 partitions** and **8 consumers**.

Each consumer can own multiple partitions. For example:

```text
Consumer 1 ---> P0 P1 P2
Consumer 2 ---> P3 P4
Consumer 3 ---> P5 P6 P7
...
```

The exact assignment is controlled by Kafka's consumer-group rebalancing.

---

## 7. Why 8 Consumers If There Are 20 Partitions?

Very good interviewer question.

You can say:

> We chose 20 partitions to provide enough parallelism and future scaling headroom, while running 8 consumers initially based on the processing throughput requirement. Kafka allows multiple partitions to be assigned to a single consumer, so 8 consumers can process all 20 partitions concurrently. If throughput increases, we can add consumers up to the partition count.

**Important statement:**

> Maximum active consumers in one consumer group ≤ number of partitions

So 20 partitions allows up to **20 active consumers** for that topic/consumer group.

If you run 25 consumers: 20 consumers → active, 5 consumers → idle.

---

## 8. Why Not Just Use 20 Consumers?

Because more consumers aren't automatically better.

Each consumer has threads, network connections, memory, processing overhead, and downstream connections.

If your workload only requires 8 consumers, 20 consumers may waste resources.

So **partition count** and **consumer count** are independent capacity decisions.

---

## 9. Why Kafka Instead of RabbitMQ?

This is a classic cross-question.

| Kafka is excellent for | RabbitMQ is excellent for |
| --- | --- |
| High-throughput event streaming | Traditional message queues |
| Durable event storage | Routing |
| Partition-based parallelism | Task distribution |
| Replay | Relatively straightforward work queues |
| Consumer groups | |
| Large event volumes | |

For your use case, Kafka is a strong choice because you have a high-volume event stream and potentially need replayability.

**Interview answer:**

> We chose Kafka because the workload was event-driven and high-volume. We wanted durable event storage, partition-based parallel consumption, consumer groups, and the ability to replay events if downstream processing failed.

---

## 10. Why Kafka Instead of Redis?

Redis is excellent for: caching, fast lookups, counters, locks, short-lived state.

Kafka is designed for: event streaming, durable event logs, high-throughput asynchronous processing, replay, consumer groups.

Therefore: **Redis ≠ Kafka**.

Don't say: "Kafka is faster than Redis." That's an oversimplification.

Instead: they solve different problems. Redis is primarily an in-memory data store, whereas Kafka provides a durable distributed event log and streaming platform.

---

## 11. Why Asynchronous Architecture?

This is probably the most important conceptual question.

**Synchronous:** `A ---> B ---> C` — A waits for B. B waits for C.

**Asynchronous:** `A ---> Kafka ---> B ---> C` — A publishes the event and can continue.

This provides **decoupling**.

If Notification Service slows down:

```text
Mutual Fund
     |
     v
Kafka
     |
     | backlog
     v
Notification
```

The events can remain in Kafka until consumers catch up, subject to Kafka retention and operational configuration.

---

## 12. What Happens If Notification Service Is Down?

Very important.

```text
Mutual Fund
     |
     v
Kafka
     |
     v
Payments Consumer
     |
     X
Notification DOWN
```

The consumer will fail processing the message.

Depending on your implementation, you can:

- Retry
- Avoid committing the offset until successful processing
- Use retry topics
- Use a DLQ/DLT
- Pause/recover processing
- Replay events

The interviewer may ask: "Would Kafka lose the message?"

**Correct answer:**

> Not necessarily. Kafka stores the record independently of the consumer. If the consumer fails before successfully processing and committing the offset, the record can be consumed again, depending on the consumer and retry configuration.

---

## 13. What Happens If Consumer Crashes?

```text
Consumer 3
    |
    | processing P7
    X
  CRASH
```

Kafka consumer group detects the failure and rebalances the partitions.

Another consumer can take over: Consumer 5 ---> P7.

The new consumer continues from the **committed offset**. This gives you fault tolerance.

---

## 14. What Is an Offset?

Kafka maintains a position for each partition.

```text
Partition P1

Offset:
100
101
102
103
104
```

Consumer has processed 100, 101, 102 and committed 102.

If it crashes while processing 103, another consumer may restart from the last committed position.

Therefore: Committed = 102, Next = 103.

This is why **idempotency** becomes important.

---

## 15. What If the Same Notification Is Processed Twice?

This is a major distributed-systems question.

Kafka consumers can experience **at-least-once** processing.

```text
Consumer receives Event 123
      |
      v
Send notification successfully
      |
      X
Consumer crashes before offset commit
```

Kafka thinks Event 123 was not committed. So another consumer can process it again.

**Result:** Notification sent twice.

Therefore, you need **idempotency**. For example `eventId = EVT123`:

```text
Store processed event IDs:
EVT123 -> PROCESSED

Before processing:
if alreadyProcessed(eventId):
    skip
```

This prevents duplicate side effects.

Only claim this was implemented if it was actually part of your implementation. If the interviewer asks about production robustness, you can present it as the **design consideration**.

---

## 16. What Key Did You Use for Kafka Partitioning?

This is a very likely cross-question.

You need to be careful here because your notes don't specify your actual key.

A reasonable design would be `key = userId` or `key = sipId`.

Why? Kafka hashes the key to determine the partition.

```text
userId C123
     |
     v
hash(C123)
     |
     v
Partition 7
```

This can preserve ordering for events having the same key.

**Interview answer if you don't remember the actual key:** Don't invent it.

> The exact partition key was an implementation detail I'd want to verify from the production code. Conceptually, I would choose a stable business identifier such as SIP ID or user ID depending on whether ordering is required per SIP or per customer.

That's much safer than falsely claiming one.

---

## 17. Why Is Partition Key Important?

Suppose PDN and DN for the same SIP.

You may want PDN → DN to be processed in order.

If both events use the same key:

```text
SIP789
   |
   v
Partition 5

PDN
DN
```

Kafka maintains ordering **within a partition**.

**Important:** Kafka does **NOT** provide global ordering across all partitions.

---

## 18. Why Not One Partition?

Because one partition limits consumer parallelism.

```text
1 partition
     |
1 active consumer
```

Even if you have 8 consumers, only one can actively consume that partition in the same consumer group.

So 20 partitions provides much better parallelism.

---

## 19. Why Not 100 Partitions?

Another tradeoff.

More partitions give more parallelism and more scalability, but also introduce more metadata, broker overhead, file handles, replication overhead, and management complexity.

Therefore partition count should be chosen based on: throughput, message size, consumer processing time, desired parallelism, future growth, broker capacity.

**A strong answer:**

> I wouldn't choose 20 just because 20 is a standard number. The partition count should come from throughput and consumer-latency measurements, with some headroom for future growth.

---

## 20. What Does 3 Brokers Mean?

Your configuration **Broker = 3** means you have a Kafka cluster containing three broker nodes.

```text
          Kafka Cluster

       +-------+-------+-------+
       |Broker1|Broker2|Broker3|
       +-------+-------+-------+
```

Partitions are distributed across them.

If replication factor is configured appropriately, replicas can be placed on different brokers.

**Important:** Your notes don't mention replication factor. So don't claim `replication factor = 3` unless you actually know that.

Instead say: "We had three brokers. The exact replication factor was a separate Kafka configuration."

---

## 21. What If One Kafka Broker Dies?

Suppose Broker 2 is DOWN.

If the affected partitions have replicas on other brokers, Kafka can elect another replica as leader.

This is why replication is important for availability.

Again, don't say your system had RF=3 unless you know it.

---

## 22. What Was the Producer?

```text
Mutual Fund Service
        |
        v
Kafka
```

**Mutual Fund Service = Kafka Producer.**

Its responsibility:

- Generate PDN/DN event
- Serialize event
- Send event to Kafka topic
- Handle producer acknowledgement/failure
- Continue business processing

---

## 23. What Was the Consumer?

Your notes say: **Consumer = Payments Service**.

```text
Kafka
  |
  v
Payments Service
```

Payments Service consumes the event and calls Notification Service for Push, Email, and In-app notification.

---

## 24. Why Does Payments Service Call Notification Service?

The architectural reason is **separation of responsibility**.

| Service | Owns |
| --- | --- |
| Mutual Fund Service | SIP / mutual fund business logic |
| Payments Service | Relevant payment-side processing |
| Notification Service | Actual notification delivery |

This prevents the Mutual Fund service from becoming responsible for every downstream notification concern.

---

## 25. What Does the Complete Flow Look Like?

This is the diagram I recommend memorizing for the interview:

```text
                         ┌──────────────────┐
                         │   SIP Scheduler  │
                         └────────┬─────────┘
                                  │
                                  v
                     ┌────────────────────────┐
                     │ Mutual Fund Service    │
                     │ Kafka Producer         │
                     └────────────┬───────────┘
                                  │
                          PDN / DN Event
                                  │
                                  v
                    ┌──────────────────────────┐
                    │      Kafka Cluster       │
                    │                          │
                    │  3 Brokers               │
                    │  20 Partitions           │
                    └────────────┬─────────────┘
                                 │
                                 v
                    ┌──────────────────────────┐
                    │ Payments Service         │
                    │ Kafka Consumer Group     │
                    │ 8 Consumers              │
                    └────────────┬─────────────┘
                                 │
                                 v
                    ┌──────────────────────────┐
                    │ Notification Service     │
                    └────────────┬─────────────┘
                         ┌───────┼────────┐
                         │       │        │
                         v       v        v
                       Push   In-App    Email
```

---

## 26. How Did Kafka Reduce CPU?

Be careful here. Don't say: "Kafka reduced CPU because Kafka is faster." That's weak.

**Before:** Mutual Fund Service did business processing + synchronous downstream calls + waiting/retries/connection management + large concurrent workload.

**After:** Mutual Fund Service did business processing + Kafka publish.

The notification processing was moved asynchronously to consumers.

So the producer-side service had less synchronous work and less waiting under burst traffic.

If your Grafana dashboard showed a 30% CPU reduction, you can say:

> We compared the CPU profile during equivalent SIP processing windows before and after the Kafka migration using Grafana. We observed approximately a 30% reduction in CPU spikes.

But be prepared for: "How did you ensure it wasn't simply lower traffic?"

You should answer:

> We compared equivalent SIP-hour traffic windows and looked at request volume/throughput alongside CPU and memory metrics rather than looking at CPU in isolation.

Only say this if that's actually how you measured it.

---

## 27. What Metrics Would You Monitor?

This is where you can sound much more senior.

| Category | Metrics |
| --- | --- |
| **Producer** | Throughput, latency, send failures, retries, record size |
| **Kafka** | Messages/sec, bytes/sec, partition distribution, under-replicated partitions, broker CPU/disk/network |
| **Consumer** | **Consumer lag**, processing latency, throughput, errors, retry count |
| **Notification service** | Request rate, error rate, latency, 5xx, connection pool |
| **Business** | PDN generated, DN generated, PDN/DN successfully delivered, failed notifications, duplicate notifications |

---

## 28. What Is Consumer Lag?

This is an extremely common Kafka question.

Suppose Kafka has latest offset = 10,000 and consumer has processed 9,500.

```text
Consumer lag = 10,000 - 9,500 = 500
```

Meaning: the consumer is 500 messages behind the latest available messages.

If lag continuously increases:

```text
Producer
  |
  | 1000 msg/s
  v
Kafka
  |
  | 700 msg/s
  v
Consumer

Lag increases
```

That means consumers cannot keep up.

---

## 29. How Would You Handle Increasing Consumer Lag?

**Option 1 — Increase consumers:** From 8 to 12 consumers, provided you have enough partitions. You have 20 partitions, so there is room.

**Option 2 — Increase consumer processing capacity:** Optimize DB calls, downstream calls, serialization, network calls, unnecessary processing.

**Option 3 — Increase partitions:** If 20 partitions are insufficient for future throughput, increase partition count. But partition increases should be planned carefully because they affect ordering and operational behavior.

---

## 30. What Happens If Notification Service Is Slower Than Kafka Consumption?

Suppose Kafka → 1000 events/sec, Notification → 300 events/sec.

If the Payments consumers immediately process all 1000, Notification Service can be **overloaded**.

So the consumer layer should have appropriate: concurrency, rate limiting, retries, backpressure, timeout, circuit breaker — depending on the actual system.

**Strong tradeoff:** Kafka absorbs bursts, but Kafka does **not magically increase** the capacity of the downstream Notification Service.

That's a very important interview point.

---

## 31. Kafka Doesn't Solve Everything

If the interviewer says: "So why didn't you just increase Kafka partitions?"

**Answer:** Because the bottleneck could still be Notification Service. Kafka only decouples and buffers.

```text
Producer = 1000 TPS
Kafka = 1000 TPS
Consumer = 1000 TPS
Notification = 200 TPS
```

Eventually Notification backlog / failures occur. So the **entire pipeline** needs capacity planning.

---

## 32. What If Kafka Itself Is Down?

The producer attempts Mutual Fund → Kafka and Kafka is unavailable.

Then producer configuration should determine: retries, timeout, acknowledgement, error handling.

For a critical event, you don't want: business transaction succeeds BUT event disappears.

This leads to the **dual-write problem**.

---

## 33. What Is the Dual-Write Problem?

Suppose: 1. Update database  2. Publish Kafka event

What if: DB update = SUCCESS, Kafka publish = FAILURE

Now: Database = updated, Kafka = no event. Downstream never receives the event.

This is a classic distributed-systems consistency problem.

A sophisticated solution is the **Transactional Outbox Pattern**:

```text
Business Service
      |
      +---- DB Transaction
      |       |
      |       +--> Business data
      |       |
      |       +--> Outbox event
      |
      v
Outbox Publisher
      |
      v
Kafka
```

Then a separate process publishes the outbox events to Kafka.

**Do not claim you implemented Outbox unless you actually did.**

You can say:

> One design improvement I'd consider is the transactional outbox pattern to guarantee reliable publication when the database update and Kafka publication need atomic consistency.

---

## 34. Why Not Directly Call Notification Service Asynchronously Using a Thread Pool?

Interviewer: "Why Kafka? Couldn't you just use an executor/thread pool?"

**Answer:** A local thread pool provides asynchronous execution inside the application, but it doesn't provide the same durable distributed queue semantics.

With a thread pool: Application → memory queue. If the pod crashes: **Pending events → LOST**.

Kafka provides: durable event storage, consumer groups, replay, partitioning, horizontal scaling, cross-service decoupling.

That's a much stronger answer.

---

## 35. Why Not Use `@Async` in Spring Boot?

Same principle.

`@Async`: Application → thread pool.

Kafka: Application → distributed durable event stream.

`@Async` is useful for lightweight asynchronous tasks, but for a high-volume cross-service event pipeline, Kafka is more appropriate.

---

## 36. How Did You Coordinate With the Mutual Fund Team?

Your notes say you gave the topic name to the Mutual Fund team and they pushed events to the topic.

**Interview answer:**

> I coordinated with the Mutual Fund team on the event contract and Kafka topic. We agreed on the topic name, event structure, required fields, and publishing semantics. They integrated their service as the producer and published PDN/DN events, while our Payments service consumed those events and integrated with the Notification Service.

You should ideally know: topic name, event schema, key, partitions, serialization format, required fields, error handling.

If you don't remember the exact topic name, **don't invent one**.

---

## 37. What Event Schema Would You Use?

A reasonable example:

```json
{
  "eventId": "EVT-12345",
  "eventType": "PDN",
  "userId": "USER-123",
  "sipId": "SIP-456",
  "amount": 5000,
  "eventTime": "2026-09-09T10:00:00Z"
}
```

| Field | Purpose |
| --- | --- |
| `eventId` | Idempotency |
| `eventType` | PDN / DN |
| `userId` | Identify customer |
| `sipId` | Identify SIP |
| `amount` | Notification data |
| `eventTime` | Event timestamp |

---

## 38. What Serialization Format?

Possible choices: JSON, Avro, Protobuf.

If your actual implementation used JSON, say JSON. If you don't remember, don't fabricate it.

**A strong general answer:**

> For an internal event pipeline, I'd consider Avro or Protobuf when schema evolution and compact serialization are important. JSON is simpler operationally and easier to debug but has higher payload overhead.

---

## 39. JSON vs Avro

| | JSON | Avro |
| --- | --- | --- |
| **Pros** | Easy to read, easy debugging, simple integration | Compact, schema-based, good schema evolution, commonly used with Kafka |
| **Cons** | Larger payload, weaker schema enforcement, serialization overhead | More infrastructure/tooling, harder to inspect manually |

---

## 40. What If Event Schema Changes?

Suppose today's event is `{ "userId": "...", "sipId": "..." }` and tomorrow you add `amount`.

Consumers shouldn't suddenly break. You need backward/forward compatibility strategy.

For schema-based systems, schema registry + compatibility rules can help.

---

## 41. What Delivery Guarantee Did You Have?

Kafka supports patterns around: at-most-once, at-least-once, exactly-once.

For notification pipelines, **at-least-once + idempotent processing** is often a practical design.

**Why?** Because losing a notification can be worse than processing it twice, but duplicates must be prevented at the side-effect boundary.

Again, if your actual implementation didn't explicitly implement idempotency, don't claim "We had exactly-once semantics."

Instead:

> Kafka consumption was designed around reliable processing, and for a production-grade implementation I'd ensure idempotency at the notification side-effect boundary because retries can result in duplicate delivery attempts.

---

## 42. Exactly-Once — Interviewer Trap

If interviewer asks: "Kafka gives exactly-once, right?" Don't simply say yes.

Kafka's exactly-once semantics have scope and conditions. Once you call an external Notification Service, Kafka's transaction cannot automatically make that external side effect exactly once.

Kafka cannot magically rollback "Push notification sent." Therefore external side effects require their own **idempotency mechanism**.

---

## 43. What If Producer Sends Duplicate Events?

Suppose producer sends EVT123 twice. Consumer sees two events.

You can use `eventId` as an idempotency key.

```text
processed_events
----------------
EVT123

Before sending:
Does EVT123 already exist?
YES -> skip
NO  -> process
```

---

## 44. What Happens If One Partition Becomes Hot?

Suppose partitioning uses `userId` and one user generates huge traffic.

```text
P7 = 50K events
P8 = 2K
P9 = 2K
...
```

Now P7 becomes a **hot partition**.

You can consider: better partition key, composite key, workload distribution, increasing partitions.

But there's a tradeoff: changing the partition key can affect ordering guarantees.

---

## 45. What Happens When You Add Consumers?

Suppose 20 partitions, 8 consumers. Then add 4 consumers. Kafka triggers a **rebalance**.

Now 12 consumers, 20 partitions. Partitions are redistributed.

During rebalance, consumption can temporarily pause depending on the consumer/client configuration.

So blindly adding consumers isn't always free.

---

## 46. What Happens When a Consumer Is Slow?

Suppose Consumer 1 = 100 msg/sec, Consumer 2 = 100 msg/sec, Consumer 3 = 20 msg/sec.

If Consumer 3 owns a busy partition, partition lag increases.

Kafka doesn't automatically move individual messages between consumers. **Partition ownership is the unit of parallelism.**

This is another reason partition distribution matters.

---

## 47. What If a Message Is Malformed?

Example: `{ "eventType": null }`

You don't want the consumer to continuously fail on that event. That can create a **poison message**.

A production design can use:

```text
Main Topic
    |
    v
Consumer
    |
    X
    |
Retry Topic
    |
    X
    |
DLQ / DLT
```

Then engineers can inspect the failed event.

---

## 48. What Is DLQ/DLT?

**Dead Letter Queue / Dead Letter Topic.**

Used for messages that cannot be processed successfully after retries.

```text
PDN Topic
    |
    v
Consumer
    |
    X
    |
 Retry
    |
    X
    |
    v
DLT
```

This prevents one bad event from blocking normal processing.

---

## 49. What Retry Strategy Would You Use?

Don't immediately retry aggressively. That can overload the downstream service.

**Better (exponential backoff):**

| Attempt | Delay |
| --- | --- |
| 1st retry | 1 sec |
| 2nd retry | 5 sec |
| 3rd retry | 30 sec |
| 4th retry | 2 min |

```text
Retry Topic
    |
    v
Delayed retry
    |
    v
Main processing
```

---

## 50. How Did You Test the Kafka Pipeline?

**Unit testing:** Event creation, serialization, consumer processing, error handling, validation.

**Integration testing:** Producer → Kafka → Consumer.

**Failure testing:** Kafka unavailable, consumer crash, Notification Service timeout, malformed event, duplicate event.

**Load testing:** Generate high-volume PDN/DN events and monitor TPS, CPU, memory, Kafka lag, consumer latency, downstream latency.

---

## 51. How Did Grafana Help?

Your notes specifically mention Grafana.

**Before Kafka:** Monitor CPU, memory, TPS, request latency, notification calls. You observed high resource utilization around SIP processing windows.

**After Kafka:** Compare CPU, memory, latency, throughput, and monitor Kafka-specific metrics: consumer lag, consumer throughput, producer errors.

**A strong interview statement:**

> Grafana was used to compare the system behavior during SIP processing windows and to monitor the Kafka pipeline after migration. We looked not only at CPU but also throughput, latency and consumer lag to make sure the improvement wasn't achieved by simply shifting the bottleneck elsewhere.

---

## 52. What Was YOUR Individual Contribution?

This question is extremely important because interviewers don't want "We implemented Kafka." They want "What did YOU do?"

Based on your notes, structure it like this:

1. Identified synchronous notification bottleneck
2. Proposed asynchronous Kafka-based design
3. Defined Kafka topic integration with Mutual Fund team
4. Worked on producer/consumer integration
5. Configured/validated partitions and consumer concurrency
6. Integrated consumer with Notification Service
7. Handled failure/retry scenarios
8. Monitored metrics through Grafana
9. Coordinated with other engineers
10. Validated performance improvement

**Only claim the items you actually performed.**

---

## 53. How Would You Answer "Explain This Project in 2 Minutes?"

Memorize this version:

> At Paytm Money, we had a high-volume SIP workflow where we generated Pre-Debit Notifications and Debit Notifications for roughly 80K daily SIPs. During SIP processing hours, the notification flow was synchronous, so the Mutual Fund service was directly involved in downstream notification processing. This created higher resource utilization and also tightly coupled the Mutual Fund workflow with the Notification Service.
>
> I worked on moving this flow to an asynchronous Kafka-based architecture. The Mutual Fund service became the producer and published PDN/DN events to Kafka. We had a Kafka cluster with 3 brokers and 20 partitions. The Payments service consumed those events using a consumer group with 8 consumers and then called the Notification Service for push, in-app notification, and email delivery.
>
> The main benefit was that the Mutual Fund service no longer had to synchronously wait for the downstream notification processing. Kafka acted as a durable buffer between the producer and consumer, allowing us to absorb traffic bursts and process events in parallel through multiple partitions and consumers.
>
> I also coordinated with the Mutual Fund team around the Kafka topic and event contract and worked with the engineering team on the integration and monitoring. We used Grafana to compare the system metrics before and after the change and monitor the Kafka pipeline, including consumer behavior and system resource utilization.
>
> The key trade-off was that asynchronous processing introduced eventual consistency and required us to think carefully about retries, duplicate processing, consumer failures, ordering, and downstream backpressure. Overall, the architecture allowed us to handle the additional 100–150 TPS traffic while reducing the synchronous workload on the services.

---

## 54. Your Complete Interviewer Cross-Question Bank

You should prepare these very seriously.

### Architecture

- Why Kafka?
- Why asynchronous processing?
- Why not synchronous?
- Why not RabbitMQ?
- Why not Redis?
- Why not SQS?
- Why not `@Async`?
- Why separate Payments and Notification services?
- Why not call Notification directly from Mutual Fund?
- What happens if Notification Service is down?

### Kafka

- What is a Kafka broker?
- What is a partition?
- Why 20 partitions?
- Why 3 brokers?
- Why 8 consumers?
- What is a consumer group?
- What is consumer lag?
- What is an offset?
- How does Kafka maintain ordering?
- Does Kafka guarantee global ordering?
- What happens if consumer crashes?
- What happens if broker crashes?
- What happens if Kafka is unavailable?
- What happens when consumer lag increases?
- How do you scale consumers?
- How do you scale partitions?
- What happens during consumer rebalance?
- What is a hot partition?
- How did you select partition key?
- What is replication factor?

### Reliability

- What if producer sends duplicate event?
- What if consumer processes event twice?
- How do you ensure idempotency?
- What if notification call succeeds but Kafka offset isn't committed?
- At-most-once vs at-least-once?
- What is exactly-once?
- What is DLQ/DLT?
- What is poison message?
- How would you implement retry?
- What is exponential backoff?

### Database / consistency

- What if DB update succeeds but Kafka publish fails?
- What is dual-write problem?
- What is transactional outbox?
- Does asynchronous architecture introduce eventual consistency?
- How would you guarantee event publication?

### Performance

- How did Kafka reduce CPU?
- How did you measure 30% improvement?
- What was your TPS?
- What was the bottleneck?
- How did you load test?
- How do you determine partition count?
- How do you determine consumer count?
- What if traffic increases from 150 TPS to 1,000 TPS?
- What if Notification Service can handle only 100 TPS?
- How do you prevent downstream overload?

### Collaboration

- What was your individual contribution?
- How did you coordinate with Mutual Fund team?
- How did you define the event contract?
- What problems did you face during integration?
- How did you monitor production?

---

## 55. The 10 Questions I Expect to Be Asked First

If this is an SDE1 interview, prioritize these:

| Priority | Question |
| --- | --- |
| 1 | Why did you introduce Kafka? |
| 2 | Explain the architecture before and after Kafka |
| 3 | Why 20 partitions? |
| 4 | Why 8 consumers? |
| 5 | What happens if consumer crashes? |
| 6 | What happens if Notification Service is down? |
| 7 | How do you handle duplicate messages? |
| 8 | What is consumer lag? |
| 9 | How did you measure 30% CPU reduction? |
| 10 | What exactly was your individual contribution? |

### One thing I strongly recommend

Your resume says 100–150 TPS increase, ~80K daily SIPs, while your personal notes say 30% CPU reduction.

Before the interview, make sure you can explain exactly what those numbers mean:

```text
80K daily SIPs
      ↓
How many PDN/DN events?
      ↓
Peak events/sec?
      ↓
100–150 TPS = what exact API/event traffic?
      ↓
30% CPU = which service?
      ↓
30% compared over what period?
      ↓
What was CPU before?
      ↓
What was CPU after?
```

These numbers are where an interviewer can quickly detect whether you actually worked on the system. If you don't remember an exact implementation detail, it's much safer to say **"I don't remember the exact configuration, but the design reason was…"** than to invent a value.

---

# Part 2 — Interview-Ready Cross-Question Answers

Below are interview-ready answers for the cross-questions on your first Paytm Money Kafka project. Answers are consistent with: ~80K daily SIPs, 100–150 TPS additional traffic, Kafka with 3 brokers / 20 partitions / 8 consumers, Mutual Fund Service as producer, Payments Service as consumer, and downstream Notification Service.

**Important:** Where your notes don't specify an exact implementation detail — such as replication factor, partition key, serialization format, or exact retry mechanism — these are **safe interview answers**, not claims that you implemented something.

---

## A. Architecture Cross-Questions

### 1. Why did you introduce Kafka?

The main reason was that the PDN/DN notification flow was synchronous and created significant load during SIP processing hours. The Mutual Fund service had to directly interact with the downstream notification flow, which increased coupling and resource utilization.

We introduced Kafka to make the flow asynchronous. The Mutual Fund service publishes the PDN/DN event to Kafka and doesn't need to wait for the complete notification processing. The Payments service consumes the event and calls the Notification Service.

This allowed us to absorb traffic bursts, process events in parallel, and reduce the synchronous workload on the upstream service.

**If interviewer asks: "Why couldn't you just optimize the existing service?"**

> We could optimize the existing service, but that would only improve the efficiency of the synchronous architecture. The fundamental problem was the coupling between the SIP workflow and downstream notification processing. Kafka addressed that architectural problem by decoupling the producer and consumer.

### 2. Explain the architecture before Kafka

```text
SIP Processing
      |
      v
Mutual Fund Service
      |
      | synchronous call
      v
Notification Service
      |
   +--+--+
   |  |  |
 Push Email In-App
```

Earlier, once the Mutual Fund service generated the PDN/DN event, the notification processing happened synchronously. During SIP hours, a large number of events could arrive together. The upstream service therefore had to maintain connections and wait for downstream processing, which increased latency and resource utilization.

### 3. Explain the architecture after Kafka

```text
                 +----------------+
                 | Mutual Fund    |
                 | Service        |
                 | Producer       |
                 +-------+--------+
                         |
                         | PDN/DN event
                         v
                +------------------+
                | Kafka Cluster    |
                | 3 Brokers        |
                | 20 Partitions    |
                +--------+---------+
                         |
                         v
                +------------------+
                | Payments Service |
                | 8 Consumers      |
                +--------+---------+
                         |
                         v
                +------------------+
                | Notification     |
                | Service          |
                +------------------+
                    /     |      \
                  Push   In-App   Email
```

The Mutual Fund service became the Kafka producer. It publishes PDN/DN events to Kafka. The Payments service consumes those events using a consumer group with eight consumers and then calls the Notification Service. The Notification Service handles the actual delivery channels such as push, notification and email.

### 4. Why asynchronous processing?

In synchronous processing, the upstream service waits for the downstream service. With asynchronous processing, the producer publishes an event and the downstream processing happens independently.

This is particularly useful when traffic is bursty. Kafka can buffer events while consumers process them at their own rate.

**Synchronous:** `MF → Notification → Response`

**Asynchronous:** `MF → Kafka → Response`, then Kafka → Notification

### 5. What is the biggest advantage of this architecture?

**Decoupling.**

The biggest architectural benefit was decoupling the SIP processing flow from notification processing. It also gave us buffering and parallel processing through Kafka partitions and consumers.

### 6. What is the downside of asynchronous processing?

This is important. Don't present Kafka as having only advantages.

The main trade-off is that the system becomes **eventually consistent**. The event is accepted by Kafka first and processed by the consumer later, so the notification isn't necessarily completed immediately.

It also introduces additional complexity around retries, duplicate processing, ordering, monitoring consumer lag and handling downstream failures.

---

## B. Kafka Questions

### 7. What is Kafka?

Kafka is a distributed event-streaming platform. Producers publish events to topics, Kafka stores those events in partitions, and consumers read them asynchronously.

In our use case, the Mutual Fund service was the producer, Kafka stored PDN/DN events, and the Payments service consumed those events.

### 8. What is a Kafka topic?

A topic is a logical category or stream to which producers publish messages and consumers subscribe.

```text
PDN/DN events
      ↓
Kafka Topic
      ↓
Payments Consumer
```

### 9. What is a partition?

A partition is an ordered, append-only sequence of records inside a Kafka topic.

```text
Topic
 |
 +-- Partition 0
 +-- Partition 1
 +-- Partition 2
 ...
 +-- Partition 19
```

Each partition allows independent parallel consumption.

### 10. Why did you use 20 partitions?

We used 20 partitions to provide sufficient parallelism for the event volume and allow multiple consumers to process events concurrently. Partitions are also useful for future scalability because consumer parallelism is bounded by the number of partitions.

The exact partition count should ideally be determined from throughput, processing latency, expected growth and broker capacity rather than choosing an arbitrary number.

**Interviewer: "Why exactly 20?"** Don't panic.

> 20 was the configured partition count for the workload. The design consideration was to have enough parallelism for the expected SIP-hour traffic while keeping broker and operational overhead reasonable.

Don't invent a mathematical calculation if you don't remember one.

### 11. Why 3 brokers?

Three brokers provided a distributed Kafka cluster rather than relying on a single broker. This allows partitions to be distributed across multiple machines and provides better fault tolerance when replication is configured appropriately.

**Interviewer: "Does 3 brokers mean replication factor is 3?"**

> No. Broker count and replication factor are separate configurations. We had three brokers, but I wouldn't claim a replication factor of three unless I verify the actual Kafka configuration.

This is a very good answer because it shows you understand Kafka rather than memorizing numbers.

### 12. Why 8 consumers?

We initially used eight consumers in the consumer group based on the processing requirement. Since there were 20 partitions, the partitions could be distributed among the eight consumers.

If traffic increased, we could increase the number of consumers, up to the number of partitions for active parallelism.

Exact assignment is handled by Kafka.

### 13. Why not 20 consumers?

We could run up to 20 active consumers for 20 partitions, but more consumers aren't automatically better. Every consumer has CPU, memory and network overhead. We selected the consumer count according to the processing throughput requirement.

### 14. What happens if you have 25 consumers and 20 partitions?

Only 20 consumers can actively own partitions in that consumer group. The remaining five consumers would be idle because a partition can be actively consumed by only one consumer within a consumer group at a time.

### 15. What is a consumer group?

A consumer group is a set of consumers that cooperate to consume a topic. Kafka distributes partitions among consumers within the same group so that each partition is processed by one consumer at a time.

```text
Topic
P0 P1 P2 P3 P4 P5

Consumer Group
C1 → P0 P1
C2 → P2 P3
C3 → P4 P5
```

### 16. What happens if one consumer crashes?

Kafka detects the consumer failure and triggers a rebalance. The partitions owned by that consumer are assigned to other consumers in the same consumer group. The new consumer resumes processing from the appropriate committed offset.

### 17. What is an offset?

An offset is the position of a record within a Kafka partition.

If the consumer has committed offset 102, it knows where it was in that partition.

### 18. What is consumer lag?

Consumer lag is the difference between the latest available offset and the offset processed or committed by the consumer.

Example: Latest offset = 10,000, Consumer offset = 9,500 → Lag = 500.

**Interviewer: "What does increasing lag mean?"**

It generally means consumers are processing slower than producers are publishing. I would investigate consumer processing latency, downstream latency, consumer errors, database calls and whether we need additional consumers or partitions.

### 19. Does Kafka guarantee ordering?

Kafka guarantees ordering **within a partition**, not across the entire topic.

```text
P1: A B C
P2: X Y Z
```

Kafka doesn't provide a global ordering between P1 and P2.

### 20. How would you preserve ordering for the same SIP?

I would use a stable key such as SIP ID as the partition key. Events with the same key would normally map to the same partition, which preserves their ordering within that partition.

If interviewer asks "Did you use SIP ID?" and you don't remember:

> I don't want to claim the exact production key without checking the implementation. But if ordering was required at the SIP level, SIP ID would be a natural partition key.

That is much safer.

---

## C. Failure & Reliability

### 21. What happens if Notification Service is down?

```text
Mutual Fund
     |
     v
   Kafka
     |
     v
Payments Consumer
     |
     X
Notification DOWN
```

The consumer cannot successfully complete the event. We need a retry/failure-handling mechanism rather than losing the event. Depending on the implementation, the consumer can retry the operation, avoid committing the offset until successful processing, or route repeatedly failing messages to a retry topic/DLT.

### 22. Will Kafka lose the message if consumer fails?

Not necessarily. Kafka stores the event independently of the consumer. If the consumer fails before successfully processing and committing its offset, the record can be processed again after recovery or reassignment.

### 23. What if consumer processes the event successfully but crashes before committing the offset?

```text
Kafka
  |
  v
Consumer
  |
  v
Notification SUCCESS
  |
  X
Consumer crashes
  |
  v
Offset NOT committed
```

Kafka may deliver the event again. This can lead to duplicate processing, so consumers should be designed to be **idempotent** when the downstream operation has side effects.

### 24. What is idempotency?

Idempotency means performing the same operation multiple times produces the same final result as performing it once.

```text
eventId = EVT123

First request:  EVT123 → process → SUCCESS
Second request: EVT123 → already processed → ignore
```

This prevents duplicate notifications or duplicate business operations.

### 25. How would you implement idempotency?

```text
Consumer
   |
   v
Check eventId
   |
   +---- Already processed → Ignore
   |
   +---- New event
            |
            v
       Process event
            |
            v
       Mark processed
```

You could maintain an idempotency record in a database/Redis depending on consistency requirements.

Don't claim Redis/database idempotency was implemented in this particular project unless you know that it was.

### 26. What delivery guarantee would you choose?

For a notification pipeline: I would generally prefer **at-least-once processing combined with idempotent consumers**. Losing a notification can be undesirable, while duplicate processing can be controlled using an idempotency key.

### 27. What is at-least-once delivery?

A message will be processed one or more times, so duplicates are possible.

```text
Event → Process → Crash before commit → Process again
```

### 28. What is at-most-once?

The event is processed zero or one time. There can be no duplicate processing from the consumer semantics, but an event can be lost if failure happens at the wrong point.

### 29. What is exactly-once?

Exactly-once processing means a record's effect is applied exactly once within the guarantees of the system. But it's important not to casually claim exactly-once for an external API call because Kafka's transaction cannot automatically make an external Notification Service operation exactly once.

This is a high-quality answer.

### 30. What if Kafka is down when Mutual Fund Service publishes?

The producer will experience a publishing failure or timeout. For critical events, the producer should have appropriate retries and error handling. More importantly, if the business database update and Kafka publication need to be consistent, we need to consider the dual-write problem.

### 31. What is the dual-write problem?

Suppose: 1. Update DB  2. Publish Kafka event.

What if: DB → SUCCESS, Kafka → FAILURE.

Now the database says the operation happened, but downstream services never received the event. That's the dual-write problem.

### 32. How would you solve the dual-write problem?

**Transactional Outbox:**

```text
Application
    |
    v
Database Transaction
   /       \
Business   Outbox
Data       Event
             |
             v
        Outbox Publisher
             |
             v
           Kafka
```

The business update and outbox event are written in the same database transaction. A separate publisher then publishes the outbox event to Kafka.

If the interviewer asks whether you implemented it:

> It wasn't part of the implementation I personally owned, but it would be one approach I'd consider if strict database-to-event consistency were required.

---

## D. Retry Questions

### 33. How would you implement retry?

```text
Main Topic → Consumer → X → Retry mechanism → Consumer

Retry 1 → Retry 2 → Retry 3 → DLT
```

Use backoff rather than immediate retry.

### 34. Why exponential backoff?

Suppose Notification Service is overloaded. If 1,000 consumers immediately retry, you create a **retry storm**.

With backoff (1 sec, 5 sec, 30 sec, 2 min), the system gets time to recover.

### 35. What is a DLT?

**DLT = Dead Letter Topic.**

If a message repeatedly fails processing, instead of blocking normal processing indefinitely, we can move it to a dead-letter topic for investigation or manual recovery.

### 36. What is a poison message?

A poison message is a message that repeatedly fails processing — for example because the payload is malformed or contains invalid data.

Without proper handling it can continuously block progress.

---

## E. Performance Questions

### 37. How did Kafka reduce CPU?

Kafka reduced the amount of synchronous downstream work performed by the upstream flow. Previously, the service was directly involved in synchronous notification processing and had to wait on downstream operations. After introducing Kafka, the producer mainly published the event, while the notification processing was handled asynchronously by consumers.

This reduced the synchronous workload and helped the service handle traffic bursts more efficiently.

### 38. You said CPU reduced by 30%. How did you measure it?

If you genuinely have this measurement:

> We used Grafana to compare CPU utilization during comparable SIP processing windows before and after the Kafka migration. We also looked at traffic volume and other system metrics so that we weren't interpreting a lower CPU number without considering workload.

Then interviewer: "Which service?" You need to know this.

If you don't:

> I'd need to verify the exact dashboard/service before quoting that number. I don't want to misrepresent the metric.

That answer is better than guessing.

### 39. What was the actual bottleneck?

The primary issue was synchronous downstream notification processing during SIP hours. The high event volume caused increased resource usage and put pressure on the downstream Notification Service.

### 40. What happens if Kafka receives 1,000 TPS but consumers can process only 500 TPS?

Producer = 1000 TPS, Consumer = 500 TPS → **Lag increases**.

Kafka acts as a buffer, but the backlog grows.

You can: increase consumers, optimize consumer processing, increase partitions if necessary, optimize downstream calls, apply appropriate backpressure/rate limiting, scale the Notification Service.

### 41. What if Notification Service supports only 100 TPS?

This is where many candidates give a bad answer. Don't say: "Increase Kafka consumers to 100." That can make it worse.

**Correct answer:** Kafka can absorb the burst, but we shouldn't overwhelm the downstream service. We need to control consumer concurrency or rate-limit calls to the Notification Service. Meanwhile, the downstream service can be scaled if possible.

```text
Kafka
  |
  v
8 Consumers
  |
  | controlled concurrency
  v
Notification Service
       |
       | 100 TPS capacity
```

### 42. Does Kafka remove the bottleneck?

**No.** It changes where and how the bottleneck is handled. Kafka provides buffering and decoupling, but the consumer and downstream services still need sufficient capacity.

Excellent interview statement.

### 43. How would you scale this system if traffic becomes 10x?

Current: 3 brokers, 20 partitions, 8 consumers.

For 10x traffic, I would evaluate: producer throughput, Kafka broker capacity, partition throughput, consumer processing rate, consumer lag, Notification Service capacity, database capacity.

Then scale the appropriate layer: more partitions, more consumers, Notification scaling.

---

## F. Database / Consistency Questions

### 44. Does asynchronous processing create eventual consistency?

Yes. Once the event is published, the downstream notification may be processed later. Therefore there can be a small delay between the business event and notification delivery.

### 45. Is eventual consistency acceptable here?

For notification delivery, a small processing delay is generally more acceptable than blocking the core SIP processing flow. The important requirement is that events aren't silently lost and that failures can be retried or recovered.

### 46. What if DB update succeeds but Kafka fails?

That's the dual-write problem. Depending on the business criticality, I would consider a transactional outbox, reliable retries, or another mechanism to guarantee eventual event publication.

---

## G. Partitioning Questions

### 47. What happens if one partition gets much more traffic?

That's called a **hot partition**.

```text
P0 → 1,000,000 events
P1 → 10,000
P2 → 10,000
```

Adding consumers won't necessarily solve it if P0 remains the hot partition.

Potential solutions: better partition key, distribute keys more evenly, increase partitions, redesign the keying strategy.

But: you have to balance this against ordering requirements.

### 48. Why not randomly distribute messages?

Because you may need ordering for related events.

If SIP123 PDN and DN are randomly distributed (P1 → PDN, P7 → DN), you lose partition-level ordering between them.

Using a stable business key can keep related events together.

### 49. What is the trade-off of using SIP ID as partition key?

**Advantage:** Same SIP → same partition. Therefore ordering is preserved.

**Disadvantage:** If one SIP/user generates disproportionate traffic, that partition can become hot.

So: partition-key selection is a trade-off between **ordering** and **load distribution**.

---

## H. Kafka vs Other Technologies

### 50. Why Kafka instead of SQS?

This is especially important because your resume also contains AWS SQS work.

Both can provide asynchronous processing, but they solve slightly different problems. Kafka is particularly suitable for high-throughput event streaming, partition-based parallelism, consumer groups and replayable event logs. SQS is more naturally suited to managed message-queue workloads and task processing.

For this high-volume event-streaming use case, Kafka was a better fit.

### 51. Why Kafka instead of RabbitMQ?

RabbitMQ is excellent for traditional message queuing and routing. Kafka was preferable here because of the high-volume event-streaming model, partition-based parallelism, consumer groups and replayability.

### 52. Why Kafka instead of Redis?

Redis is primarily an in-memory data store used for caching, fast lookups, counters and other low-latency operations. Kafka is designed around durable distributed event streams. They aren't direct replacements.

### 53. Why not use a database queue?

A database queue could work at lower scale, but using the primary database as a high-throughput event queue can create contention with business transactions and doesn't provide the same partitioned event-streaming model as Kafka.

### 54. Why not simply use a thread pool?

A thread pool provides asynchronous execution within a single application instance, but it doesn't provide durable distributed storage. If the pod crashes, in-memory tasks can be lost. Kafka provides durability, consumer groups, replay and horizontal scaling.

---

## I. Collaboration Questions

### 55. What exactly was your individual contribution?

> My contribution was primarily around the implementation and integration of the Kafka-based asynchronous notification flow. I worked on the consumer-side integration, Kafka topic integration, and downstream Notification Service interaction. I also coordinated with the Mutual Fund team regarding the topic and event contract and used Grafana to monitor the system behavior before and after the change.

If you genuinely worked on producer implementation too, say so. Don't claim things you didn't do.

### 56. How did you coordinate with the Mutual Fund team?

We aligned on the Kafka topic and event contract. I provided the topic details to the Mutual Fund team, and they published the PDN/DN events to that topic. We discussed the event structure, required fields and integration behavior so that our consumer could process the events correctly.

### 57. What problems did you face during integration?

A realistic answer:

> The main integration concerns were making sure both services agreed on the event contract, handling failures correctly, and ensuring that the consumer could process events reliably without creating additional load on the downstream Notification Service.

Don't invent a specific production incident unless you remember one.

### 58. How did you test producer and consumer integration?

We tested the event flow by generating representative PDN/DN events, verifying that they were published to Kafka, consumed by the Payments service, and correctly forwarded to the Notification Service. We also monitored processing behavior and system metrics through Grafana.

If you performed specific load testing, mention it. Otherwise don't claim it.

---

## J. Monitoring Questions

### 59. What did you monitor in Grafana?

We monitored application CPU and memory, traffic/throughput, latency and the behavior of the Kafka consumers. For Kafka specifically, consumer lag is an important metric because it tells us whether consumers are keeping up with the producer.

### 60. What alerts would you configure?

| Area | Alerts |
| --- | --- |
| **Kafka** | Consumer lag, broker health, under-replicated partitions, producer failures |
| **Consumer** | Processing failures, processing latency, consumer restarts |
| **Application** | CPU, memory, TPS, error rate, P95/P99 latency |
| **Notification** | 4xx/5xx, latency, timeout rate |

---

## K. Scenario-Based Questions

These are very likely in an SDE interview.

### 61. Kafka is receiving 1 million messages but consumers are slow. What do you do?

First I'd check consumer lag and processing latency. Then determine whether the bottleneck is CPU, database calls, network calls or the Notification Service.

If the consumer is under-capacity and there are enough partitions, I can increase consumers. If partition parallelism is insufficient, I can consider increasing partitions. If the downstream service is the bottleneck, I would control consumer concurrency rather than blindly adding consumers.

### 62. Consumer keeps crashing on one message. What do you do?

That's likely a poison message. I wouldn't let it continuously block processing. I'd isolate the failed event using retry handling and eventually a DLT, investigate the payload/error, fix the underlying problem and replay the message after correction.

### 63. Notification Service is returning 500 for every request. What happens?

I would avoid aggressive retries because that could amplify the outage. I'd use appropriate timeouts, backoff and potentially circuit-breaking/rate limiting. Kafka can retain the events while the downstream service recovers, subject to retention and system configuration.

### 64. Consumer successfully sends notification but crashes before offset commit. What happens?

The event can be delivered again. That's why the consumer/downstream operation should be idempotent if duplicate side effects are unacceptable.

### 65. Producer sends the same event twice. What happens?

Kafka can contain both records. The consumer should use an event identifier or another idempotency mechanism to prevent duplicate business effects.

### 66. One Kafka broker goes down. What happens?

If the affected partitions have replicas on other healthy brokers, Kafka can elect an available replica as leader. Availability therefore depends on proper replication configuration.

### 67. All Kafka brokers go down. What happens?

The Kafka pipeline becomes unavailable. Producers can't successfully publish and consumers can't consume. The system needs operational recovery, and critical upstream workflows should have appropriate failure handling rather than assuming Kafka is always available.

### 68. What if Kafka is slower than the producer?

Producer-side backpressure can occur. I'd inspect broker CPU, disk, network, partition distribution and producer latency. We may need to scale brokers, rebalance partitions or optimize message size and producer configuration.

### 69. What if consumer lag keeps increasing?

First, I would identify whether the issue is producer rate or consumer processing capacity. Then I'd check consumer processing latency, CPU, database calls, downstream latency and errors. If the consumers are the bottleneck and partitions allow it, I'd scale consumers. If the downstream service is the bottleneck, I'd control consumption rather than simply increasing consumers.

---

## L. Deep Kafka Questions

### 70. What is replication?

Replication means Kafka maintains copies of partition data across multiple brokers. This provides fault tolerance because if a broker fails, another replica can potentially become the leader.

### 71. Leader and follower?

```text
Partition P0

Broker 1 → Leader
Broker 2 → Replica
Broker 3 → Replica
```

The leader handles normal reads/writes while replicas maintain copies.

### 72. What is ISR?

**ISR = In-Sync Replicas.**

These are replicas that are sufficiently caught up with the partition leader and eligible according to Kafka's replication rules.

If the leader fails, an eligible ISR replica can become leader.

### 73. What is acks?

Producer acknowledgement configuration determines how much confirmation the producer requires.

| Setting | Meaning |
| --- | --- |
| `acks=0` | Don't wait for broker acknowledgement |
| `acks=1` | Leader acknowledgement |
| `acks=all` | Acknowledgement from required in-sync replicas |

The exact semantics depend on Kafka configuration/version.

### 74. Which acks would you choose for critical events?

For important financial-system events, I would generally favor stronger durability guarantees, such as `acks=all`, combined with appropriate replication and producer retry configuration. But the final configuration is a trade-off between durability and latency.

### 75. What is the trade-off of acks=all?

**Better:** Durability, reliability.

**Cost:** Potentially higher latency, more coordination.

### 76. How does Kafka scale?

Kafka scales primarily through: more brokers + more partitions + parallel consumers.

```text
Producer
   |
Kafka
 | | |
B1 B2 B3
   |
20 partitions
   |
8 consumers
```

### 77. Can two consumers consume the same partition simultaneously?

Within the same consumer group, normally **no**. One partition is assigned to one consumer at a time.

However, different consumer groups can independently consume the same partition.

### 78. Can multiple services consume the same Kafka topic?

Yes.

```text
PDN/DN Topic
     |
     +---- Payments Consumer Group
     |
     +---- Analytics Consumer Group
     |
     +---- Audit Consumer Group
```

Each consumer group gets its own logical consumption.

---

## M. Design Trade-off Questions

### 79. What are the trade-offs of your Kafka design?

| Advantages | Costs |
| --- | --- |
| Asynchronous processing | Eventual consistency |
| Decoupling | Operational complexity |
| Burst absorption | Duplicate processing possibility |
| Parallel processing | Ordering complexity |
| Horizontal scalability | Consumer lag monitoring |
| Consumer recovery | Kafka infrastructure |
| Event retention/replay capability | Retry/DLT complexity |

**Excellent interview answer:**

> The main trade-off was moving complexity from synchronous request handling into distributed event processing. We gained scalability and decoupling, but had to consider eventual consistency, retries, duplicate events, ordering and monitoring.

### 80. Why is Kafka particularly suitable for your SIP use case?

Your strongest answer:

> SIP processing is a good candidate for asynchronous event processing because the business event and notification delivery don't necessarily have to happen in the same synchronous request. We can publish the PDN/DN event quickly, let Kafka absorb bursts, and allow consumers to process the notification workload independently.

---

## N. The "Interviewer Keeps Digging" Version

This is how a real interview can go.

| Interviewer | You |
| --- | --- |
| Why Kafka? | To decouple the synchronous notification flow and handle bursty SIP traffic asynchronously. |
| Why does decoupling help? | The Mutual Fund service doesn't have to wait for downstream notification processing. |
| Then what happens to the event? | It's published to Kafka and processed by the Payments consumer. |
| Why 20 partitions? | To provide parallelism and future scaling capacity. |
| Why 8 consumers? | Eight consumers were sufficient for the processing workload, while 20 partitions provided additional parallelism capacity. |
| What if one consumer crashes? | Kafka rebalances the partitions and another consumer takes over. |
| What if the consumer already sent the notification before crashing? | The event may be processed again, so idempotency is important. |
| What if Notification Service is down? | The consumer shouldn't aggressively retry. We need retry/backoff and potentially DLT handling while Kafka retains the event. |
| What if Kafka is down? | Producer publication fails, so we need producer retries/error handling. If database and event publication must remain consistent, I'd consider an outbox pattern. |
| So is Kafka exactly once? | Not automatically for an external notification side effect. At-least-once processing with idempotency is generally a practical approach. |
| How did you know Kafka improved the system? | We monitored CPU, memory, throughput and latency using Grafana and compared equivalent SIP-processing periods before and after the change. |
| What exactly did YOU do? | I worked on the Kafka-based notification integration, consumer-side processing and downstream notification integration, coordinated the event contract/topic with the Mutual Fund team, and monitored the resulting system behavior. |

---

## O. 15 Answers You Should Memorize

If you have limited preparation time, memorize these:

| # | Question | Answer |
| --- | --- | --- |
| 1 | Why Kafka? | To decouple the synchronous notification flow and handle bursty SIP traffic asynchronously. |
| 2 | Why partitions? | To enable parallel processing and horizontal scalability. |
| 3 | Why 20? | It provided sufficient parallelism and scaling headroom for the workload. |
| 4 | Why 8 consumers? | Eight consumers were sufficient for the initial processing workload; Kafka could distribute the 20 partitions among them. |
| 5 | What is lag? | The difference between the latest available offset and the consumer's processed/committed position. |
| 6 | What if consumer crashes? | Kafka rebalances the partition to another consumer and processing resumes from the committed offset. |
| 7 | What if duplicate processing happens? | Use idempotency based on a unique event/business identifier. |
| 8 | What if Notification Service is down? | Retry with backoff and avoid overwhelming the downstream service; use DLT for persistent failures where appropriate. |
| 9 | Does Kafka guarantee ordering? | Ordering is guaranteed within a partition, not across partitions. |
| 10 | What if Kafka is down? | Producer publication can fail, so reliable producer retries/error handling are needed. |
| 11 | What is dual-write? | Updating the DB and publishing Kafka separately can leave one successful and the other failed. |
| 12 | Solution? | Transactional outbox. |
| 13 | Biggest benefit? | Decoupling and asynchronous processing. |
| 14 | Biggest drawback? | Eventual consistency and increased distributed-system complexity. |
| 15 | Your contribution? | Kafka integration, event flow/consumer processing, Notification Service integration, coordination with the Mutual Fund team, and monitoring/validation. |

---

## One Final Warning for Your Interview

There are four things you should **NOT** randomly invent if the interviewer asks:

- Exact partition key
- Replication factor
- Serialization format
- Exact Kafka producer/consumer configuration

Your current material only establishes:

| Fact | Value |
| --- | --- |
| Producer | Mutual Fund Service |
| Topic | PDN/DN |
| Kafka | 3 brokers |
| Partitions | 20 |
| Consumers | 8 |
| Consumer | Payments Service |
| Downstream | Notification Service |
| Channels | Push / Notification / Email |
| Volume | ~80K daily SIPs |
| Traffic | +100–150 TPS |
| Monitoring | Grafana |

These are the facts you can confidently anchor your story around.

If you get asked for an exact detail you genuinely don't remember, don't make one up. Say:

> I don't remember the exact production configuration value, but the design consideration was X.

For an SDE1 interview, that is far better than giving a technically incorrect answer and then getting trapped by 4–5 follow-ups.
