# Availability & Reliability Fundamentals — HLD

These are Availability & Reliability fundamentals for HLD. I'll explain them in simple interview language, with examples and common follow-up questions.

---

## Table of Contents

- [1. Availability](#1-availability)
- [2. Reliability](#2-reliability)
- [3. SLA, SLO, SLI](#3-sla-slo-sli)
- [4. Fault Tolerance](#4-fault-tolerance)
- [5. Redundancy](#5-redundancy)
- [6. Failover](#6-failover)
- [7. Active-Active](#7-active-active)
- [8. Active-Passive](#8-active-passive)
- [9. Health Checks](#9-health-checks)
- [10. Heartbeats](#10-heartbeats)
- [11. Disaster Recovery](#11-disaster-recovery)
- [12. RTO — Recovery Time Objective](#12-rto--recovery-time-objective)
- [13. RPO — Recovery Point Objective](#13-rpo--recovery-point-objective)
- [14. Graceful Degradation](#14-graceful-degradation)
- [15. Retry](#15-retry)
- [16. Timeout](#16-timeout)
- [17. Circuit Breaker](#17-circuit-breaker)
- [How These Concepts Connect](#how-these-concepts-connect)
- [Interview Cheat Sheet](#interview-cheat-sheet)

---

## 1. Availability

Availability = How often the system is up and usable.

Example:

If a payment service is available 99.99% of the year, users should be able to use it almost all the time.

### Availability formula

$$ Availability = \frac{Uptime}{Uptime + Downtime} $$

Common availability levels:

| Availability | Approx. downtime/year |
| --- | --- |
| 99% | 3.65 days |
| 99.9% | 8.76 hours |
| 99.99% | 52.6 minutes |
| 99.999% | 5.26 minutes |

### Interview answer

> Availability means the percentage of time a system remains accessible and operational.

---

## 2. Reliability

Reliability = Ability of the system to keep working correctly over time.

Availability asks:

> Is the system up?

Reliability asks:

> Does the system continue to work correctly without failures?

Example:

A service may be available but occasionally process a payment twice.

It is available, but not sufficiently reliable.

### Simple difference

| Concept | Meaning |
| --- | --- |
| Availability | Is it running? |
| Reliability | Is it working correctly and consistently? |

---

## 3. SLA, SLO, SLI

These three are commonly asked together.

### SLI — Service Level Indicator

Actual measurement.

Examples:

- 99.95% successful requests
- 200 ms average latency
- 99.99% availability

It tells us what actually happened.

### SLO — Service Level Objective

Target we want to achieve.

Example:

```text
SLO:
99.99% availability
p99 latency < 300 ms
```

### SLA — Service Level Agreement

Contractual agreement with the customer.

Example:

> Our service will provide 99.9% availability. If we fail to meet it, customers may receive service credits.

### Easy way to remember

| Term | Meaning |
| --- | --- |
| SLI | Measurement |
| SLO | Target |
| SLA | Contract |

---

## 4. Fault Tolerance

Fault tolerance = System continues working even when some components fail.

Example:

```text
           Load Balancer
              /     \
             /       \
        Server 1   Server 2
           ❌          ✅
```

Server 1 crashes.

Server 2 continues serving requests.

The system is fault tolerant.

---

## 5. Redundancy

Redundancy = Having extra components so that failure of one doesn't stop the system.

Example:

```text
Primary DB
     +
Replica DB
```

If primary fails, replica can take over.

Redundancy can exist at:

- Server level
- Database level
- Network level
- Data-center level
- Region level

---

## 6. Failover

Failover = Switching from a failed component to a backup component.

Example:

```text
Primary DB
    ↓
  CRASH ❌
    ↓
Replica DB
    ↓
Become Primary
```

The actual switching process is failover.

### Redundancy vs Failover

| Concept | Meaning |
| --- | --- |
| Redundancy | Backup exists |
| Failover | We switch to the backup |

---

## 7. Active-Active

In active-active, multiple instances actively serve traffic.

```text
             Load Balancer
             /           \
            ↓             ↓
        Server A       Server B
        ACTIVE          ACTIVE
```

Traffic can go to both.

### Advantages

- Better resource utilization
- Higher availability
- Easy horizontal scaling

### Problem

Data consistency can become complicated if both sides modify the same data.

---

## 8. Active-Passive

Only one instance actively handles traffic.

```text
Primary
ACTIVE
   |
   | failure
   ↓
Backup
PASSIVE
```

The backup waits until the primary fails.

### Advantages

- Simpler
- Easier data management

### Disadvantages

- Backup resources may be underutilized
- Failover may take some time

---

## 9. Health Checks

A health check determines whether a service is healthy.

Example:

```http
GET /health
```

Response:

```json
{
  "status": "UP"
}
```

Load balancer might check every 5 seconds.

```text
LB
 |
 +-- Server A → Healthy ✅
 |
 +-- Server B → Unhealthy ❌
```

Traffic is removed from Server B.

### Types

**Liveness**

Is the application running?

**Readiness**

Is the application ready to receive traffic?

This distinction is especially important in Kubernetes.

---

## 10. Heartbeats

A heartbeat is a periodic signal saying:

> I'm alive.

Example:

```text
Server A → ❤️ → Server B
Server A → ❤️ → Server B
Server A → ❤️ → Server B
```

If Server B doesn't receive heartbeats for some period:

```text
Heartbeat timeout
       ↓
Assume A failed
       ↓
Start failover
```

### Health check vs heartbeat

| Concept | Meaning |
| --- | --- |
| Health check | Actively asks "Are you healthy?" |
| Heartbeat | Component periodically says "I'm alive." |

---

## 11. Disaster Recovery

Disaster Recovery (DR) = Strategy for recovering the system after a major failure.

Examples:

- Data center failure
- Region failure
- Major database corruption
- Ransomware/security incident
- Large infrastructure outage

Example:

```text
Primary Region
Mumbai
   |
   | replication
   ↓
DR Region
Hyderabad
```

If Mumbai becomes unavailable, traffic can potentially move to Hyderabad.

---

## 12. RTO — Recovery Time Objective

RTO = Maximum acceptable time to recover the system.

Example:

```text
RTO = 30 minutes
```

If the system goes down at:

```text
10:00 AM
```

It should be recovered by:

```text
10:30 AM
```

---

## 13. RPO — Recovery Point Objective

RPO = Maximum acceptable amount of data loss measured in time.

Example:

```text
RPO = 5 minutes
```

If failure occurs at:

```text
10:00 AM
```

You may accept losing data from approximately:

```text
9:55 → 10:00
```

### RTO vs RPO

Very important interview question:

| Concept | Meaning |
| --- | --- |
| RTO | How quickly must we recover? |
| RPO | How much data can we afford to lose? |

---

## 14. Graceful Degradation

Graceful degradation = When something fails, provide reduced functionality instead of completely failing.

Example: E-commerce website.

Suppose recommendation service fails:

```text
Recommendation Service ❌
          ↓
Homepage still works ✅
          ↓
"Recommended for you" section hidden
```

Instead of:

```text
Entire website ❌
```

This improves availability and user experience.

---

## 15. Retry

If a temporary failure occurs, the client tries again.

```text
Request
  ↓
Server ❌
  ↓
Retry
  ↓
Server ✅
```

Example:

```text
Attempt 1 → timeout
Attempt 2 → timeout
Attempt 3 → success
```

### Important

Don't retry indefinitely.

Usually use:

**Exponential backoff**

```text
Retry 1 → 100 ms
Retry 2 → 200 ms
Retry 3 → 400 ms
Retry 4 → 800 ms
```

Also add jitter so thousands of clients don't retry simultaneously.

---

## 16. Timeout

A timeout specifies:

> How long should I wait for this operation?

Example:

```text
Service A → Service B

Timeout = 2 seconds
```

If B doesn't respond within 2 seconds:

```text
Request cancelled ❌
```

Without timeouts, requests can remain stuck and consume threads/connections.

### Interview point

Every network call should have a reasonable timeout.

---

## 17. Circuit Breaker

Circuit breaker protects your system from repeatedly calling a failing service.

Imagine:

```text
Order Service
     ↓
Payment Service ❌
```

Payment service is down.

Without circuit breaker:

```text
Request → Payment ❌
Request → Payment ❌
Request → Payment ❌
Request → Payment ❌
...
```

This can overload the already-failing service and waste resources.

With circuit breaker:

```text
Order Service
     ↓
Circuit Breaker
     ↓
Payment ❌
```

After repeated failures:

```text
CLOSED
  ↓
many failures
  ↓
OPEN
  ↓
don't call Payment
```

After some time:

```text
OPEN
 ↓
test request
 ↓
HALF-OPEN
 ↓
success → CLOSED
```

### Three states

```text
CLOSED
  ↓ failures
OPEN
  ↓ wait
HALF-OPEN
  ↓
success → CLOSED
failure → OPEN
```

---

## How These Concepts Connect

This is the important HLD picture:

```text
                    Load Balancer
                         |
              +----------+----------+
              |                     |
           Server A              Server B
           ACTIVE                ACTIVE
              |                     |
              +----------+----------+
                         |
                      Database
                    /          \
               Primary        Replica
                  |               |
                  +---------------+
                     Redundancy
```

- Health checks → detect failures
- Heartbeats → detect liveness
- Failover → switch to backup
- Retry → handle temporary failures
- Timeout → prevent requests hanging
- Circuit breaker → stop calling failing services
- Graceful degradation → keep partial functionality

For major disasters:

```text
             Primary Region
                   |
                Replication
                   ↓
                DR Region
```

- RTO → How fast can we recover?
- RPO → How much data can we lose?

---

## Interview Cheat Sheet

| Concept | Simple meaning |
| --- | --- |
| Availability | System is up |
| Reliability | System works correctly over time |
| SLI | Actual measurement |
| SLO | Target |
| SLA | Customer contract |
| Fault tolerance | Keeps working despite failures |
| Redundancy | Extra/backup components |
| Failover | Switch to backup |
| Active-active | Both instances serve traffic |
| Active-passive | One serves, one waits |
| Health check | Check whether service is healthy |
| Heartbeat | Periodic "I'm alive" signal |
| DR | Recovery from major disaster |
| RTO | Maximum recovery time |
| RPO | Maximum acceptable data-loss window |
| Graceful degradation | Reduced functionality instead of total failure |
| Retry | Try failed operation again |
| Timeout | Stop waiting after a limit |
| Circuit breaker | Stop calling a failing service |

### One interview scenario

If the interviewer says:

> Your Payment Service is down. How will you design the system?

You can discuss:

```text
Timeout
   ↓
Retry with exponential backoff + jitter
   ↓
Circuit breaker
   ↓
Fallback / graceful degradation
   ↓
Health checks
   ↓
Redundant payment instances
   ↓
Failover
   ↓
DR for major regional failures
```

That's the kind of chain interviewers expect you to connect rather than explaining each concept in isolation.
