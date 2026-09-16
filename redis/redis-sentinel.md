# Day 26 — Redis Sentinel

Redis Sentinel is used to provide **high availability** for Redis.

The main problem it solves is: **What happens if the Redis master server goes down?**

Without Sentinel, an application may continue trying to connect to a dead master. Someone would have to manually promote a replica and update the application configuration.

Sentinel automates this process.

---

## Table of Contents

- [1. Basic Architecture](#1-basic-architecture)
- [2. What Does Sentinel Do?](#2-what-does-sentinel-do)
- [3. What Is Quorum?](#3-what-is-quorum)
- [4. SDOWN vs ODOWN](#4-sdown-vs-odown)
- [5. Automatic Failover](#5-automatic-failover)
- [6. How Does Sentinel Choose a Replica?](#6-how-does-sentinel-choose-a-replica)
- [7. Sentinel Election](#7-sentinel-election)
- [8. What Is Sentinel Quorum Actually For?](#8-what-is-sentinel-quorum-actually-for)
- [9. Configuration Discovery](#9-configuration-discovery)
- [10. Sentinel Is NOT Replication](#10-important-sentinel-is-not-replication)
- [11. What Happens to Writes During Failover?](#11-what-happens-to-writes-during-failover)
- [12. Can Data Be Lost?](#12-can-data-be-lost)
- [13. Sentinel vs Cluster](#13-sentinel-vs-cluster)
- [14. Interview Example](#14-interview-example)
- [15. Important Interview Cross-Questions](#15-important-interview-cross-questions)
- [16. The Complete Flow You Should Remember](#16-the-complete-flow-you-should-remember)

---

## 1. Basic Architecture

Suppose we have:

```text
                 Application
                      |
                      v
                   Master
                  /      \
                 v        v
            Replica 1   Replica 2
                  ^
                  |
              Sentinel
```

In a real production setup, you typically run **multiple Sentinel processes**, not just one:

```text
                  Application
                       |
                       v
                    Master
                   /      \
                  v        v
             Replica 1   Replica 2

             ↑      ↑       ↑
             |      |       |
          Sentinel Sentinel Sentinel
```

Sentinels monitor the Redis instances.

---

## 2. What Does Sentinel Do?

There are four important responsibilities.

### 1. Monitoring

Sentinel continuously checks whether Redis instances are reachable.

```text
Sentinel
   |
   | PING
   v
Master
   |
   | PONG
   v
Sentinel
```

If the master stops responding, Sentinel notices.

### 2. Failure Detection

Sentinel determines whether the master is actually unavailable.

But one Sentinel shouldn't immediately assume that the master has failed. This is where **quorum** becomes important.

---

## 3. What Is Quorum?

Quorum means: the **minimum number of Sentinel nodes** that must agree that a master is unreachable before Sentinel considers it objectively down.

Suppose we have Sentinel 1, 2, 3, and quorum is 2.

If Sentinel 1 and 2 say master unreachable, Sentinel 3 says reachable: 2 ≥ quorum 2.

The master can be considered **objectively down (ODOWN)**.

---

## 4. SDOWN vs ODOWN

This is an important interview topic.

**SDOWN — Subjectively Down**

One Sentinel thinks: "I cannot reach the master." So that Sentinel marks it SDOWN. Meaning: from this Sentinel's perspective, the master appears down.

**ODOWN — Objectively Down**

Multiple Sentinels agree that the master is unreachable.

If quorum is satisfied: SDOWN + agreement from Sentinels → ODOWN.

Now Sentinel can proceed with failover.

---

## 5. Automatic Failover

```text
             Master
                |
          ❌ crashes
                |
        Sentinel detects
                |
                v
          Master = ODOWN
                |
                v
        Sentinel election
                |
                v
       Select a replica
                |
                v
        Replica promoted
                |
                v
         New Master
```

Before failure: Master + Replica 1 + Replica 2.

Master crashes. Sentinel selects one replica (e.g. Replica 1) and promotes it to New Master. Replica 2 remains a replica.

---

## 6. How Does Sentinel Choose a Replica?

Sentinel considers factors such as: replication state, replication offset, replica priority, replica availability, replica ID as a tie-breaker.

The important idea for an interview is:

> Sentinel tries to promote a suitable replica that has the most appropriate replication state, while respecting configured replica priority.

Don't simply say "Sentinel always chooses Replica 1." It doesn't work like that.

---

## 7. Sentinel Election

Suppose there are three Sentinels: S1, S2, S3. Master is down. The Sentinels need to coordinate the failover.

One Sentinel becomes the **leader** for the failover.

```text
S1 ─────┐
S2 ─────┼──> Elect failover leader
S3 ─────┘
              |
              v
         Leader Sentinel
              |
              v
       Promote replica
```

The election requires enough Sentinel votes according to the Sentinel configuration.

---

## 8. What Is Sentinel Quorum Actually For?

This is a common interview trap.

People often say: "Quorum elects the new master." That's not the best explanation.

Quorum primarily helps determine: **whether enough Sentinels agree that the master is objectively down**.

Failover also requires Sentinel coordination and authorization.

Remember:

```text
Quorum
   ↓
Agree master is down
   ↓
Failover process
   ↓
Sentinel leader elected
   ↓
Replica promoted
```

---

## 9. Configuration Discovery

Another important responsibility of Sentinel is **service discovery**.

Suppose your application initially connects to Redis Master `10.0.0.10:6379`. Then the master fails. Sentinel promotes `10.0.0.11:6379` as the new master.

The application should not permanently hardcode `10.0.0.10:6379`.

Instead, Redis clients that support Sentinel can ask Sentinel: **"Who is currently the master for this Redis service?"**

```text
Application
     |
     | Who is master?
     v
 Sentinel
     |
     | New Master = 10.0.0.11
     v
Application
     |
     v
10.0.0.11:6379
```

This is called configuration discovery.

---

## 10. Important: Sentinel Is NOT Replication

These are different concepts.

**Redis Replication** provides Master + Replica. It copies data from master to replicas.

**Redis Sentinel** provides: Monitoring, Failure detection, Failover, Service discovery.

Together: Replication + Sentinel → Redis High Availability.

Sentinel depends on replication to have replicas available for promotion.

---

## 11. What Happens to Writes During Failover?

Master crashes. During the failover period, there can be a temporary period where writes fail.

Sentinel detects the failure and promotes a replica. The application reconnects to the new master.

So Sentinel provides high availability, but it does **not** mean zero downtime.

There can be a short failover window.

---

## 12. Can Data Be Lost?

**Yes.** This is very important.

Suppose a write goes to Master memory, but hasn't reached the replica yet. Then Master crashes. The replica may not contain that latest write. If that replica becomes the new master, the latest write is missing.

Therefore: **Redis Sentinel does not guarantee zero data loss.**

Replication is asynchronous by default, so there can be replication lag.

---

## 13. Sentinel vs Cluster

| Redis Sentinel | Redis Cluster |
| --- | --- |
| High availability | High availability + sharding |
| Usually one master + replicas | Multiple masters + replicas |
| No automatic data sharding | Automatic data partitioning |
| Failover | Failover |
| Service discovery | Cluster-aware discovery |
| Suitable when dataset fits on one master | Useful when dataset needs to be distributed |

Simple way to remember:

```text
Sentinel
    ↓
"Make my Redis deployment highly available"

Cluster
    ↓
"Distribute my Redis data across multiple masters
 and provide high availability"
```

---

## 14. Interview Example

Imagine your application processes payment requests. Redis is being used for distributed locks, idempotency keys, caching.

```text
                 Application Servers
                  /       |       \
                 /        |        \
                v         v         v
             Sentinel  Sentinel  Sentinel
                 \        |        /
                  \       |       /
                       Master
                      /      \
                     v        v
                Replica 1  Replica 2
```

If Master crashes: Sentinels detect failure → Quorum achieved → Failover initiated → Replica promoted → New Master → Application discovers new master.

---

## 15. Important Interview Cross-Questions

### Q1. Why do we need multiple Sentinels?

Because one Sentinel could itself fail or have a network problem. Multiple Sentinels provide distributed failure detection and agreement.

### Q2. Why not use only one Sentinel?

If that Sentinel goes down, you lose the monitoring/failover coordinator. Multiple Sentinels improve availability and help distinguish an actual Redis failure from a problem seen by only one Sentinel.

### Q3. Does Sentinel replicate data?

No. Redis replication copies data. Sentinel monitors Redis and coordinates failover.

### Q4. Does Sentinel eliminate downtime?

No. There is normally a failover interval during which clients may experience connection/write failures.

### Q5. Does Sentinel guarantee no data loss?

No. Replication can lag behind the master.

### Q6. What happens to the old master when it comes back?

It does not simply become master again. After recovery, Sentinel can reconfigure it as a replica of the current master, depending on the failover process/configuration.

```text
Old Master returns
       |
       v
Current Master
       |
       v
Old Master becomes Replica
```

---

## 16. The Complete Flow You Should Remember

```text
                Redis Master
                 /        \
                v          v
           Replica 1    Replica 2

            ↑     ↑        ↑
            |     |        |
          Sentinel Sentinel Sentinel
                  |
                  |
          Monitor Redis
                  |
            Master fails
                  |
                  v
             SDOWN
                  |
          Sentinels communicate
                  |
                  v
             ODOWN
                  |
                  v
          Failover election
                  |
                  v
          Select replica
                  |
                  v
          Promote replica
                  |
                  v
            New Master
                  |
                  v
        Clients discover
          new master
```

### One-line interview answer

> Redis Sentinel is a high-availability system that monitors Redis instances, detects master failures, coordinates automatic failover by promoting a replica, and helps clients discover the current master.

### Key terms to know

```text
Sentinel
   ↓
Monitoring
   ↓
SDOWN
   ↓
ODOWN
   ↓
Quorum
   ↓
Failover
   ↓
Replica Promotion
   ↓
New Master
   ↓
Configuration Discovery
```

### Most important distinction

> Replication stores copies of the data; Sentinel manages availability and failover.
