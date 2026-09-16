# Day 18 — Redis Distributed Lock

A distributed lock is used when multiple servers may try to process the same resource at the same time, but only one server should be allowed to do it.

This is especially useful in distributed systems where we have multiple instances of the same application.

---

## Table of Contents

- [1. The Problem](#1-the-problem)
- [2. What Is a Distributed Lock?](#2-what-is-a-distributed-lock)
- [3. Basic Redis Lock](#3-basic-redis-lock)
- [4. Server A Gets the Lock](#4-server-a-gets-the-lock)
- [5. Server B Tries](#5-server-b-tries)
- [6. Why Do We Need EX?](#6-why-do-we-need-ex)
- [7. Complete Flow](#7-complete-flow)
- [8. Releasing the Lock](#8-releasing-the-lock)
- [9. Why DEL Can Be Dangerous](#9-why-del-can-be-dangerous)
- [10. Use a Unique Lock Token](#10-use-a-unique-lock-token)
- [11. Java/Spring Boot Example](#11-javaspring-boot-example)
- [12. Distributed Lock vs Java synchronized](#12-distributed-lock-vs-java-synchronized)
- [13. Important Interview Point](#13-important-interview-point)
- [14. Redis Lock Interview Answer](#14-redis-lock-interview-answer)

---

## 1. The Problem

Suppose we have a payment `paymentId = 101`, and our application has two servers:

```text
        Payment Request
              |
       paymentId = 101
              |
        ┌─────┴─────┐
        ↓           ↓
    Server A     Server B
        |           |
        ↓           ↓
   Process       Process
   payment       payment
```

Without any locking, both servers process payment 101.

Example: Initial balance = ₹10,000. Server A deducts ₹1,000. Server B deducts ₹1,000. Final balance = ₹8,000. But the user should have been charged only once.

---

## 2. What Is a Distributed Lock?

A distributed lock is a mechanism that allows multiple servers to coordinate and agree: **"Only one server can work on this resource at a time."**

Redis is commonly used for this because Redis operations such as `SET ... NX` are atomic.

Think of Redis as a shared locker:

```text
             Redis
               |
       lock:payment:101
               |
       ┌───────┴───────┐
       ↓               ↓
   Server A         Server B
```

If Server A gets the lock, Server B must wait/fail.

---

## 3. Basic Redis Lock

The common command is:

```text
SET lock:payment:101 serverA NX EX 30
```

**`lock:payment:101`** — Redis key. Lock for payment 101. Each payment gets its own lock.

**`serverA`** — value stored in the lock: `lock:payment:101 → serverA`. The owner can identify itself when releasing the lock.

In production, a unique random token/UUID is safer than simply `"serverA"`: `lock:payment:101 → 8f72a1c9-...`

**`NX`** — set the key only if it does not already exist. If the lock doesn't exist: Success. If it already exists: Failure.

**`EX 30`** — expire the key after 30 seconds.

The expiration is extremely important.

---

## 4. Server A Gets the Lock

Initially the key doesn't exist. Server A executes `SET lock:payment:101 serverA NX EX 30`. Redis returns `OK`.

Now: `lock:payment:101 → serverA`, TTL = 30 sec.

Server A acquired lock → process payment 101.

---

## 5. Server B Tries

Server B also receives `paymentId = 101`. It executes `SET lock:payment:101 serverB NX EX 30`.

But the key already exists. NX prevents Server B from overwriting it. Server B gets `nil` / failure.

So: Server A → Lock acquired → Process payment. Server B → Lock failed → Don't process.

---

## 6. Why Do We Need EX?

Imagine Server A gets the lock, then crashes.

Without expiration, the lock could remain forever. Then Server B/C/D try lock → FAIL. The payment could become stuck.

That's why we use `EX 30`. After 30 seconds the key expires and is removed. Another server can acquire the lock.

---

## 7. Complete Flow

```text
                 Payment Request
                       |
                       ↓
                Server A / B
                       |
                       ↓
             Try Redis Distributed Lock
                       |
                 ┌─────┴─────┐
                 ↓             ↓
              Success        Failure
                 |             |
                 ↓             ↓
          Process payment    Don't process
                 |
                 ↓
          Release lock
```

---

## 8. Releasing the Lock

After Server A finishes processing, the key should be deleted: `DEL lock:payment:101`. Then another server can acquire it.

However, there is an important problem with simply doing `DEL lock:payment:101`.

---

## 9. Why DEL Can Be Dangerous

Server A gets lock → A. Suppose the lock expires after 30 seconds because Server A is taking longer than expected. Now Server B gets the lock → B. But Server A is still running. Then Server A finishes and executes `DEL lock:payment:101`.

Oops! Server A has just deleted Server B's lock. That's dangerous.

---

## 10. Use a Unique Lock Token

Instead of `lock:payment:101 → serverA`, use something unique: `lock:payment:101 → 8f72a1c9-abc...`

Server A remembers its token. When releasing the lock, it should first check: **"Does this lock still belong to me?"**

Conceptually:

```text
IF Redis lock value == myToken
    DELETE lock
```

This check-and-delete should be atomic, typically using a small Lua script:

```lua
if redis.call("GET", KEYS[1]) == ARGV[1] then
    return redis.call("DEL", KEYS[1])
end

return 0
```

So Server A cannot accidentally delete Server B's lock.

---

## 11. Java/Spring Boot Example

```java
String lockKey = "lock:payment:" + paymentId;
String token = UUID.randomUUID().toString();

Boolean acquired = redisTemplate.opsForValue()
        .setIfAbsent(lockKey, token, Duration.ofSeconds(30));

if (Boolean.TRUE.equals(acquired)) {
    try {
        processPayment(paymentId);
    } finally {
        releaseLock(lockKey, token);
    }
} else {
    // Another server is processing this payment
}
```

`setIfAbsent(...)` is essentially Redis `SET key value NX EX 30`.

---

## 12. Distributed Lock vs Java synchronized

**Java `synchronized`** works only inside one JVM instance.

```text
             Load Balancer
              /         \
             ↓           ↓
        Server A      Server B
          JVM            JVM
```

Server A's lock doesn't automatically lock Server B.

**Redis Distributed Lock** — Redis is shared. Both servers see the same lock. Therefore Redis can coordinate locking across multiple application instances.

---

## 13. Important Interview Point

A distributed lock does **not** automatically make payment processing exactly-once.

Suppose Server A acquires lock, call payment service succeeds, then Server A crashes. The lock eventually expires. Then Server B acquires lock and processes payment again. Now we could still have duplicate processing.

Therefore, for payment systems, we generally also need **idempotency**.

For example `paymentId = 101` — the payment service/database can enforce: already processed → return existing result.

Think of it this way:

- **Distributed lock:** "Try to make only one server process it at a time."
- **Idempotency:** "Even if it gets processed again, don't perform the business operation twice."

For payments, idempotency is critical.

---

## 14. Redis Lock Interview Answer

If the interviewer asks: **"How would you prevent two servers from processing the same payment simultaneously?"**

> I would use a distributed lock using Redis. For a payment ID, I would create a lock key such as `lock:payment:101` and acquire it using `SET key value NX EX 30`. NX ensures only one server can acquire the lock, while EX prevents the lock from staying forever if the server crashes. The lock value should contain a unique token, and while releasing the lock I would verify that the token still belongs to the current server before deleting it. For a payment system, I would also use idempotency because a distributed lock alone cannot guarantee exactly-once processing.
