# Phase 1 — Kafka Fundamentals

## Day 1 & Day 2 Detailed Tutorial

---

## Table of Contents

- [What is Apache Kafka?](#what-is-apache-kafka)
- [Why Was Kafka Created?](#why-was-kafka-created)
- [Problem With Traditional Synchronous Communication](#problem-with-traditional-synchronous-communication)
- [Solution: Asynchronous Communication](#solution-asynchronous-communication)
- [Producer-Consumer Model](#producer-consumer-model)
- [Message Queue vs Event Streaming](#message-queue-vs-event-streaming)
- [Kafka vs Traditional Message Brokers](#kafka-vs-traditional-message-brokers)
- [Kafka Use Cases](#kafka-use-cases)
- [Kafka Terminology](#kafka-terminology)
- [Real-World Example](#real-world-example)
- [Kafka Architecture Overview](#kafka-architecture-overview)
- [Why Kafka Is Fast?](#why-kafka-is-fast)
- [Practice: Order Processing System](#practice-order-processing-system)
- [Why Kafka Instead Of REST?](#why-kafka-instead-of-rest)
- [Day 2 — Kafka Core Concepts](#day-2--kafka-core-concepts)
- [Complete Kafka Flow](#complete-kafka-flow)
- [Hands-On Practice (CLI)](#hands-on-practice-cli)
- [Interview Questions](#interview-questions)

---

## What is Apache Kafka?

Apache Kafka is a **distributed event streaming platform** used to exchange data between applications in a reliable, scalable, and fault-tolerant manner.

Think of Kafka as a high-speed data highway where applications continuously send and receive events.

**Examples of events:**

- User places order
- Payment completed
- SIP mandate created
- Notification sent
- Stock price updated

All these events can flow through Kafka.

---

## Why Was Kafka Created?

Before Kafka, applications usually communicated **directly**.

**Example:**

```text
Order Service
      |
      v
Payment Service
      |
      v
Inventory Service
      |
      v
Notification Service
```

Every service calls another service using REST APIs.

**Problems:**

- Tight coupling
- Slow response
- Failures propagate
- Difficult scaling

---

## Problem With Traditional Synchronous Communication

Suppose a user places an order. Order Service then calls, via REST:

- Payment Service
- Inventory Service
- Notification Service
- Analytics Service

### Scenario

Notification Service is down.

```text
Order Service
    |
    +--> Payment      ✓
    |
    +--> Inventory    ✓
    |
    +--> Notification ❌
```

Now Order Service must:

- Retry
- Wait
- Handle timeout

User response becomes slow.

### Problems

#### 1. High Latency

Every call waits for a response.

```text
Order → Payment → Inventory → Notification
```

**Total latency:** `100 + 100 + 100 = 300ms`

#### 2. Tight Coupling

Order Service knows:

- Notification endpoint
- Analytics endpoint
- Inventory endpoint

Changing one service impacts others.

#### 3. Failure Propagation

One service down can affect the whole chain.

#### 4. Scalability Issues

10M requests means **10M direct API calls** — huge load.

---

## Solution: Asynchronous Communication

Instead of directly calling services:

```text
Order Service
      |
      v
   Kafka
   / | \
  /  |  \
Payment
Inventory
Notification
```

Order Service only **sends an event**. Kafka handles distribution.

---

## Producer-Consumer Model

This is the **core Kafka concept**.

### Producer

Producer **sends** messages.

Example: Order Service sends the following to Kafka:

```json
{
  "orderId": 101,
  "amount": 500
}
```

**Producer = Sender**

### Consumer

Consumer **reads** messages.

Example: Inventory Service reads the order event.

**Consumer = Receiver**

### Analogy — WhatsApp Group

| Role | WhatsApp | Kafka |
| --- | --- | --- |
| You send a message | You | **Producer** |
| The group | Group chat | **Kafka Topic** |
| Friends | People who read | **Consumers** |

---

## Message Queue vs Event Streaming

Many people confuse these.

### Traditional Queue

Examples: RabbitMQ, SQS, ActiveMQ

Message is consumed **once**.

```text
Producer
    |
  Queue
    |
Consumer
```

After consumption, the **message is removed**.

### Kafka Event Streaming

Kafka **keeps data**.

```text
Producer
    |
  Kafka
    |
Consumers
```

- Message remains stored
- Multiple consumers can read independently

**Example:** `Order Created` can be read by:

- Payment
- Analytics
- Inventory
- Notification

all independently.

---

## Kafka vs Traditional Message Brokers

| Feature | Kafka | RabbitMQ |
| --- | --- | --- |
| Storage | Persistent | Mostly queue-based |
| Throughput | Very High | Moderate |
| Replay Messages | Yes | Limited |
| Retention | Days / Months | Usually deleted |
| Scalability | Excellent | Good |
| Event Streaming | Yes | No |
| Analytics Use Cases | Excellent | Limited |

---

## Kafka Use Cases

### 1. Order Processing

Amazon-like systems.

```text
Order Service
     |
     v
   Kafka
```

**Consumers:** Payment, Inventory, Notification

### 2. Notification Systems

Like a Paytm Money PDN/DN system.

Topic: `sip.pdn.debit.events`

Kafka topic receives events. Consumers send:

- SMS
- Push
- Email

### 3. Activity Tracking

- User Login
- User Click
- User Purchase

Billions of events.

### 4. Banking

Transactions such as:

- Debit
- Credit
- Mandate Created

### 5. IoT

Sensors continuously send data.

### 6. Real-Time Analytics

Used by Netflix, Uber, Swiggy, Paytm.

---

## Kafka Terminology

### Event

Something happened.

```json
{
  "orderId": 101
}
```

Example: Order created.

### Record

A single message inside Kafka.

### Topic

Logical category — like a database table.

Examples:

- `order-created`
- `payment-success`
- `user-login`

### Partition

A topic divided into chunks. Improves parallelism.

```text
Order Topic
  ├── Partition 0
  ├── Partition 1
  └── Partition 2
```

### Offset

Unique message number inside a partition — like a row number.

```text
Partition 0
  ├── Offset 0
  ├── Offset 1
  ├── Offset 2
  └── Offset 3
```

### Broker

A Kafka server. Stores data.

### Consumer Group

A group of consumers. Allows parallel processing.

---

## Real-World Example

**Paytm Notification Flow**

```text
Mutual Fund Service
       |
       v
Kafka Topic
(sip.pdn.debit.events)
       |
       +--> SMS Service
       |
       +--> Push Service
       |
       +--> Analytics Service
```

Producer publishes once. Multiple consumers process independently.

---

## Kafka Architecture Overview

```text
                Producer
                    |
                    v
          +------------------+
          |      Topic       |
          +------------------+
             |    |    |
            P0   P1   P2
                    |
                    v
          +------------------+
          |      Broker      |
          +------------------+
                    |
                    v
             Consumer Group
            C1     C2     C3
```

---

## Why Kafka Is Fast?

### Sequential Disk Writes

Instead of random writes (`A -> B -> C -> D`), Kafka **appends** messages. Very efficient.

### Partitioning

Multiple partitions process data simultaneously.

### Batch Processing

Producer sends batches — not one message at a time.

### Zero Copy Transfer

Kafka can transfer data from disk to network without copying into application memory repeatedly.

---

## Practice: Order Processing System

### Requirement

User places an order. Need:

- Payment
- Inventory
- Notification

### Without Kafka

```text
Order Service
   |
   +--> Payment API
   |
   +--> Inventory API
   |
   +--> Notification API
```

**Problems:** Slow, tight coupling.

### With Kafka

```text
Order Service
      |
      v
Order Topic
      |
      +--> Payment Consumer
      |
      +--> Inventory Consumer
      |
      +--> Notification Consumer
```

**Benefits:**

- Independent services
- Better scaling
- Failure isolation

---

## Why Kafka Instead Of REST?

**REST is better when** you need an immediate response:

- Login
- Get user profile
- Check balance

**Kafka is better when** it is fire-and-forget:

- Order placed
- Send email
- Analytics
- Notifications

| REST | Kafka |
| --- | --- |
| Synchronous | Asynchronous |
| Request / Response | Event Driven |
| Tight Coupling | Loose Coupling |
| Real-time API | Event Processing |
| Limited Scalability | Massive Scalability |

---

## Day 2 — Kafka Core Concepts

### Broker

A Kafka server that stores messages.

Example cluster:

- Broker 1
- Broker 2
- Broker 3

### Topic

A category of messages.

Examples: `orders`, `payments`, `notifications`

### Partition

Topics are split for parallel processing and scalability.

```text
orders
  ├── P0
  ├── P1
  └── P2
```

### Record

A single Kafka message.

```json
{
  "orderId": 101,
  "amount": 500
}
```

### Key

Used for partitioning.

Example: `UserID = 1001`

Kafka hashes the key to determine the partition:

```text
hash(1001) % 3 = 2  →  Partition 2
```

All events for the same user stay **ordered**.

### Timestamp

Message creation time.

Example: `2026-01-01 10:00:00`

### Headers

Metadata.

```text
source=payment-service
version=v1
```

### Offset

Message index. Consumers track offsets.

```text
Partition 0
  ├── Offset 0
  ├── Offset 1
  ├── Offset 2
  └── Offset 3
```

### Consumer Group

Suppose 3 partitions and a consumer group with C1, C2, C3.

**Assignment:**

| Partition | Consumer |
| --- | --- |
| P0 | C1 |
| P1 | C2 |
| P2 | C3 |

This enables parallel processing.

### Replication

Copies of partitions.

**Example:** Replication Factor = 3

Partition stored on Broker1, Broker2, Broker3.

### Leader

Receives reads and writes.

**Example:** P0 Leader → Broker1

### Followers

Replicas that copy leader data.

- Broker2
- Broker3

If the leader crashes, a follower becomes leader. **No downtime.**

---

## Complete Kafka Flow

```text
Producer
   |
   v
Topic
   |
   v
Partition
   |
   v
Broker
   |
   v
Consumer Group
   |
   v
Consumer
```

### Example Flow

Producer sends:

```json
{
  "orderId": 1001
}
```

| Step | Value |
| --- | --- |
| Topic | `orders` |
| Partition (after hash) | Partition 1 |
| Stored on | Broker 2 |
| Offset | 35 |
| Consumer Group | `order-processors` |
| Consumer | Consumer 1 |

Consumer 1 reads Offset 35 → processing complete → **offset committed**.

---

## Hands-On Practice (CLI)

### Create Topic

```bash
kafka-topics.sh \
  --create \
  --topic orders \
  --partitions 3 \
  --replication-factor 1 \
  --bootstrap-server localhost:9092
```

### List Topics

```bash
kafka-topics.sh \
  --list \
  --bootstrap-server localhost:9092
```

### Describe Topic

```bash
kafka-topics.sh \
  --describe \
  --topic orders \
  --bootstrap-server localhost:9092
```

**Output:**

```text
Topic: orders
PartitionCount: 3
  Partition 0
  Partition 1
  Partition 2
```

### Produce Messages

```bash
kafka-console-producer.sh \
  --topic orders \
  --bootstrap-server localhost:9092
```

Enter:

```text
order1
order2
order3
```

### Consume Messages

```bash
kafka-console-consumer.sh \
  --topic orders \
  --from-beginning \
  --bootstrap-server localhost:9092
```

### Check Consumer Group

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --describe \
  --group order-group
```

---

## Interview Questions

**Why partitions?**

To achieve parallelism and scalability.

**Why offsets?**

To track which messages are consumed.

**Why consumer groups?**

To distribute workload across consumers.

**Why replication?**

Fault tolerance and high availability.

**Difference between partition and consumer group?**

- Partition **stores** data
- Consumer group **processes** data

**If a topic has 3 partitions and 5 consumers?**

Only 3 consumers will be active. 2 consumers remain idle.

**If a topic has 10 partitions and 3 consumers?**

One consumer handles multiple partitions.
