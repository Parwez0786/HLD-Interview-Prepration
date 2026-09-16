# Cache Penetration

Cache penetration happens when requests repeatedly ask for data that **does not exist**.

The important problem is that because the data doesn't exist, the request cannot get a useful cache hit, so it keeps reaching the database.

---

## Table of Contents

- [Example](#example)
- [Why Is This a Problem?](#why-is-this-a-problem)
- [Solution: Negative Caching](#solution-negative-caching)
- [Why Do We Need TTL?](#why-do-we-need-ttl)
- [Another Solution: Bloom Filter](#another-important-solution-bloom-filter)
- [Cache Penetration vs Cache Stampede](#cache-penetration-vs-cache-stampede)
- [Interview One-Liner](#interview-one-liner)

---

## Example

Suppose our application has `GET /users/999999999`, but user 999999999 does not exist.

```text
Request
   |
   v
Redis
   |
   | MISS
   v
Database
   |
   | User doesn't exist
   v
NOT FOUND
```

Now imagine an attacker or buggy client sends this request 10,000 times:

```text
10,000 requests
       |
       v
     Redis
       |
       | MISS every time
       v
   Database
       |
       v
   NOT FOUND
```

Redis isn't protecting the database because there is nothing to cache.

---

## Why Is This a Problem?

Normally, caching reduces database traffic: Request → Redis HIT → Response.

But with cache penetration: Request → Redis MISS → Database NOT FOUND → Response.

If millions of invalid requests arrive, the database can become overloaded.

This is especially dangerous if the API is publicly accessible.

---

## Solution: Negative Caching

Negative caching means caching the fact that data **does not exist**.

For example: `user:999999999 → NOT_FOUND`, TTL = 60 seconds.

First request:

```text
Request
   |
   v
Redis
   |
   | MISS
   v
Database
   |
   | NOT FOUND
   v
Redis
   |
   | Store NOT_FOUND
   v
Response: 404
```

The next request: Redis HIT → NOT_FOUND → Response: 404. Database is not called.

So instead of 10,000 Redis misses / 10,000 DB queries, we can have approximately 1 DB query and 9,999 Redis responses during that negative-cache TTL.

---

## Why Do We Need TTL?

We should not permanently cache NOT_FOUND.

Imagine: User 999999999 doesn't exist → cache NOT_FOUND. Later the user gets created. But Redis still says NOT_FOUND. The application could incorrectly return 404.

Therefore, use a short TTL: 30–60 seconds.

After expiration: Redis MISS → DB → User exists.

---

## Another Important Solution: Bloom Filter

For very large systems, we can also use a Bloom Filter.

```text
Request
   |
   v
Bloom Filter
   |
   | Definitely doesn't exist
   |---------------------> 404
   |
   | Might exist
   v
Redis
   |
   v
Database
```

A Bloom Filter is useful for quickly checking whether a key **definitely does not exist**.

Remember: Bloom Filter can have **false positives**, but not **false negatives**.

- "Definitely doesn't exist" → don't query DB
- "Might exist" → still check Redis/DB

---

## Cache Penetration vs Cache Stampede

These two are easy to confuse.

**Cache Penetration** — the requested data doesn't exist.

```text
Invalid ID
   ↓
Redis MISS
   ↓
DB
   ↓
NOT FOUND
```

Common solution: Negative caching / Bloom Filter.

**Cache Stampede** — the data **does exist**, but its cache entry expires and many requests simultaneously hit the DB.

```text
Cache expires
     ↓
10,000 requests
     ↓
10,000 DB queries
```

Common solutions: Distributed lock, request coalescing, randomized TTL, background refresh.

---

## Interview One-Liner

> Cache penetration occurs when requests repeatedly query non-existent data, causing cache misses and unnecessary database queries. We can handle it using negative caching with a short TTL, and for large-scale systems we can also use a Bloom Filter.
