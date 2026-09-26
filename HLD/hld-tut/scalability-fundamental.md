Day 2 — Scalability Fundamentals

In an HLD interview, scalability means:

Can our system handle increasing traffic, users, data, and requests without becoming slow or unreliable?

1. Vertical Scaling — Scale Up

Increase the power of a single machine.

Before
Server
4 CPU
16 GB RAM

        ↓ Scale Up

After
Server
16 CPU
64 GB RAM
Advantages
Simple
No major architectural changes
Easy to implement
Disadvantages
Hardware has a limit
Can become expensive
Single machine can become a single point of failure
Interview line

"Vertical scaling is useful initially because it is simple, but eventually we usually need horizontal scaling for large systems."

2. Horizontal Scaling — Scale Out

Add more machines.

              Load Balancer
              /     |      \
             ↓      ↓       ↓
          Server1 Server2 Server3

Instead of making one server bigger, we add more servers.

Advantages
Handles large traffic
Better fault tolerance
Can continue adding servers
Disadvantage
More complexity
Requires load balancing
Stateful services become harder to scale
3. Scale-Up vs Scale-Out
	Scale Up	Scale Out
Meaning	Bigger machine	More machines
Complexity	Low	Higher
Limit	Hardware limit	Much larger
Fault tolerance	Lower	Higher
Cost at large scale	Expensive	Usually better
Example	8 → 32 CPU	2 → 10 servers
4. Stateless vs Stateful Services

This is very important for horizontal scaling.

Stateless Service

Server doesn't store user-specific session state locally.

             Load Balancer
              /    |    \
             ↓     ↓     ↓
            S1    S2     S3

Request 1 can go to S1.

Request 2 can go to S3.

No problem.

Example:

GET /users/123

The server gets required information from the database/cache.

Why stateless is good?

Because we can easily add more instances.

Traffic ↑
   ↓
Add S4
   ↓
Add S5
Stateful Service

The server keeps important state locally.

User → Server 1
       ↓
   Session stored here

If the next request goes to Server 2, the session may not exist there.

Solutions include:

Redis/session store
Database
Sticky sessions

For scalable systems, stateless application servers + external state storage is usually preferred.

5. Load Balancer

A load balancer distributes requests across servers.

                Users
                  |
                  ↓
            Load Balancer
            /     |      \
           ↓      ↓       ↓
         S1       S2      S3

Suppose:

10,000 requests/sec

Instead of one server handling everything:

S1 → 3,300 req/s
S2 → 3,300 req/s
S3 → 3,400 req/s

The load balancer can also detect unhealthy servers.

S1 → Healthy
S2 → Healthy
S3 → DOWN

             ↓

Traffic → S1 / S2
6. Scaling Databases

The application isn't always the bottleneck.

Eventually:

Application Servers
        ↓
     Database
        ↓
      Bottleneck

There are several approaches.

Option 1 — Vertical scaling

Make the database server bigger.

16 CPU → 64 CPU
32 GB → 128 GB RAM

Simple, but eventually limited.

Option 2 — Replication

Create multiple database copies.

             Primary
             /     \
            ↓       ↓
         Replica1 Replica2

Writes:

Application → Primary

Reads:

Application → Replica1
            → Replica2

This is particularly useful for read-heavy systems.

7. Read-Heavy vs Write-Heavy

This is a common interview discussion.

Read-heavy

Example:

100,000 reads/sec
10,000 writes/sec

Strategy:

             Primary
             /     \
            ↓       ↓
        Replica1  Replica2

Add read replicas.

Caching with Redis can also reduce database reads:

Application
     ↓
   Redis
  /     \
Hit     Miss
 ↓       ↓
Return   DB
Write-heavy

Example:

10,000 reads/sec
100,000 writes/sec

Read replicas don't solve the primary write bottleneck.

Possible strategies:

Better database hardware
Batch writes
Async processing
Kafka/message queues
Database sharding
Partitioning
Write optimization

Example:

Application
     ↓
    Kafka
     ↓
Consumers
     ↓
Database

The queue can absorb bursts of writes.

8. Replication

Replication means keeping multiple copies of data.

Example:

              Primary DB
             /          \
            ↓            ↓
        Replica 1     Replica 2
Why?

Availability

If one replica fails:

Replica 1 ❌

Reads → Replica 2

Read scalability

Reads → Replica 1
Reads → Replica 2
Reads → Replica 3
9. Sharding

Replication creates copies of the same data.

