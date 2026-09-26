These are Availability & Reliability fundamentals for HLD. I'll explain them in simple interview language, with examples and common follow-up questions.

1. Availability

Availability = How often the system is up and usable.

Example:

If a payment service is available 99.99% of the year, users should be able to use it almost all the time.

Availability formula
$$ Availability = \frac{Uptime}{Uptime + Downtime} $$

Common availability levels:

Availability	Approx. downtime/year
99%	3.65 days
99.9%	8.76 hours
99.99%	52.6 minutes
99.999%	5.26 minutes
Interview answer

Availability means the percentage of time a system remains accessible and operational.

2. Reliability

Reliability = Ability of the system to keep working correctly over time.

Availability asks:

"Is the system up?"

Reliability asks:

"Does the system continue to work correctly without failures?"

Example:

A service may be available but occasionally process a payment twice.

It is available, but not sufficiently reliable.

Simple difference
Availability → Is it running?
Reliability  → Is it working correctly and consistently?
3. SLA, SLO, SLI

These three are commonly asked together.

SLI — Service Level Indicator

Actual measurement.

Examples:

99.95% successful requests
200 ms average latency
99.99% availability

It tells us what actually happened.

SLO — Service Level Objective

Target we want to achieve.

Example:

SLO:
99.99% availability
p99 latency < 300 ms
SLA — Service Level Agreement

Contractual agreement with the customer.

Example:

"Our service will provide 99.9% availability. If we fail to meet it, customers may receive service credits."

Easy way to remember
SLI → Measurement
SLO → Target
SLA → Contract
4. Fault Tolerance

Fault tolerance = System continues working even when some components fail.

Example:

           Load Balancer
              /     \
             /       \
        Server 1   Server 2
           ❌          ✅

Server 1 crashes.

Server 2 continues serving requests.

The system is fault tolerant.

5. Redundancy

Redundancy = Having extra components so that failure of one doesn't stop the system.

Example:

Primary DB
     +
Replica DB

If primary fails, replica can take over.

Redundancy can exist at:

Server level
Database level
Network level
Data-center level
Region level
6. Failover

Failover = Switching from a failed component to a backup component.

Example:

Primary DB
    ↓
  CRASH ❌
    ↓
Replica DB
    ↓
Become Primary

The actual switching process is failover.

Redundancy vs Failover
Redundancy → Backup exists

Failover   → We switch to the backup
7. Active-Active

In active-active, multiple instances actively serve traffic.

             Load Balancer
             /           \
            ↓             ↓
        Server A       Server B
        ACTIVE          ACTIVE

Traffic can go to both.

Advantages
Better resource utilization
Higher availability
Easy horizontal scaling
Problem

Data consistency can become complicated if both sides modify the same data.

8. Active-Passive

Only one instance actively handles traffic.

Primary
ACTIVE
   |
   | failure
   ↓
Backup
PASSIVE

The backup waits until the primary fails.

Advantages
Simpler
Easier data management
Disadvantages
Backup resources may be underutilized
Failover may take some time
9. Health Checks

A health check determines whether a service is healthy.

Example:

GET /health

Response:

{
  "status": "UP"
}

Load balancer might check every 5 seconds.

LB
 |
 +-- Server A → Healthy ✅
 |
 +-- Server B → Unhealthy ❌

Traffic is removed from Server B.

Types

Liveness

Is the application running?

Readiness

Is the application ready to receive traffic?

This distinction is especially important in Kubernetes.

10. Heartbeats

A heartbeat is a periodic signal saying:

"I'm alive."

Example:

Server A → ❤️ → Server B
Server A → ❤️ → Server B
Server A → ❤️ → Server B

If Server B doesn't receive heartbeats for some period:

Heartbeat timeout
       ↓
Assume A failed
       ↓
Start failover
Health check vs heartbeat
Health check → actively asks "Are you healthy?"

Heartbeat   → component periodically says "I'm alive."
11. Disaster Recovery

Disaster Recovery (DR) = Strategy for recovering the system after a major failure.

Examples:

Data center failure
Region failure
Major database corruption
Ransomware/security incident
Large infrastructure outage

