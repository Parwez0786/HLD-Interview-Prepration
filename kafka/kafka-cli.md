# Kafka CLI Commands

Assume Kafka is running locally on `localhost:9092`.

This note is the hands-on companion to topics, partitions, consumer groups, offsets, and lag.

---

## Table of Contents

- [1. Create a Topic](#1-create-a-topic)
- [2. List Topics](#2-list-topics)
- [3. Describe a Topic](#3-describe-a-topic)
- [4. Produce Messages](#4-produce-messages)
- [5. Consume Messages](#5-consume-messages)
- [6. Consumer Groups](#6-consumer-groups)
- [7. List Consumer Groups](#7-list-consumer-groups)
- [8. Describe Consumer Group](#8-describe-consumer-group)
- [9. Offset](#9-offset)
- [10. Check Topic Configuration](#10-check-topic-configuration)
- [11. Change Topic Configuration](#11-change-topic-configuration)
- [12. Delete Topic](#12-delete-topic)
- [13. CLI Cheat Sheet](#13-important-cli-cheat-sheet)
- [Part 2 — Build a Simple Order Event System](#part-2--build-simple-order-event-system)
- [14. Why Kafka Here?](#14-why-kafka-here)
- [15. Order Event](#15-order-event)
- [16. Create Orders Topic](#16-create-orders-topic)
- [17. Start Payment Consumer](#17-start-payment-consumer)
- [18. Start Notification Consumer](#18-start-notification-consumer)
- [19. Produce Order Events](#19-produce-order-events)
- [20. Now Add Partitions](#20-now-add-partitions)
- [21. Scale Payment Service](#21-scale-payment-service)
- [22. 4 Consumers and 3 Partitions](#22-what-if-we-have-4-consumers-and-3-partitions)
- [23. Consumer Groups vs Multiple Consumers](#23-consumer-groups-vs-multiple-consumers)
- [24. Check Everything](#24-check-everything)
- [25. Interview-Level Flow](#25-interview-level-flow)
- [26. Hands-On Challenge](#26-hands-on-challenge)
- [Day 7 Interview Questions](#day-7-interview-questions)

---

## 1. Create a Topic

Create an `orders` topic with 3 partitions and replication factor 1:

```bash
kafka-topics.sh \
  --create \
  --topic orders \
  --bootstrap-server localhost:9092 \
  --partitions 3 \
  --replication-factor 1
```

Think:

```text
orders
 ├── P0
 ├── P1
 └── P2
```

**Why 3 partitions?**

Because Kafka distributes messages across partitions.

It also allows multiple consumers in the same consumer group to process messages in parallel.

---

## 2. List Topics

```bash
kafka-topics.sh \
  --list \
  --bootstrap-server localhost:9092
```

Example:

```text
orders
payments
notifications
```

---

## 3. Describe a Topic

```bash
kafka-topics.sh \
  --describe \
  --topic orders \
  --bootstrap-server localhost:9092
```

You may see something like:

```text
Topic: orders
PartitionCount: 3
ReplicationFactor: 1

Partition: 0
Leader: 1
Replicas: 1
Isr: 1

Partition: 1
Leader: 1
Replicas: 1
Isr: 1

Partition: 2
Leader: 1
Replicas: 1
Isr: 1
```

### Interview understanding

**Partition**

```text
P0
P1
P2
```

**Leader**

The broker currently responsible for handling reads/writes for that partition.

**Replicas**

Copies of the partition.

**ISR**

In-Sync Replicas — replicas that are caught up with the leader.

---

## 4. Produce Messages

Use Kafka's console producer:

```bash
kafka-console-producer.sh \
  --topic orders \
  --bootstrap-server localhost:9092
```

Now type:

```text
order-101
order-102
order-103
order-104
```

Each line becomes a Kafka message.

Architecture:

```text
Terminal
   ↓
Kafka Producer
   ↓
orders topic
   ↓
Partition
```

---

## 5. Consume Messages

Open another terminal:

```bash
kafka-console-consumer.sh \
  --topic orders \
  --bootstrap-server localhost:9092
```

**Important:** by default, the consumer generally starts from **new** messages, not all historical messages.

To consume from the beginning:

```bash
kafka-console-consumer.sh \
  --topic orders \
  --bootstrap-server localhost:9092 \
  --from-beginning
```

You should see:

```text
order-101
order-102
order-103
order-104
```

---

## 6. Consumer Groups

A consumer normally belongs to a consumer group.

Run:

```bash
kafka-console-consumer.sh \
  --topic orders \
  --bootstrap-server localhost:9092 \
  --group payment-group
```

Now Kafka knows:

```text
Consumer
    ↓
payment-group
    ↓
orders
```

---

## 7. List Consumer Groups

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --list
```

Example:

```text
payment-group
notification-group
```

---

## 8. Describe Consumer Group

This is one of the most important Kafka commands for interviews.

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --describe \
  --group payment-group
```

Example:

```text
GROUP          TOPIC   PARTITION   CURRENT-OFFSET   LOG-END-OFFSET   LAG

payment-group  orders  0           100              105              5
payment-group  orders  1           200              200              0
payment-group  orders  2           150              153              3
```

Understand these three numbers:

### Current Offset

Where the consumer has processed/committed up to.

`CURRENT-OFFSET = 100`

### Log End Offset

Latest offset available in Kafka.

`LOG-END-OFFSET = 105`

### Lag

```text
LAG = LOG-END-OFFSET - CURRENT-OFFSET
```

So:

```text
105 - 100 = 5
```

Consumer is **5 messages behind**.

---

## 9. Offset

Suppose Kafka has:

```text
Partition 0

Offset
  ↓

0   order-1
1   order-2
2   order-3
3   order-4
4   order-5
```

The offset identifies the position of a message inside a partition.

**Important:** Kafka offsets are maintained separately for each partition.

So:

```text
P0 → offset 100
P1 → offset 250
P2 → offset 80
```

---

## 10. Check Topic Configuration

```bash
kafka-configs.sh \
  --bootstrap-server localhost:9092 \
  --entity-type topics \
  --entity-name orders \
  --describe
```

You can inspect topic-level configurations.

---

## 11. Change Topic Configuration

For example, retention:

```bash
kafka-configs.sh \
  --bootstrap-server localhost:9092 \
  --entity-type topics \
  --entity-name orders \
  --alter \
  --add-config retention.ms=86400000
```

This means approximately **24 hours**.

After the configured retention period, Kafka can delete old messages.

**Important interview point:** Kafka retention is **time/size based**, not "delete immediately after consumer reads."

---

## 12. Delete Topic

If your local Kafka configuration allows topic deletion:

```bash
kafka-topics.sh \
  --delete \
  --topic orders \
  --bootstrap-server localhost:9092
```

---

## 13. Important CLI Cheat Sheet

| Task | Command |
| --- | --- |
| Create topic | `kafka-topics.sh --create` |
| List topics | `kafka-topics.sh --list` |
| Describe topic | `kafka-topics.sh --describe` |
| Produce | `kafka-console-producer.sh` |
| Consume | `kafka-console-consumer.sh` |
| Consumer groups | `kafka-consumer-groups.sh` |
| Describe group | `kafka-consumer-groups.sh --describe` |
| Topic config | `kafka-configs.sh` |
| Delete topic | `kafka-topics.sh --delete` |

---

## Part 2 — Build Simple Order Event System

Now let's build something closer to a real backend architecture.

### Architecture

```text
                    Kafka
                      │
                      ▼
              ┌───────────────┐
              │ orders topic  │
              └───────────────┘
                 │         │
                 ▼         ▼
        Payment Service   Notification Service
```

But conceptually, we have:

```text
Order Service
     │
     │ publish OrderCreated
     ▼
Kafka
     │
     ▼
orders topic
     │
     ├──────────────► Payment Service
     │
     └──────────────► Notification Service
```

---

## 14. Why Kafka Here?

**Without Kafka:**

```text
Order Service
     │
     ├──► Payment Service
     │
     └──► Notification Service
```

Order Service has to directly call both services.

If Payment Service is down:

```text
Order Service
     │
     └──X Payment
```

The order flow may become slow or fail.

**With Kafka:**

```text
Order Service
     │
     ▼
 Kafka
     │
     ▼
orders
```

Order Service can successfully publish the event and continue.

Payment and Notification process it asynchronously.

---

## 15. Order Event

Let's define an event:

```json
{
  "eventId": "evt-101",
  "orderId": "ORD-1001",
  "userId": "USER-10",
  "amount": 999,
  "eventType": "ORDER_CREATED"
}
```

In a real Spring Boot application, this could be a Java class:

```java
public class OrderCreatedEvent {

    private String eventId;
    private String orderId;
    private String userId;
    private double amount;
    private String eventType;
}
```

---

## 16. Create Orders Topic

```bash
kafka-topics.sh \
  --create \
  --topic orders \
  --bootstrap-server localhost:9092 \
  --partitions 3 \
  --replication-factor 1
```

Check:

```bash
kafka-topics.sh \
  --describe \
  --topic orders \
  --bootstrap-server localhost:9092
```

---

## 17. Start Payment Consumer

Terminal 1:

```bash
kafka-console-consumer.sh \
  --topic orders \
  --bootstrap-server localhost:9092 \
  --group payment-service
```

Think:

```text
orders
   ↓
payment-service
```

---

## 18. Start Notification Consumer

Terminal 2:

```bash
kafka-console-consumer.sh \
  --topic orders \
  --bootstrap-server localhost:9092 \
  --group notification-service
```

Now:

```text
                 orders
                    │
           ┌────────┴────────┐
           ▼                 ▼
   payment-service   notification-service
```

### Very important concept

These are **two different consumer groups**.

Therefore, **both groups receive the same events**.

```text
                orders
                   │
          ┌────────┴─────────┐
          │                  │
          ▼                  ▼
     Group A             Group B
     Payment             Notification
```

This is exactly why Kafka consumer groups are powerful.

---

## 19. Produce Order Events

Open another terminal:

```bash
kafka-console-producer.sh \
  --topic orders \
  --bootstrap-server localhost:9092
```

Send:

```json
{"eventId":"evt-1","orderId":"ORD-101","userId":"USER-1","amount":500,"eventType":"ORDER_CREATED"}
```

Then:

```json
{"eventId":"evt-2","orderId":"ORD-102","userId":"USER-2","amount":1000,"eventType":"ORDER_CREATED"}
```

Both consumers should receive the events.

```text
Payment Service
    ↓
evt-1
evt-2

Notification Service
    ↓
evt-1
evt-2
```

---

## 20. Now Add Partitions

Suppose:

```text
orders

P0
P1
P2
```

And Payment Service has:

```text
C1
C2
C3
```

Kafka can assign:

```text
P0 → C1
P1 → C2
P2 → C3
```

So three messages can potentially be processed in parallel.

---

## 21. Scale Payment Service

Start another consumer using the **same group**:

```bash
kafka-console-consumer.sh \
  --topic orders \
  --bootstrap-server localhost:9092 \
  --group payment-service
```

Now:

```text
Payment Group

C1 → P0
C2 → P1
```

If you have 3 partitions:

```text
P0 → C1
P1 → C2
P2 → nobody
```

Start a third:

```text
P0 → C1
P1 → C2
P2 → C3
```

This is **horizontal scaling**.

---

## 22. What If We Have 4 Consumers and 3 Partitions?

```text
Partitions = 3
Consumers = 4
```

Maximum active consumers: **3**

One consumer remains idle.

```text
P0 → C1
P1 → C2
P2 → C3

C4 → idle
```

### Interview rule

In a consumer group, **one partition can be assigned to only one consumer at a time**.

Therefore:

> Useful consumers ≤ number of partitions

---

## 23. Consumer Groups vs Multiple Consumers

This is extremely important.

### Same group

```text
orders
  │
  ▼
payment-group
  ├── C1
  ├── C2
  └── C3
```

Messages are **distributed** among consumers.

Used for: **scaling one service**.

### Different groups

```text
                orders
              /        \
             /          \
    payment-group   notification-group
```

Both groups receive the events **independently**.

Used for: **different services consuming the same event**.

---

## 24. Check Everything

After running the system:

### Topics

```bash
kafka-topics.sh \
  --list \
  --bootstrap-server localhost:9092
```

### Topic details

```bash
kafka-topics.sh \
  --describe \
  --topic orders \
  --bootstrap-server localhost:9092
```

### Consumer groups

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --list
```

### Payment group

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --describe \
  --group payment-service
```

### Notification group

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --describe \
  --group notification-service
```

---

## 25. Interview-Level Flow

If the interviewer asks:

> Explain your Kafka order system.

Say:

> The Order Service publishes an OrderCreated event to the orders Kafka topic. The topic is divided into multiple partitions for scalability. Payment Service and Notification Service consume this topic using separate consumer groups. Because they use different groups, both services receive every order event. Within the Payment Service group, multiple consumers can process different partitions in parallel. Kafka maintains offsets for each consumer group, so if a consumer restarts, it can continue from its last committed offset.

Then explain:

```text
Order Service
     │
     │ OrderCreated
     ▼
Kafka orders topic
     │
     ├───────────────┐
     ▼               ▼
Payment Group   Notification Group
     │               │
 C1 C2 C3            C1
```

---

## 26. Hands-On Challenge

Don't just run the commands once. Do this sequence yourself:

**Step 1** — Create `orders` with 3 partitions.

**Step 2** — Start `payment-service` with 2 consumers.

**Step 3** — Start `notification-service` with 1 consumer.

**Step 4** — Produce 10 order events.

**Step 5** — Describe both consumer groups. Look at `CURRENT-OFFSET`, `LOG-END-OFFSET`, `LAG`.

**Step 6** — Kill one Payment consumer. Produce 10 more messages. Observe the rebalance.

**Step 7** — Start the consumer again. Observe how Kafka assigns partitions again.

**Step 8** — Stop Payment Service completely. Produce messages. Then restart Payment Service and see what happens to the backlog.

This will make consumer groups, offsets, lag, partition assignment, and rebalancing much easier to understand.

---

## Day 7 Interview Questions

You should be able to answer these without notes:

- What is a Kafka topic?
- What is a partition?
- What is an offset?
- What is a consumer group?
- Why do we need consumer groups?
- Can two consumers in the same group consume the same partition?
- What happens if consumers > partitions?
- What happens if partitions > consumers?
- What is consumer lag?
- How do you check consumer lag from CLI?
- How do you check topic partitions?
- How do two different consumer groups receive the same Kafka message?
- What happens when a consumer crashes?
- What is rebalancing?
- How would you scale Payment Service?
- Why would Payment and Notification use different consumer groups?
- Why doesn't Kafka delete a message immediately after a consumer reads it?
- How would you troubleshoot a growing consumer lag?

### Most important Day 7 takeaway

```text
Topic
  ↓
Partitions
  ↓
Consumer Group
  ↓
Consumers
  ↓
Offsets
  ↓
Lag
  ↓
Rebalancing
```

If you understand this chain practically, you're ready to move into **Day 8 — Kafka Reliability & Delivery Semantics**: at-most-once, at-least-once, exactly-once, retries, idempotency, DLQ, and duplicate events.