Sharding divides data across different databases.

Example:

Users

userId 1-1M
      ↓
   Shard 1

userId 1M-2M
      ↓
   Shard 2

userId 2M-3M
      ↓
   Shard 3

Another common strategy:

hash(userId) % 3

        ↓

Shard 0
Shard 1
Shard 2

Now different servers hold different portions of data.

Why sharding?

Because one database may not be able to handle:

huge data
huge write throughput
huge query load
Problem

Sharding introduces complexity:

Choosing shard key
Hot partitions
Resharding
Cross-shard queries
Transactions across shards
10. Replication vs Sharding

Very important distinction:

Replication
Same data
   ↓
DB1
DB2
DB3

Purpose:

Availability + read scalability

Sharding
Different data

DB1 → Users 1–1M
DB2 → Users 1M–2M
DB3 → Users 2M–3M

Purpose:

Scale storage and read/write capacity.

They can also be combined:

                 Shard 1
              /           \
         Replica 1      Replica 2

                 Shard 2
              /           \
         Replica 1      Replica 2

This is common in large-scale systems.

11. Bottleneck

A bottleneck is the component limiting the whole system.

Example:

Users
  ↓
Load Balancer
  ↓
10 App Servers
  ↓
1 Database
  ↓
🔥 Bottleneck

Adding more application servers won't help much if the database is already overloaded.

Another example:

App → Redis → DB
              ↑
           Bottleneck
Interview approach

Always ask:

"What is the current bottleneck?"

Then scale that component.

12. Single Point of Failure — SPOF

A component whose failure can bring down the system.

Bad architecture:

       Load Balancer
             |
             ↓
         Server 1
             |
             ↓
          Database

If Server 1 dies → system unavailable.

Better:

          Load Balancer
          /           \
         ↓             ↓
       S1              S2
       |               |
       └──────┬────────┘
              ↓
         DB Primary
              |
        ┌─────┴─────┐
        ↓           ↓
      Replica      Replica

We try to eliminate critical SPOFs through redundancy.

13. Example Architecture

Let's design a simple e-commerce backend.

                   Users
                     |
                     ↓
               Load Balancer
                 /    |    \
                ↓     ↓     ↓
              App1   App2   App3
                 \     |     /
                  \    |    /
                   ↓   ↓   ↓
                   Redis
                     |
                Cache Miss
                     ↓
                DB Primary
                 /       \
                ↓         ↓
           Read Replica  Read Replica

For asynchronous operations:

App Servers
     |
     ↓
   Kafka
     |
     ├── Order Consumer
     ├── Notification Consumer
     └── Analytics Consumer

At very large scale:

                     Users
                       |
                 Load Balancer
                       |
          ┌────────────┼────────────┐
          ↓            ↓            ↓
        App1          App2         App3
          └────────────┼────────────┘
                       ↓
                     Redis
                       ↓
                  DB Router
                 /         \
                ↓           ↓
            Shard 1       Shard 2
           /      \       /      \
        Replica Replica Replica Replica
14. How to Think About Scalability in an Interview

When the interviewer gives you a system-design problem, follow this thought process:

1. Estimate traffic
       ↓
2. Identify bottlenecks
       ↓
3. Scale application servers
       ↓
4. Make services stateless
       ↓
5. Add load balancer
       ↓
6. Add caching
       ↓
7. Scale database
       ↓
8. Replication
       ↓
9. Sharding if required
       ↓
10. Remove SPOFs
A very good interview statement

"I would first identify which component is becoming the bottleneck. For the application tier, I would keep services stateless and horizontally scale them behind a load balancer. For read-heavy workloads, I would use caching and read replicas. If the database becomes a write or storage bottleneck, I would consider sharding. Finally, I would add redundancy to critical components to avoid single points of failure."

⭐ Most Important Things to Remember
Vertical scaling → Bigger machine

Horizontal scaling → More machines

Stateless → Easy to scale

Load Balancer → Distributes traffic

Replication → Copies data

Sharding → Splits data

Read-heavy → Cache + Read replicas

Write-heavy → Queue + DB optimization + Sharding

Bottleneck → Component limiting system

SPOF → One failure can take system down

Interview cross-question to practice:

"Your application currently has 10 servers, but traffic has increased 10×. You added 50 more servers, yet the system is still slow. What would you investigate?"

The key answer is: don't blindly add servers—find the bottleneck (database, cache, network, downstream service, connection pool, CPU, disk I/O, locks, etc.).