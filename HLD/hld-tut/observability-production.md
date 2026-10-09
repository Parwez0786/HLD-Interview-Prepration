# Observability & Production Systems — Complete Interview Tutorial

For an SDE-1 / Backend Java interview, you should not learn observability as isolated tools like Prometheus or Kibana. Interviewers usually test whether you can answer:

> Your production API is slow/failing. How do you know what is wrong, and how do you debug it?

The core mental model is:

- **Logs** → What happened?
- **Metrics** → How much / how often?
- **Traces** → Where did it happen?
- **Monitoring** → Is the system healthy?
- **Alerting** → When should humans react?

---

## Table of Contents

- [Part 1 — Observability Fundamentals](#part-1--observability-fundamentals)
  - [1. What is Observability?](#1-what-is-observability)
  - [2. Logging](#2-logging)
  - [3. Log Levels](#3-log-levels)
  - [4. What Should You NOT Log?](#4-what-should-you-not-log)
  - [5. Structured Logging](#5-structured-logging)
  - [6. ELK Stack](#6-elk-stack)
  - [7. Elasticsearch](#7-elasticsearch)
  - [8. Logstash](#8-logstash)
  - [9. Kibana](#9-kibana)
  - [10. Metrics](#10-metrics)
  - [11. Types of Metrics](#11-types-of-metrics)
  - [12. Latency Percentiles](#12-latency-percentiles)
  - [13. Why P99 Is Important](#13-why-p99-is-important)
- [Part 2 — Prometheus, Grafana & Monitoring](#part-2--prometheus-grafana--monitoring)
  - [14. Prometheus](#14-prometheus)
  - [15. Spring Boot + Prometheus](#15-spring-boot--prometheus)
  - [16. Prometheus Pull Model](#16-prometheus-pull-model)
  - [17. PromQL](#17-promql)
  - [18. Grafana](#18-grafana)
  - [19. Prometheus vs Grafana](#19-prometheus-vs-grafana)
  - [20. Monitoring](#20-monitoring)
  - [21. The Four Golden Signals](#21-the-four-golden-signals)
  - [22. RED Method](#22-red-method)
  - [23. USE Method](#23-use-method)
  - [24. Alerting](#24-alerting)
  - [25. Good vs Bad Alerts](#25-good-vs-bad-alerts)
- [Part 3 — Health Checks, Tracing & Correlation](#part-3--health-checks-tracing--correlation)
  - [26. Health Checks](#26-health-checks)
  - [27. Liveness vs Readiness](#27-liveness-vs-readiness)
  - [28. Startup Probe](#28-startup-probe)
  - [29. Correlation ID](#29-correlation-id)
  - [30. Correlation ID Example](#30-correlation-id-example)
  - [31. Correlation ID vs Trace ID](#31-correlation-id-vs-trace-id)
  - [32. Distributed Tracing](#32-distributed-tracing)
  - [33. Span](#33-span)
  - [34. OpenTelemetry](#34-opentelemetry)
  - [35. Logs vs Metrics vs Traces](#35-logs-vs-metrics-vs-traces)
- [Part 4 — Production Debugging](#part-4--production-debugging)
  - [36. Production Debugging — Most Important Section](#36-production-debugging--most-important-section)
  - [37. Step 2 — Check Traffic](#37-step-2--check-traffic)
  - [38. Step 3 — Check Error Rate](#38-step-3--check-error-rate)
  - [39. Step 4 — Check Saturation](#39-step-4--check-saturation)
  - [40. Step 5 — Check Distributed Traces](#40-step-5--check-distributed-traces)
  - [41. Step 6 — Check Logs](#41-step-6--check-logs)
  - [42. Step 7 — Check Recent Changes](#42-step-7--check-recent-changes)
  - [43. Step 8 — Mitigate First](#43-step-8--mitigate-first)
  - [44. Example Production Incident](#44-example-production-incident)
  - [45. Example: CPU 95%](#45-example-cpu-95)
  - [46. Example: Memory 95%](#46-example-memory-95)
  - [47. Example: Kafka Consumer Lag](#47-example-kafka-consumer-lag)
  - [48. Database Debugging](#48-database-debugging)
  - [49. Connection Pool Exhaustion](#49-connection-pool-exhaustion)
  - [50. Retry Storm](#50-retry-storm)
  - [51. Circuit Breaker](#51-circuit-breaker)
  - [52. Monitoring a Microservice](#52-monitoring-a-microservice)
  - [53. Technical vs Business Metrics](#53-technical-vs-business-metrics)
- [Part 5 — SLOs, RCA & Architecture](#part-5--slos-rca--architecture)
  - [54. Health Check vs Monitoring](#54-health-check-vs-monitoring)
  - [55. SLI, SLO, SLA](#55-sli-slo-sla)
  - [56. Error Budget](#56-error-budget)
  - [57. Production Dashboard](#57-production-dashboard)
  - [58. Production Debugging Cheat Sheet](#58-production-debugging-cheat-sheet)
  - [59. RCA — Root Cause Analysis](#59-rca--root-cause-analysis)
  - [60. Five Whys](#60-five-whys)
  - [61. Observability Architecture — Interview Level](#61-observability-architecture--interview-level)
- [Part 6 — Interview Questions & Scenarios](#part-6--interview-questions--scenarios)
  - [62. Interview Questions You Must Prepare](#62-interview-questions-you-must-prepare)
  - [63. Intermediate Questions](#63-intermediate-questions)
  - [64. Advanced Interview Questions](#64-advanced-interview-questions)
  - [65. Scenario You Should Practice](#65-scenario-you-should-practice)
  - [66. Scenario: Everything Looks Green but Users Complain](#66-scenario-everything-looks-green-but-users-complain)
  - [67. Scenario: One User's Request Failed](#67-scenario-one-users-request-failed)
  - [68. What You Should Say in a Real Interview](#68-what-you-should-say-in-a-real-interview)
  - [69. Final Mental Model](#69-final-mental-model)
  - [The 10 things I would prioritize for your interview](#the-10-things-i-would-prioritize-for-your-interview)

---

# Part 1 — Observability Fundamentals

## 1. What is Observability?

Observability is the ability to understand the internal state of a system from its external outputs.

The three main pillars are:

```text
                OBSERVABILITY
                     |
        +------------+------------+
        |            |            |
      Logs        Metrics       Traces
        |            |            |
   What happened? How much?   Where/why?
```

### Example

Suppose users complain:

> Payment API is taking 5 seconds.

You investigate:

**Metrics**

```text
payment_api_latency_p99 = 5 sec
payment_api_error_rate = 8%
CPU = 40%
Memory = 60%
```

You know there is a problem, but not necessarily why.

**Logs**

```text
2026-10-08 08:30:12 ERROR PaymentService
Timeout while calling BankService
```

Now you know the payment service is seeing bank-service timeouts.

**Trace**

```text
API Gateway       20ms
     |
Payment Service   50ms
     |
Redis             10ms
     |
Bank Service     4800ms  <-- problem
```

Now you know exactly where the latency comes from.

---

## 2. Logging

Logging means recording events that happen inside your application.

Example:

```java
log.info("Payment initiated paymentId={}", paymentId);

log.error("Payment failed paymentId={} userId={}",
          paymentId, userId, exception);
```

A production log should ideally contain:

- timestamp
- log level
- service
- environment
- message
- request/correlation ID
- user/request identifier where appropriate
- exception
- important business context

Example:

```text
2026-10-08T08:30:10Z
ERROR
payment-service
prod

requestId=abc123
paymentId=p987

Payment failed while calling bank service
timeout=3000ms
```

---

## 3. Log Levels

Common levels:

- TRACE
- DEBUG
- INFO
- WARN
- ERROR
- FATAL

### DEBUG

Detailed information useful during development/debugging.

```text
DEBUG Calling BankService with paymentId=p123
```

Usually you don't want huge amounts of DEBUG logs in production.

### INFO

Normal important application events.

```text
INFO Payment successfully completed paymentId=p123
```

### WARN

Something unusual happened but the application can continue.

```text
WARN Redis unavailable, falling back to database
```

### ERROR

Something failed.

```text
ERROR Payment processing failed
```

---

## 4. What Should You NOT Log?

Very important interview question.

Never casually log:

- password
- credit card number
- OTP
- authentication token
- secret keys
- private personal information

**Bad:**

```java
log.info("user password = {}", password);
```

**Good:**

```java
log.info("Login failed userId={}", userId);
```

Also avoid excessive logging because:

```text
More logs
   ↓
More storage
   ↓
More network traffic
   ↓
Higher cost
   ↓
Potential performance impact
```

---

## 5. Structured Logging

Instead of:

```text
Payment failed for user 123 payment 456
```

prefer structured data:

```json
{
  "level": "ERROR",
  "service": "payment-service",
  "event": "payment_failed",
  "userId": "123",
  "paymentId": "456",
  "requestId": "abc123"
}
```

This makes searching and aggregation much easier.

---

## 6. ELK Stack

ELK is commonly:

- **E** = Elasticsearch
- **L** = Logstash
- **K** = Kibana

### Flow

```text
Application
     |
     | logs
     ↓
 Logstash
     |
     | parse/filter/transform
     ↓
Elasticsearch
     |
     ↓
 Kibana
     |
     ↓
Developer
```

---

## 7. Elasticsearch

Elasticsearch stores and indexes logs so you can search them quickly.

For example:

```text
service = payment-service
level = ERROR
paymentId = 123
```

You can search millions of logs much more efficiently than scanning raw files.

---

## 8. Logstash

Logstash can:

- collect
- parse
- transform
- filter
- forward

Example:

```text
Raw log
   ↓
Logstash
   ↓
extract timestamp
extract service
extract level
extract requestId
   ↓
Elasticsearch
```

---

## 9. Kibana

Kibana provides a UI for searching and visualizing Elasticsearch data.

You might create dashboards:

- Payment Errors
- API Errors
- Top Exceptions
- Request Volume
- Latency
- Failed Transactions

Example:

**Payment Service**

| Metric | Value |
| --- | --- |
| Requests | 120,000 |
| Errors | 1,200 |
| Error Rate | 1% |
| P95 latency | 400ms |
| P99 latency | 1.2s |

---

## 10. Metrics

Metrics are numerical measurements collected over time.

Examples:

- CPU usage
- Memory usage
- Request count
- Error count
- Latency
- Kafka consumer lag
- Database connections
- Cache hit ratio

Unlike logs:

- **Logs** → individual events
- **Metrics** → aggregated numerical measurements

Example:

```text
10:00 → 100 requests
10:01 → 120 requests
10:02 → 500 requests
```

---

## 11. Types of Metrics

A common interview topic is:

### Counter

Only increases.

- `requests_total`
- `errors_total`
- `payments_total`

Example:

```text
requests_total = 1,000,000
```

It can reset when the application restarts.

### Gauge

Can increase or decrease.

- CPU usage
- memory usage
- active connections
- queue size
- Kafka lag

Example:

```text
active_connections = 150
```

Later:

```text
active_connections = 80
```

### Histogram

Used to measure distributions such as latency.

Example request latency:

- 50ms
- 70ms
- 100ms
- 120ms
- 500ms
- 2000ms

Useful for:

- P50
- P90
- P95
- P99

### Summary

Another metric type that can represent quantiles, although histograms are generally more flexible for aggregating across instances.

For interviews, know:

| Type | Meaning |
| --- | --- |
| Counter | cumulative events |
| Gauge | current value |
| Histogram | distribution |
| Summary | quantiles / statistical summaries |

---

## 12. Latency Percentiles

Very important.

Suppose:

- 100 requests

and:

- P50 = 100ms
- P95 = 300ms
- P99 = 1 sec

Meaning approximately:

### P50

50% of requests are faster than 100ms.

### P95

95% are faster than 300ms.

### P99

99% are faster than 1 second.

The remaining 1% are slower.

---

## 13. Why P99 Is Important

Average latency can hide problems.

Suppose:

- 99 requests = 100ms
- 1 request = 10 seconds

Average:

```text
≈199ms
```

That doesn't look terrible.

But one customer experienced:

```text
10 seconds
```

P99 exposes tail latency much better.

**Interview answer:**

> I monitor latency percentiles such as P95 and P99 rather than relying only on averages because averages can hide slow tail requests.

---

# Part 2 — Prometheus, Grafana & Monitoring

## 14. Prometheus

Prometheus is primarily a metrics collection and monitoring system.

Typical architecture:

```text
Application
    |
    | /metrics
    ↓
Prometheus
    |
    ↓
PromQL
    |
    ↓
Grafana
```

Prometheus commonly uses a **pull model**.

It periodically scrapes metrics from applications/exporters.

Example:

```http
GET /actuator/prometheus
```

---

## 15. Spring Boot + Prometheus

In Spring Boot, you might expose metrics using Actuator and Micrometer.

Conceptually:

```text
Spring Boot
     |
 Micrometer
     |
 /actuator/prometheus
     |
 Prometheus
```

Metrics could look like:

```text
http_server_requests_seconds_count
http_server_requests_seconds_sum
jvm_memory_used_bytes
process_cpu_usage
```

---

## 16. Prometheus Pull Model

This is a very common interview question.

**Pull**

```text
Prometheus
    |
    | GET /metrics
    ↓
Application
```

Prometheus asks:

> Give me your current metrics.

**Why pull?**

Advantages include:

- simple service instrumentation
- centralized scraping configuration
- easy target health detection
- Prometheus can determine whether a target is reachable

---

## 17. PromQL

Prometheus Query Language.

Example:

```promql
rate(http_requests_total[5m])
```

Meaning approximately:

Calculate the per-second request rate over the last 5 minutes.

Error rate conceptually:

```promql
rate(errors_total[5m])
/
rate(requests_total[5m])
```

You can use PromQL to build Grafana dashboards and alerts.

---

## 18. Grafana

Grafana is primarily a visualization and dashboarding platform.

It can connect to:

- Prometheus
- Elasticsearch
- Loki
- MySQL
- PostgreSQL
- CloudWatch
- etc.

Typical architecture:

```text
              Grafana
              /    \
             /      \
      Prometheus   Elasticsearch
          |             |
       Metrics         Logs
```

Example Grafana dashboard:

```text
┌───────────────────────────────┐
│ Request Rate                  │
│ 25K req/sec                   │
├───────────────────────────────┤
│ P99 Latency                   │
│ 850 ms                        │
├───────────────────────────────┤
│ Error Rate                    │
│ 2.3%                          │
├───────────────────────────────┤
│ CPU                           │
│ 78%                           │
└───────────────────────────────┘
```

---

## 19. Prometheus vs Grafana

Very common interview question.

| Prometheus | Grafana |
| --- | --- |
| Collects metrics | Visualizes data |
| Stores time-series metrics | Creates dashboards |
| PromQL | Panels/queries |
| Alerting capability | Dashboard/visualization |
| Monitoring system | Visualization platform |

**Simple answer:**

> Prometheus is primarily responsible for collecting and querying metrics, while Grafana is primarily used to visualize those metrics and build dashboards.

---

## 20. Monitoring

Monitoring means continuously observing system health.

You should monitor:

### Application

- Request rate
- Error rate
- Latency
- Throughput

### Infrastructure

- CPU
- Memory
- Disk
- Network

### Database

- Connections
- Query latency
- Locks
- CPU
- Replication lag

### Kafka

- Consumer lag
- Throughput
- Broker health
- Partition distribution
- Under-replicated partitions

### Redis

- Memory
- Hit ratio
- Evictions
- Latency
- Connections

---

## 21. The Four Golden Signals

Very important.

Google's SRE concept:

1. Latency
2. Traffic
3. Errors
4. Saturation

### Latency

How long requests take.

```text
P95 = 300ms
P99 = 1s
```

### Traffic

How much demand exists.

```text
10K requests/sec
```

### Errors

How many requests fail.

```text
Error rate = 2%
```

### Saturation

How close the system is to capacity.

Examples:

- CPU = 95%
- DB connections = 95%
- Kafka lag increasing
- Disk = 90%

---

## 22. RED Method

For request-driven services:

- **R** = Rate
- **E** = Errors
- **D** = Duration

Example:

```text
Rate       = 20K req/sec
Errors     = 1.5%
Duration   = P99 700ms
```

Useful for APIs/microservices.

---

## 23. USE Method

For infrastructure resources:

- **U** = Utilization
- **S** = Saturation
- **E** = Errors

Example for CPU:

```text
Utilization = 90%
Saturation = high load average
Errors = CPU throttling
```

---

## 24. Alerting

Monitoring tells you:

> Something is happening.

Alerting tells you:

> Someone needs to act.

Example:

```text
IF

error_rate > 5%

FOR

5 minutes

THEN

send alert
```

Possible destinations:

- PagerDuty
- Slack
- Email
- Opsgenie
- SMS

---

## 25. Good vs Bad Alerts

**Bad:**

```text
CPU > 80%
```

This may generate too many alerts.

**Better:**

```text
P99 latency > 2 sec
AND
error rate > 5%
FOR 5 minutes
```

The objective is to reduce alert fatigue.

**Interview phrase:**

> Alerts should be actionable and tied to user impact or a meaningful risk, rather than every abnormal metric.

---

# Part 3 — Health Checks, Tracing & Correlation

## 26. Health Checks

Health checks determine whether a service is healthy.

In Spring Boot Actuator:

```http
/actuator/health
```

You might get:

```json
{
  "status": "UP"
}
```

But production systems usually need more detailed health information.

---

## 27. Liveness vs Readiness

Very important for Kubernetes.

### Liveness

Question:

> Is the application process alive?

If no:

- restart container

Example:

```text
Application deadlocked
      ↓
Liveness fails
      ↓
Kubernetes restarts pod
```

### Readiness

Question:

> Can this instance receive traffic?

Example:

```text
Application running
but
database unavailable
```

The process is alive but shouldn't receive traffic.

```text
Readiness = FAIL
      ↓
Remove pod from service
```

Don't necessarily restart it.

---

## 28. Startup Probe

Kubernetes can also use a startup probe for applications that take a long time to initialize.

```text
Container starts
      ↓
Startup probe
      ↓
Application initialized
      ↓
Liveness/readiness begin
```

This prevents liveness checks from killing a slow-starting application.

---

## 29. Correlation ID

This is extremely important in microservices.

Suppose one request travels:

```text
Client
 ↓
API Gateway
 ↓
Order Service
 ↓
Payment Service
 ↓
Bank Service
```

Without correlation ID:

> Which logs belong to this request?

With:

```http
X-Correlation-ID: abc123
```

every service logs:

```text
correlationId=abc123
```

Now you can search:

```text
correlationId = abc123
```

and reconstruct the request flow.

---

## 30. Correlation ID Example

**Gateway:**

```text
requestId = abc123
```

**Order Service:**

```text
INFO requestId=abc123 Order created
```

**Payment Service:**

```text
INFO requestId=abc123 Payment initiated
```

**Bank Service:**

```text
ERROR requestId=abc123 Bank timeout
```

Now debugging becomes much easier.

---

## 31. Correlation ID vs Trace ID

Important distinction.

### Correlation ID

An identifier used to correlate related logs/events.

```text
correlationId=abc123
```

### Trace ID

An identifier representing a distributed trace.

A trace contains multiple spans.

```text
traceId=xyz789

    span A
       |
    span B
       |
    span C
```

They can be the same conceptually in some systems, but don't assume they are always identical.

---

## 32. Distributed Tracing

Distributed tracing follows a request across multiple services.

Example:

```text
                    Trace
                      |
       +--------------+--------------+
       |              |              |
    Gateway         Order         Payment
      10ms           50ms           500ms
                                      |
                                   Bank API
                                   450ms
```

Each individual operation is called a **span**.

---

## 33. Span

A span represents one operation.

Example:

```text
Span:
payment-service.callBank()

Start: 10:00:00
End:   10:00:500
Duration: 500ms
```

**Trace:**

```text
Trace ID: abc123

Span 1: API Gateway       10ms
Span 2: Order Service     50ms
Span 3: Payment Service   500ms
Span 4: Bank API          450ms
```

Now you immediately know the Bank API is responsible for most of the latency.

---

## 34. OpenTelemetry

For modern distributed systems, know OpenTelemetry.

It provides standardized instrumentation for:

- traces
- metrics
- logs

Conceptually:

```text
Application
    |
OpenTelemetry
    |
Collector
    |
+---+---------+
|             |
Jaeger       Prometheus
```

or other observability backends.

---

## 35. Logs vs Metrics vs Traces

Memorize this table.

| Question | Tool |
| --- | --- |
| What happened? | Logs |
| How many? | Metrics |
| How long? | Metrics |
| Where did request spend time? | Traces |
| Why did this particular request fail? | Logs + Trace |
| Is system healthy? | Metrics |
| Search exact exception | Logs |
| Dashboard | Grafana |
| Distributed request path | Tracing |

---

# Part 4 — Production Debugging

## 36. Production Debugging — Most Important Section

Imagine interviewer says:

> Users are complaining that your API has become slow. What will you do?

Don't immediately say:

> I'll check logs.

Give a structured answer.

### Step 1 — Confirm the problem

First check:

> Is latency actually increasing?

Look at:

- P50
- P95
- P99

Compare:

- current
- vs
- normal baseline

---

## 37. Step 2 — Check Traffic

Maybe traffic suddenly increased.

```text
Normal:
5K req/sec

Current:
20K req/sec
```

If traffic increased 4×, the latency problem may be capacity-related.

---

## 38. Step 3 — Check Error Rate

Check:

- 4xx
- 5xx
- timeouts
- exceptions

Example:

```text
Error rate:
0.5% → 8%
```

Now investigate failures.

---

## 39. Step 4 — Check Saturation

Check:

- CPU
- Memory
- DB connections
- Thread pools
- Connection pools
- Kafka lag
- Disk
- Network

Example:

```text
CPU = 45%
Memory = 60%
DB connections = 99%
```

This strongly suggests DB connection pool exhaustion.

---

## 40. Step 5 — Check Distributed Traces

Suppose trace says:

```text
API                    50ms
Order Service           20ms
Payment Service        900ms
Database               850ms
```

Then investigate database.

---

## 41. Step 6 — Check Logs

Search logs using:

- traceId
- correlationId
- service
- time range
- error

Example:

```text
traceId=abc123
```

Search Kibana.

Maybe you find:

```text
ERROR Query timeout after 5 seconds
```

---

## 42. Step 7 — Check Recent Changes

Very important.

Ask:

- Was there a recent deployment?
- Configuration change?
- Database migration?
- Feature flag?
- Traffic spike?
- Dependency failure?

Production debugging isn't just looking at metrics.

You need to correlate:

```text
incident
+
metrics
+
logs
+
traces
+
deployments
```

---

## 43. Step 8 — Mitigate First

In a production incident:

Restore service first, investigate deeply second.

Possible mitigation:

- rollback deployment
- scale instances
- disable feature flag
- increase DB capacity
- reduce traffic
- enable fallback
- stop problematic consumer

Then perform RCA.

---

## 44. Example Production Incident

**Interviewer:**

> Payment API P99 latency increased from 300ms to 3 seconds. What do you do?

**Strong answer:**

> First I'd confirm the increase using P95/P99 latency and check whether traffic or error rate also changed. Then I'd check saturation signals such as CPU, memory, thread pools, database connections and downstream dependency latency. I'd use distributed tracing to identify which service or dependency is contributing most to the latency. Once I identify the affected request path, I'd use the trace or correlation ID to search Kibana for relevant logs and exceptions. I'd also check recent deployments and configuration changes. If there is active customer impact, I'd first mitigate through rollback, scaling, feature flag or fallback depending on the root cause, and then perform a deeper RCA.

That's a very strong interview answer.

---

## 45. Example: CPU 95%

**Interviewer:**

> CPU suddenly becomes 95%. What do you do?

Don't say:

> Increase CPU.

Instead:

```text
CPU high
 ↓
Check traffic
 ↓
Check deployment
 ↓
Check endpoint causing load
 ↓
Check request rate
 ↓
Check thread activity
 ↓
Check GC
 ↓
Check expensive computation
 ↓
Check DB/query behavior
```

Potential causes:

- traffic spike
- infinite loop
- bad algorithm
- excessive serialization
- GC pressure
- N+1 queries
- retry storm
- dependency failure

---

## 46. Example: Memory 95%

Investigate:

- Heap usage
- Non-heap
- GC activity
- GC pauses
- Object allocation
- Caches
- Large collections
- Memory leak

Potential problem:

> Cache has no eviction

or:

> Objects are retained unexpectedly

Don't blindly increase memory.

---

## 47. Example: Kafka Consumer Lag

Given your backend/Kafka background, this is especially important.

Suppose:

```text
Producer:
10,000 msg/sec

Consumer:
7,000 msg/sec
```

Then:

- lag increases

Investigate:

- consumer processing time
- consumer count
- partition count
- CPU
- DB latency
- downstream dependency
- rebalance
- exceptions
- retry/DLQ

Potential solution:

- increase consumers

but only up to the number of partitions for parallelism within a consumer group.

Also investigate why consumers became slower.

---

## 48. Database Debugging

Suppose:

- API latency increased

Trace:

```text
API = 2 sec
DB = 1.8 sec
```

Now investigate:

- slow queries
- query execution plan
- indexes
- locks
- connection pool
- CPU
- IO
- deadlocks
- replication lag

Example:

```sql
EXPLAIN ANALYZE
SELECT ...
```

Maybe an index was missing after a new query was introduced.

---

## 49. Connection Pool Exhaustion

Suppose:

```text
DB pool size = 100

active connections = 100

waiting requests = 500
```

Requests start waiting.

Result:

```text
DB connection wait
      ↓
API latency
      ↓
timeouts
      ↓
retries
      ↓
more load
      ↓
system becomes worse
```

This is a classic production failure loop.

---

## 50. Retry Storm

Very important distributed systems concept.

Suppose Bank Service becomes slow.

Payment Service:

```text
request
 ↓
timeout
 ↓
retry
 ↓
timeout
 ↓
retry
```

Now thousands of clients retry simultaneously.

```text
Failure
 ↓
Retries
 ↓
More traffic
 ↓
More overload
 ↓
More failures
```

This is called a **retry storm**.

Solutions:

- exponential backoff
- jitter
- retry limits
- circuit breaker
- timeouts
- bulkheads
- dead-letter queues where appropriate

---

## 51. Circuit Breaker

If downstream service repeatedly fails:

```text
Payment
   |
   X Bank Service
```

Instead of continuously calling it:

```text
Payment
   |
Circuit Breaker
   |
   X
Bank
```

Circuit breaker opens:

```text
CLOSED
   ↓
failures increase
   ↓
OPEN
   ↓
reject calls quickly
   ↓
after timeout
   ↓
HALF-OPEN
   ↓
test request
   ↓
success → CLOSED
```

This protects the system from cascading failure.

---

## 52. Monitoring a Microservice

A production Java service should ideally expose:

### Application metrics

- request count
- error count
- latency
- active requests
- thread pool

### JVM

- GC

### Dependency metrics

- DB latency
- Redis latency
- Kafka
- external APIs

### Business metrics

These are extremely valuable.

For a payment service:

- `payments_started`
- `payments_success`
- `payments_failed`
- `payments_timeout`

Technical health alone isn't enough.

---

## 53. Technical vs Business Metrics

**Technical:**

```text
CPU = 40%
Latency = 200ms
Error rate = 1%
```

**Business:**

- Payment success rate
- Order conversion
- SIP success rate
- Mandate success rate

Imagine:

```text
CPU = 30%
Latency = 100ms
Error rate = 0.1%
```

Everything looks healthy.

But:

```text
Payment success rate dropped 30%
```

That's a major production problem.

---

# Part 5 — SLOs, RCA & Architecture

## 54. Health Check vs Monitoring

**Health check:**

> Is this particular instance healthy?

**Monitoring:**

> How is the overall system behaving?

Example:

```http
/health
```

might say:

```text
UP
```

while Grafana shows:

```text
P99 = 5 seconds
Error rate = 10%
```

So:

A service being "UP" does not mean the system is healthy from the user's perspective.

---

## 55. SLI, SLO, SLA

Good interview topic.

### SLI

Service Level Indicator.

Actual measurement.

```text
99.5% successful requests
```

### SLO

Target.

```text
99.9% successful requests
```

### SLA

Contractual agreement with customers.

Example:

```text
99.9% availability
```

with possible penalties.

Simple:

| Term | Meaning |
| --- | --- |
| SLI | measurement |
| SLO | target |
| SLA | contractual commitment |

---

## 56. Error Budget

Suppose SLO:

```text
99.9% availability
```

Allowed failure:

```text
0.1%
```

That allowed failure is your **error budget**.

If you consume the budget too quickly:

- slow down risky releases
- focus on reliability

This is an important SRE concept.

---

## 57. Production Dashboard

For an API, I would create a dashboard containing:

```text
                    API DASHBOARD

Traffic
 └── requests/sec

Latency
 ├── P50
 ├── P95
 └── P99

Errors
 ├── 4xx
 ├── 5xx
 └── timeout rate

Infrastructure
 ├── CPU
 ├── Memory
 └── Network

JVM
 ├── Heap
 ├── GC
 └── Threads

Database
 ├── Connections
 ├── Query latency
 └── Errors

Dependencies
 ├── Redis latency
 ├── Kafka lag
 └── External API latency
```

---

## 58. Production Debugging Cheat Sheet

When something goes wrong, follow:

```text
                INCIDENT
                   |
                   ↓
              Is it real?
                   |
                   ↓
          Check 4 Golden Signals
                   |
       +-----------+-----------+
       |           |           |
    Traffic      Errors     Latency
                               |
                          Saturation
                               |
                               ↓
                       Identify service
                               |
                               ↓
                           Trace
                               |
                               ↓
                            Logs
                               |
                               ↓
                       Recent changes
                               |
                               ↓
                         Find root cause
                               |
                               ↓
                           Mitigate
                               |
                               ↓
                             RCA
```

---

## 59. RCA — Root Cause Analysis

After the incident, answer:

**What happened?**

Payment latency increased.

**Why?**

Database connection pool exhausted.

**Why did that happen?**

New endpoint generated excessive DB queries.

**Why wasn't it caught?**

No alert on DB connection utilization.

**Fix**

- Add index
- Optimize query
- Add monitoring
- Add alert
- Add load test

---

## 60. Five Whys

Example:

Why did payments fail?

- → Payment service timed out.

Why timeout?

- → DB query was slow.

Why slow?

- → Missing index.

Why was index missing?

- → New query wasn't covered by performance testing.

Why wasn't it caught?

- → No database latency regression test.

This gives a deeper RCA instead of simply saying:

> Database was slow.

---

## 61. Observability Architecture — Interview Level

A good architecture answer:

```text
                  Users
                    |
                API Gateway
                    |
              Load Balancer
                    |
        +-----------+-----------+
        |                       |
   Service A                Service B
        |                       |
        +-----------+-----------+
                    |
              OpenTelemetry
                    |
              OTEL Collector
                    |
       +------------+-------------+
       |            |             |
     Logs         Metrics       Traces
       |            |             |
 Elasticsearch  Prometheus      Tempo/
       |            |           Jaeger
       |            |             |
       +------------+-------------+
                    |
                 Grafana
                    |
                Engineers
```

For a traditional stack, you may also see:

```text
Application
   ↓
Logs → Logstash → Elasticsearch → Kibana

Metrics → Prometheus → Grafana

Traces → Jaeger/Tempo
```

---

# Part 6 — Interview Questions & Scenarios

## 62. Interview Questions You Must Prepare

### Basic

**Q1. What is observability?**

**Answer:**

> Observability is the ability to understand the internal behavior and state of a system from its outputs, primarily using logs, metrics and traces.

**Q2. Three pillars?**

- Logs
- Metrics
- Traces

**Q3. Logs vs metrics?**

> Logs represent individual events and provide detailed context, while metrics are numerical time-series data used to understand trends, rates and system health.

**Q4. Why P99 instead of average?**

> Average latency can hide tail latency. P99 tells us how slow the worst-performing portion of requests is.

**Q5. Prometheus vs Grafana?**

> Prometheus collects and queries time-series metrics, while Grafana visualizes metrics and other data sources through dashboards.

**Q6. What is ELK?**

> Elasticsearch, Logstash and Kibana. Logstash collects/processes logs, Elasticsearch indexes and stores them for search, and Kibana provides visualization and exploration.

---

## 63. Intermediate Questions

**Q7. What is correlation ID?**

> A correlation ID is an identifier propagated across services so that logs and events belonging to the same logical request can be correlated during debugging.

**Q8. What is distributed tracing?**

> Distributed tracing tracks a request across multiple services using a trace composed of spans, allowing us to identify where time is spent and where failures occur.

**Q9. What is a span?**

> A span represents one unit of work within a distributed trace, such as an HTTP call, database query or Kafka operation.

**Q10. What is consumer lag?**

> Consumer lag is the amount of unprocessed Kafka data behind the latest available offset. Increasing lag usually means consumers aren't processing messages as quickly as producers are generating them.

---

## 64. Advanced Interview Questions

**Q11. API latency increased. How would you debug it?**

Use:

1. Confirm P95/P99 increase
2. Check traffic
3. Check error rate
4. Check saturation
5. Check dependency latency
6. Check distributed traces
7. Search logs using trace/correlation ID
8. Check recent deployments/config changes
9. Mitigate
10. RCA

**Q12. CPU is 95%, what do you do?**

- Traffic?
- Recent deployment?
- Which endpoint?
- Thread pool?
- GC?
- Expensive computation?
- Infinite loop?
- Retry storm?
- DB behavior?

**Q13. Memory keeps increasing. What do you investigate?**

- Heap
- GC
- allocation rate
- cache
- object retention
- heap dump
- memory leak

**Q14. Error rate suddenly increases after deployment.**

**Strong answer:**

```text
Check deployment timeline
        ↓
Compare old/new version
        ↓
Check error metrics
        ↓
Check logs
        ↓
Check traces
        ↓
Identify failing endpoint
        ↓
Rollback if customer impact
        ↓
RCA
```

---

## 65. Scenario You Should Practice

**Interviewer:**

> Your payment API was working fine for months. Suddenly P99 latency went from 500ms to 5 seconds. CPU is only 40%. How will you debug?

Your thought process should be:

```text
CPU isn't the bottleneck
       ↓
Check traffic
       ↓
Check DB connections
       ↓
Check DB latency
       ↓
Check Redis
       ↓
Check downstream APIs
       ↓
Distributed trace
       ↓
Find slow span
       ↓
Search logs using traceId
```

Suppose trace shows:

```text
Payment Service       100ms
Redis                  50ms
Database              100ms
Bank API             4.7 sec
```

Then:

The bottleneck is the Bank API.

Next investigate:

- Bank API latency
- timeouts
- network
- retries
- connection pool
- recent bank changes

---

## 66. Scenario: Everything Looks Green but Users Complain

This is a very good SDE interview question.

Suppose:

```text
CPU = 40%
Memory = 50%
Error rate = 0.1%
P99 = 300ms
```

But:

```text
Payment success rate ↓ 25%
```

**Answer:**

> I would check business-level metrics because infrastructure metrics can look healthy while a business workflow is failing.

Then investigate:

- payment initiation
- payment authorization
- bank response
- payment completion

This demonstrates mature production thinking.

---

## 67. Scenario: One User's Request Failed

Suppose user gives:

```text
requestId = abc123
```

You can follow:

```text
Kibana
   ↓
search requestId
   ↓
Gateway log
   ↓
Order Service
   ↓
Payment Service
   ↓
Bank Service
```

Or:

```text
Trace ID
   ↓
Distributed trace
   ↓
failed span
   ↓
service logs
```

This is why correlation IDs and distributed tracing are so valuable.

---

## 68. What You Should Say in a Real Interview

For your Java/Spring Boot backend interviews, memorize this production-debugging framework:

> I generally start with the four golden signals: traffic, errors, latency and saturation. First I confirm whether the issue is real and compare the current metrics with the baseline. Then I identify the affected service or endpoint using metrics and distributed traces. Once I have a trace or correlation ID, I search the centralized logs in Kibana for detailed errors and context. I also check dependencies such as the database, Redis, Kafka and external APIs, along with recent deployments or configuration changes. If there is customer impact, I prioritize mitigation through rollback, scaling, feature flags or fallback, and then perform RCA and add preventive monitoring or tests.

That answer works for a large number of production-debugging questions.

---

## 69. Final Mental Model

Remember this:

```text
                 PRODUCTION ISSUE
                        |
                        ↓
                 "What changed?"
                        |
                        ↓
             ┌─────────────────────┐
             │  4 GOLDEN SIGNALS   │
             └─────────────────────┘
               /      |       \
          Traffic   Errors   Latency
                         \      /
                          Saturation
                              |
                              ↓
                         DISTRIBUTED
                           TRACE
                              |
                              ↓
                            LOGS
                              |
                              ↓
                       CORRELATION ID
                              |
                              ↓
                       FIND ROOT CAUSE
                              |
                              ↓
                           MITIGATE
                              |
                              ↓
                             RCA
                              |
                              ↓
                    PREVENT RECURRENCE
```

---

## The 10 things I would prioritize for your interview

1. Logs vs Metrics vs Traces
2. Prometheus architecture + PromQL basics
3. Grafana
4. ELK + Kibana
5. Correlation ID
6. Distributed tracing + spans
7. P50/P95/P99
8. Four Golden Signals
9. Liveness vs Readiness
10. Production debugging scenarios

For your Paytm Money-style backend experience, especially be ready to connect this with Kafka consumer lag, Redis failures, DB latency, Spring Boot Actuator, Kibana logs, Rundeck jobs, retries, DLQs and production incident RCA. Those combinations are much more likely to be asked than isolated definitions.
