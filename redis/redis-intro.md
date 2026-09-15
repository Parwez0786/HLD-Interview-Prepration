# Day 1 — Redis Introduction — Deep Dive

---

## Table of Contents

- [1. What Exactly Is Redis?](#1-what-exactly-is-redis)
- [2. Why Do We Need Redis?](#2-why-do-we-need-redis)
- [3. Redis as a Cache](#3-redis-as-a-cache)
- [4. Redis vs PostgreSQL](#4-redis-vs-postgresql)
- [5. Redis Data Model](#5-redis-data-model)
- [6. Why Is Redis So Fast?](#6-why-is-redis-so-fast)
- [7. Reason 1 — RAM Is Much Faster Than Disk](#7-reason-1--ram-is-much-faster-than-disk)
- [8. Reason 2 — Simple Operations](#8-reason-2--simple-operations)
- [9. Reason 3 — Efficient Data Structures](#9-reason-3--efficient-data-structures)
- [10. Reason 4 — Low Network/Protocol Overhead](#10-reason-4--low-networkprotocol-overhead)
- [11. Reason 5 — Many Operations Are O(1)](#11-reason-5--many-operations-are-o1)
- [12. Is Redis Single-Threaded?](#12-is-redis-single-threaded)
- [13. Redis Architecture](#13-redis-architecture)
- [14. Redis Server vs Redis Client](#14-redis-server-vs-redis-client)
- [15. Redis Default Port](#15-redis-default-port)
- [16. Key-Value Model in Detail](#16-key-value-model-in-detail)
- [17. Redis Values Aren't Only Strings](#17-redis-values-arent-only-strings)
- [18. Installing Redis Using Docker](#18-installing-redis-using-docker)
- [19. Connect Using redis-cli](#19-connect-using-redis-cli)
- [20. First Redis Commands](#20-first-redis-commands)
- [21. EXISTS](#21-exists)
- [22. TYPE](#22-type)
- [23. KEYS *](#23-keys-)
- [24. Use SCAN Instead](#24-use-scan-instead)
- [25. Redis Expiration](#25-redis-expiration--extremely-important)
- [26. Why Expiration Is Important for Caching](#26-why-expiration-is-important-for-caching)
- [27. Redis Persistence](#27-redis-persistence)
- [28. Redis Is Not Necessarily Your Primary Database](#28-redis-is-not-necessarily-your-primary-database)
- [29. Real-World Example: Product API](#29-real-world-example-product-api)
- [30. Session Management](#30-redis-use-case--session-management)
- [31. Why Redis Is Useful in Distributed Systems](#31-why-redis-is-useful-in-distributed-systems)
- [32. Redis for Rate Limiting](#32-redis-for-rate-limiting)
- [33. Redis for Counters](#33-redis-for-counters)
- [34. Redis for Leaderboards](#34-redis-for-leaderboards)
- [35. Redis for Distributed Locks](#35-redis-for-distributed-locks)
- [36. Important Redis Interview Distinction](#36-important-redis-interview-distinction)
- [37. One Complete Mental Model](#37-one-complete-mental-model)
- [38. Interview Answer: What Is Redis?](#38-interview-answer-what-is-redis)
- [39. Interview Answer: Why Is Redis Fast?](#39-interview-answer-why-is-redis-fast)
- [40. Interview Answer: Redis vs PostgreSQL?](#40-interview-answer-redis-vs-postgresql)
- [41. Day 1 Commands to Practice](#41-day-1-commands-you-should-actually-practice)
- [42. What You Should Remember from Day 1](#42-what-you-should-remember-from-day-1)

---

## 1. What Exactly Is Redis?

Redis is an **in-memory data structure server**.

The simplest mental model is:

```text
Application
     |
     | GET / SET
     v
   Redis
     |
     v
   RAM
```

Unlike a traditional relational database such as PostgreSQL, Redis is designed so that the working dataset is primarily kept in memory.

For example:

```text
Key                  Value
--------------------------------
user:101              "Parwez"
user:102              "Rahul"
user:103              "Amit"
```

You can think of Redis as a very fast shared data structure that your application can access over the network.

### Why is it called Redis?

Redis originally stood for **Remote Dictionary Server**.

The "dictionary" idea comes from: **KEY → VALUE**.

But Redis is much more than a simple key-value map because values can be:

- String
- Hash
- List
- Set
- Sorted Set
- Stream
- Bitmap
- HyperLogLog
- ...

---

## 2. Why Do We Need Redis?

Suppose your application has:

```text
User Service
      |
      v
PostgreSQL
```

Every time the user opens their profile `GET /user/101`, the application might execute:

```sql
SELECT * FROM users WHERE id = 101;
```

Now imagine this user opens the profile 100,000 times.

You don't necessarily want PostgreSQL to perform the same work 100,000 times.

Instead:

```text
                 ┌──────────────┐
                 │ Application  │
                 └──────┬───────┘
                        |
                        v
                  ┌───────────┐
                  │  Redis    │
                  └─────┬─────┘
                        |
                  Cache Hit?
                   /          \
                 YES           NO
                  |             |
                  v             v
               Return       PostgreSQL
                              |
                              v
                            Redis
                         Store result
```

This is called **caching**.

---

## 3. Redis as a Cache

Suppose User ID = 101, Name = Parwez, Email = parwez@gmail.com.

Initially Redis doesn't have the user.

Application: `GET user:101`

Redis: `(nil)`

That's a **cache miss**.

Application then goes to PostgreSQL:

```text
PostgreSQL
    |
    v
User 101
```

It gets:

```json
{
    "id": 101,
    "name": "Parwez",
    "email": "parwez@gmail.com"
}
```

Then application stores it in Redis: `SET user:101 "{...}"`

Next request: `GET user:101` — Redis already has it.

That's a **cache hit**.

```text
Application
     |
     v
Redis
     |
     v
User data
```

PostgreSQL isn't touched.

---

## 4. Redis vs PostgreSQL

This is an important interview topic.

### PostgreSQL

PostgreSQL is a relational database.

Data is organized into:

```text
Database
   |
   └── Tables
          |
          ├── Rows
          └── Columns
```

Example:

```text
users

id | name   | email
------------------------------
101| Parwez | parwez@gmail.com
102| Rahul  | rahul@gmail.com
```

You can perform:

```sql
SELECT *
FROM users
WHERE id = 101;
```

PostgreSQL provides things Redis isn't primarily designed for:

- SQL
- joins
- transactions
- relational constraints
- complex queries
- durable persistent storage
- foreign keys
- indexes
- ACID semantics

---

## 5. Redis Data Model

Redis looks much simpler: **KEY → VALUE**.

For example:

```text
user:101 → "Parwez"
```

Or:

```text
user:101:email → "parwez@gmail.com"
```

Or `user:101 → Hash`, where the hash contains:

```text
id       → 101
name     → Parwez
email    → parwez@gmail.com
age      → 26
```

This is one of the major differences:

```text
PostgreSQL
     ↓
Relational model

Redis
     ↓
Key + Data Structure
```

---

## 6. Why Is Redis So Fast?

This is one of the most common Redis interview questions.

If interviewer asks: Why is Redis faster than a traditional database?

Don't simply say: "Because Redis is single-threaded."

That's incomplete.

A better answer:

> Redis is fast mainly because its active dataset is kept in memory, it uses efficient data structures, commands are simple and optimized, and it has low overhead for many common operations. Redis traditionally executes commands in a serialized manner, avoiding much of the locking complexity associated with concurrent command execution. Modern Redis can also use multiple threads for networking and background tasks.

Let's break this down.

---

## 7. Reason 1 — RAM Is Much Faster Than Disk

Suppose:

```text
Application
    |
    v
Redis
    |
    v
RAM
```

Memory access is extremely fast compared with storage access.

Traditional databases also use RAM heavily.

This is important: **PostgreSQL is not simply "disk-based and slow."**

PostgreSQL also uses memory and OS page cache aggressively.

The difference is that Redis is designed around an **in-memory data model**, whereas PostgreSQL's durable source of truth is persistent storage.

So don't say: ❌ PostgreSQL always reads from disk.

Say:

> ✅ PostgreSQL may serve data from memory/cache, but its persistent data is stored on disk; Redis is designed to keep its active dataset in memory.

---

## 8. Reason 2 — Simple Operations

Redis operations are usually straightforward.

For example: `GET user:101` or `SET user:101 Parwez`.

Compare that with a complex relational query:

```sql
SELECT ...
FROM ...
JOIN ...
WHERE ...
ORDER BY ...
GROUP BY ...
```

Redis is optimized for its own data structures and commands.

---

## 9. Reason 3 — Efficient Data Structures

Redis has highly optimized implementations for things like: Hash, List, Set, Sorted Set, Stream.

For example, `SADD users:online 101` adds a user to a Redis Set.

Then `SISMEMBER users:online 101` can efficiently check whether the user exists in the set.

---

## 10. Reason 4 — Low Network/Protocol Overhead

Redis uses a lightweight request/response protocol.

For example:

```text
Client
   |
   | GET user:101
   v
Redis
   |
   | value
   v
Client
```

There is very little overhead for simple operations.

Of course, network latency still exists.

Redis being in-memory does not mean 0 ms latency.

It still requires:

```text
Application
    |
    | network
    v
Redis
```

So application-to-Redis network latency matters.

---

## 11. Reason 5 — Many Operations Are O(1)

For example: `GET`, `SET`, `EXISTS`, `DEL` are generally O(1) operations under normal conditions.

For example, `SET name Parwez` doesn't need to search through millions of values.

Redis can locate the key efficiently.

But be careful: **not every Redis command is O(1).**

For example, some operations on lists, sorted sets, streams, etc. can be O(N) or O(log N), depending on the command.

This is important in interviews.

---

## 12. Is Redis Single-Threaded?

This question is often asked.

Historically, Redis used a model where command execution was primarily single-threaded.

Imagine:

```text
Client 1 ──┐
Client 2 ──┤
Client 3 ──┼──> Redis command execution
Client 4 ──┘
```

Commands are processed in a serialized manner.

For example:

```text
SET A 10
SET B 20
GET A
INCR B
```

The command execution doesn't have multiple threads simultaneously modifying the same data structures.

This simplifies concurrency.

### But modern Redis is not simply "single-threaded"

Modern Redis versions can use threads for certain networking/background activities, while the core command execution model has traditionally remained serialized.

Therefore, interview answer:

> Redis historically uses a single-threaded command execution model, which simplifies concurrency and avoids many locking costs. However, modern Redis can use multiple threads for networking and other work, so saying Redis is completely single-threaded is no longer accurate.

---

## 13. Redis Architecture

At a high level:

```text
             Application
                  |
                  v
            Redis Client
                  |
                  v
        ┌──────────────────┐
        │      Redis       │
        │                  │
        │ Command handling  │
        │ Data structures   │
        │ Persistence       │
        └────────┬─────────┘
                 |
                 v
                RAM
```

The client could be: Java, Python, Node.js, Go, C++.

For your Java backend:

```text
Spring Boot
     |
     | Redis client
     v
   Redis
```

Common Java clients include: **Lettuce**, **Jedis**.

Spring Boot commonly uses Spring Data Redis.

---

## 14. Redis Server vs Redis Client

This distinction is important.

### Redis Server

The Redis server stores data and processes commands: `redis-server`.

### Redis Client

A client connects to Redis. For example: `redis-cli`.

Architecture:

```text
redis-cli
    |
    | TCP
    v
redis-server
    |
    v
RAM
```

Your Spring Boot application is also a Redis client.

```text
Spring Boot
     |
     v
Redis Server
```

---

## 15. Redis Default Port

Redis commonly runs on **6379**.

So:

```text
Application
    |
    | TCP :6379
    v
Redis
```

For local development: `localhost:6379`.

---

## 16. Key-Value Model in Detail

Suppose `SET name "Parwez"`.

Redis stores: `name → Parwez`.

Then `GET name` returns `Parwez`.

Another example:

```text
SET user:101:name "Parwez"
SET user:101:email "parwez@gmail.com"
```

Now:

```text
user:101:name  → Parwez
user:101:email → parwez@gmail.com
```

The naming convention `entity:id:property` is very common.

For example:

- `user:101`
- `user:101:profile`
- `user:101:session`
- `user:101:cart`

---

## 17. Redis Values Aren't Only Strings

This is a major concept.

Redis supports multiple data structures.

### String

```text
SET name Parwez
GET name
```

### Hash

Useful for objects.

```text
HSET user:101 name Parwez email parwez@gmail.com age 26
```

Now:

```text
user:101
    |
    ├── name  → Parwez
    ├── email → parwez@gmail.com
    └── age   → 26
```

Retrieve: `HGET user:101 name` → `Parwez`.

### List

Ordered collection.

```text
LPUSH queue order1
LPUSH queue order2
LPUSH queue order3
```

Conceptually:

```text
queue

order3
order2
order1
```

Useful for certain queue-like workloads.

### Set

Unique values.

```text
SADD online_users 101
SADD online_users 102
SADD online_users 101
```

The second 101 isn't added again.

```text
online_users

101
102
```

Useful for: unique users, tags, membership, sets of IDs.

### Sorted Set

A set where each member has a score.

Example:

```text
ZADD leaderboard 100 user101
ZADD leaderboard 250 user102
ZADD leaderboard 180 user103
```

Conceptually:

```text
user102 → 250
user103 → 180
user101 → 100
```

Excellent for: leaderboards, ranking, priority-like use cases.

### Stream

Redis Streams are designed for append-oriented event data and consumer processing.

Conceptually:

```text
Stream
  |
  ├── Event 1
  ├── Event 2
  ├── Event 3
  └── Event 4
```

We'll cover this much deeper later.

---

## 18. Installing Redis Using Docker

For learning:

```bash
docker run -d \
  --name redis \
  -p 6379:6379 \
  redis
```

Let's understand it.

| Flag | Meaning |
| --- | --- |
| `docker run` | Creates and starts a container |
| `-d` | Detached mode — container runs in the background |
| `--name redis` | Container name: `redis` |
| `-p 6379:6379` | Port mapping: your machine:6379 → container:6379 |
| `redis` | The Docker image |

---

## 19. Connect Using redis-cli

You can execute `redis-cli`.

Then: `127.0.0.1:6379>`

Now `PING` → Response: `PONG`.

This is basically a health check.

---

## 20. First Redis Commands

### SET

```text
SET name "Parwez"
```

Response: `OK`

This means: `name → Parwez`.

### GET

```text
GET name
```

Response: `"Parwez"`

### DEL

```text
DEL name
```

Response: `(integer) 1`

It means one key was deleted.

Now `GET name` returns `(nil)`.

---

## 21. EXISTS

```text
SET name Parwez
EXISTS name
```

Result: `(integer) 1` — key exists.

If `EXISTS xyz` → `(integer) 0` — key doesn't exist.

---

## 22. TYPE

Suppose `SET name Parwez`.

Then `TYPE name` returns `string`.

For a hash: `HSET user:101 name Parwez`

Then `TYPE user:101` returns `hash`.

This is useful for debugging.

---

## 23. KEYS *

You can run `KEYS *`.

Suppose Redis contains: `user:101`, `user:102`, `product:1`, `product:2`, `session:abc`.

It might return all of them.

This is convenient during development.

**But don't use it on a large production Redis instance.**

Why? Because `KEYS *` scans the entire keyspace.

Imagine 10 million keys. Redis needs to process the request across the keyspace.

Because Redis command processing is sensitive to long-running commands, this can cause latency problems for other clients.

---

## 24. Use SCAN Instead

Instead of `KEYS *`, use `SCAN 0`.

Redis returns a cursor and some keys.

For example conceptually, `SCAN 0` might return:

```text
25
user:101
user:102
product:1
```

The cursor `25` means there is more work.

Then `SCAN 25` and continue until the returned cursor becomes `0`.

Conceptually:

```text
SCAN 0
   |
   v
cursor 25
   |
   v
SCAN 25
   |
   v
cursor 70
   |
   v
SCAN 70
   |
   v
cursor 0
   |
   v
DONE
```

This is called **incremental scanning**.

**Important interview point:**

> SCAN is preferred over KEYS * for production keyspace iteration because it incrementally scans instead of requesting the entire keyspace in one command.

---

## 25. Redis Expiration — Extremely Important

Redis can automatically delete data after a certain period.

Example:

```text
SET OTP:101 123456 EX 300
```

Meaning:

```text
OTP:101 → 123456
TTL = 300 seconds
```

After 5 minutes, Redis expires the key.

You can check `TTL OTP:101`. For example `(integer) 250` means approximately 250 seconds remain.

This is extremely useful for: OTP, sessions, cache, temporary tokens, temporary locks.

---

## 26. Why Expiration Is Important for Caching

Suppose `user:101 → Parwez`. You cache it forever.

But PostgreSQL changes: Parwez → Parwez Ansari.

Redis could still contain Parwez.

This is called **stale cache data**.

One solution is expiration:

```text
SET user:101 "Parwez" EX 300
```

After 5 minutes:

```text
Redis
   |
   v
expired
```

Next request:

```text
Redis MISS
    |
    v
PostgreSQL
    |
    v
Fresh data
```

We'll later study: cache-aside, TTL, cache invalidation, cache stampede, cache penetration, cache avalanche.

These are important backend interview topics.

---

## 27. Redis Persistence

A common misconception is:

> Redis stores everything only in RAM, so restarting Redis always loses everything.

Not necessarily.

Redis supports persistence mechanisms such as **RDB** and **AOF**.

### RDB

Periodic snapshots.

Conceptually:

```text
RAM
 |
 | periodically
 v
RDB snapshot
 |
 v
Disk
```

### AOF

Append-only log of write operations.

Conceptually:

```text
SET A 10
SET B 20
INCR A
```

can be recorded in an append-only log.

On restart, Redis can reconstruct the dataset from persisted data.

We'll study RDB/AOF deeply later.

---

## 28. Redis Is Not Necessarily Your Primary Database

Suppose your architecture is:

```text
                    ┌──────────────┐
                    │ Spring Boot  │
                    └──────┬───────┘
                           |
                     ┌─────┴─────┐
                     |           |
                     v           v
                  Redis      PostgreSQL
                   Cache       Source
```

Typically:

```text
PostgreSQL
    ↓
Source of truth

Redis
    ↓
Fast cache / temporary state
```

If Redis goes down, your application may still be able to retrieve data from PostgreSQL, depending on the architecture.

This is one of the most important ways to think about Redis.

---

## 29. Real-World Example: Product API

Suppose `GET /products/101`.

**Without Redis:**

```text
Client
  |
  v
Spring Boot
  |
  v
PostgreSQL
  |
  v
Product
```

**With Redis:**

```text
Client
  |
  v
Spring Boot
  |
  v
Redis
  |
  +---- HIT ----> Product
  |
  +---- MISS
           |
           v
       PostgreSQL
           |
           v
         Redis
```

Pseudo-code:

```text
getProduct(id):

    product = redis.get("product:" + id)

    if product exists:
        return product

    product = postgres.getProduct(id)

    redis.set("product:" + id, product, TTL)

    return product
```

This pattern is called **Cache-Aside Pattern**.

Very important for backend interviews.

---

## 30. Redis Use Case — Session Management

Suppose a user logs in.

Application creates `session:abc123` and stores `userId → 101`.

Redis: `session:abc123 → user:101` with TTL 30 minutes.

Every request:

```text
Client
  |
  | session ID
  v
Backend
  |
  v
Redis
  |
  v
user 101
```

This works especially well when you have multiple backend servers.

---

## 31. Why Redis Is Useful in Distributed Systems

Suppose:

```text
              Load Balancer
                    |
          ┌─────────┼─────────┐
          |         |         |
          v         v         v
       Server1   Server2   Server3
          |         |         |
          └─────────┼─────────┘
                    |
                    v
                  Redis
```

All application servers can access the same Redis instance/cluster.

This allows shared state such as: sessions, counters, rate limits, locks, cached data — without storing that state only inside one application server.

---

## 32. Redis for Rate Limiting

Suppose an API allows 100 requests/minute/user.

Redis can maintain `rate:user:101 → 57`.

Every request: `INCR rate:user:101`.

Then use an expiration window.

Because Redis provides fast atomic operations, it's very useful for distributed rate limiting.

Later we'll learn: `INCR`, `EXPIRE`, `MULTI`/`EXEC`, Lua, atomic rate limiter.

---

## 33. Redis for Counters

Suppose `video:101:views` starts at 100.

When someone watches: `INCR video:101:views` → now 101.

Another user: `INCR video:101:views` → now 102.

This is much simpler than repeatedly reading and updating a value manually.

---

## 34. Redis for Leaderboards

Suppose:

```text
user101 → 500 points
user102 → 900 points
user103 → 700 points
```

Use a Sorted Set:

```text
ZADD leaderboard 500 user101
ZADD leaderboard 900 user102
ZADD leaderboard 700 user103
```

Then retrieve top users.

This is why Redis is popular for: gaming leaderboards, ranking systems, scoreboards.

---

## 35. Redis for Distributed Locks

Suppose two backend servers try to process the same job:

```text
Server 1 ──┐
           |
           v
        Redis
           ^
           |
Server 2 ──┘
```

You want only one server to acquire the lock.

Redis can help implement distributed locking using commands such as:

```text
SET lock:key value NX EX 30
```

Conceptually:

```text
NX
↓
Set only if key doesn't exist

EX 30
↓
Expire after 30 seconds
```

This prevents the lock from remaining forever if the server crashes.

We'll study distributed locks separately because there are important correctness considerations.

---

## 36. Important Redis Interview Distinction

**Redis is not just a cache.**

It can be used as:

- Cache
- Session store
- Counter store
- Rate limiter
- Distributed lock mechanism
- Leaderboard
- Pub/Sub system
- Stream/event system
- Temporary state store
- Queue-like structure

But: just because Redis can store data persistently doesn't mean it should automatically replace your relational database.

Choose the storage based on the workload and consistency/durability requirements.

---

## 37. One Complete Mental Model

Remember Redis like this:

```text
                         Redis
                           |
            ┌──────────────┼───────────────┐
            |              |               |
         Keys/Data      Commands       Persistence
            |              |               |
            v              v               v
       Data Structures   GET/SET          RDB/AOF
            |
    ┌───────┼────────┬─────────┐
    |       |        |         |
 String   Hash     List       Set
                         \
                          Sorted Set
```

And underneath:

```text
             Redis Server
                  |
          ┌───────┴────────┐
          |                |
       Memory           Persistence
          |                |
       Fast access      RDB / AOF
```

---

## 38. Interview Answer: "What is Redis?"

If interviewer asks: What is Redis?

Give this answer:

> Redis is an in-memory data structure store that works using a key-value model. It supports data structures such as strings, hashes, lists, sets, sorted sets and streams. Because the active dataset is primarily kept in memory and Redis provides efficient operations with low overhead, it is commonly used for caching, sessions, counters, rate limiting, distributed coordination, leaderboards and real-time applications. Redis also supports persistence through mechanisms such as RDB and AOF.

That's a strong SDE-1 answer.

---

## 39. Interview Answer: "Why is Redis fast?"

Say:

> Redis is fast mainly because it keeps the active dataset in memory, uses efficient data structures, has optimized and relatively simple commands, and has low protocol overhead. Its traditional serialized command execution also avoids many locking costs. However, Redis is not fast simply because it is single-threaded; modern Redis can use multiple threads for networking and other tasks.

---

## 40. Interview Answer: "Redis vs PostgreSQL?"

A simple comparison:

| Redis | PostgreSQL |
| --- | --- |
| In-memory data structure store | Relational database |
| Key/data-structure model | Tables/rows/columns |
| Very fast for simple operations | Supports complex queries |
| Excellent for caching | Excellent as source of truth |
| TTL/expiration built in | Persistence is fundamental |
| Hash, List, Set, Sorted Set, Stream | SQL, joins, constraints |
| Commonly used for temporary/shared state | Commonly used for durable business data |
| Limited query model compared with SQL | Powerful query engine |

The key interview statement:

> Redis and PostgreSQL are not necessarily competitors. A common production architecture uses PostgreSQL as the source of truth and Redis as a high-speed cache or shared state store.

---

## 41. Day 1 Commands You Should Actually Practice

Run these yourself:

```text
PING
SET name Parwez
GET name
EXISTS name
TYPE name
DEL name
GET name
```

Then practice expiration:

```text
SET otp:101 123456 EX 60
TTL otp:101
GET otp:101
```

Then practice hashes:

```text
HSET user:101 name Parwez email parwez@gmail.com
HGET user:101 name
HGETALL user:101
TYPE user:101
```

Then sets:

```text
SADD online_users 101
SADD online_users 102
SADD online_users 101
SMEMBERS online_users
```

Then sorted sets:

```text
ZADD leaderboard 500 user101
ZADD leaderboard 900 user102
ZADD leaderboard 700 user103
ZRANGE leaderboard 0 -1 WITHSCORES
```

Finally practice `SCAN 0` instead of relying on `KEYS *`.

---

## 42. What You Should Remember from Day 1

If you remember only these 10 things, you're good for the fundamentals:

1. Redis is an in-memory data structure store.
2. Its basic model is key → value.
3. Values can be String, Hash, List, Set, Sorted Set, Stream, etc.
4. Redis is fast mainly because of memory + efficient data structures + low overhead.
5. Don't explain Redis performance simply as "single-threaded."
6. Redis is commonly used for caching.
7. Redis can also handle sessions, counters, rate limiting, locks, leaderboards and temporary state.
8. Redis commonly runs on port 6379.
9. `KEYS *` can be dangerous on large production datasets; prefer `SCAN`.
10. Redis supports persistence such as RDB and AOF, so "Redis data always disappears after restart" is incorrect.

### Your Redis learning progression

| Day | Topic |
| --- | --- |
| Day 1 | Introduction ← **TODAY** |
| Day 2 | Redis Data Types |
| Day 3 | Strings + Hashes |
| Day 4 | Lists + Sets |
| Day 5 | Sorted Sets |
| Day 6 | TTL + Expiration |
| Day 7 | Redis Transactions |
| Day 8 | Persistence (RDB/AOF) |
| Day 9 | Memory Management |
| Day 10 | Eviction Policies |
| Day 11 | Redis Replication |
| Day 12 | Redis Sentinel |
| Day 13 | Redis Cluster |
| Day 14 | Caching Patterns |
| Day 15 | Distributed Locks |
| Day 16 | Rate Limiting |
| Day 17 | Pub/Sub |
| Day 18 | Redis Streams |
| Day 19 | Production Architecture |
| Day 20 | Redis Interview Questions + HLD |

The most important next concept is **Redis Data Types**, because once you understand why you would choose String vs Hash vs List vs Set vs Sorted Set, Redis starts becoming much easier to use in real backend system design.
