# Day 16 — Cache Stampede

Cache stampede is one of the most important problems to understand when using Redis as a cache.

---

## Table of Contents

- [1. What Is Cache Stampede?](#1-what-is-cache-stampede)
- [2. What Happens When the Cache Expires?](#2-what-happens-when-the-cache-expires)
- [3. Why Is It Dangerous?](#3-why-is-it-dangerous)
- [4. Solution 1 — Distributed Lock](#4-solution-1--distributed-lock)
- [5. Solution 2 — Request Coalescing](#5-solution-2--request-coalescing)
- [6. Solution 3 — Randomized TTL](#6-solution-3--randomized-ttl)
- [7. Solution 4 — Background Refresh](#7-solution-4--background-refresh)
- [8. Stale-While-Revalidate](#8-another-important-solution--stale-while-revalidate)
- [9. Complete Architecture](#9-complete-architecture)
- [10. Interview Example](#10-interview-example)
- [11. Remember This](#11-remember-this)

---

## 1. What Is Cache Stampede?

A cache stampede happens when a **popular cache entry expires**, and a large number of requests try to fetch the same data at almost the same time.

**Normal situation**

```text
Request
   |
   v
 Redis
   |
   | HIT
   v
Response
```

Redis handles the request, so the database is protected.

---

## 2. What Happens When the Cache Expires?

Suppose we have `product:101`, TTL = 60 seconds.

After 60 seconds: `product:101` → expired.

Now imagine this product is very popular. Suddenly:

```text
10,000 requests
       |
       v
     Redis
       |
       | MISS
       v
   Database
```

All 10,000 requests see a cache miss.

Without protection, they may all execute `SELECT * FROM product WHERE id = 101;`.

So instead of 10,000 requests → Redis, we effectively get 10,000 requests → Database.

The database can become overloaded.

This is called a **cache stampede** or **thundering herd** problem.

---

## 3. Why Is It Dangerous?

Normally: 1,000,000 requests → Redis, only a few misses → Database.

But during a stampede: 10,000 requests → Redis MISS → Database (10,000 queries).

The database suddenly receives a huge amount of traffic.

This can cause: high database CPU, connection pool exhaustion, increased latency, request timeouts, cascading failures.

---

## 4. Solution 1 — Distributed Lock

The idea is: when the cache misses, **only one request** should query the database and rebuild the cache.

```text
10,000 requests
      |
      v
    Redis
      |
      | MISS
      v
 Distributed Lock
      |
      +---- Request 1 → gets lock
      |
      +---- Request 2 → waits
      +---- Request 3 → waits
      +---- ...
      +---- Request 10000 → waits
```

Request 1 queries the database, writes Redis, then releases the lock. Now other requests can read Redis → HIT.

Redis example: `SET lock:product:101 1 NX EX 5`

`NX` means create the key only if it doesn't already exist. So only one application instance gets the lock.

**Important:** The lock should have a TTL.

Without TTL: if the application crashes, lock remains forever → nobody can rebuild cache.

With TTL: lock expires → another request can acquire it.

---

## 5. Solution 2 — Request Coalescing

Request coalescing means: multiple requests for the same missing data are combined into a **single database request**.

```text
Request 1 ─┐
Request 2 ─┤
Request 3 ─┤
Request 4 ─┼──→ Single DB request
Request 5 ─┤
   ...     │
Request 10k┘
```

Instead of 10,000 DB queries, we make 1 DB query. The result is then shared with all waiting requests.

**Difference from distributed lock**

| Distributed lock | Request coalescing |
| --- | --- |
| One request gets the lock; others wait/retry | Multiple identical requests → one in-flight operation → result shared |

In a single application instance, request coalescing can often be implemented using in-memory mechanisms such as a future/promise map. Across multiple application instances, you need a distributed coordination mechanism.

---

## 6. Solution 3 — Randomized TTL

Another problem is that many cache entries might expire at exactly the same time.

For example all products TTL 60 sec → after 60 seconds A, B, C, D all expire.

Instead, add some randomness:

```text
Product A → 63 sec
Product B → 58 sec
Product C → 67 sec
Product D → 61 sec
```

Example: `TTL = 60 + random(0, 10)` so actual TTL could be 60, 63, 67, 69 sec.

This reduces synchronized expiration.

**Important:** Randomized TTL helps reduce the probability of a stampede, but it does not completely solve a stampede for an extremely hot key.

---

## 7. Solution 4 — Background Refresh

Instead of waiting until the cache expires, we refresh the cache **before** it expires.

Suppose TTL = 60 seconds. At around 50 seconds, the application can refresh the value:

```text
Redis
  |
  | still serving old data
  |
  +---- Background refresh
             |
             v
          Database
             |
             v
           Redis
```

Users continue receiving the cached value while the background process refreshes it.

**Common approach:** Soft TTL + Hard TTL.

Example: Soft TTL = 50 sec, Hard TTL = 60 sec.

At 50 seconds: start background refresh. At 60 seconds: force expiration.

This is particularly useful for very hot data.

---

## 8. Another Important Solution — Stale-While-Revalidate

Serve slightly stale data while refreshing it in the background.

```text
Request
   |
   v
 Redis
   |
   | stale but acceptable
   v
Return old data
   |
   +---- Background refresh
              |
              v
           Database
              |
              v
            Redis
```

This gives very low latency because the user doesn't have to wait for the database.

Useful when slightly outdated data is acceptable: product catalog, homepage recommendations, popular articles.

It may not be appropriate when data must be immediately consistent, such as some financial or inventory operations.

---

## 9. Complete Architecture

A robust cache-read flow could look like:

```text
                  Request
                     |
                     v
                   Redis
                     |
             +-------+-------+
             |               |
            HIT             MISS
             |               |
             v               v
          Response      Acquire Lock
                             |
                      +------+------+
                      |             |
                    Got lock      Lock busy
                      |             |
                      v             v
                  Database      Wait/Retry
                      |             |
                      v             |
                 Update Redis <-----+
                      |
                      v
                   Response
```

Additionally: Randomized TTL + Background Refresh + Distributed Lock can be combined depending on the workload.

---

## 10. Interview Example

**Interviewer:** What happens if a popular Redis key expires and 10,000 requests arrive simultaneously?

> This can cause a cache stampede. All requests see a cache miss and may query the database simultaneously, potentially overloading it. I can prevent this using a distributed lock so that only one request rebuilds the cache while others wait or retry. For hot keys, I can also use randomized TTLs and background refresh to reduce synchronized expiration.

That's a strong SDE-1 level answer.

---

## 11. Remember This

```text
Cache Stampede
      |
      v
Popular key expires
      |
      v
Many simultaneous cache misses
      |
      v
Many DB queries
      |
      v
Database overload
```

| Solution | Basic idea |
| --- | --- |
| Distributed Lock | Only one request rebuilds cache |
| Request Coalescing | Combine identical requests |
| Randomized TTL | Spread expirations |
| Background Refresh | Refresh before expiration |
| Stale-While-Revalidate | Serve old data while refreshing |

### One-line memory trick

> Stampede = cache expires → everyone goes to DB at once. Lock = one goes to DB. Coalescing = combine requests. Random TTL = don't expire together. Background refresh = refresh before expiry.
