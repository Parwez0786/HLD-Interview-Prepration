# Rate Limiter — HLD Interview

## Interview question

> Design a distributed rate limiter that can limit the number of requests a user/client can make to an API.

A good answer should **not** immediately jump to Redis or Token Bucket. Start by clarifying the requirements.

---

## Table of Contents

- [Step 1 — Clarifying Questions](#step-1--clarifying-questions)
- [Step 2 — Functional Requirements](#step-2--functional-requirements)
- [Step 3 — Non-Functional Requirements](#step-3--non-functional-requirements)
- [Step 4 — High-Level Architecture](#step-4--high-level-architecture)
- [Step 5 — Where Should Rate Limiting Happen?](#step-5--where-should-rate-limiting-happen)
- [Step 6 — Rate Limiting Algorithms](#step-6--rate-limiting-algorithms)
- [Step 7 — Redis Data Model](#step-7--redis-data-model)
- [Step 8 — Request Flow](#step-8--request-flow)
- [Step 9 — The Most Important Problem: Concurrency](#step-9--the-most-important-problem-concurrency)
- [Step 10 — How Do We Solve It?](#step-10--how-do-we-solve-it)
- [Step 11 — Distributed Architecture](#step-11--distributed-architecture)
- [Step 12 — Redis Scaling](#step-12--redis-scaling)
- [Step 13 — What If Redis Goes Down?](#step-13--what-if-redis-goes-down)
- [Step 14 — Response Headers](#step-14--response-headers)
- [Step 15 — Multi-Level Rate Limiting](#step-15--multi-level-rate-limiting)
- [Step 16 — Different Limits for Different Users](#step-16--different-limits-for-different-users)
- [Step 17 — IP-Based Rate Limiting](#step-17--ip-based-rate-limiting)
- [Step 18 — Complete Architecture](#step-18--complete-architecture)
- [Step 19 — Capacity Estimation](#step-19--capacity-estimation)
- [Step 20 — Bottlenecks](#step-20--bottlenecks)
- [Step 21 — Important Interview Trade-offs](#step-21--important-interview-trade-offs)
- [Final Interview Answer](#final-interview-answer)
- [Atomic Lua Script — Deep Dive](#atomic-lua-script--deep-dive)
- [Interview Follow-ups](#rate-limiter--interview-follow-ups)

---

## Step 1 — Clarifying Questions

I would ask the interviewer:

### 1. What are we limiting?

- Per user?
- Per IP?
- Per API?
- Per API key?
- Global?

**Assumption:** we need to limit requests per user / API key.

Example:

- User A → maximum 100 requests/minute
- User B → maximum 100 requests/minute

### 2. Is the limit the same for every API?

Could be:

| API | Limit |
| --- | --- |
| `GET /profile` | 1000 req/min |
| `POST /payment` | 10 req/min |
| `GET /search` | 100 req/min |

**Assumption:** different APIs can have different limits.

### 3. What should happen when the limit is exceeded?

Return `HTTP 429 Too Many Requests`.

```json
{
  "error": "Rate limit exceeded",
  "retry_after": 20
}
```

### 4. Do we need a distributed rate limiter?

**Yes.**

Because our application may have many servers:

```text
              Load Balancer
              /     |     \
             /      |      \
         Server1  Server2  Server3
```

If each server maintains its own counter, a user could bypass the limit by sending requests to different servers.

Therefore: we need **shared distributed state**.

---

## Step 2 — Functional Requirements

Our rate limiter should:

- Accept a request
- Identify the client
- Check whether the client has exceeded its limit
- Allow the request if under the limit
- Reject the request if the limit is exceeded
- Support different rate limits for different APIs / users
- Work across multiple application servers
- Return appropriate rate-limit information

---

## Step 3 — Non-Functional Requirements

### Low latency

Rate limiting happens on **every request**.

So we cannot afford:

```text
Request → Rate limiter → 100 ms DB call → Application
```

Ideally:

```text
Rate limiter latency < 1–5 ms
```

### High availability

If the rate limiter goes down, we don't want the entire application to stop working.

We need to decide whether to **fail-open** or **fail-closed**.

| API type | Redis down | Behavior |
| --- | --- | --- |
| Normal API | Allow request | Fail-open |
| Payment / security-sensitive API | Reject request | Fail-closed |

### Scalability

It should support millions / billions of requests.

### Distributed consistency

Multiple application servers should see approximately the same rate-limit state.

---

## Step 4 — High-Level Architecture

A basic design:

```text
                     Client
                       |
                       v
                 +-----------+
                 |    LB     |
                 +-----------+
                       |
             +---------+---------+
             |         |         |
             v         v         v
        +---------+ +---------+ +---------+
        | Server1 | | Server2 | | Server3 |
        +---------+ +---------+ +---------+
             |         |         |
             +---------+---------+
                       |
                       v
              +------------------+
              |   Rate Limiter   |
              +------------------+
                       |
                       v
                 +-----------+
                 |   Redis   |
                 +-----------+
                       |
                       v
                  Application
```

In a real system, I would generally put rate limiting **before** the application servers.

```text
                         Client
                           |
                           v
                    +-------------+
                    | API Gateway |
                    +-------------+
                           |
                    Rate Limiter
                           |
             +-------------+-------------+
             |             |             |
             v             v             v
         Server 1      Server 2      Server 3
             |             |             |
             +-------------+-------------+
                           |
                           v
                       Services
```

This is cleaner because rejected requests don't consume application resources.

---

## Step 5 — Where Should Rate Limiting Happen?

There are several possibilities.

### Option 1 — Application code

```text
Client
  |
  v
Application
  |
  v
Rate limiter
```

**Problem:** every application service needs rate-limiting logic.

### Option 2 — API Gateway

```text
Client
   |
   v
API Gateway
   |
   +---- Rate Limiter
   |
   v
Application
```

**Better.** Centralized and reusable.

### Option 3 — Dedicated Rate Limiter Service

```text
Client
   |
   v
API Gateway
   |
   v
Rate Limiter Service
   |
   v
Application
```

Useful when rate limiting becomes a large platform capability.

**For our design, I'd choose:** API Gateway + distributed Redis-based rate limiter.

---

## Step 6 — Rate Limiting Algorithms

This is an important interview discussion.

### 1. Fixed Window

Suppose: **100 requests / minute**

We maintain:

```text
user:123
window: 12:00
count: 85
```

At 12:01: `count = 0`

#### Problem

Boundary issue. A user can send:

```text
12:00:59 → 100 requests
12:01:00 → 100 requests
```

Potentially **200 requests** within a very short period.

### 2. Sliding Window

Instead of fixed buckets, look at requests within the last N seconds.

Example:

- Limit = 100 requests
- Window = 60 seconds

At `12:00:45`, we check requests between `11:59:45 → 12:00:45`.

More accurate, but requires more state.

### 3. Token Bucket

This is one of the most commonly discussed algorithms in interviews.

Imagine a bucket:

- Capacity = 100 tokens
- Tokens are added at a fixed rate: 10 tokens/sec
- Each request consumes 1 token

```text
Bucket
+----------------+
| ● ● ● ● ● ● ● |
| ● ● ● ● ● ● ● |
+----------------+
       80 tokens
```

Request arrives:

```text
Token available?
      |
   YES → consume token → allow
      |
   NO  → reject
```

Token Bucket has an important advantage: it allows **controlled bursts** while maintaining an **average rate**.

**For our design, I would choose Token Bucket.**

---

## Step 7 — Redis Data Model

Suppose:

- User = 123
- API = `/search`
- Limit = 100 requests/minute

We could maintain:

```text
rate_limit:user:123:/search
```

with information such as:

- `tokens`
- `last_refill_timestamp`

Conceptually:

```text
Key:
rate_limit:user123:search

Value:
tokens = 73
last_refill = 169....
```

TTL can be used so inactive users don't occupy memory forever.

---

## Step 8 — Request Flow

Let's walk through one request.

```http
GET /search
Authorization: user123
```

| Step | What happens |
| --- | --- |
| 1 | API Gateway extracts `userId = 123` and `API = /search` |
| 2 | Gateway asks the rate limiter: can `user123` call `/search`? |
| 3 | Rate limiter checks Redis: `tokens = 73` |
| 4 | One token is consumed: `tokens = 72` |
| 5 | Return `ALLOW` |
| 6 | Request goes to backend |

```text
Client
  |
  v
API Gateway
  |
  v
Rate Limiter
  |
  v
Redis
  |
  v
ALLOW
  |
  v
Backend
```

---

## Step 9 — The Most Important Problem: Concurrency

This is where an interviewer may go deeper.

Suppose only one token is left.

Two requests arrive simultaneously: Request A and Request B.

Both read `tokens = 1`. Both think they can proceed.

Then:

- A → consume token
- B → consume token

Now we allowed **2 requests** even though only **1 token** existed.

This is a **race condition**.

---

## Step 10 — How Do We Solve It?

We need the check + update to be **atomic**.

With Redis, we can use a **Lua script**.

Conceptually:

1. Read current token count
2. Calculate token refill
3. Check whether a token exists
4. Decrease token
5. Save updated state

All inside **one atomic Redis operation**.

```text
Request A ──┐
            ├──> Redis atomic operation
Request B ──┘
```

Only one request gets the final token.

---

## Step 11 — Distributed Architecture

Now imagine:

```text
              Load Balancer
             /      |      \
            /       |       \
           v        v        v
       Gateway1  Gateway2  Gateway3
           \        |        /
            \       |       /
             +------+------+
                    |
                 Redis
```

All gateways access the same rate-limit state.

```text
Gateway1 → user123 → Redis
Gateway2 → user123 → Redis
Gateway3 → user123 → Redis
```

They all share the same bucket.

---

## Step 12 — Redis Scaling

What if Redis becomes a bottleneck?

We can use **Redis Cluster**.

```text
                  Redis Cluster

             +------------------+
             |     Shard 1      |
             | users 1–1M       |
             +------------------+

             +------------------+
             |     Shard 2      |
             | users 1M–2M      |
             +------------------+

             +------------------+
             |     Shard 3      |
             | users 2M–3M      |
             +------------------+
```

Redis Cluster distributes keys across shards.

Since the key contains the user ID (`rate_limit:user123`), the same user's rate-limit state consistently maps to one shard.

---

## Step 13 — What If Redis Goes Down?

Very common interview question. We have two strategies.

### Fail Open

```text
Redis unavailable
       ↓
Allow request
```

| | |
| --- | --- |
| **Pros** | Application remains available |
| **Cons** | Users can exceed limits |

### Fail Closed

```text
Redis unavailable
       ↓
Reject request
```

| | |
| --- | --- |
| **Pros** | Strong protection |
| **Cons** | Legitimate users may be blocked |

### My choice

| API type | Strategy |
| --- | --- |
| General APIs | Fail-open, possibly with local emergency protection |
| Payments / security-sensitive | Fail-closed or a stricter fallback |

This is a **business decision**, not purely a technical one.

---

## Step 14 — Response Headers

A good rate limiter should communicate the limit.

When allowed:

```http
HTTP/1.1 200 OK
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 73
X-RateLimit-Reset: 169...
```

When exceeded:

```http
HTTP/1.1 429 Too Many Requests
Retry-After: 20
```

---

## Step 15 — Multi-Level Rate Limiting

Real systems often need multiple limits.

For example:

- Per second: 10 requests
- Per minute: 100 requests
- Per hour: 1000 requests

A request must pass all three:

```text
                 Request
                    |
          +---------+---------+
          |         |         |
          v         v         v
       10/sec    100/min   1000/hour
          |         |         |
          +---------+---------+
                    |
                 ALLOW
```

If any one fails: `429 Too Many Requests`.

---

## Step 16 — Different Limits for Different Users

We might have:

| Plan | Limit |
| --- | --- |
| Free | 100 req/min |
| Premium | 1000 req/min |
| Enterprise | 10000 req/min |

We can store configuration separately:

```text
User
  |
  v
Plan
  |
  v
Rate Limit Configuration
```

Configuration could be cached locally or in Redis.

---

## Step 17 — IP-Based Rate Limiting

Sometimes the user isn't authenticated.

For example: `POST /login`

We can rate-limit using IP address.

```text
IP: 192.168.1.10
Limit: 20 requests/min
```

But IP-based limiting has a problem. Many users can share the same IP because of:

- NAT
- Corporate networks
- Mobile networks

Therefore, authenticated APIs should preferably use **userId / API key** rather than only IP.

---

## Step 18 — Complete Architecture

The final design:

```text
                         Clients
                            |
                            v
                    +----------------+
                    | Load Balancer  |
                    +----------------+
                            |
                            v
                    +----------------+
                    |  API Gateway   |
                    +----------------+
                            |
                            v
                  +---------------------+
                  |   Rate Limiter      |
                  |   Token Bucket      |
                  +---------------------+
                            |
                            v
                  +---------------------+
                  |    Redis Cluster    |
                  |                     |
                  | Shard 1 | Shard 2   |
                  | Shard 3 | Shard 4   |
                  +---------------------+
                            |
                         ALLOW
                            |
                            v
                  +---------------------+
                  | Application Servers  |
                  +---------------------+
                     /       |       \
                    v        v        v
                  DB       Kafka    Other Services
```

---

## Step 19 — Capacity Estimation

Let's say:

- 100 million users
- 10% active users = **10 million active users**

Suppose each active user makes **10 requests/min**.

```text
10M × 10 = 100M requests/min

100M / 60 ≈ 1.67M requests/sec
```

So our rate limiter needs to support roughly **1.7 million rate-limit checks/sec**.

This is why these become important:

- Redis
- Redis Cluster
- Atomic Lua operations
- Efficient key design
- Horizontal scaling

---

## Step 20 — Bottlenecks

### Bottleneck 1: Redis

**Solution:** Redis Cluster + replication + sharding.

### Bottleneck 2: Network latency

Every request requires a Redis call.

**Solution:**

- Keep Redis geographically close
- Use persistent connections
- Connection pooling
- Avoid unnecessary network calls

### Bottleneck 3: Hot users

Suppose one API key generates **100K requests/sec**.

That particular Redis key becomes extremely hot.

Possible solutions:

- Local pre-checking
- Hierarchical rate limiting
- Distribute load carefully
- Dedicated handling for extremely high-volume clients

---

## Step 21 — Important Interview Trade-offs

| Approach | Advantage | Problem |
| --- | --- | --- |
| Fixed Window | Simple | Boundary problem |
| Sliding Window | Accurate | More memory |
| Token Bucket | Efficient + burst support | Slightly more complex |
| Redis | Fast shared state | Operational dependency |
| Local memory | Extremely fast | Doesn't work well across servers |
| Fail Open | High availability | Limits can be bypassed |
| Fail Closed | Strong protection | Can reduce availability |

---

## Final Interview Answer

If the interviewer says:

> Give me your final design.

You can say:

> I would design a distributed rate limiter at the API Gateway layer using the Token Bucket algorithm. The gateway identifies the user using an API key or user ID and checks a shared Redis Cluster for the user's bucket state. The token check and update would be performed atomically using a Redis Lua script to prevent race conditions when multiple gateway instances process requests concurrently. Redis would be sharded for horizontal scalability, and inactive rate-limit keys would use TTLs. If the bucket has a token, we consume one and forward the request to the backend; otherwise, we return HTTP 429 with a Retry-After header. For normal APIs, I'd prefer fail-open behavior during a Redis outage, while security- or payment-sensitive APIs could use fail-closed behavior. We can also support different limits by API, user tier, IP, and multiple time windows.

### The 5 things you absolutely should remember

1. Token Bucket
2. Redis
3. Atomic Lua script
4. Redis Cluster / sharding
5. API Gateway

That combination is a very strong SDE-1 / SDE-2 HLD interview answer.

---

# Atomic Lua Script — Deep Dive

Atomic Lua script in Redis is one of the most important concepts for a distributed rate limiter.

## 1. First understand the problem

Suppose our rate limiter has:

- User: 123
- Available tokens: 1

Two requests arrive at almost exactly the same time:

```text
Request A ──┐
            ├──> Redis
Request B ──┘
```

We need to perform:

1. Check tokens
2. If token > 0
3. Decrease token by 1
4. Allow request

The problem occurs if these operations are separate.

### Without atomicity

```text
Request A              Redis              Request B

   READ ----------------> 1
                                              READ -------> 1

   "1 token available"

   WRITE ---------------> 0
                                              WRITE -------> 0

   ALLOW                                  ALLOW
```

Both requests were allowed, even though there was only one token.

This is a **race condition**.

## 2. What does "atomic" mean?

Atomic means: the entire operation happens as **one indivisible unit**.

Nobody can execute another Redis command in the middle of our operation.

```text
                Redis

Request A ---> [CHECK + UPDATE] ---> DONE

Request B ------------------------------> waits
```

Then:

- Request A → gets token → `tokens = 0` → ALLOW
- Request B → checks → `tokens = 0` → REJECT

That's exactly what we want.

## 3. Why Lua?

Redis supports executing Lua scripts atomically.

Instead of doing this from our application:

```text
GET
 ↓
check
 ↓
SET
```

we send one Lua script to Redis:

```text
Redis:
    execute this entire script
```

Redis executes the script as a single atomic operation.

## 4. Simple example

Imagine Redis contains `tokens = 5`. We want to consume one token.

A simplified Lua script:

```lua
local tokens = redis.call("GET", KEYS[1])

if tonumber(tokens) > 0 then
    redis.call("DECR", KEYS[1])
    return 1
end

return 0
```

```text
GET tokens
      |
      v
Is tokens > 0?
   /       \
 YES        NO
  |          |
DECR        return 0
  |
return 1
```

- `1` means ALLOW
- `0` means REJECT

## 5. Why is this different from Java code?

Suppose we do this in Java:

```java
Long tokens = redis.get(key);

if (tokens > 0) {
    redis.decrement(key);
    return true;
}

return false;
```

This looks correct, but it's **not atomic**.

Because `redis.get()` and `redis.decrement()` are two separate Redis operations.

Another request can execute between them.

## 6. Lua makes it one operation

```text
Java
  |
  | send Lua script
  v
Redis
  |
  | GET
  | CHECK
  | DECREMENT
  | RETURN
  v
Java
```

Redis executes the entire script sequentially.

So another Redis command doesn't get inserted between:

```text
GET
CHECK
DECREMENT
```

## 7. In Token Bucket specifically

A real token bucket needs more than just tokens.

We usually maintain:

- `tokens`
- `last_refill_time`

Suppose:

- `capacity = 100`
- `refill_rate = 10 tokens/sec`

Redis key `rate_limit:user:123` contains something conceptually like:

```text
tokens = 73
last_refill = 18:00:10
```

A request arrives at `18:00:12`.

The Lua script calculates:

```text
elapsed = 2 seconds

new_tokens = 73 + (2 × 10) = 93
tokens = min(93, capacity) = 93
```

Request consumes one: `tokens = 92`

And updates: `last_refill = 18:00:12`

All of that happens inside **one atomic Lua script**.

## 8. Why not use Redis transactions?

You might hear about `MULTI` / `EXEC`.

Redis transactions can group commands, but for a rate limiter, Lua is often more convenient because the logic itself can happen **inside Redis**:

```text
IF tokens > 0
    decrement
ELSE
    reject
```

You don't have to do `GET → application → decision → SET`.

The decision happens inside Redis.

## 9. Interview-level explanation

If the interviewer asks:

> Why do you need a Lua script?

Say:

> The rate limiter requires a read-modify-write operation. We need to read the current token count, calculate refill, check whether a token is available, decrement it, and update the timestamp. If these operations are separate Redis calls, concurrent requests can observe the same token and both get approved. I use a Redis Lua script so the entire check-and-update operation executes atomically inside Redis, preventing race conditions.

That's a very good interview answer.

### Remember this pattern

**Without Lua:**

```text
GET → Application logic → SET
      ↑
   Race condition
```

**With Lua:**

```text
┌─────────────────────────────┐
│ Redis Lua Script            │
│                             │
│ GET                         │
│ Calculate refill            │
│ Check limit                 │
│ Decrement                   │
│ Update timestamp            │
│                             │
└─────────────────────────────┘
              ↓
         One atomic operation
```

**Key idea:** Lua isn't making Redis "faster" here. It makes the whole rate-limit decision **atomic**, so concurrent requests cannot interfere with each other.

---

# Rate Limiter — Interview Follow-ups

After you give the Token Bucket + Redis + Atomic Lua Script design, an interviewer will likely start drilling deeper. Here is a realistic follow-up sequence.

---

## Follow-up 1. Why did you choose Token Bucket over Fixed Window?

**Answer:**

> Fixed Window is simpler, but it has a boundary problem where a client can send many requests at the end of one window and many more at the beginning of the next window. Token Bucket avoids this issue and also allows controlled bursts while maintaining a fixed average rate.

Example:

- Limit = 100/min
- `12:00:59` → 100 requests
- `12:01:00` → 100 requests

Fixed Window → potentially **200 requests in ~1 second**.

Token Bucket handles this more smoothly.

---

## Follow-up 2. Why Redis?

> The rate limiter is a distributed component, so all application servers need access to the same state. Redis provides very low-latency reads and writes, supports atomic operations and Lua scripts, and can be horizontally scaled using Redis Cluster.

```text
Redis
 ├── Low latency
 ├── Shared state
 ├── Atomic operations
 ├── Lua scripting
 └── Cluster / sharding
```

---

## Follow-up 3. Why can't we store the counter in application memory?

Suppose we have:

```text
             Load Balancer
             /     |     \
            ↓      ↓      ↓
         Server1 Server2 Server3

         counter counter counter
```

User sends 100 requests. They may be distributed:

- Server1 → 30
- Server2 → 35
- Server3 → 35

Each server thinks `35 < 100`.

So the user effectively gets **100+ requests**, depending on distribution.

Therefore:

> Local memory is unsuitable for a globally consistent distributed limit.

---

## Follow-up 4. Why Lua? Can't you just use GET and DECR?

This is a very important follow-up.

You say:

> GET followed by DECR is not atomic.

Example:

```text
Tokens = 1

Request A → GET → 1
Request B → GET → 1

A → DECR → 0
B → DECR → 0
```

Both requests can be accepted.

With Lua:

```text
Request A
   ↓
[GET + CHECK + DECR]
   ↓
DONE

Request B
   ↓
[GET + CHECK + DECR]
   ↓
REJECT
```

The entire operation is atomic.

---

## Follow-up 5. What exactly would your Lua script do?

For a simplified Token Bucket:

**Input:**

- `capacity`
- `refillRate`
- `currentTime`
- `requestedTokens`

**Redis stores:**

- `tokens`
- `lastRefillTime`

**The script:**

1. Read `tokens`
2. Read `lastRefillTime`
3. Calculate elapsed time
4. Calculate `tokensToAdd = elapsedTime × refillRate`
5. `tokens = min(capacity, tokens + tokensToAdd)`
6. If `tokens >= requestedTokens`: subtract and ALLOW. Else REJECT.
7. Save tokens
8. Save current timestamp

All inside **one Lua execution**.

---

## Follow-up 6. What if Redis goes down?

Interviewer:

> Your Redis is unavailable. What happens?

Don't immediately say "reject everything."

Say:

> It depends on the API. For normal APIs, I would prefer fail-open to preserve availability, potentially with a local emergency limiter. For security-sensitive or payment APIs, I would prefer fail-closed because allowing unlimited requests could be dangerous.

| API type | Redis down |
| --- | --- |
| Normal API | ALLOW |
| Payment / security API | REJECT |

This shows you understand availability vs protection trade-offs.

---

## Follow-up 7. What if Redis becomes a bottleneck?

**Answer:**

> I would use Redis Cluster to shard rate-limit keys across multiple Redis nodes. Since the key can contain the user ID, different users will naturally distribute across shards.

Example:

| Key | Shard |
| --- | --- |
| `user:100` | Shard 1 |
| `user:200` | Shard 2 |
| `user:300` | Shard 3 |
| `user:400` | Shard 1 |

---

## Follow-up 8. What if one user generates 1 million requests/sec?

Now you have a **hot key** problem.

Suppose `rate_limit:user:123` receives 1M requests/sec.

All operations target the same Redis key. Even with Redis Cluster, that key generally belongs to one shard.

You can say:

> Redis Cluster handles distribution across users, but a single extremely hot key can still become a bottleneck. For such clients, I would consider a hierarchical limiter: a local limiter at the gateway for fast rejection combined with a global distributed limiter.

```text
Client
  ↓
Gateway
  ↓
Local limiter
  ↓
Global Redis limiter
  ↓
Backend
```

---

## Follow-up 9. What if there are 1 billion users?

Interviewer:

> Are you storing one Redis object for every user?

**Answer:**

> I would create rate-limit state lazily only when a user actually makes a request, and assign a TTL to inactive keys. This prevents inactive users from consuming Redis memory indefinitely.

```text
1B registered users
        ↓
Only active users
        ↓
Redis state
```

---

## Follow-up 10. How do you handle different limits?

Interviewer:

> Free users have 100 requests/minute and premium users have 10,000. How do you support this?

```text
User
 ↓
Plan
 ↓
Rate Limit Configuration
```

| Plan | Limit |
| --- | --- |
| Free | 100/min |
| Premium | 10,000/min |
| Enterprise | 100,000/min |

The gateway can retrieve the user's plan and pass the appropriate configuration to the limiter.

---

## Follow-up 11. Can you rate-limit different APIs differently?

Yes.

Use a key such as:

```text
rate_limit:{userId}:{api}
```

For example:

```text
rate_limit:123:/search
rate_limit:123:/payment
rate_limit:123:/profile
```

Then:

| API | Limit |
| --- | --- |
| `/search` | 100/min |
| `/payment` | 10/min |
| `/profile` | 500/min |

---

## Follow-up 12. How would you rate-limit unauthenticated users?

Use IP address:

```text
rate_limit:ip:103.20.10.5
```

But mention the limitation:

> IP-based limiting can affect multiple users behind NAT, corporate networks, or mobile carriers. For authenticated requests, user ID or API key is preferable.

---

## Follow-up 13. What happens when the limit is exceeded?

Return `HTTP 429 Too Many Requests`.

And preferably:

```http
Retry-After: 10
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 0
X-RateLimit-Reset: ...
```

---

## Follow-up 14. What if requests come from multiple API gateways?

This is exactly why we need shared state.

```text
                Load Balancer
                 /    |    \
                ↓     ↓     ↓
              GW1   GW2   GW3
                \     |    /
                 \    |   /
                  Redis
```

All gateways access the same bucket.

```text
GW1 → user123 → Redis
GW2 → user123 → Redis
GW3 → user123 → Redis
```

The limit remains globally enforced.

---

## Follow-up 15. What if two requests arrive simultaneously?

This is where you bring up atomicity.

Say: `tokens = 1`

```text
A ───────┐
         ├── Redis Lua Script
B ───────┘
```

Redis executes one script first:

- A → token available → consume → ALLOW
- Then B → token unavailable → REJECT

Therefore:

> The Lua script prevents the check-and-update race condition.

---

## Follow-up 16. What about Redis replication?

Interviewer:

> What happens if the Redis primary dies?

```text
             Redis Cluster

             Primary
                |
          +-----+-----+
          ↓           ↓
       Replica      Replica
```

Use replication and automatic failover.

But there's a trade-off: during failover, there can be a small window where rate-limit state is not perfectly consistent depending on replication guarantees.

For rate limiting, **slight inconsistency is often acceptable** compared with the latency and complexity of strong distributed consistency.

That's a very good HLD point.

---

## Follow-up 17. Is rate limiting strongly consistent?

Usually **no**. We generally don't need strict global consistency for rate limiting.

For example, if the limit is 100 requests/min and due to a tiny race / failover window we allow 101 requests, that's usually acceptable.

But for bank balance, payment, or inventory, you need much stronger consistency.

This demonstrates that you understand **business requirements drive architecture**.

---

## Follow-up 18. What if the client intentionally sends requests from many IPs?

If authenticated, **userId / API key** should be the primary identity.

You can also combine limits:

- User limit
- IP limit
- API limit
- Global limit

For example:

| Scope | Limit |
| --- | --- |
| User | 1000/min |
| IP | 100/min |
| Payment API | 10/min |

A request must pass **all** relevant limits.

---

## Follow-up 19. Where exactly should the rate limiter sit?

A strong answer:

```text
Client
   ↓
Load Balancer
   ↓
API Gateway
   ↓
Rate Limiter
   ↓
Backend Services
```

Why?

> We want to reject excessive traffic as early as possible so that invalid requests don't consume backend CPU, database connections, thread pools, or downstream resources.

---

## Follow-up 20. Final "Deep Dive" Question

The interviewer may ask:

> Walk me through exactly what happens when 10,000 requests from the same user arrive simultaneously.

Your answer:

```text
10,000 requests
       ↓
Load Balancer
       ↓
Multiple API Gateways
       ↓
Rate Limiter
       ↓
Redis
       ↓
Atomic Lua execution
       ↓
Only requests with available tokens
       ↓
ALLOW → Backend

No tokens
       ↓
429
```

The key point is: every request does **not** independently read and then update the bucket. The entire token calculation + decision + update happens **atomically in Redis**.

---

## Interviewer's likely follow-up chain

If you're practicing this as a real interview, memorize this sequence:

```text
Design Rate Limiter
       ↓
Why Token Bucket?
       ↓
Why Redis?
       ↓
Why not local memory?
       ↓
Why Lua?
       ↓
What does Lua do?
       ↓
Race condition?
       ↓
Redis failure?
       ↓
Redis bottleneck?
       ↓
Hot key?
       ↓
Redis Cluster?
       ↓
1B users?
       ↓
Different plans?
       ↓
Different APIs?
       ↓
IP vs user?
       ↓
429 response?
       ↓
Strong consistency?
       ↓
Final architecture
```

If you can confidently answer this chain, you can handle a Rate Limiter HLD interview round at a much deeper level.
