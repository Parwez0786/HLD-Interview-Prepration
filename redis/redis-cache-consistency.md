# Day 17 — Cache Consistency

Cache consistency means keeping the cache and database in sync so that users don't receive outdated data.

Suppose we have:

```text
Database:
user:101 → Parwez

Redis:
user:101 → Rahul
```

Redis contains an old/stale value. If the application reads Redis first, it may return Rahul even though the actual value in the database is Parwez.

This is the main problem cache consistency tries to solve.

---

## Table of Contents

- [1. Cache Invalidation](#1-cache-invalidation)
- [2. Cache-Aside](#2-cache-aside)
- [3. Read-Through Cache](#3-read-through-cache)
- [4. Write-Through Cache](#4-write-through-cache)
- [5. Write-Back Cache](#5-write-back-cache)
- [6. Important Comparison](#6-important-comparison)
- [7. Most Important Interview Pattern](#7-most-important-interview-pattern)
- [8. Why Delete Cache After DB Update?](#8-interview-question-why-delete-cache-after-db-update)
- [9. One Important Real-World Problem](#9-one-important-real-world-problem)

---

## 1. Cache Invalidation

One common strategy is: **Update DB → Delete Cache**.

For example, user changes their name. Old: DB → Parwez, Redis → Parwez.

Application updates the database: DB → Rahul. Then deletes the Redis key: `DEL user:101`.

Now: DB → Rahul, Redis → key doesn't exist.

Next request:

```text
Request
   ↓
Redis
   ↓
Cache MISS
   ↓
Database
   ↓
Rahul
   ↓
Store in Redis
```

So finally: DB → Rahul, Redis → Rahul.

This is called **cache invalidation**.

---

## 2. Cache-Aside

This is one of the most common caching patterns. The application itself manages the cache.

**Read**

```text
Application
     |
     v
   Redis
     |
   MISS
     |
     v
    DB
     |
     v
Store in Redis
     |
     v
 Response
```

Example:

```java
User user = redis.get("user:101");

if (user == null) {
    user = database.getUser(101);
    redis.set("user:101", user);
}

return user;
```

**Write** — usually: Update DB → Delete Redis.

```sql
UPDATE users
SET name = 'Rahul'
WHERE id = 101;
```

```text
DEL user:101
```

**Why delete instead of directly updating cache?** Because deleting the cache is often simpler and safer. Next read reconstructs the cache from the DB.

---

## 3. Read-Through Cache

With read-through, the application talks to the cache, and the cache is responsible for loading missing data from the database.

```text
Application
     |
     v
   Cache
     |
   MISS
     |
     v
    DB
     |
     v
   Cache
     |
     v
Application
```

The application doesn't explicitly do: if cache miss → query DB → put in cache. Instead, the caching layer handles it.

**Difference**

- **Cache-aside:** Application → Cache, Application → DB. Application manages both.
- **Read-through:** Application → Cache → DB. Cache layer manages the DB loading.

---

## 4. Write-Through Cache

With write-through, when data is written, the cache is updated and the database is also updated.

```text
Application
     |
     v
   Cache
     |
     v
    DB
```

Update name Parwez → Rahul: Redis = Rahul, then DB = Rahul. Both are updated during the write operation.

**Advantage:** Cache is generally fresh after a successful write.

**Disadvantage:** Every write involves the database, so writes can have higher latency.

---

## 5. Write-Back Cache

The application first writes to the cache. Later, the cache system asynchronously writes the change to the DB.

```text
Application
     |
     v
   Redis
     |
     |
     ↓
   later
     |
     v
    DB
```

Example: Redis = Rahul, DB may still contain Parwez. Later: Redis = Rahul → async write → DB = Rahul.

**Advantage:** Very fast writes because the database doesn't have to be updated immediately.

**Major risk:** If Redis fails before the data reaches the database, the update could potentially be lost.

Therefore, write-back requires careful durability and failure handling.

---

## 6. Important Comparison

| Strategy | Read | Write | Who manages cache? |
| --- | --- | --- | --- |
| Cache-aside | App checks cache, then DB on miss | DB → delete cache | Application |
| Read-through | Cache loads from DB on miss | Depends on implementation | Cache layer |
| Write-through | Cache | Cache + DB synchronously | Cache layer |
| Write-back | Cache | Cache first, DB later | Cache layer |

---

## 7. Most Important Interview Pattern

For a typical Spring Boot + Redis application, you will often see:

**Read:** Request → Redis HIT → Response, or MISS → DB → Redis SET → Response.

**Update:** Request → Update DB → Delete Redis.

This is **cache-aside with cache invalidation**.

---

## 8. Interview Question: Why Delete Cache After DB Update?

Suppose Redis → Parwez, DB → Parwez. User changes name to Rahul.

If we only do DB → Rahul, then Redis → Parwez ❌, DB → Rahul ✅. The next request gets the stale Redis value.

Therefore: Update DB → Delete Redis. Then the next request loads the latest value from DB.

---

## 9. One Important Real-World Problem

Even this approach has a small race condition.

Suppose:

- T1: Update DB → Rahul
- T2: Another request reads old Redis → Parwez
- T3: Delete Redis

That request may temporarily receive stale data.

There are also more complicated races involving concurrent reads and writes, so large systems may use techniques such as distributed locks, versioning, atomic operations, delayed double deletion, or event-based invalidation, depending on the consistency requirement.

### What you should remember for interview

```text
Cache-Aside
Application manages cache
        ↓
Most common

Read-Through
Cache loads DB data on miss
        ↓
Read responsibility moves to cache

Write-Through
Write cache + DB together
        ↓
Cache stays fresh

Write-Back
Write cache first
        ↓
DB updated asynchronously
        ↓
Fast but more risk
```

### One-line definition

> Cache consistency is the problem of ensuring that cached data does not incorrectly differ from the source-of-truth database.
