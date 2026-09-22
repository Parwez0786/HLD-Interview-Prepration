# Notification System — HLD Interview

## Interview question

> Design a notification system that can send push, SMS, email, and WhatsApp notifications at scale.

A good answer should **not** immediately jump to Kafka or Twilio. Start by clarifying the requirements.

---

## Table of Contents

- [Step 1 — Clarifying Questions](#step-1--clarifying-questions)
- [Step 2 — Functional Requirements](#step-2--functional-requirements)
- [Step 3 — Non-Functional Requirements](#step-3--non-functional-requirements)
- [Step 4 — Basic Architecture](#step-4--basic-architecture)
- [Step 5 — Why Kafka?](#step-5--why-kafka)
- [Step 6 — Kafka Topic Design](#step-6--kafka-topic-design)
- [Step 7 — Notification Flow](#step-7--notification-flow)
- [Step 8 — Database Design](#step-8--database-design)
- [Step 9 — Redis](#step-9--redis)
- [Step 10 — Template Rendering](#step-10--template-rendering)
- [Step 11 — Retry Mechanism](#step-11--retry-mechanism)
- [Step 12 — Dead Letter Queue](#step-12--dead-letter-queue)
- [Step 13 — Idempotency](#step-13--idempotency--very-important-follow-up)
- [Step 14 — The Idempotency Problem](#step-14--but-there-is-a-problem-with-idempotency)
- [Step 15 — Provider Failover](#step-15--provider-failover)
- [Step 16 — Rate Limiting](#step-16--rate-limiting)
- [Step 17 — Bulk Notifications](#step-17--bulk-notifications)
- [Step 18 — Scheduled Notifications](#step-18--scheduled-notifications)
- [Step 19 — Priority Notifications](#step-19--priority-notifications)
- [Step 20 — Ordering](#step-20--ordering)
- [Step 21 — Backpressure](#step-21--backpressure)
- [Step 22 — Auto Scaling](#step-22--auto-scaling)
- [Step 23 — Monitoring](#step-23--monitoring)
- [Step 24 — Final Architecture](#step-24--final-architecture)
- [What I Would Say in the Interview](#what-i-would-say-in-the-interview)
- [Interview Follow-ups](#interview-follow-ups-you-should-prepare)
- [Deeper Follow-ups](#deeper-follow-ups)
- [The 10 Follow-ups I Would Expect](#the-10-follow-ups-i-would-expect-in-an-sde-2-interview)

---

## Step 1 — Clarifying Questions

In an interview, I would first say:

> Before designing the system, I would like to clarify the requirements.

### Q1. What notification channels do we support?

Let's assume:

- Push notification
- SMS
- Email
- WhatsApp

### Q2. Is notification synchronous or asynchronous?

Notification sending should be **asynchronous**.

```text
Order Service
     |
     | "Order placed"
     ↓
Notification Service
     |
     ↓
Kafka
     |
     ↓
Notification Workers
     |
     ↓
SMS / Email / Push
```

The Order Service should **not** wait for SMS/Email delivery.

### Q3. Do we need immediate delivery?

**Yes**, for critical notifications:

- Payment successful
- OTP
- Order placed
- Transaction failed

We want **near-real-time** delivery.

For marketing notifications, some delay is acceptable:

- Sale starts tomorrow
- Weekend offer
- New product available

### Q4. Do users have notification preferences?

**Yes.** For example, User 101:

| Preference | Setting |
| --- | --- |
| Email | ON |
| SMS | OFF |
| Push | ON |
| Marketing | OFF |
| Transaction | ON |

### Q5. Do we need retry?

**Yes.** External providers can fail.

```text
Notification Service
       |
       ↓
    Twilio
       |
    FAILURE
       |
       ↓
     Retry
```

We should retry **temporary** failures.

---

## Step 2 — Functional Requirements

I would define the following.

### Core requirements

- Send notification
- Support multiple channels
- Support notification templates
- Respect user preferences
- Retry failed notifications
- Track notification status
- Support scheduled notifications
- Prevent duplicate notifications
- Support bulk notifications

**Example API:**

```text
sendNotification(
    userId,
    eventType,
    channel,
    data
)
```

---

## Step 3 — Non-Functional Requirements

### High availability

The notification system should remain available even if one provider goes down.

### Scalability

Suppose:

- 100 million users
- 10 million notifications/day

The architecture should handle traffic spikes.

### Low latency

For OTP / payment notifications:

```text
Event → Notification → Provider
```

should ideally take only a few seconds.

### Reliability

We should avoid losing notifications.

### Fault tolerance

If SMS Provider A fails, we should potentially use SMS Provider B.

---

## Step 4 — Basic Architecture

I would start with a simple architecture and then scale it.

```text
                         +----------------+
                         |  Client / Apps |
                         +-------+--------+
                                 |
                                 ↓
                         +---------------+
                         |  API Gateway  |
                         +-------+-------+
                                 |
                                 ↓
                     +------------------------+
                     | Notification Service   |
                     +-----------+------------+
                                 |
              +------------------+------------------+
              |                  |                  |
              ↓                  ↓                  ↓
         Preference          Template            Notification
           Service            Service              DB
              |                  |                  |
              +------------------+------------------+
                                 |
                                 ↓
                              Kafka
                                 |
          +----------------------+----------------------+
          |                      |                      |
          ↓                      ↓                      ↓
    Push Worker             SMS Worker             Email Worker
          |                      |                      |
          ↓                      ↓                      ↓
     Push Provider          SMS Provider          Email Provider
```

---

## Step 5 — Why Kafka?

This is an important interview discussion.

Suppose Order Service directly calls:

```text
Order Service → Notification Service → SMS Provider
```

**Problem:** If SMS provider takes 5 seconds:

```text
Order API
   |
   ↓
Notification
   |
   ↓
SMS Provider
   |
   ↓
5 seconds
```

The order API becomes slow.

**Instead:**

```text
Order Service
      |
      | event
      ↓
    Kafka
      |
      ↓
Notification Service
```

Order Service doesn't wait for notification delivery.

Kafka also gives us:

- Buffering
- Retry capability
- Horizontal scaling
- Decoupling
- Durable events

---

## Step 6 — Kafka Topic Design

I would create separate topics:

- `notification.push`
- `notification.sms`
- `notification.email`
- `notification.whatsapp`

Or we can have a single `notification.events` topic with channel information inside the message.

```json
{
  "notificationId": "N123",
  "userId": "U100",
  "type": "ORDER_PLACED",
  "channel": "SMS",
  "templateId": "ORDER_SMS_V1",
  "data": {
    "orderId": "O123"
  }
}
```

### Which approach would I choose?

For a large-scale system, I prefer **separate topics** for major channels because:

- Different throughput
- Different retry requirements
- Independent scaling
- Easier monitoring

---

## Step 7 — Notification Flow

Suppose a user places an order.

### Step 1

Order Service creates the order, then publishes `OrderPlacedEvent` to Kafka.

```text
Order Service
     |
     ↓
Order DB
```

### Step 2

Notification Service consumes the event and checks **user preferences**.

```text
Kafka
  |
  ↓
Notification Service
```

### Step 3

It creates notification records:

| Field | Description |
| --- | --- |
| `notification_id` | Unique ID |
| `user_id` | Target user |
| `event_type` | e.g. `ORDER_PLACED` |
| `channel` | SMS / Email / Push |
| `status` | Current state |
| `created_at` | Timestamp |

### Step 4

Message is published to a channel-specific Kafka topic, e.g. `notification.sms`.

### Step 5

SMS Worker consumes it and calls the SMS Provider.

```text
SMS Worker
    |
    ↓
SMS Provider
```

### Step 6

Provider returns `SUCCESS`. We update:

```text
notification.status = SENT
```

---

## Step 8 — Database Design

We don't need a huge number of tables.

### User Preferences — `user_preferences`

| Column | Description |
| --- | --- |
| `user_id` | User |
| `channel` | SMS / EMAIL / PUSH |
| `notification_type` | TRANSACTION / MARKETING |
| `enabled` | true / false |
| `updated_at` | Last change |

**Example:**

```text
101 | SMS   | TRANSACTION | true
101 | EMAIL | MARKETING   | false
101 | PUSH  | TRANSACTION | true
```

### Notification — `notifications`

| Column | Description |
| --- | --- |
| `id` | Notification ID |
| `user_id` | User |
| `event_type` | Event |
| `channel` | Channel |
| `template_id` | Template used |
| `status` | Current status |
| `retry_count` | How many retries |
| `provider` | Which provider sent it |
| `provider_message_id` | Provider's ID |
| `created_at` | Created |
| `updated_at` | Last update |

**Status:**

- `PENDING`
- `PROCESSING`
- `SENT`
- `FAILED`

### Template — `templates`

| Column | Description |
| --- | --- |
| `id` | Template ID |
| `event_type` | Event |
| `channel` | Channel |
| `version` | e.g. V2 |
| `content` | Template body |
| `created_at` | Created |
| `updated_at` | Last update |

**Example:**

```text
ORDER_PLACED
SMS
V2

"Your order {{orderId}} has been placed."
```

---

## Step 9 — Redis

Now the interviewer may ask:

> Why do you need Redis?

We can cache frequently accessed information.

For example: `user_preferences:101`

Instead of hitting the DB every time:

```text
Notification Worker
      |
      ↓
      DB
```

Use Redis:

```text
Notification Worker
      |
      ↓
    Redis
      |
      ↓
 preferences
```

This reduces DB load.

---

## Step 10 — Template Rendering

Suppose the template is:

```text
"Your order {{orderId}} has been successfully placed."
```

Event:

```json
{
  "orderId": "ORD123"
}
```

Template engine converts it to:

```text
Your order ORD123 has been successfully placed.
```

We should keep templates **outside application code**.

**Why?** Because business teams may need to modify SMS text, email content, and push titles **without deploying the service**.

---

## Step 11 — Retry Mechanism

Suppose the SMS Provider returns an error. We don't immediately mark it permanently failed. We retry.

| Attempt | Delay |
| --- | --- |
| Attempt 1 | immediately |
| Attempt 2 | 10 sec |
| Attempt 3 | 30 sec |
| Attempt 4 | 2 min |

This is **exponential backoff**.

---

## Step 12 — Dead Letter Queue

What if all retries fail?

```text
Kafka
  ↓
SMS Worker
  ↓
FAILED
  ↓
Retry
  ↓
Retry
  ↓
Retry
  ↓
DLQ
```

**DLQ = Dead Letter Queue.**

We can later inspect and replay these messages.

---

## Step 13 — Idempotency — Very Important Follow-up

Interviewer:

> What happens if Kafka delivers the same notification twice?

**Without idempotency:**

```text
Kafka
 ↓
SMS Worker
 ↓
SMS sent
```

Then the same message comes again:

```text
Kafka
 ↓
SMS Worker
 ↓
SMS sent again
```

User gets **two SMS messages**.

### Solution

Give every notification a unique ID: `notificationId = N123`

Before sending:

> Does N123 already exist as SENT?

If yes: **don't send again**.

We can use Redis or DB for deduplication.

---

## Step 14 — But There Is a Problem With Idempotency

Interviewer may ask:

> What if SMS is successfully sent but your DB update fails?

```text
SMS Provider
     ↓
SUCCESS
     ↓
SMS delivered
     ↓
DB update fails
```

System thinks `status = FAILED`, then retries.

**Result:** User gets duplicate SMS.

This is a classic distributed systems problem.

We cannot easily get **exactly-once** delivery across our DB + external provider.

So we generally design for:

> **At-least-once processing + idempotency** where the provider supports it.

If the provider supports an idempotency key, send:

```text
idempotencyKey = notificationId
```

That is much safer.

---

## Step 15 — Provider Failover

Suppose SMS Provider A is down.

```text
SMS Worker
    |
    +----> Provider A
    |
    +----> Provider B
```

**Flow:**

```text
Provider A
   ↓
FAIL
   ↓
Provider B
   ↓
SUCCESS
```

We can use a provider abstraction:

```text
NotificationProvider
    send()
```

**Implementations:**

- `TwilioProvider`
- `ProviderB`
- `AWSProvider`

The worker doesn't care which provider is being used.

---

## Step 16 — Rate Limiting

Interviewer:

> What if one user requests 100,000 notifications?

We need rate limiting.

For example:

```text
userId = 101
max = 10 notifications/minute
```

Redis can implement this.

Also **provider-level limits** matter.

For example: SMS Provider allows 1000 requests/sec. Our worker must not exceed it.

---

## Step 17 — Bulk Notifications

Now interviewer asks:

> How do you send a notification to 50 million users?

Don't create 50 million synchronous API calls.

Instead:

```text
Campaign Service
      |
      ↓
User Segment
      |
      ↓
Kafka
      |
      ↓
Workers
      |
      ↓
Providers
```

We can partition Kafka heavily.

Example topic: `notification.bulk`

```text
Partition 0
Partition 1
Partition 2
...
Partition 100
```

Workers process partitions in parallel.

---

## Step 18 — Scheduled Notifications

Example: send notification tomorrow at 10 AM.

We need scheduling.

**One approach:**

```text
Scheduled Notification DB
          |
          ↓
Scheduler
          |
          ↓
Kafka
          |
          ↓
Workers
```

For very large scale, we can use a **time-bucket** approach:

```text
10:00 bucket
10:01 bucket
10:02 bucket
```

Scheduler fetches notifications whose execution time has arrived and pushes them to Kafka.

---

## Step 19 — Priority Notifications

Not all notifications have the same priority.

| Type | Priority |
| --- | --- |
| OTP | HIGH |
| Payment | HIGH |
| Order placed | MEDIUM |
| Marketing | LOW |

We can create:

- `notification.high`
- `notification.normal`
- `notification.low`

Workers consume high-priority messages first.

This prevents marketing traffic from delaying OTPs.

---

## Step 20 — Ordering

Interviewer:

> Do you guarantee notification ordering?

Suppose:

```text
Order Created
Order Cancelled
```

We don't want:

```text
Order Cancelled
Order Created
```

For ordering, use `userId` as the Kafka partition key.

```text
Kafka partition = hash(userId)
```

Therefore:

```text
User 101
   ↓
Partition 3

Notification A
Notification B
Notification C
```

will maintain ordering within that partition.

**Important:** Kafka only guarantees ordering **within a partition**, not across partitions.

---

## Step 21 — Backpressure

Suppose providers can handle 10,000 requests/sec, but Kafka receives 100,000 requests/sec.

Kafka acts as a **buffer**.

```text
100K/sec
   ↓
 Kafka
   ↓
10K/sec
   ↓
Provider
```

Consumer workers process at the provider's safe rate.

We can increase workers when backlog increases.

---

## Step 22 — Auto Scaling

**Important metric:** Kafka consumer lag

If lag increases:

```text
Consumer Lag ↑
      ↓
Workers ↑
```

If lag decreases:

```text
Consumer Lag ↓
      ↓
Workers ↓
```

**Other metrics:**

- CPU
- Requests/sec
- Provider latency
- Failure rate

---

## Step 23 — Monitoring

We should monitor:

### System metrics

- Kafka lag
- CPU
- Memory
- Throughput
- Latency

### Notification metrics

- Sent
- Failed
- Retried
- Delivered

### Provider metrics

- Provider A success rate
- Provider B success rate
- Provider latency

**Example:**

```text
SMS success rate = 98.2%
SMS failure rate = 1.8%
```

---

## Step 24 — Final Architecture

Putting everything together:

```text
                         +----------------+
                         | Order/Payment  |
                         | Other Services |
                         +-------+--------+
                                 |
                                 ↓
                         +---------------+
                         | Event Gateway |
                         +-------+-------+
                                 |
                                 ↓
                              Kafka
                                 |
                  +--------------+--------------+
                  |              |              |
                  ↓              ↓              ↓
              High Priority   Normal          Bulk
                  |              |              |
                  +--------------+--------------+
                                 |
                                 ↓
                    +------------------------+
                    | Notification Service   |
                    +-----------+------------+
                                |
              +-----------------+----------------+
              |                 |                |
              ↓                 ↓                ↓
           Redis             Template DB      User Pref DB
              |
              ↓
        Channel Topics
       /      |       \
      ↓       ↓        ↓
    Push     SMS      Email
   Worker   Worker   Worker
      |        |        |
      ↓        ↓        ↓
 Provider   Provider  Provider
      |        |        |
      +--------+--------+
               |
               ↓
          Delivery Status
               |
               ↓
        Notification DB
```

**Failure path:**

```text
Failures
   ↓
Retry
   ↓
DLQ
```

---

## Interview Follow-ups You Should Prepare

For this problem, an interviewer can go much deeper.

### Follow-up 1 — Why Kafka instead of RabbitMQ/SQS?

| System | Strength |
| --- | --- |
| Kafka | High throughput + replay + partitioning |
| SQS | Simpler queue semantics + managed AWS service |
| RabbitMQ | Routing + traditional messaging |

### Follow-up 2 — How do you guarantee no notification is lost?

```text
Producer acknowledgment
+
Durable Kafka
+
Consumer offset management
+
Retry
+
DLQ
+
Persistent notification state
```

### Follow-up 3 — How do you prevent duplicate SMS?

```text
notificationId
+
idempotency
+
provider idempotency key
```

### Follow-up 4 — How would you handle 10M notifications in one minute?

```text
Kafka partitions
+
horizontal workers
+
batching
+
provider rate limits
+
autoscaling
```

### Follow-up 5 — What if Kafka is down?

```text
Producer retry
+
local buffering where appropriate
+
transactional / outbox pattern
```

### Follow-up 6 — What if the notification service crashes after consuming Kafka?

Don't commit Kafka offset until processing reaches the appropriate durable point.

### Follow-up 7 — How do you guarantee OTP is delivered quickly when millions of marketing notifications are running?

```text
Priority topics
+
separate consumer groups
+
dedicated workers
+
provider capacity reservation
```

### Follow-up 8 — How do you handle user preference changes while a notification is already in Kafka?

This is a good real-world question.

We can check preference **as close to sending time as possible**, rather than relying only on the preference captured when the event was created.

---

## What I Would Say in the Interview

A strong 2-minute summary would be:

> I would design the notification system as an asynchronous, event-driven system. Business services publish notification events to Kafka instead of directly calling notification providers. The notification service checks user preferences, resolves the appropriate template, and routes the notification to channel-specific Kafka topics. Separate workers handle SMS, email, push and WhatsApp. Workers communicate with external providers through a provider abstraction, with retries, exponential backoff and provider failover. I would use Redis for frequently accessed preferences and rate limiting, and a database for notification state and templates. Kafka partitions provide scalability and ordering per user, while consumer lag can drive autoscaling. For reliability, I would use durable messaging, idempotency, retries and a DLQ. For very large campaigns, I would separate high-priority transactional notifications from bulk marketing traffic so OTPs and payments are not delayed.

That is the core HLD answer. The interviewer will usually spend the remaining time drilling into Kafka, reliability, idempotency, scale estimation, DB design, retries, provider failures, and exactly-once vs at-least-once delivery.

---

## Deeper Follow-ups

For a Notification System HLD, here are the deeper follow-ups an interviewer can ask. Format: **question → what interviewer expects → precise answer**.

---

### 1. Scale Estimation Follow-ups

**Q: Assume 100M users and 10M notifications/day. What is average QPS?**

```text
10M / 86,400 ≈ 116 notifications/sec
```

But average QPS is not enough.

Assume peak traffic is 10×:

```text
Peak ≈ 1,160 notifications/sec
```

For a festival/campaign, we might see much higher spikes.

> **Interview point:** Design for peak, not average.

**Q: What if we need to send 50M notifications in 10 minutes?**

Required throughput:

```text
50M / 600 ≈ 83,333 notifications/sec
```

Now we'd need:

- Many Kafka partitions
- Large number of consumers
- Horizontal scaling
- Provider rate-limit management
- Batching where supported
- Separate bulk infrastructure

---

### 2. Kafka Follow-ups

**Q: How many Kafka partitions do you need?**

Don't randomly say "100 partitions." Calculate based on throughput.

For example:

```text
Required throughput = 100K msg/sec
One consumer/partition ≈ 1K msg/sec

Partitions ≈ 100
```

Then validate using load testing.

**Q: How do you choose Kafka partition key?**

For transactional notifications:

```text
partitionKey = userId
```

**Why?** Same user's notifications go to the same partition, giving ordering.

**But there's a problem:** a very large customer/user can create a **hot partition**.

For bulk campaigns, we may instead distribute work more evenly rather than strictly partitioning by user.

**Q: Can multiple consumers consume the same Kafka partition?**

**No.** Within a consumer group, one partition is assigned to only one consumer at a time.

Example: 6 partitions, 3 consumers

| Consumer | Partitions |
| --- | --- |
| C1 | P0, P1 |
| C2 | P2, P3 |
| C3 | P4, P5 |

This allows horizontal scaling.

**Q: What happens if consumers are fewer than partitions?**

Some consumers receive multiple partitions.

```text
6 partitions, 2 consumers

C1 → P0 P1 P2
C2 → P3 P4 P5
```

**Q: What happens if consumers are more than partitions?**

Some consumers remain idle.

```text
3 partitions, 5 consumers

C1 → P0
C2 → P1
C3 → P2
C4 → idle
C5 → idle
```

Therefore:

> Maximum useful consumer parallelism for a consumer group is bounded by partition count.

---

### 3. Kafka Offset Follow-ups

**Q: When do you commit Kafka offset?**

This is important. We shouldn't blindly commit immediately after receiving the message.

```text
Consume
   ↓
Process
   ↓
Persist / send
   ↓
Commit offset
```

If we commit before processing:

```text
Consume
 ↓
Commit
 ↓
Worker crashes
```

The message may **never be processed again**.

**Q: What if worker crashes after sending SMS but before committing offset?**

Kafka redelivers the message.

```text
SMS sent
   ↓
Worker crashes
   ↓
Message processed again
```

This is why we need **idempotency**.

---

### 4. Exactly Once Follow-ups

**Q: Can you guarantee exactly-once notification delivery?**

Generally, **not end-to-end** when external providers are involved.

We can provide:

> At-least-once processing + idempotency

Because our DB + Kafka + external SMS provider are separate systems.

There can always be an uncertain state between the external send and our status update.

---

### 5. Outbox Pattern

**Q: What if Order DB transaction succeeds but Kafka publish fails?**

This is a very important question.

**Bad architecture:**

```text
Order DB
   ↓
Commit

Kafka publish
   ↓
FAIL
```

Order exists but notification event is **lost**.

**Solution: Transactional Outbox**

Order transaction writes `orders` and `outbox_events` in the **same DB transaction**.

```text
Transaction
 ├── Order inserted
 └── Outbox event inserted
        ↓
      COMMIT
```

Then an Outbox Publisher reads:

```text
outbox_events
       ↓
     Kafka
```

This gives much stronger reliability.

---

### 6. Duplicate Outbox Events

**Q: What if the Outbox Publisher publishes the same event twice?**

```text
Outbox
 ↓
Kafka publish SUCCESS
 ↓
Publisher crashes before marking event published
```

It may publish again.

Therefore downstream processing must also be **idempotent**.

This is a common distributed-systems pattern:

> At-least-once + Idempotent consumers

---

### 7. Provider Failure

**Q: What happens if SMS provider is down for 30 minutes?**

Don't continuously hammer it. Use a **circuit breaker**.

```text
Provider
  ↓
Failures increase
  ↓
Circuit OPEN
  ↓
Stop sending temporarily

After some time:
HALF OPEN
   ↓
Test request
   ↓
SUCCESS → CLOSED
```

**Q: When should you retry?**

Retry **transient** failures:

- Timeout
- 503
- Network error
- Rate limit

Don't endlessly retry **permanent** failures:

- Invalid phone number
- Invalid email
- User blocked

---

### 8. Retry Storm

**Q: What happens if 1M notifications fail simultaneously and all retry?**

You can create a **retry storm**.

**Solution:**

```text
Exponential backoff
+
Jitter
+
Maximum retry count
+
Circuit breaker
```

Jitter prevents all workers from retrying at exactly the same time.

---

### 9. Dead Letter Queue

**Q: What happens after all retries fail?**

Move the message to `notification.DLQ`.

```text
Monitoring
    ↓
Alert
    ↓
Investigation
    ↓
Replay if appropriate
```

We should preserve enough metadata to understand why it failed.

---

### 10. Provider Rate Limiting

**Q: Provider allows only 10K requests/sec. Kafka has 100K/sec. What do you do?**

Don't send everything immediately. Use a rate limiter:

```text
Kafka
 ↓
Workers
 ↓
Rate Limiter
 ↓
10K/sec
 ↓
Provider
```

Redis can implement distributed rate limiting.

We can also control concurrency at the worker level.

---

### 11. Provider Selection

**Q: How do you decide which SMS provider to use?**

Create a provider routing layer.

```text
Notification Worker
        |
        ↓
 Provider Router
    /        \
   ↓          ↓
Provider A  Provider B
```

Routing can consider:

- Cost
- Success rate
- Latency
- Region
- Current provider health
- Provider quota

---

### 12. Multi-Region

**Q: How would you make the system multi-region?**

```text
                 Global Load Balancer
                         |
              +----------+----------+
              |                     |
              ↓                     ↓
          India Region          US Region
              |                     |
           Kafka                  Kafka
              |                     |
          Workers                Workers
```

Users can be routed to the nearest/appropriate region.

**Benefits:**

- Lower latency
- Disaster recovery
- Regional isolation

---

### 13. Region Failure

**Q: What if the entire India region goes down?**

Traffic can fail over to another region.

But the difficult part is **data replication**.

We need to consider:

- User preferences
- Notification state
- Templates
- Scheduled notifications
- Kafka / event replication

We shouldn't simply say "use another region." We need to define the **consistency model**.

---

### 14. Database Scaling

**Q: Notification DB has 10B records. What do you do?**

We don't keep everything in one huge table forever.

**Options:**

Partition by time:

```text
notifications_2026_01
notifications_2026_02
notifications_2026_03
```

or database-native time partitioning.

**Archive old data:**

```text
Hot DB
   ↓
Recent notifications

Cold storage
   ↓
Old notifications
```

---

### 15. SQL vs NoSQL

**Q: Why SQL?**

Notification state has structured relationships and requires reliable updates.

SQL works well for:

- Notification status
- User preferences
- Templates

**Why NoSQL?**

For very high-volume notification/event data, a distributed NoSQL store may be useful.

The answer shouldn't be "MongoDB is faster."

Instead:

> The choice depends on access patterns, consistency requirements, scale, and operational constraints.

---

### 16. Notification History

**Q: User opens the app and wants to see notification history. How do you support it?**

API:

```text
GET /users/{userId}/notifications
```

Query by `user_id` and `created_at`.

Use **cursor pagination**:

```text
GET /notifications?cursor=abc&limit=20
```

Avoid deep offset pagination (`OFFSET 1000000`) because it becomes expensive at scale.

---

### 17. Read vs Delivery Status

**Q: Is "SENT" the same as "DELIVERED"?**

**No.** Possible states:

```text
CREATED
   ↓
QUEUED
   ↓
PROCESSING
   ↓
SENT
   ↓
DELIVERED
   ↓
READ
```

For SMS/email, provider callbacks/webhooks may give delivery status.

For push, the app may provide read/open events.

---

### 18. Webhook Follow-up

**Q: How do you process provider callbacks?**

```text
SMS Provider
     |
     | webhook
     ↓
Webhook API
     |
     ↓
Kafka
     |
     ↓
Notification Status Consumer
     |
     ↓
DB
```

Again, webhook events can arrive multiple times, so we need **idempotency**.

---

### 19. User Preference Race Condition

**Q: User disables SMS while an SMS is already queued. Should it be sent?**

This is a **business decision**.

- **Marketing:** usually don't send if the latest preference is OFF
- **Critical notifications:** preferences may not apply, depending on product/legal requirements

Architecturally, check preferences **as late as reasonably possible** before sending.

---

### 20. Template Versioning

**Q: Marketing changes the template while old notifications are queued. Which template should they use?**

Store `templateId` and `templateVersion` with the notification event.

Example: `ORDER_SMS_V2`

This prevents an old notification from unexpectedly using a newly changed template.

---

### 21. Template Localization

**Q: How do you support Hindi, English, Spanish etc.?**

Store templates by:

```text
template + channel + locale + version
```

Example:

```text
ORDER_PLACED | SMS | en-IN | V2
ORDER_PLACED | SMS | hi-IN | V2
```

User preference/profile determines locale.

---

### 22. Notification Deduplication

**Q: What if Order Service accidentally generates the same event twice?**

Use an event ID:

```text
eventId = ORDER123_CREATED
```

or a unique event UUID.

Notification service maintains `processed_events` or uses a unique DB constraint.

---

### 23. Priority

**Q: How do you guarantee OTP isn't delayed by marketing campaigns?**

Separate queues:

```text
                 Kafka
                   |
        +----------+----------+
        |                     |
        ↓                     ↓
   Transactional           Marketing
        |                     |
    High priority          Low priority
        |                     |
    Dedicated             Dedicated
    workers               workers
```

This is better than putting everything into one queue.

---

### 24. Poison Message

**Q: What if one malformed message keeps failing?**

If we retry forever:

```text
Message
 ↓
Fail
 ↓
Retry
 ↓
Fail
 ↓
Retry...
```

It can **block processing**.

Use:

```text
Retry topic
 ↓
Limited attempts
 ↓
DLQ
```

---

### 25. Consumer Lag

**Q: How do you know the notification system is falling behind?**

Monitor **Kafka Consumer Lag**.

| Lag | Meaning |
| --- | --- |
| 1K | Healthy |
| 100K | Investigate |
| 10M | Serious backlog |

Autoscaling can use lag as one of its signals.

---

### 26. Cache Failure

**Q: What if Redis goes down?**

Don't make Redis the only source of truth.

```text
Redis
  ↓ miss/failure
Database
```

Redis is a **cache**. For critical preference data, the DB remains authoritative.

---

### 27. Cache Stampede

**Q: What if Redis expires millions of user preferences at once?**

All workers hit DB simultaneously. This is a **cache stampede**.

**Solutions include:**

- TTL jitter
- Cache warming
- Request coalescing
- Rate limiting DB fallback

---

### 28. Security

**Q: How do you prevent someone from sending arbitrary notifications?**

Authentication + authorization.

```text
Service A
   ↓
Authenticated API
   ↓
Authorization
   ↓
Notification Service
```

Also:

- Encrypt sensitive data
- Don't log OTPs
- Don't expose provider credentials
- Validate templates/input
- Audit notification requests

---

### 29. PII

**Q: Should Kafka contain phone numbers and email addresses?**

Prefer minimizing sensitive data.

Instead of:

```json
{
  "phone": "+91..."
}
```

send:

```json
{
  "userId": "123"
}
```

Worker can fetch required information from an appropriate user/profile service.

However, this introduces another dependency, so we need to balance **privacy vs latency/reliability**.

---

### 30. Scheduled Notification at Huge Scale

**Q: 100M users schedule notifications for exactly 9 AM. What happens?**

This creates a massive spike.

Don't have every notification independently trigger a timer.

Use:

```text
Scheduled DB
     ↓
Time buckets
     ↓
Scheduler
     ↓
Kafka
     ↓
Workers
```

And spread work where business requirements allow.

---

### 31. What If Kafka Is Full?

Kafka isn't generally treated like a fixed-size queue that you "fill up" in the same way as an in-memory queue, but brokers have finite disk and operational limits.

We should monitor:

- Disk
- Retention
- Producer throughput
- Consumer lag
- Broker health

If consumers fall behind significantly, scale consumers and investigate the bottleneck.

---

### 32. How Would You Handle a Black Friday Event?

This is a very strong system-design follow-up.

I'd say:

```text
                    Events
                       ↓
                     Kafka
                       ↓
          +------------+------------+
          |                         |
          ↓                         ↓
   Transactional              Marketing
   notification               notification
          |                         |
    High priority              Bulk workers
          |                         |
          +------------+------------+
                       ↓
                 Provider Router
                       ↓
               Multiple Providers
```

**Before the event:**

- Capacity planning
- Load testing
- Increase Kafka partitions
- Pre-scale workers
- Validate provider quotas
- Cache templates/preferences
- Set alerts
- Test provider failover

---

### 33. Disaster Recovery

**Q: What happens if DB is unavailable?**

For critical notification state:

- DB replication
- Automated failover
- Backups
- Point-in-time recovery

Kafka retains events so processing can resume after recovery.

---

### 34. Most Important Follow-up

Interviewer:

> Give me the biggest problems in your design.

A strong answer:

| # | Problem | Solution |
| --- | --- | --- |
| 1 | Duplicate notifications | Idempotency |
| 2 | Provider failure | Retry + circuit breaker + failover |
| 3 | Traffic spikes | Kafka + horizontal scaling |
| 4 | Notification loss | Outbox + durable Kafka |
| 5 | Retry storm | Exponential backoff + jitter |
| 6 | OTP starvation | Priority queues + dedicated workers |
| 7 | Huge campaigns | Bulk pipeline + partitioning |
| 8 | Provider rate limits | Distributed rate limiter |
| 9 | Kafka consumer failures | Offset management + idempotent consumers |
| 10 | Multi-region failure | Replication + failover |

---

## The 10 Follow-ups I Would Expect in an SDE-2 Interview

If you have limited preparation time, master these first:

| # | Follow-up | Importance |
| --- | --- | --- |
| 1 | How do you prevent duplicate notifications? | ⭐⭐⭐⭐⭐ |
| 2 | Kafka offset + consumer crash? | ⭐⭐⭐⭐⭐ |
| 3 | Exactly-once vs at-least-once? | ⭐⭐⭐⭐⭐ |
| 4 | How do you handle provider failure? | ⭐⭐⭐⭐⭐ |
| 5 | How do you handle 50M notifications quickly? | ⭐⭐⭐⭐⭐ |
| 6 | Why Kafka? | ⭐⭐⭐⭐ |
| 7 | How do you handle provider rate limits? | ⭐⭐⭐⭐ |
| 8 | Why Outbox Pattern? | ⭐⭐⭐⭐ |
| 9 | How do you prioritize OTP over marketing? | ⭐⭐⭐⭐ |
| 10 | How do you scale to multiple regions? | ⭐⭐⭐⭐ |
