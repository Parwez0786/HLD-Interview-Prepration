# Concurrency + Concurrent System Design — SDE-1 Interview

For SDE-1 Backend/Java interviews, prepare concurrency in two layers:

1. **Concurrency fundamentals + coding**
2. **Concurrency system design** — how to make a backend system safe when many requests/threads execute simultaneously

The key is not memorizing `synchronized`, `volatile`, etc. You need to understand **what can go wrong** when multiple threads/requests access the same data, and **which mechanism fixes which problem**.

---

## Table of Contents

- [Part A — Interview Roadmap](#part-a--interview-roadmap)
- [1. First Understand the Core Idea](#1-first-understand-the-core-idea)
- [2. Topics You Should Prepare](#2-topics-you-should-prepare)
- [3. Critical Section](#3-critical-section)
- [4. Mutex / Lock](#4-mutex--lock)
- [5. Atomic Variables](#5-atomic-variables)
- [6. volatile](#6-volatile)
- [7. Thread Safety](#7-thread-safety)
- [8. Deadlock](#8-deadlock)
- [9. Starvation](#9-starvation)
- [10. Livelock](#10-livelock)
- [11. Producer-Consumer](#11-producer-consumer)
- [12. Thread Pool](#12-thread-pool)
- [13. Concurrent Collections](#13-concurrent-collections)
- [14. Database Concurrency](#14-database-concurrency)
- [15. Optimistic Locking](#15-optimistic-locking)
- [16. Pessimistic Locking](#16-pessimistic-locking)
- [17. Isolation Levels](#17-isolation-levels)
- [18. Distributed Concurrency](#18-distributed-concurrency)
- [19. Distributed Lock](#19-distributed-lock)
- [20. Idempotency](#20-idempotency)
- [21. Concurrency System-Design Framework](#21-concurrency-system-design-interview-framework)
- [22. Example: Inventory Reservation](#22-example-interview-problem)
- [23. Kafka Concurrency](#23-kafka-concurrency)
- [24. Kafka Duplicate Processing](#24-kafka-duplicate-processing)
- [25. Interview Cheat Sheet](#25-your-interview-cheat-sheet)
- [Part B — In-Depth Tutorial](#part-b--in-depth-tutorial)
- [B1. What is Concurrency?](#b1-what-is-concurrency)
- [B2. Why Do We Need Concurrency?](#b2-why-do-we-need-concurrency)
- [B3. Shared State](#b3-shared-state)
- [B4. Race Condition](#b4-race-condition)
- [B5. Critical Section](#b5-critical-section)
- [B6. Mutex / Lock](#b6-mutex--lock)
- [B7. synchronized](#b7-synchronized)
- [B8. Object Lock vs Class Lock](#b8-object-lock-vs-class-lock)
- [B9. ReentrantLock](#b9-reentrantlock)
- [B10. Semaphore](#b10-semaphore)
- [B11. volatile](#b11-volatile)
- [B12. Atomic Variables](#b12-atomic-variables)
- [B13. CAS](#b13-cas)
- [B14. Atomicity vs Visibility](#b14-atomicity-vs-visibility)
- [B15. Thread Safety](#b15-thread-safety)
- [B16. Immutability](#b16-immutability)
- [B17. Deadlock](#b17-deadlock)
- [B18. Four Conditions for Deadlock](#b18-four-conditions-for-deadlock)
- [B19. Deadlock Prevention](#b19-deadlock-prevention)
- [B20. tryLock() for Deadlock Avoidance](#b20-trylock-for-deadlock-avoidance)
- [B21. Starvation](#b21-starvation)
- [B22. Livelock](#b22-livelock)
- [B23. Thread Pool](#b23-thread-pool)
- [B24. Thread Pool Internals](#b24-thread-pool-internals)
- [B25. Producer-Consumer](#b25-producer-consumer)
- [B26. Why Bounded Queues?](#b26-why-bounded-queues)
- [B27. ConcurrentHashMap](#b27-concurrenthashmap)
- [B28. Database Concurrency](#b28-database-concurrency)
- [B29. Lost Update](#b29-lost-update)
- [B30. Database Transactions](#b30-database-transactions)
- [B31. Isolation](#b31-isolation)
- [B32. Optimistic Locking](#b32-optimistic-locking)
- [B33. When Optimistic Locking Is Useful](#b33-when-optimistic-locking-is-useful)
- [B34. Pessimistic Locking](#b34-pessimistic-locking)
- [B35. Atomic Database Update](#b35-atomic-database-update)
- [B36. Distributed Lock](#b36-distributed-lock)
- [B37. Idempotency](#b37-idempotency)
- [B38. Kafka Concurrency](#b38-kafka-concurrency)
- [B39. Kafka Ordering](#b39-kafka-ordering)
- [B40. Kafka Duplicate Processing](#b40-kafka-duplicate-processing)
- [B41. Exactly Once vs At Least Once](#b41-exactly-once-vs-at-least-once)
- [B42. Concurrency in a Spring Boot Backend](#b42-concurrency-in-a-spring-boot-backend)
- [B43. Restaurant Table Booking](#b43-a-real-interview-example)
- [B44. Naive Solution](#b44-naive-solution)
- [B45. Better Solution](#b45-better-solution)
- [B46. Concurrency Design Checklist](#b46-concurrency-design-checklist)
- [B47. The Most Important Mental Model](#b47-the-most-important-mental-model)
- [B48. What to Prepare](#b48-what-i-would-prepare-for-your-interview)
- [The 5 Problems You Should Be Able to Solve](#the-5-problems-you-should-be-able-to-solve)

---

# Part A — Interview Roadmap

## 1. First Understand the Core Idea

Suppose 1,000 users try to book the same restaurant table at exactly the same time.

Without concurrency control:

```text
Thread 1 → sees table available
Thread 2 → sees table available
Thread 3 → sees table available

Thread 1 → books table
Thread 2 → books table
Thread 3 → books table
```

You have **one table but three bookings**.

Concurrency design is about ensuring that **shared state remains correct** when multiple operations happen simultaneously.

---

## 2. Topics You Should Prepare

### Level 1 — Basic concepts

**Threads**

```text
Process
  |
  +-- Thread 1
  +-- Thread 2
  +-- Thread 3
```

Threads inside the same process **share memory**.

**Race condition**

```java
int count = 0;

Thread 1: count++;
Thread 2: count++;
```

`count++` is actually approximately: read count → add 1 → write count.

Both threads can read the same value.

Expected: `count = 2`. Actual: `count = 1`.

This is a race condition.

---

## 3. Critical Section

A critical section is code accessing **shared mutable data**.

```java
balance = balance - 100;
```

If multiple threads can execute this simultaneously, you need synchronization.

```text
Thread 1
   |
   v
[ Critical Section ]
   ^
   |
Thread 2
```

Only one should modify the shared state at a time if the operation requires **mutual exclusion**.

---

## 4. Mutex / Lock

Java:

```java
synchronized(lock) {
    balance -= 100;
}
```

or:

```java
Lock lock = new ReentrantLock();

lock.lock();

try {
    balance -= 100;
} finally {
    lock.unlock();
}
```

Know: `synchronized`, `ReentrantLock`, `tryLock()`, `ReadWriteLock`, `Condition`.

---

## 5. Atomic Variables

For simple operations:

```java
AtomicInteger count = new AtomicInteger(0);

count.incrementAndGet();
```

Understand **CAS — Compare And Swap**.

Conceptually: current value = 10, expected = 10, new value = 11.

```text
CAS:
If current == expected
       update to 11
else
       retry/fail
```

Important classes: `AtomicInteger`, `AtomicLong`, `AtomicBoolean`, `AtomicReference`.

---

## 6. volatile

Very common interview question.

```java
volatile boolean running = true;
```

`volatile` mainly provides **visibility**, not general atomicity.

Example:

```java
Thread 1:
running = false;

Thread 2:
while (running) {
}
```

Without proper visibility guarantees, Thread 2 may not observe the update as expected.

But:

```java
volatile int count;
count++;
```

is **not atomic**, because read + write are separate operations.

---

## 7. Thread Safety

A class is thread-safe when multiple threads can use it concurrently without producing incorrect state.

```java
class Counter {
    private int count;

    public synchronized void increment() {
        count++;
    }
}
```

You should be able to explain: the lock protects the shared mutable state, so only one thread can execute the critical section at a time.

---

## 8. Deadlock

This is especially important given the interview question you recently encountered.

Classic example:

```text
Thread 1                    Thread 2

Lock A                      Lock B
  ↓                           ↓
waiting for B              waiting for A
```

Neither can proceed.

**Four conditions:**

1. **Mutual exclusion** — resource can be held by only one thread
2. **Hold and wait** — thread holds one resource while waiting for another
3. **No preemption** — resource cannot simply be taken away
4. **Circular wait** — T1 waits for T2, T2 waits for T1

**Prevention:** use a consistent lock ordering. Always acquire Lock A → Lock B. Never Thread 1: A → B and Thread 2: B → A.

---

## 9. Starvation

A thread keeps waiting because other threads repeatedly get the resource.

```text
T1 ───────────── waiting ─────────────
T2 → lock → release
T3 → lock → release
T4 → lock → release

T1 never gets a chance.
```

---

## 10. Livelock

Threads are active but don't make progress.

Example: Person A moves left, Person B moves left. A moves right, B moves right. Both keep reacting to each other.

No deadlock because they aren't blocked. But no progress either.

---

## 11. Producer-Consumer

Very important for backend systems.

```text
Producer
   |
   v
+---------+
|  Queue  |
+---------+
   |
   v
Consumer
```

```java
BlockingQueue<Task> queue =
        new ArrayBlockingQueue<>(1000);

// Producer
queue.put(task);

// Consumer
Task task = queue.take();
```

Understand: bounded queue, backpressure, blocking, producer faster than consumer, consumer failure.

---

## 12. Thread Pool

Never create unlimited threads for every request.

```text
Incoming Requests
       |
       v
+----------------+
| Thread Pool    |
|                |
| T1 T2 T3 T4    |
+----------------+
       |
       v
    Database
```

```java
ExecutorService pool =
    Executors.newFixedThreadPool(10);
```

Understand: core threads, maximum threads, queue, rejection policy, task lifecycle, shutdown.

---

## 13. Concurrent Collections

Know these: `ConcurrentHashMap`, `CopyOnWriteArrayList`, `BlockingQueue`, `ConcurrentLinkedQueue`.

Especially `ConcurrentHashMap`:

```java
ConcurrentHashMap<String, Integer> map;
```

Useful when multiple threads read/write a map.

Understand why `HashMap` is unsafe for concurrent modification without synchronization.

---

## 14. Database Concurrency

This is where concurrency becomes **system design**.

Suppose inventory = 1. Two requests arrive: Request A → buy, Request B → buy.

Both read inventory = 1. Both think they can buy. Now inventory = -1, or two orders get created.

You need **database concurrency control**.

---

## 15. Optimistic Locking

Very important.

Table `Product`: `id`, `stock`, `version`.

Initial: stock = 10, version = 5.

Request reads stock = 10, version = 5.

```sql
UPDATE product
SET stock = 9,
    version = 6
WHERE id = 1
AND version = 5;
```

If another request already updated it, version = 6, then `UPDATE ... WHERE version = 5` updates **0 rows**.

We know someone changed the record.

This is **optimistic concurrency control**.

---

## 16. Pessimistic Locking

Database locks the row.

```sql
SELECT *
FROM product
WHERE id = 1
FOR UPDATE;
```

Conceptually:

```text
Transaction A
     |
     v
Lock product row
     |
     v
Update stock
     |
     v
Commit
     |
     v
Unlock

Transaction B waits.
```

---

## 17. Isolation Levels

You should know: `READ UNCOMMITTED`, `READ COMMITTED`, `REPEATABLE READ`, `SERIALIZABLE`.

And anomalies: Dirty Read, Non-repeatable Read, Phantom Read, Lost Update.

For SDE-1, understand the **practical meaning** rather than memorizing definitions.

---

## 18. Distributed Concurrency

This is a major system-design topic.

```text
              Load Balancer
                   |
          +--------+--------+
          |                 |
       Server 1          Server 2
          |                 |
          +--------+--------+
                   |
                 Redis
                   |
                Database
```

This **won't work**:

```java
synchronized(lock) {
    bookTable();
}
```

Why? Because `synchronized` only protects threads inside **one JVM**. Server 1 and Server 2 have different JVMs.

---

## 19. Distributed Lock

You may use Redis for coordination:

```text
Server 1
   |
   | acquire lock
   v
Redis
   |
   | LOCK:TABLE:123
   |
Server 2
   |
   | acquire lock
   X
```

But don't blindly depend on Redis locks for critical correctness.

For important operations, combine coordination with: Database constraints + Transactions + Idempotency + Version/fencing checks.

---

## 20. Idempotency

Extremely important in backend interviews.

Suppose payment request is sent twice (`POST /payment`) due to timeout:

```text
Client → Server
        ↓
      Payment
        ↓
      timeout

Client retries
        ↓
      Payment
```

Without protection: ₹100 deducted, ₹100 deducted.

Use `Idempotency-Key = ABC123`. Database: `ABC123 → SUCCESS`. Second request sees the existing operation and returns the same result.

---

## 21. Concurrency System-Design Interview Framework

When interviewer gives you a problem involving concurrency, use this structure:

### Step 1 — Identify shared resource

What resource can be accessed simultaneously? Examples: seat, inventory, wallet balance, restaurant table, bank account, order, coupon.

### Step 2 — Identify race condition

What happens if two requests execute at exactly the same time?

```text
Stock = 1

Request A → read 1
Request B → read 1

A → buy
B → buy
```

### Step 3 — Decide consistency requirement

Can we tolerate temporary inconsistency? If no, use stronger synchronization/transactional mechanisms.

### Step 4 — Choose locking strategy

```text
In-process lock
        ↓
Database lock
        ↓
Optimistic locking
        ↓
Distributed lock
        ↓
Atomic DB operation
```

### Step 5 — Handle failures

Always discuss: server crash, network timeout, lock expiration, duplicate request, retry, consumer crash, database failure.

### Step 6 — Make operations idempotent

Especially for: payments, orders, bookings, Kafka consumers, external APIs.

---

## 22. Example Interview Problem

**Design an inventory reservation system.**

Product A, stock = 1. Two users simultaneously buy it.

```text
             Load Balancer
                  |
        +---------+---------+
        |                   |
     Server 1            Server 2
        |                   |
        +---------+---------+
                  |
               Database
                  |
          Product Inventory
```

Atomic update:

```sql
UPDATE product
SET stock = stock - 1
WHERE product_id = 101
AND stock > 0;
```

Then check: `rows affected == 1` → success, order created. `rows affected == 0` → out of stock.

This is often cleaner than putting a distributed lock around everything.

---

## 23. Kafka Concurrency

Since you are preparing Kafka, understand this very well.

```text
Topic
Partition 0
Partition 1
Partition 2
Partition 3

Consumer group:

Consumer 1 → P0
Consumer 2 → P1
Consumer 3 → P2
Consumer 4 → P3
```

Kafka provides ordering **within a partition**.

So if order matters for a user, produce with `key = userId`. Then events for that user go to the same partition.

But multiple consumers can process different partitions concurrently.

---

## 24. Kafka Duplicate Processing

Important interview question:

```text
Consumer receives event
       ↓
Processes event
       ↓
Database updated
       ↓
Consumer crashes BEFORE offset commit
       ↓
Kafka sends event again
```

Now the event is processed twice.

**Solution:** idempotent consumer. For example `eventId = ABC123`. Database `processed_events` with `event_id PRIMARY KEY`. If ABC123 already exists, don't process again.

---

## 25. Your Interview Cheat Sheet

Before your interview, be comfortable explaining these:

Thread, Process, Race condition, Critical section, Mutex, Semaphore, `synchronized`, `ReentrantLock`, `volatile`, `AtomicInteger`, CAS, Thread safety, Deadlock, Starvation, Livelock, Producer-consumer, `BlockingQueue`, Thread pool, `ExecutorService`, `ConcurrentHashMap`, Database transactions, Isolation levels, Optimistic locking, Pessimistic locking, `SELECT FOR UPDATE`, Distributed locks, Redis locks, Idempotency, Atomic database operations, Kafka concurrency, Kafka partition ordering, Consumer groups, Duplicate processing, Exactly-once vs at-least-once, Backpressure.

### Most important for upcoming SDE-1 interviews

I'd prioritize:

> Race condition → locks → deadlock → thread safety → thread pool → producer/consumer → database locking → optimistic locking → distributed locking → idempotency → Kafka concurrency.

If you can explain those with one concrete example each, you'll be well prepared.

---

# Part B — In-Depth Tutorial

## B1. What is Concurrency?

Concurrency means multiple tasks are making progress during **overlapping time periods**.

For example:

```text
Thread 1 → Process Order A
Thread 2 → Process Order B
Thread 3 → Process Order C
```

On a multicore CPU, they may literally execute at the same time:

```text
CPU Core 1 → Thread 1
CPU Core 2 → Thread 2
CPU Core 3 → Thread 3
```

Even on a single core, the OS can switch between threads: `T1 → T1 → T2 → T1 → T3 → T2 → ...`

So your program must work correctly regardless of the exact execution order.

---

## B2. Why Do We Need Concurrency?

Suppose your backend receives 1000 requests/second.

You don't want Request 1 finish → Request 2 finish → Request 3.

Instead:

```text
Request 1 ────────┐
Request 2 ────────┤
Request 3 ────────┼──→ concurrently
Request 4 ────────┤
Request 5 ────────┘
```

Concurrency improves: throughput, resource utilization, responsiveness, scalability.

But it introduces a major problem: **multiple execution paths can access the same state at the same time.**

---

## B3. Shared State

This is the foundation of concurrency.

```java
class Counter {
    int count = 0;
}
```

Suppose Thread A, B, C all access `count`. Then `count` is **shared mutable state**.

This is where most concurrency problems begin.

---

## B4. Race Condition

```java
int count = 0;
count++;
```

It looks like one operation. It isn't.

```text
count++
     ↓
READ count
     ↓
ADD 1
     ↓
WRITE count
```

Suppose count = 10. Two threads execute.

```text
Thread A
READ → 10
Thread B
READ → 10
Thread A
10 + 1 = 11
WRITE → 11
Thread B
10 + 1 = 11
WRITE → 11
```

Expected: 12. Actual: 11.

**Interview definition:** A race condition occurs when the result depends on the timing/interleaving of concurrent operations on shared state.

---

## B5. Critical Section

The piece of code that accesses shared state is called a critical section.

Example: `balance = balance - amount;`

If multiple threads execute this simultaneously, it may need protection.

```text
Thread A ──┐
           ↓
      [ Critical ]
      [ Section  ]
           ↑
Thread B ──┘
```

The goal is often: only one thread modifies this shared state at a time. This property is called **mutual exclusion**.

---

## B6. Mutex / Lock

A lock provides mutual exclusion.

```java
Lock lock = new ReentrantLock();

lock.lock();

try {
    balance -= 100;
} finally {
    lock.unlock();
}
```

```text
Thread A
   ↓
acquire lock
   ↓
modify balance
   ↓
release lock

Thread B
   ↓
wait
   ↓
acquire lock
   ↓
modify balance
```

Only one thread enters the critical section.

---

## B7. synchronized

Java provides a simpler mechanism:

```java
public synchronized void withdraw(int amount) {
    balance -= amount;
}
```

or:

```java
synchronized (lock) {
    balance -= amount;
}
```

**What does synchronized do?** It provides: mutual exclusion, visibility guarantees, happens-before relationships.

For an SDE-1 interview, say:

> `synchronized` ensures that only one thread at a time can execute the synchronized critical section for the same monitor, and it also provides memory visibility guarantees when entering and exiting the monitor.

---

## B8. Object Lock vs Class Lock

```java
class Counter {

    public synchronized void increment() {
        count++;
    }
}
```

This locks **this object**.

If `Counter c1 = new Counter();` and `Counter c2 = new Counter();` then `c1.increment()` locks c1, `c2.increment()` locks c2. They can execute concurrently.

But `static synchronized void increment()` locks the Class object (`Counter.class`). So all instances share that lock.

---

## B9. ReentrantLock

More flexible than `synchronized`.

```java
Lock lock = new ReentrantLock();

lock.lock();

try {
    // critical section
} finally {
    lock.unlock();
}
```

Why use it? It supports things such as `tryLock()`:

```java
if (lock.tryLock()) {
    try {
        // work
    } finally {
        lock.unlock();
    }
} else {
    // couldn't acquire lock
}
```

This is useful when you don't want a thread to wait forever.

---

## B10. Semaphore

A mutex allows **1 thread**. A semaphore can allow **N threads**.

```java
Semaphore semaphore = new Semaphore(3);
```

Maximum: 3 concurrent operations.

Imagine a database connection pool with 10 connections. A semaphore can control how many operations enter a resource simultaneously.

---

## B11. volatile

This is one of the most misunderstood Java keywords.

```java
volatile boolean running = true;
```

Thread 1: `running = false;` Thread 2: `while (running) { // work }`

`volatile` ensures **visibility** of writes between threads under Java's memory model.

But `volatile int count; count++;` is still unsafe, because READ + WRITE are separate operations.

`volatile` gives you visibility, not general compound-operation atomicity.

---

## B12. Atomic Variables

For simple atomic operations:

```java
AtomicInteger counter = new AtomicInteger(0);

counter.incrementAndGet();
```

Unlike `counter++`, `incrementAndGet()` is atomic.

Other classes: `AtomicInteger`, `AtomicLong`, `AtomicBoolean`, `AtomicReference`.

---

## B13. CAS

Atomic classes commonly rely on Compare-And-Swap style operations.

Imagine value = 10. You want 10 → 11.

CAS says: IF current value == 10, change to 11, ELSE fail/retry.

```text
         Current = 10
               |
               ↓
        Expected = 10?
          /          \
        YES           NO
         |             |
      update          retry
       11
```

This is a fundamental **lock-free** synchronization technique.

---

## B14. Atomicity vs Visibility

These are different.

**Visibility** — Thread B sees Thread A's update. A writes X = 10 → B sees X = 10.

**Atomicity** — an operation happens as one indivisible unit. Example: `count++` needs atomicity.

**Ordering** — operations are observed in an allowed order according to the memory model.

You should know these three terms well: Atomicity, Visibility, Ordering.

---

## B15. Thread Safety

A class is thread-safe if concurrent usage doesn't lead to incorrect behavior.

**Unsafe:**

```java
class Counter {
    private int count = 0;

    void increment() {
        count++;
    }
}
```

**Safe:**

```java
class Counter {
    private int count = 0;

    synchronized void increment() {
        count++;
    }
}
```

But don't automatically synchronize everything.

Locks have costs: contention, blocking, reduced parallelism, deadlock risk.

---

## B16. Immutability

One of the easiest ways to make code thread-safe is to avoid shared mutable state.

```java
final class User {
    private final String name;
    private final int age;
}
```

Once created, User cannot change. Multiple threads can safely read it.

This is why immutable objects are very valuable in concurrent systems.

---

## B17. Deadlock

This is extremely important for interviews.

Suppose Lock A, Lock B.

Thread 1: Acquire A, Acquire B. Thread 2: Acquire B, Acquire A.

```text
Thread 1                Thread 2

Acquire A               Acquire B
   ↓                        ↓
Wait for B              Wait for A
   ↓                        ↓
   └──────── DEADLOCK ──────┘
```

Neither can continue.

---

## B18. Four Conditions for Deadlock

Remember: **M-H-N-C**

1. **Mutual exclusion** — only one thread owns the resource
2. **Hold and wait** — thread holds one lock while waiting for another
3. **No preemption** — lock can't simply be forcibly taken away
4. **Circular wait** — T1 waits for T2, T2 waits for T1

All four together create the classic deadlock conditions.

---

## B19. Deadlock Prevention

The easiest practical technique: **consistent lock ordering**.

Always: Lock A → Lock B.

Never: Thread 1: A → B and Thread 2: B → A.

Instead both: A → B. Now circular waiting is avoided.

---

## B20. tryLock() for Deadlock Avoidance

Instead of waiting indefinitely:

```java
if (lock.tryLock()) {
    try {
        // work
    } finally {
        lock.unlock();
    }
}
```

You can: try → fail → release existing lock → retry later.

---

## B21. Starvation

Starvation means a thread keeps waiting for resources because other threads continually get access.

```text
T1 → waiting
T2 → gets resource
T3 → gets resource
T4 → gets resource
T5 → gets resource
...
T1 → still waiting
```

Unlike deadlock, the system may still be making progress.

---

## B22. Livelock

Threads aren't blocked. They're active. But they don't make progress.

Example: Person A moves left, Person B moves left. A moves right, B moves right. A left, B left. Both keep reacting.

---

## B23. Thread Pool

A backend server shouldn't create an unlimited number of threads.

**Bad:** Request 1 → new Thread, ... Request 1,000,000 → new Thread.

Eventually: CPU overloaded, memory exhausted, context switching, system becomes unstable.

Instead:

```text
Requests
   ↓
Thread Pool
   ↓
T1 T2 T3 T4 T5
```

```java
ExecutorService executor =
    Executors.newFixedThreadPool(10);
```

---

## B24. Thread Pool Internals

```text
             Tasks
               |
               v
        +---------------+
        |     Queue     |
        +---------------+
          ↓  ↓  ↓  ↓
        T1  T2  T3  T4
```

If all threads are busy: new task → queue → wait.

This creates a form of **backpressure**.

---

## B25. Producer-Consumer

Classic concurrency problem.

```text
Producer
    |
    v
+---------+
|  Queue  |
+---------+
    |
    v
Consumer
```

```java
BlockingQueue<Task> queue =
        new ArrayBlockingQueue<>(100);

queue.put(task);   // Producer
Task task = queue.take();  // Consumer
```

If the queue is full: producer waits. If queue is empty: consumer waits.

This is much safer than manually implementing wait/notify for many use cases.

---

## B26. Why Bounded Queues?

Suppose producer generates 100,000 tasks/sec. Consumer handles 10,000 tasks/sec.

An unlimited queue eventually becomes 1M, 10M, 100M, ... Memory gets exhausted.

A bounded queue (capacity = 10,000) forces the producer to slow down or reject tasks.

This is **backpressure**.

---

## B27. ConcurrentHashMap

Normal `HashMap<K,V>` is not designed for arbitrary concurrent modifications.

Java provides `ConcurrentHashMap<K,V>`:

```java
ConcurrentHashMap<String, Integer> map =
        new ConcurrentHashMap<>();
```

It provides thread-safe concurrent access with much better concurrency than simply synchronizing an entire map in many scenarios.

---

## B28. Database Concurrency

Now we move from thread concurrency to **system concurrency**.

Imagine an e-commerce system. Stock = 1. User A buy, User B buy.

Both requests reach different application servers:

```text
          Load Balancer
           /         \
          /           \
     Server A       Server B
          \           /
           \         /
             Database
```

`synchronized` won't help. Why? Because Server A JVM ≠ Server B JVM.

An in-memory Java lock only protects threads within one JVM/object scope.

---

## B29. Lost Update

Suppose balance = 1000. A wants to withdraw 100. B wants to withdraw 200.

Both read 1000. A calculates 900. B calculates 800. A writes 900. B writes 800.

Correct result should be **700**, but database contains **800**.

This is a concurrency problem at the database level.

---

## B30. Database Transactions

A transaction groups operations into a logical unit.

```text
BEGIN

Check balance
Deduct money
Create transaction record

COMMIT
```

If something fails: `ROLLBACK`.

You should know **ACID**:

- A → Atomicity
- C → Consistency
- I → Isolation
- D → Durability

---

## B31. Isolation

Isolation controls how concurrent transactions interact.

Common levels: `READ UNCOMMITTED`, `READ COMMITTED`, `REPEATABLE READ`, `SERIALIZABLE`.

Higher isolation generally provides stronger consistency but can reduce concurrency depending on the database/workload.

---

## B32. Optimistic Locking

Very important for interviews.

Suppose Product id = 1, stock = 10, version = 5.

Request A reads stock = 10, version = 5. Request B also reads stock = 10, version = 5.

A updates:

```sql
UPDATE product
SET stock = 9,
    version = 6
WHERE id = 1
AND version = 5;
```

Success. Now B tries the same `WHERE version = 5`. No row matches because version = 6.

So B knows: somebody changed the record after I read it. It can retry or return a conflict.

---

## B33. When Optimistic Locking Is Useful

Good when conflicts are relatively rare.

Example: profile editing, product metadata, document editing, booking updates with manageable contention.

You don't hold a database lock for the entire operation.

---

## B34. Pessimistic Locking

You lock the database row while working.

```sql
SELECT *
FROM product
WHERE id = 1
FOR UPDATE;
```

```text
Transaction A
     ↓
lock row
     ↓
update
     ↓
commit
     ↓
unlock

Transaction B
     ↓
wait
```

Useful when conflicts are common and you need serialized access to a row.

But locks can cause: contention, deadlocks, long waits.

---

## B35. Atomic Database Update

Sometimes the cleanest solution doesn't require an explicit lock.

For inventory:

```sql
UPDATE product
SET stock = stock - 1
WHERE product_id = 101
AND stock > 0;
```

Then: rows affected = 1 means reservation succeeded. Rows affected = 0 means out of stock.

This is a very powerful interview pattern:

> Push the concurrency-sensitive condition into one atomic database operation.

---

## B36. Distributed Lock

Now suppose Server A, B, C all need to coordinate around restaurant table #42.

You might use Redis:

```text
Server A
   |
   | acquire TABLE:42
   ↓
 Redis
   ↑
   |
Server B → cannot acquire
```

But there is an important interview point:

> A distributed lock should not be your only correctness mechanism for critical operations.

For financial/order/booking systems, also consider: Database transaction + Unique constraints + Idempotency + Optimistic/version checks + Atomic state transitions.

---

## B37. Idempotency

One of the most important concepts in distributed systems.

Suppose Client `POST /payment` → Server → Payment successful. But response gets lost. Client doesn't know whether payment succeeded. So it retries `POST /payment`. Now you could accidentally charge twice.

Use `Idempotency-Key: ABC123`.

Database: `ABC123 → payment result`.

First request: ABC123 not found → process payment → store result.

Second request: ABC123 found → return previous result.

---

## B38. Kafka Concurrency

This is particularly relevant for your backend preparation.

Topic `orders`: P0, P1, P2, P3.

Consumer group: Consumer 1 → P0, Consumer 2 → P1, Consumer 3 → P2, Consumer 4 → P3.

Kafka allows these partitions to be processed concurrently.

---

## B39. Kafka Ordering

Kafka guarantees ordering **within a partition**, not globally across all partitions.

Suppose `userId = 101`. Events: OrderCreated, PaymentDone, OrderShipped.

If ordering matters, use `key = userId`. Then those events can be routed to the same partition.

```text
user 101
   ↓
partition 2

OrderCreated
PaymentDone
OrderShipped
```

They maintain order within that partition.

---

## B40. Kafka Duplicate Processing

Very common interview scenario.

```text
Kafka
  ↓
Consumer
  ↓
Process event
  ↓
Database update
  ↓
Consumer crashes
  ↓
offset not committed
```

Kafka may deliver the message again. Therefore Event A, Event A can happen.

Your consumer should be designed to tolerate duplicate processing.

One approach: `processed_events` with `event_id PRIMARY KEY`. Then event ABC is processed once from the application's correctness perspective.

---

## B41. Exactly Once vs At Least Once

Understand the terminology.

| Term | Meaning |
| --- | --- |
| **At-most-once** | message → may be lost |
| **At-least-once** | message won't intentionally be lost, but may be processed multiple times |
| **Exactly-once** | stronger guarantees under a defined scope; don't casually claim the external world can never observe duplicates |

In interviews, a strong answer is:

> I would generally design the consumer to be idempotent because retries and duplicate delivery can still occur at system boundaries.

---

## B42. Concurrency in a Spring Boot Backend

```text
                Load Balancer
                     |
          +----------+----------+
          |                     |
      Spring Boot           Spring Boot
       Server A              Server B
          |                     |
          +----------+----------+
                     |
                  Database
                     |
                   Redis
                     |
                   Kafka
```

Request concurrency can happen at several levels:

```text
HTTP requests
     ↓
application threads
     ↓
database transactions
     ↓
Kafka consumers
     ↓
external services
```

You need to reason about every shared resource.

---

## B43. A Real Interview Example

**Problem:** Design a restaurant table booking system where multiple users may try to book the same table at the same time.

Start with: Restaurant → Table → Booking.

Suppose Table T1, 10:00–11:00, AVAILABLE.

Two users:

```text
User A ───────┐
              ├──→ Book T1
User B ───────┘
```

---

## B44. Naive Solution

```text
Check availability
       ↓
if available
       ↓
create booking
```

Problem:

```text
A → check → available
B → check → available

A → booking created
B → booking created
```

**Double booking.**

---

## B45. Better Solution

Use a database transaction and enforce correctness at the database level.

For example, depending on your schema, you can use: unique constraint + transaction + locking/versioning.

For a simple slot model (`table_id`, `date`, `start_time`, `end_time`), you might reserve a unique slot and let conflicting transactions fail.

For arbitrary overlapping time intervals, the database design needs more careful handling than a simple `(table_id, slot)` unique key.

The important interview idea is:

> Don't rely solely on an application-server lock. Make the database enforce the invariant.

---

## B46. Concurrency Design Checklist

Whenever interviewer gives you a concurrency problem, ask yourself:

1. **What is shared?** balance? stock? seat? table? counter? file? cache?
2. **What is the invariant?** Stock can never become negative. A table cannot have two overlapping bookings.
3. **What happens concurrently?** A reads, B reads, A writes, B writes.
4. **Where should synchronization happen?** JVM / Database / Redis / Kafka partitioning.
5. What happens if the server crashes?
6. What happens if request retries?
7. What happens if a message is delivered twice?
8. What happens if a lock expires?
9. What happens if the database is slow?
10. How do we prevent unlimited work? (This last one leads to **backpressure**.)

---

## B47. The Most Important Mental Model

When you see multiple threads, think:

```text
       Shared State
            |
            ↓
     Race Condition?
        /       \
      YES        NO
       |
       ↓
What invariant?
       |
       ↓
How protect it?
       |
   +---+---+--------+
   |       |        |
 Lock   Atomic DB  CAS
   |       |        |
   +---+---+--------+
       |
       ↓
Distributed?
       |
      YES
       ↓
Idempotency
Transactions
DB constraints
Distributed coordination
```

---

## B48. What I Would Prepare for Your Interview

Given the deadlock/concurrency questions you're getting, study in this order:

### Phase 1 — Java concurrency

1. Process vs Thread
2. Concurrency vs Parallelism
3. Race condition
4. Critical section
5. `synchronized`
6. `ReentrantLock`
7. `volatile`
8. `AtomicInteger` / CAS
9. Thread safety
10. Immutability

### Phase 2 — Thread coordination

11. Semaphore
12. CountDownLatch
13. CyclicBarrier
14. BlockingQueue
15. Producer-Consumer
16. ExecutorService
17. ThreadPoolExecutor
18. Future / Callable

### Phase 3 — Failure scenarios

19. Deadlock
20. Starvation
21. Livelock
22. Lock ordering
23. tryLock
24. Thread interruption

### Phase 4 — Database concurrency

25. Transactions
26. ACID
27. Isolation
28. Dirty read
29. Non-repeatable read
30. Phantom read
31. Lost update
32. Optimistic locking
33. Pessimistic locking
34. SELECT FOR UPDATE
35. Atomic UPDATE

### Phase 5 — Distributed concurrency

36. Distributed locks
37. Redis locks
38. Idempotency
39. Unique constraints
40. Fencing/version checks
41. Retry
42. Timeout
43. Backpressure

### Phase 6 — Kafka concurrency

44. Partitions
45. Consumer groups
46. Parallel consumption
47. Ordering
48. Duplicate processing
49. Offset commits
50. Idempotent consumers
51. At-least-once delivery

---

## The 5 Problems You Should Be Able to Solve

Before your interview, make sure you can design/explain these from scratch:

| Problem | Pattern |
| --- | --- |
| **Bank Account Transfer** | locks + deadlock prevention |
| **Inventory Reservation** | atomic DB update / optimistic locking |
| **Restaurant Table Booking** | concurrency + database invariant |
| **Payment API** | idempotency + transaction + retries |
| **Kafka Order Processing** | consumer concurrency + ordering + duplicate events |

If you master these five, you will be able to map most SDE-1 concurrency questions to a pattern rather than trying to memorize individual answers.