Example:

Primary Region
Mumbai
   |
   | replication
   ↓
DR Region
Hyderabad

If Mumbai becomes unavailable, traffic can potentially move to Hyderabad.

12. RTO — Recovery Time Objective

RTO = Maximum acceptable time to recover the system.

Example:

RTO = 30 minutes

If the system goes down at:

10:00 AM

It should be recovered by:

10:30 AM
13. RPO — Recovery Point Objective

RPO = Maximum acceptable amount of data loss measured in time.

Example:

RPO = 5 minutes

If failure occurs at:

10:00 AM

You may accept losing data from approximately:

9:55 → 10:00
RTO vs RPO

Very important interview question:

RTO → How quickly must we recover?

RPO → How much data can we afford to lose?
14. Graceful Degradation

Graceful degradation = When something fails, provide reduced functionality instead of completely failing.

Example: E-commerce website.

Suppose recommendation service fails:

Recommendation Service ❌
          ↓
Homepage still works ✅
          ↓
"Recommended for you" section hidden

Instead of:

Entire website ❌

This improves availability and user experience.

15. Retry

If a temporary failure occurs, the client tries again.

Request
  ↓
Server ❌
  ↓
Retry
  ↓
Server ✅

Example:

Attempt 1 → timeout
Attempt 2 → timeout
Attempt 3 → success
Important

Don't retry indefinitely.

Usually use:

Exponential backoff

Retry 1 → 100 ms
Retry 2 → 200 ms
Retry 3 → 400 ms
Retry 4 → 800 ms

Also add jitter so thousands of clients don't retry simultaneously.

16. Timeout

A timeout specifies:

"How long should I wait for this operation?"

Example:

Service A → Service B

Timeout = 2 seconds

If B doesn't respond within 2 seconds:

Request cancelled ❌

Without timeouts, requests can remain stuck and consume threads/connections.

Interview point

Every network call should have a reasonable timeout.

17. Circuit Breaker

Circuit breaker protects your system from repeatedly calling a failing service.

Imagine:

Order Service
     ↓
Payment Service ❌

Payment service is down.

Without circuit breaker:

Request → Payment ❌
Request → Payment ❌
Request → Payment ❌
Request → Payment ❌
...

This can overload the already-failing service and waste resources.

With circuit breaker:

Order Service
     ↓
Circuit Breaker
     ↓
Payment ❌

After repeated failures:

CLOSED
  ↓
many failures
  ↓
OPEN
  ↓
don't call Payment

After some time:

OPEN
 ↓
test request
 ↓
HALF-OPEN
 ↓
success → CLOSED
Three states
CLOSED
  ↓ failures
OPEN
  ↓ wait
HALF-OPEN
  ↓
success → CLOSED
failure → OPEN
How These Concepts Connect

This is the important HLD picture:

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

Health checks → detect failures
Heartbeats    → detect liveness
Failover      → switch to backup
Retry         → handle temporary failures
Timeout       → prevent requests hanging
Circuit breaker → stop calling failing services
Graceful degradation → keep partial functionality

For major disasters:

             Primary Region
                   |
                Replication
                   ↓
                DR Region

RTO → How fast can we recover?
RPO → How much data can we lose?
⭐ Interview Cheat Sheet
Concept	Simple meaning
Availability	System is up
Reliability	System works correctly over time
SLI	Actual measurement
SLO	Target
SLA	Customer contract
Fault tolerance	Keeps working despite failures
Redundancy	Extra/backup components
Failover	Switch to backup
Active-active	Both instances serve traffic
Active-passive	One serves, one waits
Health check	Check whether service is healthy
Heartbeat	Periodic "I'm alive" signal
DR	Recovery from major disaster
RTO	Maximum recovery time
RPO	Maximum acceptable data-loss window
Graceful degradation	Reduced functionality instead of total failure
Retry	Try failed operation again
Timeout	Stop waiting after a limit
Circuit breaker	Stop calling a failing service
One interview scenario

If the interviewer says:

"Your Payment Service is down. How will you design the system?"

You can discuss:

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

That's the kind of chain interviewers expect you to connect rather than explaining each concept in isolation.