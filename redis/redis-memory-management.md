# Day 9 — Redis Memory Management

This is an important Redis topic, especially for interviews, because Redis stores most of its working data in RAM.

---

## Table of Contents

- [1. Why Does Redis Need Memory Management?](#1-why-does-redis-need-memory-management)
- [2. What Is Eviction?](#2-what-is-eviction)
- [3. maxmemory](#3-maxmemory)
- [4. What Happens When Memory Becomes Full?](#4-what-happens-when-memory-becomes-full)
- [5. Important Eviction Policies](#5-important-eviction-policies)
- [6. noeviction](#6-noeviction)
- [7. allkeys-lru](#7-allkeys-lru)
- [8. allkeys-lfu](#8-allkeys-lfu)
- [9. LRU vs LFU](#9-lru-vs-lfu)
- [10. volatile-lru](#10-volatile-lru)
- [11. volatile-lfu](#11-volatile-lfu)
- [12. allkeys vs volatile](#12-the-important-difference-allkeys-vs-volatile)
- [13. Easy Table to Remember](#13-easy-table-to-remember)
- [14. Why Eviction Is Useful for Caching](#14-why-eviction-is-useful-for-caching)
- [15. Real-World Caching Example](#15-example-real-world-caching)
- [16. LRU Example](#16-lru-example)
- [17. LFU Example](#17-lfu-example)
- [18. Which Eviction Policy for a Cache?](#18-important-interview-question)
- [19. volatile-lru vs allkeys-lru](#19-another-important-interview-question)
- [20. What Happens to an Evicted Cache Entry?](#20-what-happens-to-an-evicted-cache-entry)
- [21. Eviction vs Expiration](#21-eviction-vs-expiration)
- [22. Production Monitoring](#22-important-production-point)
- [23. Interview Mental Model](#23-interview-mental-model)

---

## 1. Why Does Redis Need Memory Management?

Redis is primarily an in-memory data store.

```text
Application
    |
    v
  Redis
    |
    v
   RAM
```

Suppose your Redis server has RAM = 8 GB, and Redis is allowed to use `maxmemory = 6 GB`.

As more keys are added:

```text
1 GB
 ↓
2 GB
 ↓
4 GB
 ↓
5 GB
 ↓
6 GB
 ↓
Memory limit reached
```

Now Redis has to decide: **"What should I do when there is no more memory?"**

This is where eviction policies come into the picture.

---

## 2. What Is Eviction?

Eviction means Redis automatically removes existing keys to make room for new data.

Example: Redis memory limit = 1 GB. Current keys: `user:101`, `user:102`, `user:103`, `user:104`, ...

Memory becomes full. A new key arrives: `user:5000`.

Redis may remove an old/less useful key: `user:101` → removed, `user:5000` → stored.

The exact key removed depends on the configured eviction policy.

---

## 3. maxmemory

Redis can be configured with a maximum memory limit.

Example: `maxmemory 1gb`

This means Redis can use approximately 1 GB for its configured memory limit.

You can check it with `CONFIG GET maxmemory`.

You can check the current memory usage with `INFO memory`.

Important values include: `used_memory`, `maxmemory`, `maxmemory_policy`.

---

## 4. What Happens When Memory Becomes Full?

It depends on `maxmemory-policy`.

For example, `maxmemory-policy allkeys-lru` means: if Redis reaches the memory limit, remove keys using the LRU strategy.

---

## 5. Important Eviction Policies

The important ones for interviews are:

- `noeviction`
- `allkeys-lru`
- `allkeys-lfu`
- `volatile-lru`
- `volatile-lfu`

---

## 6. noeviction

This is basically: **don't remove existing keys**.

When Redis reaches its memory limit, write operations that require additional memory can fail.

Example: Memory limit = 1 GB, current usage = 1 GB. Now `SET user:5000 Parwez`.

Redis may return an error similar to:

```text
OOM command not allowed when used memory > 'maxmemory'
```

Existing data remains.

**When useful?** When you cannot afford Redis to automatically delete data. For example, Redis contains important state — you might prefer an error rather than silently deleting keys.

---

## 7. allkeys-lru

Let's break the name: **allkeys + LRU**.

**allkeys** — Redis can consider all keys.

**LRU** — Least Recently Used. Redis tries to remove keys that haven't been accessed recently.

Example:

```text
user:101 → accessed 1 minute ago
user:102 → accessed 10 minutes ago
user:103 → accessed 1 hour ago
```

If Redis needs space, `user:103` is a good eviction candidate because it hasn't been used recently.

Conceptually:

```text
Recently used
     ↑
user:101

user:102

user:103
     ↓
Least recently used
```

Very common for caching. If some products aren't requested anymore, Redis can remove them when memory is needed.

---

## 8. allkeys-lfu

**allkeys + LFU**. LFU means **Least Frequently Used**.

Instead of asking "When was this key last accessed?", it asks "How frequently is this key accessed?"

Example:

```text
product:101 → accessed 1000 times
product:102 → accessed 500 times
product:103 → accessed 2 times
```

If Redis needs memory, `product:103` is a strong eviction candidate (frequency = 2 vs 1000 and 500).

---

## 9. LRU vs LFU

This is a very common interview question.

**LRU** looks at recent usage: "When was this key used?"

**LFU** looks at usage frequency: "How many times is this key used?"

Example: Key A accessed 1000 times yesterday, Key B accessed 2 times recently.

Depending on the exact access pattern, LRU may consider B less recently used. LFU may consider A more valuable because it has high frequency.

Think:

```text
LRU → Recency
LFU → Frequency
```

---

## 10. volatile-lru

**volatile + LRU** — Redis considers only keys that have an expiration (TTL).

Example:

```text
SET user:101 Parwez          → No TTL
SET otp:101 123456 EX 300    → Has TTL
```

Under `volatile-lru`, Redis can consider `otp:101` but not `user:101`, because `user:101` doesn't have an expiration.

Among the TTL keys, Redis uses an LRU strategy.

---

## 11. volatile-lfu

**volatile + LFU** — Redis considers only keys having an expiration, then uses LFU.

Example:

```text
session:101 → TTL → accessed 100 times
session:102 → TTL → accessed 5 times
session:103 → TTL → accessed 1 time
```

Redis needs memory. Under `volatile-lfu`, it may evict `session:103` because it has the lowest access frequency among eligible keys.

---

## 12. The Important Difference: allkeys vs volatile

**allkeys** — consider all keys.

**volatile** — consider only keys with TTL.

So `allkeys-lru` means: consider all keys and use LRU.

`volatile-lru` means: consider only TTL keys and use LRU.

Similarly: `allkeys-lfu` → all keys + LFU. `volatile-lfu` → TTL keys + LFU.

---

## 13. Easy Table to Remember

| Policy | Keys considered | Strategy |
| --- | --- | --- |
| `noeviction` | None | Don't evict |
| `allkeys-lru` | All keys | Least Recently Used |
| `allkeys-lfu` | All keys | Least Frequently Used |
| `volatile-lru` | TTL keys only | Least Recently Used |
| `volatile-lfu` | TTL keys only | Least Frequently Used |

There are also other policies such as `allkeys-random`, `volatile-random`, `volatile-ttl`.

But for interviews, the above five are especially important.

---

## 14. Why Eviction Is Useful for Caching

Suppose you're using Redis as a cache:

```text
Application
     |
     v
   Redis
     |
     v
Database
```

You might cache `product:101`, `product:102`, `product:103`, ...

But you don't necessarily need every cached object forever.

When Redis becomes full:

```text
Memory full
    ↓
Eviction policy
    ↓
Remove less useful cache entries
    ↓
Make space
    ↓
Store new cache entry
```

If an evicted product is requested again (`GET product:103`), Redis returns `nil`. Application then goes to the database:

```text
Redis MISS
   ↓
Database
   ↓
Get product
   ↓
Store in Redis again
```

This is completely normal for a cache.

---

## 15. Example: Real-World Caching

Suppose an e-commerce application caches product information.

```text
product:101 → iPhone
product:102 → Samsung
product:103 → OnePlus
product:104 → Pixel
```

Assume Redis has limited memory.

Some products are extremely popular:

```text
product:101 → 10,000 requests
product:102 → 8,000 requests
product:103 → 2 requests
```

With `allkeys-lfu`, Redis is more likely to retain `product:101` and `product:102`, and remove less frequently accessed entries.

This makes LFU particularly attractive when frequently accessed ("hot") data should stay cached.

---

## 16. LRU Example

Imagine:

```text
10:00 → product:101 accessed
10:01 → product:102 accessed
10:02 → product:103 accessed
10:03 → product:101 accessed
```

At 10:04, most recently used: `product:101`, `product:103`, `product:102`.

So `product:102` is the least recently used among these three.

LRU focuses primarily on how recently the key was accessed.

---

## 17. LFU Example

Suppose:

```text
product:101 → 100 accesses
product:102 → 50 accesses
product:103 → 2 accesses
```

LFU sees 101 → 100, 102 → 50, 103 → 2. Therefore 103 is the least frequently used.

---

## 18. Important Interview Question

**Interviewer:** "Your Redis is being used as a cache. Which eviction policy would you choose?"

A good answer:

> For a cache, I would generally consider `allkeys-lru` or `allkeys-lfu`. If I want to keep recently accessed data, I would use LRU. If access frequency is more important and some keys are consistently hot, LFU can be a better choice. The final choice depends on the application's access pattern.

This is better than saying "LFU is always better." There is no universally best policy.

---

## 19. Another Important Interview Question

**Interviewer:** "What's the difference between `volatile-lru` and `allkeys-lru`?"

> `allkeys-lru` can evict any key, while `volatile-lru` can evict only keys that have an expiration time or TTL.

---

## 20. What Happens to an Evicted Cache Entry?

Suppose Redis `product:101` gets evicted.

Then:

```text
Client
  |
  | GET product:101
  v
Redis
  |
  | MISS
  v
Application
  |
  v
Database
```

Application retrieves it from DB and can put it back:

```text
Database
   ↓
product:101
   ↓
Redis
```

This is called a cache miss followed by repopulating the cache.

---

## 21. Eviction vs Expiration

These two concepts are different.

**Expiration** — you explicitly set a TTL: `SET otp:101 123456 EX 300`. After 300 seconds, the key expires.

**Eviction** — Redis removes a key because it needs memory according to the configured eviction policy.

```text
Expiration → Time-based
Eviction   → Memory-pressure-based
```

---

## 22. Important Production Point

Eviction should not be your only memory-management strategy.

You should also monitor: `used_memory`, `maxmemory`, memory fragmentation, `evicted_keys`, `expired_keys`.

For example: `INFO memory`.

You can monitor whether Redis is constantly evicting keys.

If `evicted_keys` keeps increasing rapidly, it may indicate that your Redis memory limit is too small or your cache workload has changed.

---

## 23. Interview Mental Model

Remember this diagram:

```text
             Redis
               |
               v
             RAM
               |
          Memory fills
               |
               v
        maxmemory reached
               |
               v
       maxmemory-policy
               |
       ┌───────┴────────┐
       |                |
    noeviction       Eviction
                        |
              ┌─────────┴─────────┐
              |                   |
             LRU                 LFU
              |                   |
         Recent usage        Frequency
```

And the most important shortcut:

```text
LRU = Recently used?
LFU = Frequently used?
allkeys = Any key?
volatile = Only TTL keys?
noeviction = Don't remove keys
```

### One-line interview summary

> Redis eviction policies determine which keys Redis removes when it reaches its memory limit. LRU prioritizes recency, LFU prioritizes frequency, allkeys considers all keys, volatile considers only keys with TTL, and noeviction doesn't remove keys and instead causes memory-related write failures.
