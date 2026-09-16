# Day 14 — Caching Fundamentals

Caching is one of the most important uses of Redis in backend systems.

The basic idea is: **store frequently accessed data in a fast storage layer so we don't have to query the database every time.**

---

## Table of Contents

- [1. Without Cache](#1-without-cache)
- [2. Problem With Direct Database Access](#2-problem-with-direct-database-access)
- [3. What Is a Cache?](#3-what-is-a-cache)
- [4. Cache Hit](#4-cache-hit)
- [5. Cache Miss](#5-cache-miss)
- [6. Cache-Aside](#6-this-pattern-is-called-cache-aside)
- [7. Real Example](#7-real-example)
- [8. Next Request](#8-next-request)
- [9. Why Is Redis Faster Than a Database?](#9-why-is-redis-faster-than-a-database)
- [10. Cache Hit Ratio](#10-cache-hit-ratio)
- [11. What Should We Cache?](#11-what-should-we-cache)
- [12. What Should We Not Blindly Cache?](#12-what-should-we-not-blindly-cache)
- [13. The Big Problem: Stale Data](#13-the-big-problem-stale-data)
- [14. TTL Helps](#14-ttl-helps)
- [15. Complete Interview Explanation](#15-complete-interview-explanation)
- [16. Remember This Flow](#16-remember-this-flow)

---

## 1. Without Cache

Suppose we have an API `GET /users/101`.

Without caching:

```text
Client
   |
   v
Application
   |
   v
Database
   |
   v
Application
   |
   v
Client
```

Every request goes to the database.

Example: User requests `GET /users/101`. Application executes `SELECT * FROM users WHERE id = 101;`. Database returns id = 101, name = Parwez, age = 25. Then application sends the response.

---

## 2. Problem With Direct Database Access

Imagine 10,000 requests → Database.

If many requests are asking for the same data, we're repeatedly querying the database: `GET /users/101` thousands of times.

This can cause: high database CPU, more database connections, higher latency, increased database load, poor performance during traffic spikes.

This is where caching helps.

---

## 3. What Is a Cache?

A cache is a **temporary, fast storage layer** that stores data that is likely to be requested again.

```text
Application
     |
     v
   Redis
     |
     v
 Database
```

Redis is commonly used as a cache because it stores data primarily in RAM, making reads very fast.

---

## 4. Cache Hit

A cache hit means the requested data is already present in the cache.

```text
Client
   |
   v
Application
   |
   v
Redis
   |
   | HIT
   v
Response
```

Example: Redis contains `user:101 → { "id": 101, "name": "Parwez", "age": 25 }`.

User requests `GET /users/101`. Application checks Redis: `GET user:101`. Redis returns the data.

So: Application → Redis → Response. The database isn't contacted.

---

## 5. Cache Miss

A cache miss means the requested data is not present in Redis.

```text
Client
   |
   v
Application
   |
   v
Redis
   |
   | MISS
   v
Database
   |
   v
Application
   |
   v
Redis
   |
   v
Client
```

Example: `GET user:101` → Redis `(nil)`. This is a cache miss.

Now application queries the database, stores `SET user:101 {...} EX 300`, and returns it to the user.

---

## 6. This Pattern Is Called Cache-Aside

The flow we just discussed is commonly called the **Cache-Aside Pattern**.

```text
          Request
             |
             v
       Check Redis
        /       \
      HIT       MISS
       |          |
       v          v
   Return      Query DB
                 |
                 v
            Store Redis
                 |
                 v
              Return
```

This is one of the most common caching patterns with Redis.

---

## 7. Real Example

Suppose your application has `GET /products/500`.

**Step 1 — Check Redis:** `GET product:500` → MISS.

**Step 2 — Query database:** `SELECT * FROM products WHERE id = 500;`

```json
{
    "id": 500,
    "name": "iPhone",
    "price": 70000
}
```

**Step 3 — Store in Redis:**

```text
SET product:500 '{"id":500,"name":"iPhone","price":70000}' EX 600
```

**Step 4 — Return response:** Application → Client.

---

## 8. Next Request

Another user requests `GET /products/500`. Application checks `GET product:500`. Redis has it. **HIT**.

```text
Client
  |
  v
Application
  |
  v
Redis
  |
  v
Response
```

Database isn't involved. This is the main performance benefit.

---

## 9. Why Is Redis Faster Than a Database?

Redis primarily works with RAM.

Traditional databases commonly involve additional work: connection, query processing, indexes / storage, result.

Redis can often do: Application → Redis RAM → Result.

So caching frequently accessed data can significantly reduce latency and database load.

---

## 10. Cache Hit Ratio

Formula:

```text
Cache Hit Ratio = Cache Hits / Total Requests
```

For example: Total requests = 10,000, Cache hits = 9,000 → Hit Ratio = 90%.

So 90% of requests were served from cache. The remaining 10% went to the database.

---

## 11. What Should We Cache?

Usually, cache data that is:

**Frequently read** — popular products, user profiles, configuration, frequently viewed posts.

**Expensive to calculate** — complex database query, aggregated statistics, expensive computation.

**Relatively stable** — if data changes every second, caching becomes more complicated.

---

## 12. What Should We Not Blindly Cache?

Be careful with: highly sensitive data, rapidly changing data, very large objects, data that must always be strongly consistent.

Caching introduces another copy of the data, so we need to think about stale data.

---

## 13. The Big Problem: Stale Data

Suppose database contains `user:101`, balance = ₹10,000. Redis also contains balance = ₹10,000.

Now the database is updated: balance = ₹8,000. But Redis still contains ₹10,000.

If the application reads from Redis, it receives old data.

This is called **stale cache**.

Therefore, cache invalidation/update strategy is extremely important.

---

## 14. TTL Helps

For example: `SET user:101 "{...}" EX 300` means cache this data for 300 seconds.

After 5 minutes, Redis automatically expires it.

Then the next request: Redis → MISS → Database → Redis updated.

This prevents cached data from remaining forever.

---

## 15. Complete Interview Explanation

If interviewer asks: **"How would you use Redis for caching?"**

> I would use Redis as a cache in front of the database. When a request comes, the application first checks Redis. If the data is present, it is a cache hit and we return it directly. If it is not present, it is a cache miss, so we fetch the data from the database, store it in Redis with an appropriate TTL, and return the response. This is commonly called the cache-aside pattern. It reduces database load and improves response latency.

---

## 16. Remember This Flow

**Cache Hit**

```text
Request → Application → Redis → HIT → Response
```

**Cache Miss**

```text
Request → Application → Redis → MISS → Database → Store in Redis → Response
```

### The 3 Things to Remember

```text
Cache
  ↓
Faster reads

Cache Hit
  ↓
Data found in Redis

Cache Miss
  ↓
Fetch from DB → Store in Redis → Return
```

**Day 14 key concept:** Redis is not necessarily the source of truth. In the typical cache-aside setup, the database remains the source of truth and Redis is the fast cached copy.
