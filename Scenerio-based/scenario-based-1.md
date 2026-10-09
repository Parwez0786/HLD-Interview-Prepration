# Scenario-Based Interview Questions

---

## Table of Contents

- [Microservices](#microservices)
  - [11. Service A → B → C, and C is down. How do you prevent cascading failure?](#11-service-a--b--c-and-c-is-down-how-do-you-prevent-cascading-failure)
  - [12. Same payment gets processed twice. How do you prevent duplicate processing?](#12-same-payment-gets-processed-twice-how-do-you-prevent-duplicate-processing)
  - [13. One microservice becomes slow and affects the entire system. What patterns would you use?](#13-one-microservice-becomes-slow-and-affects-the-entire-system-what-patterns-would-you-use)
  - [14. How would you trace a request across 15 microservices?](#14-how-would-you-trace-a-request-across-15-microservices)
  - [15. REST, Kafka or gRPC — which would you choose and why?](#15-rest-kafka-or-grpc--which-would-you-choose-and-why)
- [Kafka](#kafka)
  - [16. Kafka consumer processes the same message twice. How would you handle it?](#16-kafka-consumer-processes-the-same-message-twice-how-would-you-handle-it)
  - [17. One partition has much higher traffic than others. How would you fix it?](#17-one-partition-has-much-higher-traffic-than-others-how-would-you-fix-it)
  - [18. Consumer lag keeps increasing. How would you investigate?](#18-consumer-lag-keeps-increasing-how-would-you-investigate)
  - [19. A message fails repeatedly during processing. What should happen next?](#19-a-message-fails-repeatedly-during-processing-what-should-happen-next)
  - [20. How would you guarantee message ordering for a customer?](#20-how-would-you-guarantee-message-ordering-for-a-customer)
- [Database](#database)
  - [21. Query took 50 ms and now takes 10 seconds. How troubleshoot?](#21-query-took-50-ms-and-now-takes-10-seconds-how-troubleshoot)
  - [22. Database CPU reaches 100% during peak hours. What steps?](#22-database-cpu-reaches-100-during-peak-hours-what-steps)
  - [23. Two transactions update the same row simultaneously. How handle concurrency?](#23-two-transactions-update-the-same-row-simultaneously-how-handle-concurrency)
  - [24. Table has hundreds of millions of records. How improve performance?](#24-table-has-hundreds-of-millions-of-records-how-improve-performance)
  - [25. Optimistic or pessimistic locking for inventory?](#25-optimistic-or-pessimistic-locking-for-inventory)
- [System Design & AWS](#system-design--aws)
  - [26. API receives 100,000 requests/minute. How scale?](#26-api-receives-100000-requestsminute-how-scale)
  - [27. Users are abusing your API. How implement rate limiting?](#27-users-are-abusing-your-api-how-implement-rate-limiting)
  - [28. Redis crashes unexpectedly. How should application behave?](#28-redis-crashes-unexpectedly-how-should-application-behave)
  - [29. EC2 needs secure access to S3. How without credentials?](#29-ec2-needs-secure-access-to-s3-how-without-credentials)
  - [30. Deployment causes increased latency. How find root cause and roll back safely?](#30-deployment-causes-increased-latency-how-find-root-cause-and-roll-back-safely)

---

## Microservices

---

## 11. Service A → B → C, and C is down. How do you prevent cascading failure?

Suppose:

```text
Client
  ↓
Service A
  ↓
Service B
  ↓
Service C ❌
```

If B keeps waiting for C, threads/connections get occupied and eventually A also becomes slow.

I would use:

### 1. Timeout

Don't wait indefinitely.

```text
B → C
   ↓
wait max 2 seconds
   ↓
timeout
```

### 2. Circuit Breaker

If C keeps failing:

```text
C failures
   ↓
Circuit opens
   ↓
B stops calling C temporarily
   ↓
fallback response
```

After some time, allow a few test requests.

```text
CLOSED → OPEN → HALF-OPEN → CLOSED
```

### 3. Retry carefully

Retry only for temporary failures and use exponential backoff.

```text
1st retry → 100ms
2nd → 200ms
3rd → 400ms
```

Don't blindly retry because it can make an already-down service even more overloaded.

### 4. Bulkhead

Separate resources for different dependencies.

For example:

- Payment calls → 20 threads
- Notification calls → 10 threads

If notification service is down, it doesn't consume all threads.

### Interview answer

> I would use timeouts, circuit breakers, controlled retries with exponential backoff, and bulkheads. The goal is to fail fast and prevent one unhealthy service from consuming resources of the entire system.

---

## 12. Same payment gets processed twice. How do you prevent duplicate processing?

This is an idempotency problem.

Suppose:

```text
Payment request
   ↓
Payment processed
   ↓
Response lost
   ↓
Client retries
   ↓
Same payment processed again ❌
```

I would use an idempotency key.

For example:

```http
POST /payment

Idempotency-Key: abc123
```

Database:

```text
payment_request
----------------
idempotency_key UNIQUE
payment_id
status
response
```

When request comes:

```text
abc123 → doesn't exist
       ↓
process payment
       ↓
save result
```

Second request:

```text
abc123 → already exists
       ↓
return previous result
```

The unique constraint is important because two requests can arrive simultaneously.

### Interview answer

> I would make the payment API idempotent using a unique idempotency key. I would persist that key with the payment status and enforce a database unique constraint so concurrent requests cannot create duplicate payments.

For distributed systems, also consider the payment provider's own idempotency mechanism.

---

## 13. One microservice becomes slow and affects the entire system. What patterns would you use?

Main patterns:

### Timeout

Never wait forever.

### Circuit Breaker

Stop sending requests to an unhealthy service.

### Bulkhead

Isolate resources.

### Retry + exponential backoff

Only for appropriate transient failures.

### Async processing

If the operation doesn't need an immediate response:

```text
Service A
   ↓
Kafka
   ↓
Service B
```

Instead of:

```text
A → B
```

### Caching

If B repeatedly provides the same data:

```text
A → Redis
     ↓
   cache hit
```

instead of calling B every time.

---

## 14. How would you trace a request across 15 microservices?

I would use distributed tracing.

Example:

```text
Client
 ↓
A
 ↓
B
 ↓
C
 ↓
...
 ↓
O
```

Each request gets a trace ID.

For example:

```text
traceId = 7f23abc
```

Each service creates a span:

```text
Trace
 ├── A: 20ms
 ├── B: 50ms
 ├── C: 2s       ← problem
 ├── D: 30ms
 └── E: 20ms
```

Tools commonly used include:

- OpenTelemetry
- Jaeger
- Zipkin
- AWS X-Ray

Also propagate:

- traceId
- spanId

through HTTP/gRPC/Kafka metadata.

### Interview answer

> I would use distributed tracing with OpenTelemetry. Every incoming request gets a trace ID, and each microservice creates a span. This allows us to see the complete request path and identify which service or database call introduced latency.

---

## 15. REST, Kafka or gRPC — which would you choose and why?

They solve different problems.

| Technology | Use case |
| --- | --- |
| REST | Synchronous external/client APIs |
| gRPC | Fast synchronous service-to-service communication |
| Kafka | Asynchronous event-driven communication |

### Example

Client → Payment API:

REST

Payment Service → Fraud Service:

gRPC

Payment completed → Notification:

Kafka

### Important point

Don't say:

> Kafka is faster than REST, so I'll use Kafka.

They have different communication models.

### Interview answer

> For public APIs I would generally use REST. For synchronous internal service-to-service communication where performance and strongly defined contracts matter, I would consider gRPC. For asynchronous, decoupled workflows and event streaming, I would use Kafka.

---

## Kafka

---

## 16. Kafka consumer processes the same message twice. How would you handle it?

Kafka provides at-least-once processing commonly, so duplicates can happen.

Example:

```text
Consumer
 ↓
Process payment
 ↓
DB update succeeds
 ↓
Consumer crashes ❌
 ↓
Offset wasn't committed
 ↓
Message consumed again
```

Therefore, make processing idempotent.

For example:

```text
eventId = abc123
```

Database:

```text
processed_events
----------------
event_id UNIQUE
```

Before processing:

```text
Does abc123 exist?
    ↓
Yes → skip
No  → process
```

Use a database unique constraint to handle concurrent processing safely.

### Interview answer

> I wouldn't try to completely eliminate duplicates at the consumer level. I would design the consumer to be idempotent using an event ID and a unique constraint or an equivalent deduplication mechanism.

---

## 17. One partition has much higher traffic than others. How would you fix it?

This is usually partition skew / hot partition.

First check the partition key.

Bad example:

```text
key = country
```

If:

India → 70% traffic

one partition can become hot.

### Better key

Use a high-cardinality, well-distributed key.

For customer ordering:

customerId

But there's an important tradeoff:

If I need ordering per customer, I should keep:

```text
customerId → same partition
```

while ensuring different customers are distributed.

### Possible solutions

- Choose a better partition key.
- Increase partitions if overall capacity is insufficient.
- Avoid keys with low cardinality.
- For extremely hot keys, consider application-level sharding.

### Interview point

> I wouldn't blindly increase partitions because if the partition key remains skewed, the same hot key can still create a hot partition.

---

## 18. Consumer lag keeps increasing. How would you investigate?

First understand:

```text
Consumer lag =
messages produced - messages consumed
```

I would investigate step by step.

### 1. Check producer rate

Is traffic suddenly increasing?

```text
Producer = 10,000 msg/sec
Consumer = 5,000 msg/sec
```

Lag will increase.

### 2. Check consumer processing time

Maybe a DB/API call became slow.

### 3. Check consumer errors

Look for:

- exceptions
- timeouts
- retries
- rebalance

### 4. Check CPU/memory

Consumer might be resource constrained.

### 5. Check partition distribution

One consumer may be handling a hot partition.

### 6. Check number of consumers

If:

- 12 partitions
- 3 consumers

each consumer handles roughly 4 partitions.

Increasing consumers can help only until:

```text
number of consumers <= number of partitions
```

### 7. Check downstream dependencies

For example:

```text
Kafka
 ↓
Consumer
 ↓
Database ❌ slow
```

Kafka may be healthy while DB is the bottleneck.

### Interview answer

> I would first determine whether the lag is caused by increased producer traffic or reduced consumer throughput. Then I'd check consumer processing latency, errors, rebalances, CPU/memory, partition skew, and downstream dependencies such as the database.

---

## 19. A message fails repeatedly during processing. What should happen next?

Don't retry forever.

Typical flow:

```text
Kafka
 ↓
Consumer
 ↓
Processing fails
 ↓
Retry
 ↓
Retry
 ↓
Retry
 ↓
DLQ/DLT
```

Use a limited retry count.

For example:

```text
Attempt 1
Attempt 2
Attempt 3
        ↓
Dead Letter Topic
```

DLT should contain enough information to troubleshoot/reprocess:

- original topic
- partition
- offset
- event/message
- exception
- timestamp
- retry count

Then operations can investigate and replay after fixing the problem.

### Important distinction

Transient error:

network timeout

→ retry

Permanent error:

invalid customer ID

→ retrying may not help → DLT

---

## 20. How would you guarantee message ordering for a customer?

Kafka guarantees ordering within a partition, not across partitions.

Therefore:

```text
key = customerId
```

Kafka sends all events for the same customer to the same partition.

```text
Customer 101
   ↓
Partition 3

Customer 102
   ↓
Partition 7
```

So:

```text
Customer 101:
OrderCreated
PaymentCompleted
OrderShipped
```

remain ordered within partition 3.

### Interview answer

> If ordering is required per customer, I would use customerId as the Kafka partition key. Kafka preserves message order within a partition, so all events for that customer go to the same partition.

---

## Database

---

## 21. Query took 50 ms and now takes 10 seconds. How troubleshoot?

I would not immediately add an index.

First:

### 1. Check execution plan

```sql
EXPLAIN ANALYZE ...
```

Look for:

- Full table scan
- Wrong join strategy
- Missing index
- Large sort
- Large number of rows scanned

### 2. Check data growth

Maybe:

1 million → 500 million rows

### 3. Check indexes

Is the query using the expected index?

### 4. Check locks

Another transaction may be blocking it.

### 5. Check database resources

- CPU
- Memory
- Disk I/O
- Connections

### 6. Check recent changes

- schema changes
- query changes
- deployment
- statistics
- indexes

### Interview answer

> I'd compare the execution plan from when the query was fast with the current plan, then check indexes, data volume, locks, database resources, and recent application or schema changes.

---

## 22. Database CPU reaches 100% during peak hours. What steps?

First identify what is consuming CPU.

### Step 1

Check expensive queries.

Top CPU queries

### Step 2

Optimize queries/indexes.

### Step 3

Check connection pool.

Too many concurrent queries can overload DB.

### Step 4

Use caching.

```text
Application → Redis
```

for frequently-read data.

### Step 5

Read replicas

```text
Writes → Primary
Reads  → Read replicas
```

### Step 6

Scale vertically if necessary.

### Step 7

Partition/shard if the data/workload requires it.

### Interview answer

> I would first identify the source of CPU rather than immediately scaling the database. I would optimize expensive queries, indexes and connection usage, then use caching/read replicas and finally consider vertical scaling or partitioning.

---

## 23. Two transactions update the same row simultaneously. How handle concurrency?

Example:

```text
Stock = 10

Transaction A → stock = 9
Transaction B → stock = 9
```

Potential lost update.

One solution is optimistic locking.

Database:

- id
- stock
- version

Read:

```text
stock = 10
version = 5
```

Update:

```sql
UPDATE product
SET stock = 9,
    version = 6
WHERE id = 1
AND version = 5;
```

If another transaction already changed it:

```text
affected rows = 0
```

Then retry/fail.

Another option is:

```sql
SELECT ... FOR UPDATE;
```

which uses pessimistic locking.

---

## 24. Table has hundreds of millions of records. How improve performance?

I would consider:

### 1. Proper indexes

Index frequently queried columns.

### 2. Query optimization

Avoid:

```sql
SELECT *
```

when unnecessary.

### 3. Partitioning

For example:

- orders_2025
- orders_2026

based on date.

### 4. Archiving

Move old data to cheaper storage if it isn't frequently accessed.

### 5. Read replicas

For read-heavy workloads.

### 6. Caching

Frequently accessed data → Redis.

### 7. Sharding

If a single database cannot handle the workload.

### Interview point

Don't say:

> I'll shard immediately.

First optimize query/index/schema and understand workload.

---

## 25. Optimistic or pessimistic locking for inventory?

For inventory, concurrent updates are important because overselling must be prevented.

### Pessimistic

```sql
SELECT stock
FROM product
WHERE id = 10
FOR UPDATE;
```

Lock row → update stock → commit.

Good when conflicts are frequent.

### Optimistic

Use a version:

```text
stock = 5
version = 10
```

Update only if version is unchanged.

Good when conflicts are relatively rare.

### Interview answer

> For a highly contended inventory item, pessimistic locking can be appropriate because conflicts are expected and I want to serialize updates. If contention is low, optimistic locking can provide better concurrency without holding database locks.

---

## System Design & AWS

---

## 26. API receives 100,000 requests/minute. How scale?

100,000/min ≈

1,667 requests/sec

Architecture:

```text
             Load Balancer
                   ↓
        ┌──────────┼──────────┐
        ↓          ↓          ↓
     Server 1   Server 2   Server 3
        ↓          ↓          ↓
             Application
                   ↓
             Redis / DB
```

Use:

- Load balancer
- Multiple stateless application instances
- Auto Scaling
- Redis caching
- Database optimization
- Read replicas
- Queue/Kafka for async work

For AWS, could use:

```text
Route 53
   ↓
ALB
   ↓
EC2/ECS/EKS
   ↓
RDS
   ↓
Redis
```

---

## 27. Users are abusing your API. How implement rate limiting?

Example:

100 requests/minute/user

Redis is commonly useful.

Conceptually:

```text
userId → request count
```

For example:

```redis
INCR user:123
EXPIRE user:123 60
```

If:

count > 100

return:

```http
HTTP 429 Too Many Requests
```

Better algorithms include:

- Token bucket
- Leaky bucket
- Sliding window

For distributed services, Redis provides shared rate-limit state.

---

## 28. Redis crashes unexpectedly. How should application behave?

Redis should generally be treated as a cache, unless the architecture intentionally uses it as a durable datastore.

If it's a cache:

```text
Application
   ↓
Redis ❌
   ↓
Database
```

Application should fall back to DB.

But be careful: if thousands of requests simultaneously fall back to DB:

```text
Redis failure
 ↓
10,000 requests
 ↓
DB
 ↓
DB overloaded ❌
```

This is a cache stampede / thundering herd scenario.

Use:

- DB fallback
- Circuit breaker
- Request limiting
- Local cache where appropriate
- Cache warm-up
- Redis replication/sentinel/cluster depending on requirements

---

## 29. EC2 needs secure access to S3. How without credentials?

Use an IAM role attached to the EC2 instance.

```text
EC2
 ↓
IAM Role
 ↓
IAM Policy
 ↓
S3
```

For example, allow only:

`s3:GetObject`

on a specific bucket/path.

Do not put:

- AWS_ACCESS_KEY
- AWS_SECRET_KEY

inside source code or config files.

### Interview answer

> I would attach an IAM instance role to EC2 with the minimum required S3 permissions. The AWS SDK can obtain temporary credentials automatically, so I don't need to store long-lived credentials on the server.

---

## 30. Deployment causes increased latency. How find root cause and roll back safely?

I would first compare:

Before deployment

vs

After deployment

Check:

- API latency
- Error rate
- CPU/memory
- DB latency
- Kafka lag
- External API latency
- Logs
- Distributed traces

For example:

```text
Deployment
   ↓
API latency ↑
   ↓
Trace
   ↓
DB query 50ms → 2 sec
   ↓
New query/index issue
```

### Safe rollback

If impact is significant:

```text
Current version
      ↓
Rollback previous stable version
```

For safer deployments, use:

### Canary

```text
95% → old version
5%  → new version
```

Monitor:

- latency
- errors
- CPU

If healthy:

```text
5% → 25% → 50% → 100%
```

If unhealthy:

```text
5% → rollback
```

### Blue-green

```text
Blue  → current
Green → new
```

Test Green, then switch traffic.

### Interview answer

> I would correlate the latency increase with the deployment timestamp, use metrics, logs and distributed tracing to identify the bottleneck, and if the impact is significant, stop the rollout and move traffic back to the previous stable version. Canary or blue-green deployment reduces the blast radius.
