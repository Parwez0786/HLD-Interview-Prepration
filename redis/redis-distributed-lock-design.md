# Day 19 — Correct Redis Distributed Lock Design

Your notes are correct. Let's understand why each part is necessary and how you would explain it in an interview.

---

## Table of Contents

- [1. The Problem](#1-the-problem)
- [2. Lock Acquisition](#2-lock-acquisition)
- [3. Why Do We Need a Unique Lock Value?](#3-why-do-we-need-a-unique-lock-value)
- [4. Correct Lock Release](#4-correct-lock-release)
- [5. Why GET + DEL Is Still Dangerous](#5-why-get--del-is-still-dangerous)
- [6. Lua Script](#6-lua-script)
- [7. Lock Expiration](#7-lock-expiration)
- [8. Lock Renewal](#8-lock-renewal)
- [9. Complete Flow](#9-complete-flow)
- [10. Interview Answer](#10-interview-answer)

---

## 1. The Problem

Suppose we have two application servers:

```text
        Payment Request
              |
       -----------------
       |               |
   Server A         Server B
       |               |
       v               v
   Process 101     Process 101
```

Both servers receive the same `paymentId = 101`.

Without a distributed lock, both process payment 101. This can cause duplicate processing.

We want only one server to process the payment at a time.

---

## 2. Lock Acquisition

We can use Redis:

```text
SET lock:payment:101 request-A NX EX 30
```

- `lock:payment:101` → lock key
- `request-A` → unique lock owner ID
- `NX` → create only if key doesn't exist
- `EX 30` → automatically expire after 30 seconds

If Server A executes it first: Redis `lock:payment:101 → request-A`. Server A successfully owns the lock.

If Server B tries: Redis returns `(nil)` because the key already exists.

So: Server A → Lock acquired. Server B → Lock rejected.

---

## 3. Why Do We Need a Unique Lock Value?

This is one of the most important points.

Suppose Server A acquires lock (`lock:payment:101 = request-A`). 30 seconds pass. Lock expires.

Now Server B acquires the same lock: `lock:payment:101 = request-B`.

But Server A doesn't know that its old lock has expired.

If Server A now blindly executes `DEL lock:payment:101`, it will delete Server B's lock.

```text
Server A's lock expired
        ↓
Server B acquired lock
        ↓
Server A executes DEL
        ↓
Server B's lock deleted ❌
```

Therefore, we store a unique value. The value tells us who owns the lock.

---

## 4. Correct Lock Release

Before deleting the lock, we need to check: **Does the current lock value belong to me?**

```text
GET lock:payment:101
        |
        v
Is value == request-A?
      /     \
    Yes      No
     |        |
    DEL     Don't delete
```

If Redis contains `lock:payment:101 = request-A`, Server A can release it.

If `lock:payment:101 = request-B`, Server A must not delete it.

---

## 5. Why GET + DEL Is Still Dangerous

You might think:

```text
value = GET lock:payment:101

if value == request-A:
    DEL lock:payment:101
```

But this has a race condition.

```text
Server A                  Redis
   |                        |
   | GET lock               |
   |----------------------->|
   | request-A              |
   |<-----------------------|
   |
   |       Server B acquires lock
   |              |
   |              v
   |
   | DEL lock
   |----------------------->|
```

The lock could change between GET and DEL.

Therefore, the check and delete must happen **atomically**.

---

## 6. Lua Script

Redis Lua scripts execute atomically.

A common release script is:

```lua
if redis.call("GET", KEYS[1]) == ARGV[1] then
    return redis.call("DEL", KEYS[1])
else
    return 0
end
```

Parameters: `KEYS[1] = lock:payment:101`, `ARGV[1] = request-A`.

Redis effectively performs: if current owner == me → DELETE, as one atomic operation.

No other Redis command can execute in the middle of that script.

---

## 7. Lock Expiration

Why do we use `EX 30`? Because the server might crash.

Without expiration, the lock could remain forever.

With `EX 30`, Redis automatically removes it after 30 seconds. Then another server can acquire it.

```text
Server A crashes
       ↓
30 seconds
       ↓
Lock expires
       ↓
Server B can acquire
```

This is called **crash handling through lock expiration**.

---

## 8. Lock Renewal

What if the operation legitimately takes longer than 30 seconds?

Lock TTL = 30 seconds. Payment processing takes 40 seconds. The lock expires at 30 seconds while Server A is still processing. Server B could acquire the lock. Now both could process the payment.

Therefore, for long-running operations, we may need **lock renewal**.

```text
Initial TTL = 30 sec

Server A
   |
   | acquire
   |
   | 20 sec
   ↓
renew → TTL 30 sec
   |
   | 20 sec
   ↓
renew → TTL 30 sec
```

A background mechanism periodically extends the lock only if the server still owns it. Again, ownership must be checked.

---

## 9. Complete Flow

```text
             Request
                |
                v
        Generate unique ID
                |
                v
 SET lock:payment:101 ID NX EX 30
                |
          +-----+-----+
          |           |
       Success      Failure
          |           |
          v           v
    Process payment  Retry/
          |          reject
          |
          v
     Processing done
          |
          v
 Lua script:
 Check owner
     |
     +---- Yes → DEL
     |
     +---- No  → Don't delete
```

---

## 10. Interview Answer

If the interviewer asks: **"How would you implement a distributed lock using Redis?"**

> I would use Redis SET with NX and an expiration time to acquire the lock. The lock value would contain a unique request or owner ID. NX ensures that only one server can acquire the lock, while the expiration prevents a crashed server from holding the lock forever. When releasing the lock, I wouldn't blindly call DEL because the original lock might have expired and another server might have acquired it. I would use a Lua script to atomically check that the lock value still belongs to me and then delete it. For long-running operations, I would also consider lock renewal.

That is a solid SDE-1 interview-level explanation.

### Key things to remember

| Piece | Role |
| --- | --- |
| `NX` | Only one server acquires |
| `EX` | Prevents permanent lock |
| Unique ID | Identifies lock owner |
| Lua | Atomic check + release |
| Renewal | Handles long-running work |
| Crash | Expiration handles abandoned locks |

### Golden rule

> Never blindly `DEL` a distributed lock.
