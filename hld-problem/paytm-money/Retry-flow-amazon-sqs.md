# Retry Flow via Amazon SQS — Paytm Money

## Resume bullet

> Implemented retry flow via Amazon SQS with configurable delay intervals using Redis to recover 404 failures in Omne’s fund transfer API, ensuring 100% update reliability and improving system consistency.

---

## Table of Contents

- [1. First Understand the Business](#1-first-understand-the-business)
- [2. What Was the Problem?](#2-what-was-the-problem)
- [3. Why Couldn't We Simply Retry Immediately?](#3-why-couldnt-we-simply-retry-immediately)
- [4. What is SQS?](#4-what-is-sqs)
- [5. Complete Architecture](#5-your-complete-architecture)
- [6. What Happens When 404 Occurs?](#6-what-exactly-happens-when-404-occurs)
- [7. Why Redis?](#7-why-redis)
- [8. Redis Is NOT the Queue](#8-important-redis-is-not-the-queue)
- [9. What Happens With the SQS Message?](#9-now-what-happens-with-the-sqs-message)
- [10. Suppose Retry Succeeds](#10-suppose-retry-succeeds)
- [11. Suppose Retry Fails Again](#11-suppose-retry-fails-again)
- [12. What Does "Configurable Delay" Solve?](#12-what-does-configurable-delay-actually-solve)
- [13. Fixed Delay vs Exponential Backoff](#13-fixed-delay-vs-exponential-backoff)
- [14. Biggest Problem With SQS](#14-what-is-the-biggest-problem-with-sqs)
- [15. Idempotency](#15-this-is-where-idempotency-comes-in)
- [16. What Is Idempotency?](#16-what-is-idempotency)
- [17. Why Is Idempotency Especially Important Here?](#17-why-is-idempotency-especially-important-here)
- [18. Another Dangerous Scenario](#18-another-dangerous-scenario)
- [19. What Does "System Consistency" Mean?](#19-what-does-system-consistency-mean)
- [20. What Does "100% Update Reliability" Mean?](#20-what-does-100-update-reliability-mean)
- [21. What Happens If the Worker Crashes?](#21-what-happens-if-the-worker-crashes)
- [22. What Is Visibility Timeout?](#22-what-is-visibility-timeout)
- [23. What If Redis Goes Down?](#23-what-if-redis-goes-down)
- [24. Why Not Use MySQL for Retry Configuration?](#24-why-not-use-mysql-for-retry-configuration)
- [25. Why SQS Instead of Kafka?](#25-why-sqs-instead-of-kafka)
- [26. What If Omne Is Completely Down?](#26-what-if-omne-is-completely-down)
- [27. What Is a Dead Letter Queue?](#27-what-is-a-dead-letter-queue)
- [28. Conceptual Code](#28-what-would-the-code-approximately-look-like)
- [29. Java Interview Questions](#29-what-questions-can-they-ask-about-java)
- [30. Redis Interview Questions](#30-what-questions-can-they-ask-about-redis)
- [31. SQS Interview Questions](#31-what-questions-can-they-ask-about-sqs)
- [32. Standard vs FIFO SQS](#32-standard-vs-fifo-sqs)
- [33. Database Interview Questions](#33-what-questions-can-they-ask-about-databases)
- [34. Scalability Interview Questions](#34-what-questions-can-they-ask-about-scalability)
- [35. Monitoring Interview Questions](#35-what-questions-can-they-ask-about-monitoring)
- [36. The Story You Should Remember](#36-the-story-you-should-remember)
- [37. Your Interview Explanation](#37-your-interview-explanation)
- [38. Problem With the Resume Bullet](#38-but-theres-a-problem-with-your-resume-bullet)

---

## 1. First Understand the Business

Let's imagine Paytm Money has an investment transaction.

A user wants to invest **₹5,000**.

The simplified flow is:

```text
User
  |
  v
Paytm Money
  |
  | Fund Transfer Request
  v
Omne
  |
  v
Payment Gateway / Bank
```

### What is Omne?

**Assumption:** Omne is a downstream financial/payment service used by Paytm Money for processing or coordinating fund-transfer operations.

Your service sends a request to Omne:

```http
POST /fund-transfer
```

```json
{
  "transactionId": "TX123",
  "amount": 5000,
  "accountId": "ACC456"
}
```

Omne processes the request and returns a response.

---

## 2. What Was the Problem?

**Normally:**

```text
Paytm Money
      |
      | Fund Transfer
      v
    Omne
      |
      | 200 OK
      v
Paytm Money
```

Everything is fine.

**But sometimes:**

```text
Paytm Money
      |
      | Fund Transfer
      v
    Omne
      |
      | 404
      v
Paytm Money
```

Now what should Paytm Money do?

If you simply say:

```java
if (response == 404) {
    transaction.setStatus(FAILED);
}
```

you might **incorrectly mark something as failed**.

**Why?**

Assumption: In this particular integration, some 404 responses represented a **temporary downstream inconsistency** — for example, the resource/state wasn't available yet.

```text
404
 ↓
Don't immediately give up
 ↓
Try again later
```

That's the fundamental reason for this project.

---

## 3. Why Couldn't We Simply Retry Immediately?

Suppose Omne returns 404. You could write:

```java
callOmne();

if (404) {
    callOmne();
}

if (404) {
    callOmne();
}
```

But imagine 1,000 requests are doing this.

```text
1000 requests
     |
     v
1000 application threads
     |
     v
Omne
```

If Omne is having a temporary problem, you're actually **making the problem worse**.

You also consume:

- CPU
- Threads
- Connections
- Network resources

Therefore:

> Instead of keeping the original request waiting, we move the retry operation to an **asynchronous queue**.

That's where Amazon SQS comes in.

---

## 4. What is SQS?

**SQS = Simple Queue Service.**

Think of it like a waiting line.

Imagine a restaurant:

```text
Customers
   |
   v
Queue
   |
   v
Available waiter
```

In software:

```text
Failed requests
      |
      v
     SQS
      |
      v
Retry workers
```

The application doesn't have to retry immediately.

It puts a message into SQS:

```json
{
  "transactionId": "TX123",
  "retryCount": 1
}
```

Then a worker picks it up later.

---

## 5. Your Complete Architecture

Under our assumption, the architecture would look like:

```text
                  ┌─────────────────┐
                  │   Paytm Money   │
                  │ Fund Transfer   │
                  │    Service      │
                  └────────┬────────┘
                           │
                           │ API Call
                           ▼
                    ┌─────────────┐
                    │    Omne     │
                    └──────┬──────┘
                           │
                    ┌──────┴──────┐
                    │             │
                  200            404
                    │             │
                    ▼             ▼
                Success        Redis
                                  │
                                  ▼
                                SQS
                                  │
                                  ▼
                            Retry Worker
                                  │
                                  ▼
                                Omne
```

This is the architecture you should understand.

---

## 6. What Exactly Happens When 404 Occurs?

Let's walk through a real transaction: **TX123**, ₹5,000.

Paytm Money calls Omne:

```http
POST /fund-transfer
```

Omne returns **404 Not Found**.

Your application identifies:

```text
Is this a retryable 404?
        |
       YES
        |
        v
Create retry task
```

Something conceptually like:

```json
{
  "transactionId": "TX123",
  "retryCount": 1
}
```

Then:

```text
Paytm Money
     |
     v
    SQS
```

The original request doesn't have to sit there waiting.

---

## 7. Why Redis?

This is where your resume says:

> configurable delay intervals using Redis

Suppose you want:

| Retry | Delay |
| --- | --- |
| Retry #1 | after 30 seconds |
| Retry #2 | after 2 minutes |
| Retry #3 | after 5 minutes |

Instead of hardcoding:

```java
if (retryCount == 1)
    delay = 30;

if (retryCount == 2)
    delay = 120;

if (retryCount == 3)
    delay = 300;
```

you could maintain configuration in Redis.

```text
Redis

retry.delay.1 = 30 seconds
retry.delay.2 = 120 seconds
retry.delay.3 = 300 seconds
```

Then the application reads the configuration.

**Why is this useful?**

Suppose tomorrow you decide Retry #1 = 60 seconds.

You can change the configuration rather than changing application code and redeploying.

---

## 8. Important: Redis Is NOT the Queue

This is something you must understand.

| System | Used for |
| --- | --- |
| **Redis** | Configuration / retry state |
| **SQS** | Asynchronous work / retry messages |

Think:

```text
Redis = "How should I retry?"
SQS   = "What should I retry?"
```

That's a nice interview explanation.

---

## 9. Now What Happens With the SQS Message?

A worker continuously listens to SQS.

Conceptually:

```java
while(true) {
    message = receiveMessage();
    process(message);
}
```

It receives:

```json
{
  "transactionId": "TX123",
  "retryCount": 1
}
```

The worker knows:

- Transaction = `TX123`
- Retry count = `1`

It obtains the appropriate retry configuration and eventually calls Omne again.

```text
SQS
 |
 v
Worker
 |
 | retry
 v
Omne
```

---

## 10. Suppose Retry Succeeds

Omne returns **200 OK**.

Then:

```text
Worker
   |
   v
Update transaction
   |
   v
SUCCESS
```

And the SQS message is successfully acknowledged/deleted.

```text
SQS
 |
 v
Worker
 |
 v
Omne
 |
 v
SUCCESS
 |
 v
Delete SQS message
```

---

## 11. Suppose Retry Fails Again

Imagine:

```text
Attempt 1 → 404
Attempt 2 → 404
Attempt 3 → 404
```

You don't want to retry forever.

So we need a **maximum retry count**.

```text
Attempt 1
   ↓
Attempt 2
   ↓
Attempt 3
   ↓
Maximum retries reached
   ↓
Stop
```

The exact number is not present in your resume, so **don't claim a number** unless you can verify it.

---

## 12. What Does "Configurable Delay" Actually Solve?

Suppose Omne is temporarily inconsistent.

Immediately retrying is bad:

```text
404
 ↓
retry immediately
 ↓
404
 ↓
retry immediately
 ↓
404
```

Instead:

```text
404
 ↓
wait
 ↓
retry
 ↓
wait longer
 ↓
retry
```

This gives Omne time to recover.

---

## 13. Fixed Delay vs Exponential Backoff

An interviewer may ask this.

| Strategy | Delays |
| --- | --- |
| **Fixed delay** | 30 sec, 30 sec, 30 sec |
| **Exponential backoff** | 30 sec, 60 sec, 120 sec, 240 sec (or some capped variation) |

The idea is: the more times the operation fails, the longer we wait before retrying.

This prevents excessive pressure on the downstream service.

**Important:** your resume only says *configurable delay intervals*. Don't tell the interviewer you implemented exponential backoff unless that's actually what the system did.

---

## 14. What Is the Biggest Problem With SQS?

**Duplicate messages.**

Suppose:

```text
SQS
 |
 v
Worker A
 |
 v
Omne
 |
 v
SUCCESS
```

But Worker A crashes before acknowledging/deleting the message.

SQS may eventually give the message to Worker B.

```text
SQS
 |
 +----> Worker A → Omne → SUCCESS
 |
 +----> Worker B → Omne → ????
```

Now you've potentially processed the same transaction **twice**.

This is extremely important in financial systems.

---

## 15. This Is Where Idempotency Comes In

Suppose `transactionId = TX123`.

You want the operation to be **idempotent**.

Meaning:

```text
Process TX123
Process TX123 again
Process TX123 again
```

should **not** create three separate financial operations.

The final business state should remain correct.

```text
TX123
 |
 +---- Worker A
 |
 +---- Worker B
 |
 +---- Worker C
```

All should safely converge on the same transaction result.

---

## 16. What Is Idempotency?

**Interview answer:**

> Idempotency means processing the same request multiple times produces the same business result as processing it once.

For example:

```text
SET transaction TX123 = SUCCESS
```

| Executed | Result |
| --- | --- |
| Once | SUCCESS |
| Three times | SUCCESS |

It doesn't create three transactions.

---

## 17. Why Is Idempotency Especially Important Here?

Because SQS is generally **at-least-once delivery**.

That means you should design your consumer assuming that the same message can potentially be delivered more than once.

```text
SQS
 ↓
At-least-once delivery
 ↓
Possible duplicate
 ↓
Idempotent consumer
 ↓
Safe transaction processing
```

This is a very strong system-design concept to know.

---

## 18. Another Dangerous Scenario

This one is extremely important.

Imagine:

```text
Paytm Money
      |
      v
     Omne
      |
      v
Transaction SUCCESS
```

Omne successfully processes ₹5,000.

But then the network connection breaks.

```text
Omne → SUCCESS
       |
       X
   Network failure
       |
       v
Paytm Money doesn't receive response
```

Paytm Money thinks: **UNKNOWN**

If you blindly retry:

```text
Paytm Money
     |
     v
Omne
```

you might execute the transaction **twice**.

Therefore, a robust financial integration needs a stable transaction identifier / idempotency key and safe downstream semantics.

---

## 19. What Does "System Consistency" Mean?

Suppose Omne says `SUCCESS`, but Paytm Money's database says `PENDING`.

You have **inconsistent state**.

```text
Omne DB              Paytm DB

SUCCESS              PENDING
   ❌                    ❌
```

Your retry/recovery mechanism attempts to bring them back into alignment.

Eventually:

```text
Omne DB              Paytm DB

SUCCESS              SUCCESS
   ✅                    ✅
```

That's the consistency you're talking about.

---

## 20. What Does "100% Update Reliability" Mean?

Don't interpret this as:

> My system can never fail.

That's not realistic.

A better interpretation is:

> Recoverable update events were not silently lost. Instead of dropping a transaction update when Omne returned a recoverable 404, we persisted it into the retry workflow and attempted recovery according to the retry policy.

That's a much better interview answer.

---

## 21. What Happens If the Worker Crashes?

Suppose:

```text
SQS
 |
 v
Worker
 |
 X
CRASH
```

The message shouldn't disappear simply because the worker crashed.

SQS has a concept called **visibility timeout**.

```text
SQS
 |
 | give message to Worker
 v
Worker
 |
 | message temporarily hidden
 |
 X crash
 |
 | visibility timeout expires
 v
SQS
 |
 v
Another worker
```

This allows another consumer to process the message.

---

## 22. What Is Visibility Timeout?

**Interview answer:**

> Visibility timeout is the period during which an SQS message remains invisible to other consumers after a consumer receives it. If the consumer successfully processes the message, it deletes it. If it crashes and doesn't delete it, the message can become visible again after the timeout.

Know this very well.

---

## 23. What If Redis Goes Down?

This is another question.

```text
Worker
  |
  X
Redis unavailable
```

Now you need to decide whether Redis contains:

- Only retry configuration
- Temporary retry state
- Critical business state

**Assumption for our explanation:** Redis is primarily being used for retry configuration/state, while the durable transaction state remains in the database.

Then you could have:

```text
Redis unavailable
      |
      v
Use safe fallback / fail safely
```

The exact behavior depends on the actual implementation.

**Don't invent a fallback mechanism** if you don't know what your production system did.

---

## 24. Why Not Use MySQL for Retry Configuration?

Because Redis provides extremely fast reads.

Suppose thousands of retry workers repeatedly ask: "What's the retry delay?"

You don't necessarily want every worker repeatedly querying MySQL.

```text
Workers
  |
  v
Redis
```

Redis can serve frequently accessed configuration much faster.

But MySQL remains better suited for **durable transactional business data**.

---

## 25. Why SQS Instead of Kafka?

This is almost guaranteed because your resume contains both.

| Kafka is good for | SQS is good for |
| --- | --- |
| Event streaming | Background jobs |
| High throughput | Task queues |
| Multiple consumers | Asynchronous processing |
| Event replay | Managed queue semantics |
| Event-driven architecture | |

For your retry use case:

> Please execute this operation later.

SQS is a natural fit.

**A good answer:**

> We already use Kafka for event-driven workflows, but this particular requirement was a retry/task-queue problem. SQS gave us a managed asynchronous queue suitable for background retry processing.

---

## 26. What If Omne Is Completely Down?

Suppose Omne is down, and you have 100,000 failed requests.

If you immediately retry all:

```text
100,000 requests
       ↓
      Omne
       X
```

You create a **retry storm**.

So you need:

- Delay
- Backoff
- Maximum retries
- Rate limiting if required
- Monitoring

This is a very important distributed-system concept.

---

## 27. What Is a Dead Letter Queue?

Suppose a message fails repeatedly.

```text
SQS
 |
 v
Worker
 |
 X
retry
 |
 X
retry
 |
 X
retry
 |
 v
DLQ
```

**DLQ = Dead Letter Queue.**

It stores messages that couldn't be successfully processed after the allowed attempts.

Then engineers can investigate them.

Again, your resume doesn't explicitly say you implemented a DLQ, so treat this as **architecture knowledge**, not as something to claim you personally built.

---

## 28. What Would the Code Approximately Look Like?

This is a **conceptual implementation**, not a claim about your actual Paytm Money code.

### Controller / service

```java
public void processFundTransfer(Transaction transaction) {

    OmneResponse response = omneClient.transfer(transaction);

    if (response.isSuccess()) {
        updateTransaction(transaction, SUCCESS);
        return;
    }

    if (response.getStatusCode() == 404) {
        retryService.scheduleRetry(transaction);
        return;
    }

    updateTransaction(transaction, FAILED);
}
```

### Schedule retry

```java
public void scheduleRetry(Transaction transaction) {

    int retryCount = getRetryCount(transaction);

    RetryConfig config = redis.getRetryConfig(retryCount);

    RetryMessage message = new RetryMessage(
        transaction.getId(),
        retryCount
    );

    sqs.send(message, config.getDelay());
}
```

### Worker

```java
public void processRetry(RetryMessage message) {

    Transaction transaction =
        transactionRepository.findById(message.getTransactionId());

    if (alreadyCompleted(transaction)) {
        return;
    }

    OmneResponse response =
        omneClient.transfer(transaction);

    if (response.isSuccess()) {
        updateTransaction(transaction, SUCCESS);
    } else {
        scheduleNextRetry(transaction);
    }
}
```

Again, this is a learning model of how such a system could work.

---

## 29. What Questions Can They Ask About Java?

**Q: How did you call Omne?**

Likely:

```text
Spring Boot
    ↓
REST client
    ↓
Omne API
```

Potential technologies could include:

- RestTemplate
- WebClient
- Feign

Don't claim which one unless you remember.

**Q: How did you handle exceptions?**

Conceptually:

```java
try {
    callOmne();
} catch (Exception e) {
    handleFailure();
}
```

But you should distinguish:

- Business failure
- Network failure
- Timeout
- HTTP failure
- Unexpected exception

They shouldn't all necessarily follow the same retry policy.

---

## 30. What Questions Can They Ask About Redis?

Know these:

**What is Redis?**

In-memory key-value data store.

**Why Redis?**

Fast reads/writes and useful for shared state/configuration.

**Redis vs MySQL?**

| Redis | MySQL |
| --- | --- |
| Fast / cache / transient state | Durable relational data |

**Redis data structures?**

Know: String, Hash, List, Set, Sorted Set.

**Why Hash?**

Potentially:

```text
retry_config
    attempt1 → 30
    attempt2 → 120
    attempt3 → 300
```

Again, that's an example, not your confirmed schema.

---

## 31. What Questions Can They Ask About SQS?

Know these extremely well:

- What is SQS?
- Standard vs FIFO?
- Visibility timeout?
- Message retention?
- Long polling?
- At-least-once delivery?
- DLQ?
- Duplicate messages?
- How does consumer scaling work?
- How do you acknowledge/delete messages?
- What happens if consumer crashes?
- How do you handle poison messages?

---

## 32. Standard vs FIFO SQS

| Standard | FIFO |
| --- | --- |
| Prioritizes high throughput | Stronger ordering / deduplication |
| Duplicate delivery can happen | |

For financial workflows, FIFO may sound attractive, but you should **not automatically say FIFO was used**.

If asked:

> Would FIFO solve all your problems?

**Answer:**

> No. FIFO can help with ordering and deduplication semantics, but business-level idempotency is still important because external API calls and network failures can create ambiguity.

That's a strong answer.

---

## 33. What Questions Can They Ask About Databases?

**What happens if transaction update fails?**

```text
Omne = SUCCESS
Paytm DB = PENDING
```

You need a recovery mechanism.

**What if two workers update simultaneously?**

Potential mechanisms:

- Atomic update
- Optimistic locking
- Unique constraint
- Idempotency

**How do you prevent duplicate transactions?**

Use:

- Transaction ID
- Idempotency key
- Database constraints
- Idempotent state transition

depending on system design.

---

## 34. What Questions Can They Ask About Scalability?

Suppose you suddenly have **10x transactions**.

Your architecture:

```text
          SQS
        /  |  \
       /   |   \
 Worker Worker Worker
```

You can **horizontally scale** consumers.

```text
10 messages/sec
     ↓
1 worker

1000 messages/sec
     ↓
multiple workers
```

This is one of the main advantages of asynchronous queues.

---

## 35. What Questions Can They Ask About Monitoring?

You should say you would monitor:

- SQS queue depth
- Retry count
- Retry success rate
- Failure rate
- 404 rate
- Omne latency
- Message age
- DLQ count
- Consumer processing time

For example:

```text
Queue depth suddenly increases
          ↓
More Omne failures
          ↓
Investigate Omne
```

Your resume also says you worked with Kibana and Rundeck, so these are reasonable technologies to understand in the broader context of your Paytm Money work.

---

## 36. The Story You Should Remember

Don't memorize 50 different things. Remember this story:

```text
1. Paytm Money calls Omne
             ↓
2. Some requests return 404
             ↓
3. Some 404s are recoverable
             ↓
4. Don't block the API thread
             ↓
5. Create retry task
             ↓
6. Put task into SQS
             ↓
7. Redis provides retry configuration/state
             ↓
8. Worker consumes SQS message
             ↓
9. Worker retries Omne
             ↓
10. Success → update transaction
             ↓
11. Failure → retry according to policy
             ↓
12. Maximum retries → stop/escalate
             ↓
13. Idempotency protects against duplicates
```

---

## 37. Your Interview Explanation

If they say:

> Explain your third Paytm Money project.

Use this:

> One of the issues we had was with the fund-transfer integration with Omne. In some cases, Omne returned a 404 response for a transaction/resource that could potentially become available later. If we immediately treated that response as a permanent failure, our internal transaction state could become inconsistent with the downstream system.
>
> So we implemented an asynchronous retry mechanism using Amazon SQS. When a recoverable 404 occurred, instead of blocking the original request thread and continuously retrying, we created a retry message containing the transaction information and pushed it to SQS.
>
> We used Redis for retry-related configuration/state, including configurable retry delay intervals. This allowed the retry behavior to be controlled without hardcoding all the retry timings into the application.
>
> A background worker consumed messages from SQS and attempted the operation again after the configured delay. If the retry succeeded, we updated the transaction state. If it failed again, we followed the retry policy rather than retrying indefinitely.
>
> Another important consideration was duplicate processing because queue systems can deliver a message more than once. Therefore, transaction processing needed to be idempotent so that processing the same transaction multiple times would not result in duplicate business operations.
>
> The overall goal was to make recoverable transaction updates reliable and prevent transient downstream failures from leaving Paytm Money and Omne in inconsistent states.

---

## 38. But There's a Problem With Your Resume Bullet

Your bullet is technically impressive, but because you don't remember the implementation, an interviewer could destroy you with follow-ups.

For example:

- What was the SQS visibility timeout?
- What was the maximum retry count?
- What exact Redis key did you use?
- Was it Standard or FIFO SQS?
- How did you guarantee idempotency?
- What was the message format?
- What was the Omne API endpoint?
- What did you do when Redis went down?

If you don't know these, **don't invent answers**.

Instead, learn the architecture and be honest about the parts you can defend.

### Your preparation priority

**Level 1 — MUST KNOW**

- Business problem
- Omne role
- Why 404 needed recovery
- Why SQS
- Why Redis
- Complete retry flow
- Retry policy
- Idempotency
- Duplicate messages
- SQS visibility timeout

**Level 2 — SHOULD KNOW**

- DLQ
- Exponential backoff
- Retry storm
- SQS Standard vs FIFO
- Redis vs MySQL
- Worker scaling
- Monitoring

**Level 3 — DEEP SYSTEM DESIGN**

- Exactly-once vs at-least-once
- Lost response problem
- Concurrent workers
- Atomic state transitions
- Distributed locking
- Failure recovery
- Backpressure

If you want to prepare this properly, the best next step is to build a complete hypothetical implementation of this Paytm Money system in Java + Spring Boot + Redis + AWS SQS, including the database schema, APIs, SQS message format, retry worker, idempotency, failure scenarios, and then do a 50-question interviewer cross-examination on it.
