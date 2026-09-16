# Day 20 — Redlock and Distributed Lock Interview

Today we connect everything from Day 18–19 and understand how Redis locks behave when Redis itself is distributed.

The most important interview point is:

> A Redis lock is a coordination mechanism, not a complete correctness mechanism.

For critical operations like payments, orders, or money transfers, combine locking with idempotency, database constraints, transactions, and version/fencing checks.

---

## Table of Contents

- [1. Why Do We Need a Distributed Lock?](#1-why-do-we-need-a-distributed-lock)
- [2. Redis Single-Node Lock](#2-redis-single-node-lock)
- [3. Why Do We Need a Unique Lock Value?](#3-why-do-we-need-a-unique-lock-value)
- [4. The Problem With a Single Redis Node](#4-the-problem-with-a-single-redis-node)
- [5. Redis Replication](#5-redis-replication)
- [6. Replication Failure Scenario](#6-replication-failure-scenario)
- [7. Enter Redlock](#7-enter-redlock)
- [8. High-Level Redlock Flow](#8-high-level-redlock-flow)
- [9. What Is a Lease?](#9-what-is-a-lease)
- [10. Why Lease Expiration Is Dangerous](#10-why-lease-expiration-is-dangerous)
- [11. Fencing Tokens](#11-fencing-tokens)
- [12. Why Fencing Is Powerful](#12-why-fencing-is-powerful)
- [13. Example: Payment System](#13-example-payment-system)
- [14. Redis Lock + Database Constraint](#14-redis-lock--database-constraint)
- [15. Idempotency](#15-idempotency)
- [16. Redis Lock vs Idempotency](#16-redis-lock-vs-idempotency)
- [17. Redis Lock vs Database Constraint](#17-redis-lock-vs-database-constraint)
- [18. Should We Always Use Redlock?](#18-should-we-always-use-redlock)
- [19. Single Redis vs Replication vs Redlock](#19-single-redis-vs-replication-vs-redlock)
- [20. Interview Scenario](#20-interview-scenario)
- [21. Common Interview Cross-Questions](#21-common-interview-cross-questions)
- [22. The Mental Model to Remember](#22-the-mental-model-to-remember)

---

## 1. Why Do We Need a Distributed Lock?

Suppose we have two application servers:

```text
          Request: Payment 101
                  |
          +-------+-------+
          |               |
          v               v
      Server A         Server B
          |               |
          +-------+-------+
                  |
             Payment DB
```

Both servers may receive the same payment request.

Without a lock, both process payment. This could cause duplicate processing.

A distributed lock tries to ensure: only one server owns the lock → processes the resource.

---

## 2. Redis Single-Node Lock

The basic Redis lock is:

```text
SET lock:payment:101 unique-request-id NX EX 30
```

- `lock:payment:101` → lock key
- `unique-request-id` → owner identity
- `NX` → create only if key doesn't exist
- `EX 30` → expire after 30 seconds

If Redis returns `OK`, the server acquired the lock. If `(nil)`, someone else owns the lock.

Server A: `SET lock:payment:101 request-A NX EX 30` → Success. Redis `lock:payment:101 → request-A`.

Server B: `SET lock:payment:101 request-B NX EX 30` → Fails.

So: Server A → owns lock. Server B → cannot acquire.

---

## 3. Why Do We Need a Unique Lock Value?

This is extremely important.

Suppose Server A acquires lock, TTL = 30 seconds. Then Server A becomes slow. After 30 seconds, lock expires. Server B acquires the same lock: `lock:payment:101 → request-B`.

But Server A wakes up. If Server A blindly does `DEL lock:payment:101`, it deletes Server B's lock. That's dangerous.

Therefore: **lock value = unique owner/request ID**.

Before releasing: Is Redis value still `request-A`? If yes → DEL. Otherwise → do nothing.

This check should be atomic, commonly using a Lua script.

---

## 4. The Problem With a Single Redis Node

Application → Redis. Redis crashes. The lock information may become unavailable.

So we introduce Redis replication.

---

## 5. Redis Replication

```text
             Redis Primary
                  |
             replication
                  |
                  v
             Redis Replica
```

Application writes the lock to the primary: `SET lock:payment:101 request-A NX EX 30`. Primary accepts it. Eventually the replica receives the update.

But replication is generally **asynchronous**. That creates a race.

---

## 6. Replication Failure Scenario

Server A obtains `lock:payment:101 → A` on the primary. Before the replica receives that information, Redis Primary crashes. Replica becomes primary. The new primary might not know about the lock.

Then Server B executes `SET lock:payment:101 B NX EX 30` on the new primary. It succeeds.

Now: Server A believes it owns lock. Server B believes it owns lock.

This is the **fundamental problem**.

---

## 7. Enter Redlock

Redlock is a Redis-based distributed locking algorithm designed to obtain a lock across **multiple independent Redis instances**.

```text
              Application
                  |
        +---------+---------+
        |         |         |
        v         v         v
     Redis 1   Redis 2   Redis 3
        |         |         |
     Redis 4   Redis 5   ...
```

The idea is to attempt acquiring the same lock on multiple independent Redis nodes.

Example: Redis 1 acquired, 2 acquired, 3 acquired, 4 failed, 5 acquired.

If a majority is acquired within the required time, the client considers the lock acquired.

With 5 nodes: Majority = 3. So 3/5 can constitute successful acquisition under the algorithm's timing assumptions.

---

## 8. High-Level Redlock Flow

Client generates `random value = request-A`. Then attempts `SET lock:payment:101 request-A NX PX 30000` against the Redis instances.

Suppose R1–R3, R5 success, R4 failure → 4/5 successful. Majority requirement is satisfied.

But there is another important consideration: **Time**.

You cannot simply say "I got 3 out of 5, therefore I'm safe for 30 seconds." You must also consider the remaining validity period of the lease.

---

## 9. What Is a Lease?

A distributed lock is usually a **lease**.

```text
Lock acquired
    |
    v
30-second lease
    |
    v
Lock expires
```

**Lock** sounds permanent. **Lease** means: "You are allowed to own this resource until this time."

Example: 10:00:00 → acquire, 10:00:30 → expiration. After 10:00:30, another server may acquire the lock.

---

## 10. Why Lease Expiration Is Dangerous

Server A acquires a 30-second lease. Then Server A experiences GC pause, network delay, CPU starvation, process pause. Suppose it wakes up after 35 seconds.

Meanwhile at t = 30 sec, lock expires. Server B acquires lock.

Now: Server A → old owner. Server B → current owner.

This is why lock ownership alone isn't enough for critical systems.

---

## 11. Fencing Tokens

This is one of the most important advanced concepts.

A fencing token is a **monotonically increasing number** given to each new lock holder.

Example: Server A acquires lock, token = 100. Later A's lease expires. Server B acquires token = 101.

Suppose A was paused and later resumes. A sends WRITE payment, token = 100.

The protected system can say: Current token = 101. 100 < 101 → REJECT.

Server B's operation token = 101 is accepted.

---

## 12. Why Fencing Is Powerful

Without fencing: A (old lock holder) and B (new lock holder) — A can still perform operation.

With fencing: A token 100, B token 101. Database/resource: accept token >= current token, reject older token.

Old server → stale operation → REJECTED. This protects against stale clients.

---

## 13. Example: Payment System

`paymentId = 101`. Two servers accidentally process it.

Server A lock acquired, token = 100. Starts processing. Then A pauses.

A's lease expires. B acquires lock, token = 101. B processes payment.

Then A wakes up. A tries `UPDATE payment SET status = SUCCESS WHERE payment_id = 101`.

If there is no fencing/version protection, A could potentially overwrite newer state.

With versioning: payment version = 101, A version = 100 → Reject A.

---

## 14. Redis Lock + Database Constraint

For financial systems, don't rely only on Redis.

Database can enforce `UNIQUE(payment_id)` or maintain a state machine:

```text
CREATED
   ↓
PROCESSING
   ↓
SUCCESS
```

and reject invalid transitions. For example SUCCESS → SUCCESS should not result in another payment being executed.

---

## 15. Idempotency

Client sends `POST /payments` with `Idempotency-Key: abc123`.

The server stores `abc123 → payment result`. If the same request arrives again, return existing result instead of processing again.

This is often more important for correctness than trying to make a distributed lock perfect.

---

## 16. Redis Lock vs Idempotency

These solve different problems.

**Distributed lock** protects concurrent execution: Who can execute right now?

**Idempotency** protects against duplicate requests: Have I already processed this request?

You may use both:

```text
Request
   |
   v
Idempotency check
   |
   v
Acquire lock
   |
   v
Database transaction
   |
   v
Process
   |
   v
Store result
```

---

## 17. Redis Lock vs Database Constraint

Again, different responsibilities.

- **Redis** — Coordination
- **Database** — Durable correctness

For example: Redis lock + DB UNIQUE constraint + Idempotency + Transaction gives multiple layers of protection.

---

## 18. Should We Always Use Redlock?

No.

This is a very good interview answer:

> I wouldn't blindly say Redlock solves every distributed locking problem. For critical workflows, I would evaluate the failure model and use durable correctness mechanisms such as database constraints, idempotency, transactions, and fencing/version checks. Redis can provide coordination, but I wouldn't make it the only protection against duplicate financial operations.

That's a much stronger answer than: "Redlock guarantees exactly one server will always execute."

---

## 19. Single Redis vs Replication vs Redlock

| Approach | Main idea |
| --- | --- |
| Single Redis lock | One Redis instance manages lock |
| Redis + replica | Primary replicates lock state |
| Redlock | Acquire lock across multiple independent Redis instances |
| Fencing token | Prevent stale lock holders from modifying protected resource |
| DB constraint | Durable correctness at database level |
| Idempotency | Prevent duplicate request effects |

---

## 20. Interview Scenario

**Interviewer:** Two servers are processing the same payment. How would you prevent duplicate processing?

> I would first make the payment operation idempotent using an idempotency key. For concurrent processing, I can use a distributed lock such as Redis: `SET lock:payment:101 unique-id NX EX 30`. The lock value identifies the owner, and release should happen only if the stored value still matches my ownership token. If Redis is distributed, I would consider the failure characteristics of replication and approaches such as Redlock, but I wouldn't rely on the lock alone for financial correctness. At the database layer, I would use transactions, unique constraints, valid state transitions, and, where stale clients are a concern, fencing or version checks.

So the overall design is:

```text
             Payment Request
                    |
                    v
             Idempotency Check
                    |
                    v
            Distributed Lock
                    |
                    v
             DB Transaction
                    |
          +---------+---------+
          |                   |
          v                   v
   State/Version Check    DB Constraint
          |                   |
          +---------+---------+
                    |
                    v
              Process Payment
```

---

## 21. Common Interview Cross-Questions

### Q1. Why not simply use SETNX?

Because lock acquisition needs an expiration to handle crashed clients.

Prefer `SET key value NX EX 30` rather than separate `SETNX` + `EXPIRE`, because the latter can leave a lock without expiration if the process fails between the commands.

### Q2. Why store a unique value in the lock?

Because when the lease expires and another server acquires the lock, the old owner must not delete the new owner's lock.

### Q3. Why is DEL dangerous?

A gets lock → lock expires → B gets lock → A executes DEL → A deletes B's lock.

### Q4. What happens if the lock holder crashes?

The lease eventually expires: A crashes → TTL expires → another server can acquire. This is why expiration is important.

### Q5. What if the application pauses for longer than TTL?

The application may continue executing after losing its lease. That's why critical systems may need fencing tokens + version checks.

### Q6. Is Redis lock enough for payments?

No. Use multiple layers: Idempotency + DB transaction + DB constraints + state/version checks + optional distributed lock.

### Q7. What is the key difference between a lock and fencing?

A lock says: "You currently have permission."

A fencing token lets the protected resource verify: "Is your permission newer than the previous owner's?"

That second property protects against stale clients.

---

## 22. The Mental Model to Remember

```text
                Distributed Lock
                       |
        +--------------+--------------+
        |                             |
   Redis lock                    Lease/TTL
        |                             |
        v                             v
   Ownership                    Expiration
        |
        v
   Unique token
        |
        v
 Atomic release
        |
        v
 Multiple Redis nodes
        |
        v
     Redlock
        |
        v
 Fencing / Version
        |
        v
 Durable correctness
        |
   +----+----+---------+
   |         |         |
   v         v         v
Idempotency DB Txn  Constraints
```

### One-line interview summary

> Redis locks help coordinate distributed workers, Redlock addresses lock acquisition across multiple Redis instances, leases handle crashes, fencing tokens protect against stale owners, and critical financial correctness should ultimately be enforced with durable mechanisms such as idempotency, database transactions, constraints, and version checks.
