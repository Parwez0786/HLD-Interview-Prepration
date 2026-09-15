# Kafka Mandate Reconciliation — Paytm Money

## Resume bullet

> Developed a Kafka-driven asynchronous system for bulk mandate failures, resolving incorrect SIP linkage for 100K+ dormant mandates and ensuring data consistency across PG and internal databases.

Prepare this as a **bulk mandate reconciliation pipeline**. Keep actual facts separate from reasonable implementation assumptions so you don’t accidentally claim something you didn’t do.

In the interview, consistently frame it as **mandate reconciliation / state correction**, unless “bulk mandate failures” has a specific meaning in your actual implementation.

---

## Table of Contents

- [1. First Understand the Business Problem](#1-first-understand-the-business-problem)
- [2. Why Did Product Give You a CSV?](#2-why-did-product-give-you-a-csv)
- [3. Architecture](#3-architecture-you-should-explain-in-interview)
- [4. Kafka Configuration](#4-your-kafka-configuration)
- [5. Why Use mandateId as Kafka Key?](#5-why-use-mandateid-as-kafka-key)
- [6. Why Not a Random Key?](#6-why-not-random-key)
- [7. 5 Consumers and Only 3 Partitions](#7-but-you-had-5-consumers-and-only-3-partitions)
- [8. Why 3 Partitions?](#8-why-3-partitions)
- [9. Why Kafka at All?](#9-why-kafka-at-all)
- [10. Why Not a Simple Cron Job?](#10-why-not-a-simple-cron-job)
- [11. Why Batch 10K Mandates?](#11-why-batch-10k-mandates)
- [12. Batching Tradeoff](#12-but-batching-introduces-an-important-tradeoff)
- [13. Biggest Weakness: Batch-Level Retry](#13-the-biggest-weakness-in-your-design-batch-level-retry)
- [14. How Should Idempotency Work?](#14-how-should-idempotency-work)
- [15–26. Failure, MySQL, Retry, Consumer Crash](#15-what-if-mandate-doesnt-exist)
- [27–46. Delivery, Monitoring, Improvements](#27-what-if-consumer-crashes)
- [47. 60-Second Interview Answer](#47-your-60-second-interview-answer)
- [48. 2-Minute Deeper Answer](#48-your-2-minute-deeper-answer)
- [49. Top 20 Cross-Questions](#49-top-20-cross-questions-you-must-prepare)
- [50. Fix This Before the Interview](#50-one-thing-i-strongly-recommend-you-fix-before-the-interview)
- [Interview-Ready Project Explanation](#interview-ready-project-explanation)
- [50+ Questions You Should Prepare](#the-50-questions-you-should-prepare)
- [Whiteboard Diagram](#the-interviewer-may-ask-you-to-draw-it)
- [90-Second Answer](#your-90-second-answer--memorize-this)
- [Rapid-Fire Answers](#rapid-fire-questions-you-should-be-able-to-answer)
- [Five Questions Where You Should NEVER Bluff](#five-questions-where-you-should-never-bluff)

---

## 1. First Understand the Business Problem

### What is a mandate?

A mandate is an authorization that allows a financial institution / payment system to automatically debit a customer's bank account according to some instruction.

For a SIP, the high-level relationship is roughly:

```text
User
  |
  v
SIP
  |
  v
Mandate
  |
  v
Bank / Payment Gateway
```

**Your product rule:** One active mandate per bank account/user was allowed.

**The problem:** The Payment Gateway / BSE side had some mandates that had become stale/dormant, but our internal database still considered them **ACTIVE**.

So the two systems had inconsistent states.

### Example

| Side | Mandate M123 |
| --- | --- |
| Bank account | HDFC |
| Internal DB | ACTIVE |
| BSE / PG | DORMANT |

The user tries to create another mandate:

```text
Create new mandate
       |
       v
Check existing mandate
       |
       v
M123 = ACTIVE
       |
       v
Reject new mandate
```

But externally: `M123 = DORMANT`.

Therefore the user is effectively **stuck**.

This is the core problem your pipeline solved.

---

## 2. Why Did Product Give You a CSV?

The product/operations team had identified approximately **100K dormant mandates** externally and provided a CSV containing mandate IDs that needed reconciliation.

```text
CSV
 |
 | ~100K mandate IDs
 v
Validation
 |
 v
Batching
 |
 v
Kafka
 |
 v
Mandate Service
 |
 v
MySQL
 |
 v
ACTIVE → INACTIVE
```

This was essentially a **one-time bulk reconciliation / backfill job**.

---

## 3. Architecture You Should Explain in Interview

Based on your implementation details:

```text
                 ┌─────────────────────┐
                 │     CSV File        │
                 │ ~100K Mandate IDs   │
                 └──────────┬──────────┘
                            │
                            ▼
                 ┌─────────────────────┐
                 │ CSV Validation      │
                 │                     │
                 │ Empty rows          │
                 │ Invalid columns     │
                 │ Empty mandate ID    │
                 │ Duplicate IDs       │
                 └──────────┬──────────┘
                            │
                            ▼
                 ┌─────────────────────┐
                 │ Batch Processor     │
                 │                     │
                 │ 100K IDs            │
                 │       ↓             │
                 │ 10 batches          │
                 │ 10K IDs / batch     │
                 └──────────┬──────────┘
                            │
                            ▼
                ┌──────────────────────┐
                │       Kafka           │
                │                       │
                │   Topic: Mandate      │
                │   Partitions: 3       │
                │   Brokers: 3          │
                └──────────┬────────────┘
                           │
                           ▼
              ┌─────────────────────────┐
              │ Mandate Service         │
              │ Consumer Group           │
              │                         │
              │ 5 Consumers             │
              └───────────┬─────────────┘
                          │
                          ▼
                ┌──────────────────────┐
                │       MySQL           │
                │                       │
                │ ACTIVE → INACTIVE     │
                └──────────────────────┘
```

There is an important Kafka detail here that interviewers may challenge you on.

---

## 4. Your Kafka Configuration

You said:

| Setting | Value |
| --- | --- |
| Topics | 1 |
| Partitions | 3 |
| Brokers | 3 |
| Consumer groups | 1 |
| Consumers | 5 |
| Key | `mandateId` |
| Strategy | hashing |

**Topic:** We created a dedicated Kafka topic for mandate reconciliation events, e.g. `mandate-reconciliation`.

Each message represents a mandate reconciliation request.

---

## 5. Why Use mandateId as Kafka Key?

This is a very important interview question.

You used `key = mandateId`. Kafka uses the key to determine the partition.

Conceptually:

```text
partition = hash(mandateId) % numberOfPartitions
```

For example:

```text
M123 → hash → partition 0
M456 → hash → partition 2
M789 → hash → partition 1
```

**Why is this useful?** The same mandate ID consistently goes to the same partition. That gives us **ordering** for events belonging to the same mandate.

Suppose `M123 → ACTIVE` then `M123 → INACTIVE`. If these events are sent to the same partition, Kafka maintains their ordering within that partition.

**Interview answer:**

> I used mandateId as the Kafka key so that records for the same mandate consistently map to the same partition. This preserves ordering for a given mandate while still allowing different mandates to be processed in parallel.

---

## 6. Why Not Random Key?

Suppose you don't use a key:

```text
M123 → P0
M123 → P2
M123 → P1
```

Now events for the same mandate could land in different partitions.

Kafka only guarantees ordering **within a partition**, not across partitions.

Therefore:

```text
mandateId as key
        ↓
same mandate
        ↓
same partition
        ↓
ordered processing
```

This is a strong answer in an interview.

---

## 7. But You Had 5 Consumers and Only 3 Partitions

This is a very likely interviewer cross-question.

With one consumer group: **3 partitions, 5 consumers**.

At most **3 consumers** can actively consume partitions simultaneously. Two consumers will remain idle.

```text
Consumer 1 → Partition 0
Consumer 2 → Partition 1
Consumer 3 → Partition 2
Consumer 4 → idle
Consumer 5 → idle
```

**Interviewer:** Why did you have 5 consumers when you only had 3 partitions?

**Good answer:**

> Since Kafka assigns a partition to only one consumer within a consumer group, with 3 partitions the maximum parallelism from that topic was 3 consumers. Having 5 consumers meant 2 could remain idle. If I were redesigning it for higher throughput, I would align the partition count with the desired consumer parallelism, for example 5 or more partitions.

Do **not** say all 5 consumers were processing simultaneously. That would be technically incorrect.

---

## 8. Why 3 Partitions?

You should explain this as a throughput/operational tradeoff.

You had: 100K mandates, 10 batches, 10K mandates/batch. The processing was relatively controlled. You didn't need an extremely high partition count.

| More partitions — Pros | More partitions — Cons |
| --- | --- |
| More parallelism | More Kafka metadata |
| Higher throughput | More coordination / rebalancing |
| More consumers can work simultaneously | More operational complexity |
| | Potentially more uneven partition distribution |

Therefore:

> Since this was a controlled bulk reconciliation workload rather than a high-throughput real-time pipeline, 3 partitions were sufficient for the workload at that time.

---

## 9. Why Kafka at All?

This is probably the **#1 interviewer question**.

They might ask: Why didn't you simply read the CSV and update MySQL directly?

### Direct approach

```text
CSV
 ↓
Loop 100K records
 ↓
MySQL
 ↓
Update
```

**Potential problems:**

- Long-running process
- Database gets hit continuously
- Failure can interrupt the whole process
- Difficult to control processing rate
- Retry handling becomes more complicated
- No durable queue between producer and consumer

### Kafka approach

```text
Producer
   ↓
Kafka
   ↓
Consumer
   ↓
MySQL
```

Now producer and consumer are **decoupled**.

If the consumer crashes:

```text
Kafka
 ↓
messages remain
 ↓
consumer restarts
 ↓
continue processing
```

That is a major advantage.

---

## 10. Why Not a Simple Cron Job?

You could have: Cron → Read CSV → Update DB.

But Kafka provides:

| Benefit | Meaning |
| --- | --- |
| **Decoupling** | Producer doesn't need to wait for database processing |
| **Buffering** | Kafka acts as a durable buffer |
| **Retry / reprocessing** | Failed messages can be processed again depending on consumer/error-handling design |
| **Horizontal scalability** | Increase partitions + consumers when workload grows |

---

## 11. Why Batch 10K Mandates?

You said: 100K mandates → 10 records → 10K mandate IDs per record.

The producer generated approximately:

```text
Batch 1 → 10,000 IDs
Batch 2 → 10,000 IDs
...
Batch 10 → 10,000 IDs
```

This reduces the number of Kafka records dramatically:

| Without batching | With batching |
| --- | --- |
| 100K IDs → 100K Kafka messages | 100K IDs → 10 Kafka messages |

That's a significant reduction in Kafka message overhead.

---

## 12. But Batching Introduces an Important Tradeoff

The interviewer may say: Why didn't you send one mandate per Kafka message?

### Individual message

```json
{
  "mandateId": "M123"
}
```

| Advantages | Disadvantages |
| --- | --- |
| Fine-grained retry | 100K Kafka messages |
| One failure doesn't affect other mandates | Higher producer overhead |
| Better parallelism | More Kafka metadata/processing |
| Easier tracking | More network calls |

### Batch message

```json
{
  "mandateIds": ["M1", "M2", "...", "M10000"]
}
```

| Advantages | Disadvantages |
| --- | --- |
| Fewer Kafka messages | Larger message |
| Better network efficiency | Retry granularity is worse |
| Lower producer overhead | One bad record can complicate batch processing |
| | More memory required |
| | Potentially longer processing time |

---

## 13. The Biggest Weakness in Your Design: Batch-Level Retry

This is something you should understand before the interview.

Suppose Batch 7 has 10,000 mandates. Consumer processes M1–M7000 successfully, then M7001 fails.

If you retry the entire Kafka message, you could process M1–M7000 **again**.

Therefore you need **idempotency**.

---

## 14. How Should Idempotency Work?

Suppose your update is:

```sql
UPDATE mandate
SET status = 'INACTIVE'
WHERE mandate_id = ?;
```

If you execute it twice:

```text
ACTIVE → INACTIVE
INACTIVE → INACTIVE
```

The second operation doesn't create an incorrect state.

That's why this particular reconciliation operation is naturally close to **idempotent**.

You can also check:

```sql
UPDATE mandate
SET status = 'INACTIVE'
WHERE mandate_id = ?
AND status != 'INACTIVE';
```

Then already-inactive mandates don't need another state transition.

---

## 15. What If Mandate Doesn't Exist?

Suppose Kafka contains `M999999` but MySQL doesn't contain it.

You shouldn't blindly consider the operation successful.

You could classify: `NOT_FOUND` and log it for reconciliation.

For example: `SUCCESS`, `FAILED`, `NOT_FOUND`, `ALREADY_INACTIVE`.

This becomes important for operational visibility.

---

## 16. What Does "Ensuring Data Consistency Across PG and Internal Databases" Mean?

The conceptual issue was:

| Side | State |
| --- | --- |
| External / PG | Mandate = DORMANT |
| Internal system | Mandate = ACTIVE |

Your reconciliation pipeline corrected the internal state:

```text
External: DORMANT

Internal: ACTIVE → INACTIVE
```

So future mandate creation logic sees the **correct state**.

---

## 17. Important Question: Did Your Service Call PG?

Based on the information you've given, **don't claim** that it called PG during the Kafka processing unless you actually remember doing that.

**A safe explanation:**

> The dormant mandate IDs were identified from the external/payment-side data and provided to us through the CSV. Our reconciliation pipeline used those IDs to correct the corresponding internal mandate state.

That is safer than saying "The consumer called BSE for every mandate," because you haven't established that.

---

## 18. CSV Validation

You mentioned four validations.

### 1. Empty row

Empty rows should be rejected/skipped.

### 2. Invalid columns

Expected: `mandateId`. If CSV contains extra unexpected columns, you validate the schema.

### 3. Duplicate mandate IDs

You don't want `M123` processed twice unnecessarily. You can use a `Set<String> mandateIds` during validation.

### 4. Empty mandate ID

Empty IDs are rejected.

---

## 19. Validation Architecture

```text
CSV
 ↓
Parser
 ↓
Schema Validation
 ↓
Data Validation
 ↓
Duplicate Detection
 ↓
Valid records
 ↓
Batching
 ↓
Kafka
```

Invalid records should ideally be separated:

```text
invalid records
      ↓
error report/log
```

This is better operationally than silently ignoring them.

---

## 20. Producer Flow

```text
CSV Reader
    |
    v
Validate records
    |
    v
Collect 10,000 mandate IDs
    |
    v
Create Kafka message
    |
    v
Key = mandateId
    |
    v
Send to Kafka
```

### One subtle point

You said batch record contains 10K IDs but also `key = mandateId`.

You should be prepared for this question: **If one Kafka message contains 10K mandate IDs, what is the key?**

You need to be precise about what you actually implemented.

If the entire 10K list was one Kafka message, there is only **one Kafka record key** for that record. You cannot simultaneously have 10,000 independent Kafka keys for one Kafka record.

If your implementation really was:

```text
Kafka record:
key = something
value = [10K mandate IDs]
```

then **don't say:** "Each mandate ID was individually partitioned using its own key."

Instead say:

> The reconciliation data was batched into Kafka records. The keying strategy was based on mandate ID where applicable; the exact key granularity depended on the event structure.

**Please verify this point before your interview**, because an experienced Kafka interviewer may catch the inconsistency.

---

## 21. Consumer Flow

```text
Kafka
 ↓
Consumer
 ↓
Deserialize message
 ↓
Extract 10K mandate IDs
 ↓
Process IDs
 ↓
Update MySQL
 ↓
Success
```

Potentially:

```java
for (String mandateId : mandateIds) {
    updateMandateStatus(mandateId, INACTIVE);
}
```

But an interviewer will immediately ask: **Did you execute 10K individual UPDATE queries?**

If yes, that is inefficient. A better design is **bulk update**.

```sql
UPDATE mandate
SET status = 'INACTIVE'
WHERE mandate_id IN (...);
```

or JDBC batch updates.

---

## 22. Better Database Approach

Because you have 10K IDs, don't necessarily perform 10,000 network round trips.

Instead:

```text
10K IDs
 ↓
DB batch operation
 ↓
MySQL
```

You could process in smaller chunks:

```text
10,000 Kafka IDs
      ↓
1000 IDs
1000 IDs
1000 IDs
...
      ↓
MySQL
```

This protects MySQL from excessively large queries.

---

## 23. Why MySQL?

Your existing mandate data was already stored in MySQL.

So moving it to another database would add unnecessary complexity.

**The simplest answer:**

> MySQL was already the source of truth for the internal mandate state, so reconciliation directly against MySQL avoided introducing another persistence layer.

---

## 24. Transaction Handling

Suppose a batch contains 10,000 mandates. Should all 10K updates be one transaction?

**Probably not.** A huge transaction can cause: long locks, large transaction log, rollback cost, memory/resource pressure.

A better approach is smaller transactional units:

```text
Kafka batch
   |
   +-- DB chunk 1 → transaction
   +-- DB chunk 2 → transaction
   +-- DB chunk 3 → transaction
```

Don't claim you implemented this if you didn't. Present it as how you would **improve** the design.

---

## 25. Retry Strategy

You wrote: Retry → 7–8 minutes per batch.

You should distinguish **processing time** from **retry mechanism**.

**A strong answer:**

> The batch processing time was around 7–8 minutes. For failures, the consumer-side error handling allowed failed processing to be identified and retried according to the implementation. We also monitored failures through logs.

If you actually had Kafka retry topics/DLT, say so. If you didn't, **don't invent them**.

---

## 26. What If MySQL Is Down?

This is a classic cross-question.

You should **not** acknowledge the Kafka message as successfully processed.

Otherwise: Kafka message → ack → DB update never happened. Data is lost from the processing perspective.

Instead:

```text
MySQL failure
 ↓
consumer processing fails
 ↓
message remains/retry
 ↓
MySQL recovers
 ↓
process again
```

The exact mechanism depends on the consumer framework/configuration you used.

---

## 27. What If Consumer Crashes?

This is one of Kafka's major advantages.

```text
Consumer processing
       ↓
Pod crashes
```

Kafka tracks consumer offsets. After restart/rebalance, the consumer resumes from the last committed offset.

But this creates the classic issue: **at-least-once processing**. A message may be processed more than once.

Therefore: **Kafka + idempotent DB update** is important.

---

## 28. What Delivery Guarantee Would You Claim?

Unless you specifically configured Kafka exactly-once semantics, **don't claim exactly-once**.

**A safe answer:**

> I would describe the pipeline as at-least-once processing with an idempotent database update. If a message is replayed, setting a mandate to INACTIVE again doesn't create an inconsistent state.

That's technically strong.

---

## 29. What If the Same Mandate Appears Twice?

During CSV validation, duplicate detected — ideally remove it.

But even if duplicate events somehow reach Kafka:

```text
M123 → INACTIVE
M123 → INACTIVE
```

the second operation remains safe if the update is idempotent.

---

## 30. What If Only 7,000 of 10,000 Records Succeed?

This is an important design question.

You should have **per-record processing status**.

```text
Batch 5
10,000 records

Success → 9,993
Failed  → 7
```

Then successful records → completed; failed records → retry/error handling.

This is much better than blindly retrying all 10K.

---

## 31. Monitoring

You said: Monitoring → using logs, marking status as failed.

For every batch:

| Field | Example |
| --- | --- |
| `batchId` | 7 |
| `totalRecords` | 10,000 |
| `successCount` | 9,998 |
| `failureCount` | 2 |
| `startTime` / `endTime` | — |
| `duration` | 7m 32s |

This lets you immediately identify problematic batches.

---

## 32. Why Didn't You Have a Production Issue?

**Your answer can be:**

> We controlled the rollout and validated the input before publishing. The workload was finite, approximately 100K mandates, and we monitored the processing through logs and failed-status tracking. We did not encounter a production incident during the execution.

Don't say: "Kafka guarantees there won't be production issues." It doesn't.

---

## 33. Why Asynchronous Architecture?

Without Kafka: CSV → Mandate service → MySQL. The producer/process is tightly coupled to database processing.

With Kafka:

```text
CSV Processor
     |
     v
   Kafka
     |
     v
Mandate Service
     |
     v
   MySQL
```

Now:

- Producer doesn't wait for consumer
- Consumer controls processing rate
- Failure isolation — consumer can fail without losing the persisted Kafka record

---

## 34. What If Kafka Itself Goes Down?

Producer cannot publish.

```text
CSV remains available
       ↓
retry publishing
       ↓
Kafka recovers
       ↓
continue
```

**The key idea:** Don't mark the CSV batch as successfully published until Kafka acknowledges the record.

---

## 35. Why One Topic?

That's reasonable because this pipeline represents one business event type: `MANDATE_RECONCILIATION`.

Having multiple topics unnecessarily could complicate operations, monitoring, consumer management, and deployment.

One dedicated topic is simple.

---

## 36. Why One Consumer Group?

Because there was one logical processing responsibility: **mandate reconciliation**.

Every partition is consumed by only one consumer in that group.

If you created two consumer groups, both groups would receive the records independently. That wasn't required for your use case.

---

## 37. What If We Need Another Service to Consume the Event?

Then you could create another consumer group.

```text
                  Kafka
                    |
          ┌─────────┴─────────┐
          ↓                   ↓
 Mandate Consumer       Audit Consumer
 Group A                Group B
```

Both can independently consume the same topic.

---

## 38. What Happens During Consumer Rebalance?

Suppose C1 → P0, C2 → P1, C3 → P2. C2 crashes.

Kafka detects the failure and rebalances: C1 → P0, C3 → P1/P2.

Another consumer takes ownership of the partition. This provides **fault tolerance**.

---

## 39. Biggest Tradeoffs in Your Design

Know these extremely well.

| Decision | Benefit | Tradeoff |
| --- | --- | --- |
| Kafka | Decoupling, durability | Operational complexity |
| Async processing | Doesn't block producer | Eventual consistency |
| 10K batching | Fewer Kafka messages | Coarser retry |
| 3 partitions | Simple, sufficient | Limited parallelism |
| 5 consumers | Fault tolerance / capacity | 2 idle with 3 partitions |
| `mandateId` key | Same-key ordering | Potential hot partition |
| MySQL | Existing source of truth | DB can become bottleneck |
| At-least-once | Reliable processing | Duplicate processing possible |
| Idempotent update | Safe retries | Requires careful DB design |
| CSV input | Simple operational workflow | Manual dependency |

---

## 40. Hot Partition Problem

**Interviewer:** What if one mandate ID appears many times?

Since `hash(mandateId)` maps the same key to the same partition, one extremely popular key can cause a hot partition.

However, in your reconciliation use case, each mandate was essentially processed **once**, so this wasn't expected to be a major problem.

---

## 41. What If We Increase Partitions From 3 → 10?

You get potentially more parallelism: 10 partitions, 10 consumers could allow 10 consumers processing instead of 3.

But increasing partitions isn't automatically better. You need to consider: DB capacity, consumer capacity, Kafka throughput, ordering requirements, operational complexity.

If MySQL can only handle 3 concurrent workers effectively, 10 consumers could actually make things **worse**.

---

## 42. The Database Is Probably Your Bottleneck

This is a great system-design observation.

Increasing Kafka consumers (3 → 10 → 20) doesn't necessarily increase overall throughput.

If MySQL CPU is 90%, then more consumers simply increase database pressure.

So the real bottleneck might be:

```text
Kafka → Consumer → MySQL
                   ↑
                bottleneck
```

---

## 43. How Would You Improve the System?

If interviewer asks "How would you redesign this today?"

```text
CSV
 ↓
Validation
 ↓
Deduplication
 ↓
Batch Producer
 ↓
Kafka
 ↓
Mandate Consumers
 ↓
Small DB batches
 ↓
MySQL
```

Add:

```text
                ┌→ Success metrics
                │
Kafka → Consumer ├→ Failed records
                │
                └→ DLQ / retry mechanism
```

And monitoring: Prometheus/Grafana + centralized logs, if available in the company's stack.

---

## 44. Better Retry Architecture

```text
Kafka Main Topic
       |
       v
Consumer
       |
       ├── Success → DB
       |
       └── Failure
              |
              v
        Retry mechanism
              |
        ┌─────┴─────┐
        ↓           ↓
     Retry        DLQ
```

For example: retry 1 → after 30 sec, retry 2 → after 2 min, retry 3 → after 10 min. After maximum retries → DLQ.

Then operations can investigate.

Present this as an **improvement** unless you actually implemented it.

---

## 45. Why Not Update All 100K Records With One SQL Query?

Excellent question.

If the IDs are known, `UPDATE ... WHERE mandate_id IN (...)` could potentially be much simpler.

**Your answer:**

> For a one-time 100K reconciliation, a direct database script could indeed be simpler. We chose the Kafka-driven approach because we wanted the processing to be asynchronous, durable, controllable, and handled through the existing Mandate Service rather than bypassing application-level business logic.

This is a very mature answer.

And acknowledge the tradeoff:

> If this were purely a controlled database backfill with no application-level processing requirement, a database-side batch job might be more efficient.

---

## 46. Why Not Directly Modify the DB?

Because directly modifying production DB can bypass: business rules, validation, application logic, auditing, existing service behavior.

Using the mandate service means:

```text
Business logic
      ↓
Mandate Service
      ↓
MySQL
```

rather than Script → Production DB.

This is safer when state transitions have application-level rules.

---

## 47. Your 60-Second Interview Answer

Memorize this version:

> At Paytm Money, we had a mandate reconciliation problem. The product rule allowed one active mandate per bank, but some mandates had become stale or dormant on the external payment/BSE side while our internal system still marked them as active. Because of that state mismatch, users could be prevented from creating a new mandate, which could eventually lead to payment/SIP failures.
>
> The product team provided us with a CSV containing around 100K dormant mandate IDs that needed to be reconciled.
>
> I owned the implementation and, after discussing the approach with my tech lead, built a Kafka-driven asynchronous pipeline. First, we validated the CSV for empty rows, invalid columns, empty mandate IDs and duplicate mandate IDs. We then divided the 100K IDs into 10 batches of around 10K IDs and published those batches to a dedicated Kafka topic.
>
> The Kafka setup had 3 brokers, 3 partitions and one Mandate Service consumer group. The consumer processed the reconciliation request and updated the corresponding mandate state in MySQL from ACTIVE to INACTIVE. We used mandateId as the Kafka key where the event granularity allowed it, so records for the same mandate could maintain partition affinity and ordering.
>
> The processing time was around 7–8 minutes per batch. We monitored the execution using logs and explicitly tracked failed processing. The pipeline completed without a production incident and corrected the stale mandate state for 100K+ records, bringing our internal mandate state in line with the external payment-side information.

---

## 48. Your 2-Minute Deeper Answer

If interviewer says "Can you explain the architecture in more detail?"

> The architecture had four major stages: ingestion, validation and batching, Kafka-based asynchronous processing, and database reconciliation.
>
> The first stage was CSV ingestion. Since the input came from the product/operations team, we didn't blindly process it. We validated the file for schema-related issues such as invalid columns, empty rows and empty mandate IDs. We also checked for duplicate mandate IDs so that the same mandate wouldn't unnecessarily be processed multiple times.
>
> After validation, approximately 100K mandate IDs were divided into 10 batches, with around 10K IDs per batch. These batches were published to a dedicated Kafka topic. The reason for introducing Kafka was to decouple the bulk ingestion process from mandate processing. Instead of the CSV processor directly executing 100K database operations, Kafka acted as a durable buffer between the producer and the Mandate Service.
>
> The Kafka cluster had 3 brokers and the topic had 3 partitions. We used a single consumer group belonging to the Mandate Service. There were 5 consumer instances configured, although with only 3 partitions, at most 3 consumers could actively consume partitions from that topic at a time.
>
> On the consumer side, the service consumed the reconciliation messages, extracted the mandate IDs and updated the corresponding mandate state in MySQL. The objective was to change mandates identified as dormant externally from ACTIVE to INACTIVE internally.
>
> An important property of this operation was idempotency. If the same mandate was processed again because of a retry or duplicate message, setting its state to INACTIVE again would not create an incorrect state. This is important because Kafka-based systems are commonly designed around at-least-once processing rather than assuming that every message will be processed exactly once.
>
> Each batch took roughly 7–8 minutes, and we monitored processing through logs and failure status tracking. The complete reconciliation ran without a production incident.

---

## 49. Top 20 Cross-Questions You MUST Prepare

### Kafka

| Q | Answer |
| --- | --- |
| **Q1. Why Kafka?** | Decoupling, durability, asynchronous processing, retry/reprocessing |
| **Q2. Why one topic?** | One business event type |
| **Q3. Why one consumer group?** | One logical processing application |
| **Q4. Why 3 partitions?** | Sufficient parallelism for controlled workload |
| **Q5. Why 5 consumers with 3 partitions?** | Maximum 3 active consumers; 2 remain idle |
| **Q6. Why mandateId as key?** | Partition affinity and ordering for same mandate |
| **Q7. What if consumer crashes?** | Kafka offset/reprocessing mechanism allows recovery |
| **Q8. What if Kafka goes down?** | Producer retry; don't mark batch successfully published until acknowledgement |
| **Q9. At-most-once or at-least-once?** | Prefer at-least-once + idempotency unless exactly-once was explicitly configured |
| **Q10. What happens during rebalance?** | Partitions are reassigned among consumers in the group |

### Database

| Q | Answer |
| --- | --- |
| **Q11. Why MySQL?** | Existing internal mandate store / source of truth |
| **Q12. How do you avoid 10K DB calls?** | JDBC batching / smaller bulk UPDATE chunks |
| **Q13. What if MySQL is down?** | Don't acknowledge successful processing; retry |
| **Q14. What if mandate doesn't exist?** | Mark/report as NOT_FOUND rather than silently treating it as success |
| **Q15. How do you make update idempotent?** | ACTIVE → INACTIVE operation can safely be repeated |

### Batch

| Q | Answer |
| --- | --- |
| **Q16. Why 10K per batch?** | Reduces Kafka records and network overhead while keeping message size manageable |
| **Q17. Why not one mandate per Kafka message?** | Better retry granularity but 100K messages and more overhead |
| **Q18. Why not one message containing all 100K?** | Very large message, poor retry granularity, memory/message-size concerns |

### System design

| Q | Answer |
| --- | --- |
| **Q19. Why not simply use a SQL script?** | Could be simpler for a pure DB backfill; Kafka was useful because processing went through the service asynchronously and was decoupled/durable |
| **Q20. How would you improve it?** | Per-record failure tracking, retry topic, DLQ, metrics/dashboard, controlled DB batching, and better partition/consumer alignment |

---

## 50. One Thing I Strongly Recommend You Fix Before the Interview

There is a potential inconsistency in the description:

```text
10K mandate IDs in one Kafka record
+
key = mandateId + hashing strategy
```

A Kafka record has **one key**, not 10K independent keys.

So be absolutely clear about what your actual implementation did.

If it was:

```text
Kafka record 1
key = some batch key
value = [10K mandate IDs]
```

say that.

If it was:

```text
Kafka record
key = mandateId
value = one mandate
```

then you actually had roughly **100K Kafka records**, not 10.

Don't let an interviewer discover this inconsistency before you clarify your own story.

---

# Interview-Ready Project Explanation

**Resume point (reframed):**

> Developed a Kafka-driven asynchronous system for bulk mandate reconciliation, resolving incorrect SIP linkage for 100K+ dormant mandates and ensuring consistency between external payment-side data and the internal mandate database.

### 1. Business problem

At Paytm Money, we had a mandate state reconciliation problem.

A mandate is an authorization that allows automatic debit from a customer's bank account. SIPs can use these mandates to execute recurring payments.

The product rule was that a user could have only **one active mandate** for a particular bank/account.

The problem was that some mandates had become stale or dormant on the external payment/BSE side, while our internal system still considered those mandates ACTIVE.

When the user tried to create a new mandate, our system could see M123 as an active mandate and reject the new mandate. This could prevent the user from successfully creating a new mandate and eventually cause SIP/payment failures.

The product/operations team identified around 100K such mandates and provided the mandate IDs through a CSV file.

### 2. Solution

```text
CSV
↓
Validation
↓
Deduplication
↓
Batching
↓
Kafka
↓
Mandate Service Consumer
↓
MySQL
↓
ACTIVE → INACTIVE
```

The main reason for using Kafka was to **decouple** the bulk ingestion process from mandate-state processing.

### 3. CSV validation

Checking expected CSV structure/columns, rejecting empty rows and empty mandate IDs, detecting duplicates, validating mandate ID format.

### 4. Batching

~100K IDs → 10 batches of ~10K. Batching controls workload and reduces messages, but gives coarser-grained retry.

### 5. Kafka architecture

3 brokers, 1 topic, 3 partitions, 1 consumer group, 5 consumer instances. Only 3 consumers could actively consume.

### 6. Kafka key

Where event granularity allowed it, `mandateId` was used as the Kafka key. A Kafka record has only one key — don't mix batch-of-10K with per-mandate keys.

### 7. Consumer processing

Receive → deserialize → extract IDs → look up → update ACTIVE → INACTIVE → record success/failure.

### 8. Database

MySQL was the existing source of truth. Prefer JDBC batching / smaller DB chunks over 10K individual round trips.

### 9. Idempotency

At-least-once + `ACTIVE → INACTIVE` (and `INACTIVE → INACTIVE` is safe).

### 10. Failure handling

Don't consider processing successful if the DB update did not happen. Retry/DLQ as an improvement unless actually implemented.

### 11. Consumer crash

Offsets + rebalance → possible reprocessing → idempotency required.

### 12. Monitoring

Batch ID, total, success, failed, duration (~7–8 minutes per batch).

### 13. Result

~100K dormant mandate records corrected. Users no longer blocked by stale ACTIVE state.

### 14. One-line summary

> I built a Kafka-based asynchronous reconciliation pipeline that processed approximately 100K externally identified dormant mandates, validated and batched the input, consumed it through the Mandate Service, and corrected stale ACTIVE states to INACTIVE in our internal MySQL database.

---

# The 50+ Questions You Should Prepare

## A. Basic Project Questions

**Q1. What problem were you solving?**

Some mandates had become dormant/stale on the external payment/BSE side, but our internal database still marked them as ACTIVE. Since the product allowed only one active mandate per bank/account, the stale internal state could prevent users from creating a new mandate and could lead to SIP/payment failures.

**Q2. What exactly is a mandate?**

A mandate is an authorization that allows automatic debit from a customer's bank account according to predefined instructions. For SIPs, the mandate can be used to automatically debit the customer's account for recurring investments.

**Q3. Why did this problem happen?**

There was a state synchronization gap: External = DORMANT, Internal = ACTIVE. Our system therefore made decisions based on stale information.

**Q4. Why was this a problem for SIP?**

If our system believed an old mandate was still active, it could prevent the user from creating another mandate. That could prevent the correct mandate from being created and potentially cause recurring SIP payments to fail.

**Q5. Why did you receive a CSV?**

The product/operations team had identified approximately 100K affected mandate IDs and provided them as a CSV so that we could perform a bulk reconciliation.

---

## B. Architecture Questions

**Q6. Explain the complete architecture.**

CSV → Validation → Deduplication → Batching → Kafka Producer → Kafka Topic → Mandate Service Consumer → MySQL → ACTIVE → INACTIVE.

Kafka decoupled the bulk ingestion from database processing and provided a durable asynchronous buffer.

**Q7. Why Kafka?**

Asynchronous processing; producer and consumer decoupled; Kafka could buffer the workload; consumer failures didn't require restarting the entire CSV processing; parallel processing; replay/reprocessing depending on offset/error-handling strategy.

**Q8. Why not directly update MySQL?**

For a simple one-time backfill, a SQL script could indeed be simpler and potentially faster. Using the existing Mandate Service with Kafka allowed us to decouple ingestion and processing, avoid one long-running synchronous process, reuse application-level business logic, better control processing, and handle consumer failures independently.

A mature answer is also to acknowledge that Kafka isn't automatically the best choice for every bulk update.

**Q9. Why not use a cron job?**

A cron job could work for a simple batch operation. But Kafka provides a durable queue between ingestion and processing: Producer → Kafka → Consumer → MySQL.

**Q10. Why asynchronous processing?**

The CSV ingestion process didn't need to wait for all mandate updates to finish. The producer could publish the reconciliation workload to Kafka, and consumers could process it independently.

---

## C. Kafka Questions

**Q11. What is a Kafka topic?** A logical stream/category of records. We had a dedicated topic for mandate reconciliation events.

**Q12. What is a Kafka partition?** An ordered, append-only sequence of Kafka records. Multiple partitions allow consumers to process in parallel.

**Q13. Why 3 partitions?** The workload was controlled and finite (~100K mandates). We didn't want to blindly increase partitions without considering downstream database capacity.

**Q14. Why 3 brokers?** Better distribution and fault tolerance than a single broker. Don't claim a specific replication factor unless you know it.

**Q15. What happens if one Kafka broker goes down?** If affected partitions have replicas on other brokers, Kafka can continue from an available replica. Exact behavior depends on RF and configuration.

**Q16. What is a consumer group?** A group of consumers working together to consume a topic. Within a group, a partition is assigned to only one consumer at a time.

**Q17. You had 3 partitions and 5 consumers. What happens?** Only 3 consumers can actively consume. C4 and C5 idle. Never say all 5 consumers process simultaneously.

**Q18. How would you make all 5 consumers useful?** Increase partition count to at least 5. First verify MySQL can handle the increased concurrency.

**Q19. Why use mandateId as Kafka key?** Partition affinity. Same key → same partition. Events for the same mandate can maintain ordering within that partition.

**Q20. Does Kafka guarantee ordering?** Within a partition, yes. Not globally across partitions.

**Q21. What if you don't use a key?** Records for the same mandate might not be in the same partition, so ordering for that mandate isn't guaranteed.

**Q22. What is offset?** Position of a record within a Kafka partition. Consumers use offsets to track progress.

**Q23. What happens if the consumer crashes?** If processing hasn't been successfully committed, the record can be processed again. Duplicate processing possible → idempotency important.

**Q24. What is consumer rebalance?** When consumers join, leave, or fail, Kafka redistributes partitions among remaining consumers.

---

## D. At-Least-Once / Exactly-Once

**Q25. What delivery guarantee did you use?**

> I would characterize this as at-least-once processing unless exactly-once semantics were explicitly configured. Therefore, we designed the database state transition to be idempotent.

**Q26. What is at-least-once?** A message is processed one or more times. Downside: duplicates possible.

**Q27. What is at-most-once?** Processed zero or one time. Advantage: no duplicates. Risk: message loss.

**Q28. What is exactly-once?** Each business operation effectively happens once. Difficult end-to-end. Kafka's EOS doesn't automatically make external DB side effects exactly-once. For this project, **idempotency** is the practical concept.

---

## E. Database Questions

**Q29. Why MySQL?** Existing source of truth; no extra persistence layer.

**Q30. Would you perform 10K UPDATE queries?** Avoid 10K individual round trips. JDBC batch updates, smaller bulk updates, chunking, appropriate indexing. Example: 10,000 IDs → 10 chunks × 1,000 → MySQL.

**Q31. What index should mandate_id have?** Indexed; unique index if guaranteed unique, depending on existing schema.

**Q32. What if mandateId doesn't exist?** Don't silently treat as success. Classify as `NOT_FOUND` and include in the failure/reconciliation report.

**Q33. How do you make the DB operation idempotent?**

```sql
UPDATE mandate
SET status = 'INACTIVE'
WHERE mandate_id = ?
AND status <> 'INACTIVE';
```

**Q34. What if two consumers update the same mandate?** Same key → same partition reduces concurrency. Still protect with transactions, isolation, constraints, or conditional updates.

---

## F. Batch Questions

**Q35. Why 10K per batch?** Reduced Kafka records and per-message overhead. 10K isn't magical — tradeoff between efficiency vs coarser retry vs memory.

**Q36. Why not one mandate per Kafka message?** Better retry granularity and parallelism, but ~100K Kafka records instead of ~10 batched records.

**Q37. Why not put all 100K IDs in one Kafka message?** Very large message, coarse failure handling, message-size and memory concerns.

**Q38. What if 1 ID fails out of 10K?** Mature design tracks per-mandate: 9,999 success, 1 failure, retry only the failed record. If the whole batch retries, idempotency becomes important.

---

## G. Failure Scenarios

**Q39. What if MySQL is down?** Treat as failed; don't acknowledge as successfully processed. Retry when DB is available.

**Q40. What if Kafka is down while publishing?** Don't consider the batch successfully published until Kafka acknowledges. Retry publishing. Keep original CSV/batch available for recovery.

**Q41. What if the consumer crashes after DB update but before committing the Kafka offset?** Same message processed again. Second update `INACTIVE → INACTIVE` is safe. This is why idempotency matters.

**Q42. What if the offset is committed before the DB update?** Dangerous. Kafka believes processed, DB wasn't updated → data loss. Don't treat message as successful before the business operation has succeeded.

---

## H. Retry and DLQ

**Q43. How would you implement retries?** Main topic → Consumer → Success → DB; Failure → Retry Topic → Retry Consumer; after max retries → DLQ.

**Q44. What is DLQ?** Dead Letter Queue. Records that couldn't be processed after allowed retries. Prevents a permanently bad message from blocking normal processing.

**Q45. Would you retry every failure?** No. Transient (DB down, timeout) = retry. Permanent (invalid ID, malformed payload, not found) = investigate/DLQ.

---

## I. Scaling Questions

**Q46. What if tomorrow we receive 10 million mandates?** More partitions, more consumers, more DB capacity. If MySQL is the bottleneck, optimize the database first.

**Q47. How would you scale consumers?** Increase partitions + consumer instances. Number depends on DB capacity and required throughput.

**Q48. Can we have 100 consumers?** Technically yes, but with only 3 partitions only 3 can actively consume. **Consumer parallelism ≤ partition count** for a single consumer group.

**Q49. What is the bottleneck?** Kafka throughput, consumer processing, MySQL write capacity, network, large batch size, lock contention, CPU/memory. For this pipeline, the **database** could easily become the limiting component.

---

## J. Advanced Kafka Questions

**Q50. What is partition rebalancing?** When consumer-group membership changes, Kafka redistributes partitions.

**Q51. What is a hot partition?** A partition receiving disproportionately more traffic. In this reconciliation workload, each mandate was generally processed once, so extreme skew would be less likely.

**Q52. What happens when we increase partition count?** More parallelism, more consumers can work. Tradeoffs: more metadata, operational complexity, consumer coordination, key-to-partition mapping can change.

**Q53. Does increasing consumers always increase throughput?** No. If MySQL can only handle 5 concurrent operations efficiently, extra consumers may overload the database. Throughput is determined by the bottleneck in the complete pipeline.

---

## K. Security / Data Quality

**Q54. What if CSV contains duplicate IDs?** Deduplicate with a set. Report duplicates for operational visibility.

**Q55. What if CSV has 100K invalid records?** Don't send them blindly to Kafka. Fail fast at validation or separate into an error report.

**Q56. What if the CSV contains 5 million rows?** Don't load the complete CSV into memory. Stream incrementally: read → validate → build batch → publish → clear batch → next rows.

---

## L. System Design Questions

**Q57. How would you redesign this system?**

CSV → Validation → Deduplication → Batching → Kafka Topic → Consumer Group → Per-record processing → Success → MySQL / Failure → Retry → DLQ. Plus metrics, logging, tracing, dashboard, alerts.

**Q58. What metrics would you monitor?** `total_records`, `successful_records`, `failed_records`, `processing_rate`, `consumer_lag`, `batch_duration`, DB latency/errors, Kafka publish failures, `retry_count`, `DLQ count`.

**Q59. What is consumer lag?** How far behind a consumer is compared with the latest records. If lag keeps increasing: producer rate > consumer processing rate.

**Q60. How would you know if the system is healthy?** Consumer lag down, success rate up, error rate down, DB latency acceptable, no growing retry/DLQ, all expected batches completed.

---

## M. Tricky Interviewer Questions

**Q61. "Why did you use Kafka for only 100K records? Isn't that overengineering?"**

> That's a valid concern. If this were purely a one-time database backfill with no application-level processing requirement, a direct database batch job could be simpler and potentially faster. Kafka made sense because we wanted asynchronous processing, decoupling between ingestion and processing, durable buffering, and controlled consumer-side processing. I would evaluate the tradeoff rather than saying Kafka is inherently better.

This answer sounds much more senior than blindly defending Kafka.

**Q62. "Why not use SQS instead?"** SQS is valid for queue-based async work. Kafka is attractive for partition-based parallelism, ordering within partitions, consumer groups, replayability, high-throughput event streaming.

**Q63. "Why not use Redis?"** Redis is excellent for caching, lookups, counters. Kafka is better for a durable high-throughput event stream with partitions, consumer groups and replay.

**Q64. "Why not use a database table as a queue?"** Possible, but puts queue semantics on the DB: contention, polling, locking, scaling limits. Kafka is purpose-built for this.

---

## N. The Dangerous Question About Your Implementation

**Q65. "You said 10K IDs per Kafka message and mandateId as key. How can one message have 10K keys?"**

This is the question you must be ready for.

**The technically correct answer:**

> A Kafka record has a single key. So if our actual implementation grouped 10K mandate IDs into one Kafka record, that record could not have 10K independent mandate keys. The keying strategy applies at the Kafka-record level. If we wanted mandate-level partitioning and ordering, we'd need individual mandate records or a different event structure. I would clarify the exact event structure rather than mixing the two concepts.

Do **not** try to bluff this question.

---

## O. Questions About Your Personal Contribution

**Q66. What exactly did YOU implement?**

> My contribution was primarily around the reconciliation pipeline: validating the input, preparing the data for asynchronous processing, integrating the Kafka producer/consumer flow, and handling the mandate-state update flow. I also worked on failure tracking and monitoring around the processing.

Only mention components you actually worked on.

**Q67. Did you design the entire architecture?**

Don't claim ownership if you didn't.

> The architecture was discussed with the team/tech lead, and I was responsible for implementing the relevant parts of the pipeline.

**Q68. What was the hardest part?**

> The challenging part was making the bulk reconciliation reliable. With a large number of records, we had to think about input validation, batching, asynchronous processing, database load and what would happen if processing failed midway. Idempotency was particularly important because asynchronous processing can result in retries.

**Q69. What did you learn from this project?**

> The biggest learning was that asynchronous processing is not just about introducing Kafka. We also have to think about partitioning, consumer parallelism, offset management, idempotency, retries, database bottlenecks and observability. The downstream database can easily become the bottleneck even if Kafka can process messages much faster.

That's a very strong SDE1 answer.

---

## The Interviewer May Ask You to Draw It

```text
                       PRODUCT / OPS
                            |
                          CSV
                            |
                            ▼
                  ┌───────────────────┐
                  │ CSV Processor     │
                  │                   │
                  │ Validation        │
                  │ Deduplication     │
                  │ Batching          │
                  └─────────┬─────────┘
                            |
                            ▼
                  ┌───────────────────┐
                  │ Kafka Producer    │
                  └─────────┬─────────┘
                            |
                            ▼
              ┌──────────────────────────┐
              │       Kafka Cluster      │
              │                          │
              │  Topic: Reconciliation   │
              │                          │
              │ P0 │ P1 │ P2             │
              └────┬───┬───┬────────────┘
                   │   │   │
                   ▼   ▼   ▼
                  C1  C2  C3
                   \   |   /
                    \  |  /
                     \ | /
                      ▼
              ┌───────────────────┐
              │ Mandate Service   │
              │ Consumer Group    │
              └─────────┬─────────┘
                        |
                        ▼
              ┌───────────────────┐
              │      MySQL        │
              │                   │
              │ ACTIVE            │
              │    ↓              │
              │ INACTIVE          │
              └───────────────────┘
```

Then explain: "The producer side handles validation and batching. Kafka provides asynchronous buffering. The Mandate Service consumes the events and performs the internal state reconciliation in MySQL."

---

## Your 90-Second Answer — Memorize This

> At Paytm Money, we had a mandate reconciliation problem. Some mandates had become stale or dormant on the external payment/BSE side, while our internal system still marked them as ACTIVE. Since the product allowed only one active mandate for a bank/account, this stale state could prevent users from creating a new mandate and could eventually cause SIP or payment failures.
>
> The product team provided us with a CSV containing approximately 100K affected mandate IDs.
>
> We built a Kafka-driven asynchronous reconciliation pipeline. First, we validated the CSV for schema issues, empty rows, empty mandate IDs and duplicates. We then processed the valid mandate IDs in batches of approximately 10K.
>
> These reconciliation requests were published to a dedicated Kafka topic. The Kafka setup had 3 brokers and 3 partitions, with a Mandate Service consumer group. There were 5 consumers configured, although because Kafka assigns a partition to only one consumer within a consumer group, only 3 consumers could actively consume simultaneously with 3 partitions.
>
> The consumer processed the reconciliation requests and updated the internal MySQL mandate state from ACTIVE to INACTIVE for the affected mandates.
>
> A key consideration was idempotency. Since asynchronous Kafka processing can involve retries or duplicate processing, repeating the state transition should not cause an incorrect result. An already-INACTIVE mandate remains INACTIVE.
>
> We monitored the processing through logs and failure tracking. Each batch took approximately 7–8 minutes, and the overall reconciliation successfully processed around 100K dormant mandates.
>
> The main benefit was correcting stale internal mandate states so that users were no longer incorrectly blocked by mandates that were already dormant externally.

---

## Rapid-Fire Questions You Should Be Able to Answer

| Question | One-line answer |
| --- | --- |
| Why Kafka? | Async processing, decoupling, durability, scalability |
| Why 3 partitions? | Controlled workload and required parallelism |
| Why 5 consumers? | Configured capacity, but only 3 can be active with 3 partitions |
| Why mandateId key? | Partition affinity and per-mandate ordering |
| Ordering guarantee? | Only within a partition |
| Consumer crash? | Partition reassignment and possible reprocessing |
| Delivery guarantee? | Safest claim: at-least-once + idempotency |
| Why MySQL? | Existing internal mandate store |
| Why batching? | Lower message/processing overhead |
| Why not one huge message? | Message size and coarse-grained retry |
| Why not individual messages? | More messages and overhead |
| MySQL down? | Processing fails; don't treat message as successfully completed |
| Duplicate message? | Idempotent DB update makes replay safe |
| Consumer lag? | Difference between produced and consumed progress |
| DLQ? | Stores records that repeatedly fail |
| Retry everything? | No; distinguish transient vs permanent failures |
| Scale consumers? | Increase partitions too |
| 100 consumers with 3 partitions? | Only 3 can actively consume |
| Biggest bottleneck? | Potentially MySQL |
| Why not SQL script? | SQL could be simpler; Kafka gave async/durable service-based processing |
| Why not SQS? | Kafka gives partitions, consumer groups, replay and streaming semantics |
| What did you personally do? | Explain only the components you actually touched |
| Hardest part? | Reliable bulk processing + failure/idempotency |
| Biggest learning? | Async systems require offset, retry, idempotency and DB-capacity thinking |

---

## Five Questions Where You Should NEVER Bluff

These are the areas where an experienced interviewer can immediately expose an inflated project explanation:

### 1. Exact Kafka message structure

Know whether you actually had **1 mandate = 1 Kafka record** or **10K mandates = 1 Kafka record**.

### 2. Exact Kafka key

Know whether the actual record key was `mandateId`, `batchId`, or something else.

### 3. Offset management

Know whether your application used auto commit, manual acknowledgement, or another mechanism. Don't randomly say "manual commit" if you don't know.

### 4. Retry / DLQ

Don't say "We had exponential backoff, retry topics and DLQ" unless you actually had them.

Instead: "The implementation had failure handling; if I were redesigning it, I'd introduce retry topics and a DLQ for better isolation."

### 5. Exact database update

Know whether you actually used JPA, JDBC, JdbcTemplate, native SQL, bulk update, or a stored procedure. Don't invent the implementation detail.

---

## Best Way to Position This Project in Your Interview

For an SDE1 interview, don't try to make the project sound unnecessarily complicated.

Your strongest story is:

```text
Business problem → stale mandate state
        ↓
100K affected records → CSV
        ↓
Validation + deduplication
        ↓
Kafka → asynchronous processing
        ↓
Consumer → mandate service
        ↓
MySQL → ACTIVE → INACTIVE
        ↓
Idempotency + failure handling
        ↓
100K mandates reconciled
```

That gives you enough material to discuss Kafka, distributed systems, database optimization, failure handling, scalability, and business impact **without overclaiming implementation details**.
