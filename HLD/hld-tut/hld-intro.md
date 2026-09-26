Day 1 — HLD Introduction

Since you're preparing for SDE-1 Backend Java interviews, the goal today is to learn a simple framework you can reuse for almost every HLD question.

1. What is HLD?

HLD = High-Level Design

It describes how different components of a system interact with each other.

For example, in a URL Shortener:

User
  ↓
Load Balancer
  ↓
URL Service
  ↓
Database
  ↓
Cache

HLD focuses on:

Services
Databases
Cache
Message queues
APIs
Load balancers
Scaling
Reliability
Data flow

You generally don't start with detailed classes or methods.

2. HLD vs LLD
HLD	LLD
System architecture	Class-level design
Services/components	Classes/interfaces
Database choice	Tables/entities
Kafka/Redis/API	Methods/design patterns
Scalability	Object relationships
Availability	Encapsulation
Data flow	SOLID principles
Simple example

HLD:

Client → Load Balancer → URL Service → Redis → DB

LLD:

class UrlService {
    createShortUrl();
    getOriginalUrl();
}

Think:

HLD = What components do we need and how do they communicate?

LLD = How do we implement those components?

3. What happens in an HLD interview?

A typical interview can look like:

Interviewer:
Design a URL Shortener.

You:
1. Clarify requirements
2. Define functional requirements
3. Define non-functional requirements
4. Estimate scale
5. Design APIs
6. Design database
7. Draw architecture
8. Explain request flow
9. Discuss scaling
10. Discuss failures/trade-offs

The interviewer will usually ask follow-ups like:

What if traffic increases 100x?
What if Redis goes down?
What if the database goes down?
How do you generate unique IDs?
How do you handle duplicate requests?
How do you maintain availability?
How do you partition the database?
4. Functional Requirements

Functional requirements = What should the system do?

For URL Shortener:

User can create a short URL.
User can open a short URL.
System redirects to the original URL.
Optionally, user can see URL statistics.

Keep the initial scope limited.

In an interview, say:

"I'll first design the core URL shortening and redirection flow. Analytics and authentication can be added later."

5. Non-Functional Requirements

NFR = How well should the system work?

Examples:

High availability
Low latency
Scalability
Reliability
Durability
Security
Maintainability

For URL Shortener:

"The redirect API should have low latency and the system should support high read traffic."

6. Scalability

Scalability means:

Can the system handle increasing traffic and data?

Suppose:

1,000 requests/sec
        ↓
10,000 requests/sec
        ↓
100,000 requests/sec

We should be able to scale the system.

Horizontal scaling

Instead of making one server bigger:

Server
  ↓
Bigger Server

Add more servers:

             ┌─ Server 1
Load Balancer├─ Server 2
             ├─ Server 3
             └─ Server 4

For most web systems, horizontal scaling is important.

7. Availability

Availability means:

How often is the system operational and accessible?

Suppose we have:

Server 1
Server 2
Server 3

If Server 1 fails:

          Server 1 ❌

Load Balancer
      ↓
Server 2
Server 3

The system can continue serving requests.

Key idea

Avoid a single point of failure.

8. Reliability

Reliability means:

The system consistently performs the correct operation despite failures.

Example:

User creates a URL:

POST /shorten

The system should not accidentally create five different URLs because the request was retried.

We may use:

Idempotency
Retries
Transactions
Deduplication
Monitoring
9. Performance

Performance mainly concerns:

Latency

How long one request takes.

Example:

Request → Server → Response

50 ms
Throughput

How many requests we can handle.

Example:

10,000 requests/sec

A system can have:

Low latency but low throughput
High throughput but higher latency

So always understand the requirement.

10. Consistency

Consistency asks:

When data changes, what should different users/services see?

Suppose:

Database:
URL → google.com

User updates it to:

URL → youtube.com

Should every read immediately return youtube.com?

If yes → stronger consistency.

If some users temporarily see the old value → eventual consistency may be acceptable.

Simple interview explanation

"For critical data where stale reads are not acceptable, I would prefer stronger consistency. For less critical data such as analytics, eventual consistency may be sufficient."

11. Durability

Durability means:

Once data is successfully stored, it should not be lost even after failures.

For example:

User creates URL
       ↓
Database stores it
       ↓
Server crashes
       ↓
URL should still exist

Replication, backups and persistent storage help provide durability.

12. Maintainability

A system should be easy to:

Understand
Debug
Modify
Test
Deploy
Monitor

For example, instead of one giant service:

Huge Application
 ├── URL
 ├── Analytics
 ├── User
 ├── Billing
 └── Notifications

we may eventually separate responsibilities:

URL Service
Analytics Service
User Service

But don't create microservices unnecessarily.

That's an important interview point:

"I would start with a simple architecture and introduce additional services when scale or ownership boundaries justify them."

13. How to approach ANY HLD question

Remember this sequence:

1. Requirements
       ↓
2. Scale estimation
       ↓
3. APIs
       ↓
4. Data model
       ↓
5. Basic architecture
       ↓
6. Request/data flow
       ↓
7. Scaling
       ↓
8. Reliability
       ↓
9. Bottlenecks
       ↓
10. Trade-offs

This is your HLD interview template.

14. Requirement Clarification

Never immediately start drawing boxes.

Ask questions first.

For URL Shortener:

Functional questions

"Should users be able to customize the short URL?"

"Do URLs expire?"

"Do we need authentication?"

"Do we need analytics?"

Scale questions

"How many URLs are created per day?"

"How many redirect requests per second?"

"What is the expected growth?"

Performance questions

"What latency is expected for redirects?"

Availability questions

"Is this system expected to be highly available?"

This shows that you're designing based on requirements rather than guessing.

15. Capacity Estimation

This is one of the most important HLD skills.

Suppose interviewer says:

100 million URLs are created per month.

Approximate:

100M / 30 days
≈ 3.3M/day

Per second:

3.3M / 86,400
≈ 38 writes/sec

So average write traffic is only around:

~40 writes/sec

But suppose every URL gets 100 redirects per month.

Then reads are much higher:

100M × 100
= 10B redirects/month

Approximately:

10B / 30 / 86,400
≈ 3,858 reads/sec

So:

Writes ≈ 40/sec
Reads   ≈ 3,858/sec

That's roughly a 100:1 read/write ratio.

This immediately suggests:

Caching will be very useful.

16. Back-of-the-envelope calculations

You don't need perfect numbers.

Use:

1 day = 86,400 seconds
≈ 100,000 seconds

So:

1 million requests/day
≈ 10 requests/sec

This is a very useful interview shortcut.

Storage

Suppose:

100M URLs
Average record = 500 bytes

Then:

100M × 500 bytes
= 50 GB

Add indexes, replication and metadata:

Actual storage > 50 GB

You don't need to calculate everything precisely.

The goal is to determine:

Is one DB enough?
Do we need sharding?
Do we need caching?
How many servers?
How much storage?
17. Example — Design URL Shortener

Now let's put everything together.

Requirements

Core functionality:

Original URL
     ↓
Short URL

Example:

https://example.com/very/long/path
                 ↓
https://short.ly/aB72x

When user opens:

/aB72x

System returns:

https://example.com/very/long/path
18. APIs
Create short URL
POST /urls

Request:

{
  "longUrl": "https://example.com/very/long/path"
}

Response:

{
  "shortUrl": "https://short.ly/aB72x"
}
Redirect
GET /aB72x

Response:

HTTP 301/302
Location: https://example.com/very/long/path
19. Database

Simple table:

URL
--------------------------------
id
short_code
long_url
created_at
expires_at

Index:

short_code → long_url

Because the main operation is:

short_code
     ↓
long_url
20. Basic Architecture
                 Client
                   |
                   ↓
             Load Balancer
                   |
          ┌────────┴────────┐
          ↓                 ↓
     URL Service        URL Service
          |                 |
          └────────┬────────┘
                   ↓
                 Redis
                   |
             Cache Miss
                   ↓
               Database
Redirect flow
GET /aB72x
      ↓
Load Balancer
      ↓
URL Service
      ↓
Redis
      ↓
Found?
  /       \
Yes        No
 |          |
 ↓          ↓
Return     DB
URL         ↓
            Redis
              ↓
           Return URL

This is the basic architecture you should be able to draw in an interview.

21. Why Redis?

Because URL shortening is usually read-heavy.

Suppose:

Redis:
aB72x → https://example.com/...

Then:

Request
  ↓
Redis
  ↓
URL

We avoid hitting the database for every redirect.

Benefits:

Lower latency
Lower DB load
Higher read throughput
22. How do we generate aB72x?

One simple approach:

Generate unique numeric ID
        ↓
Encode ID using Base62
        ↓
Short code

Base62 characters:

a-z
A-Z
0-9

For example:

ID = 125678
      ↓
Base62
      ↓
aB72x

The important requirement is:

The ID must be unique.

Possible ID-generation approaches include:

Database auto-increment
Distributed ID generator
Snowflake-style IDs
Random ID + collision checking

For a simple initial design, we can start with a database-generated ID.

23. What if Redis goes down?

Don't make Redis the only source of truth.

Redis ❌
  ↓
Database
  ↓
Return URL

After retrieving the URL:

Database
   ↓
Redis

Populate the cache again.

Therefore:

Database = durable source of truth
Redis    = cache
24. What if database goes down?

For high availability, we can use:

Primary DB
    ↓
Replicas

For writes:

Application → Primary

For reads:

Application → Replica

But we need to think about replication lag and consistency requirements.

At larger scale, database sharding can also be considered.

25. Interview Cross-Questions

Be ready for these:

Q: Why cache?

Because redirects are read-heavy. Caching frequently accessed short codes reduces database load and latency.

Q: What happens when cache misses?

Read from the database, return the result, and populate Redis.

Q: What happens when Redis fails?

Fall back to the database. Redis is a cache, not the source of truth.

Q: How do you generate unique short codes?

Generate a unique ID and encode it using Base62, or use another distributed ID-generation mechanism.

Q: How do you scale the application?

Keep the URL service stateless and horizontally scale multiple instances behind a load balancer.

Q: How do you handle database scaling?

Start with replication for read scaling. If the dataset/write load becomes sufficiently large, consider partitioning/sharding.

Q: What is the biggest bottleneck?

Initially:

Database reads

So:

Redis
 ↓
Reduce DB reads

At much larger scale, other bottlenecks could appear:

DB writes
ID generation
Network
Cache capacity
Hot keys
26. Your HLD Interview Mindset

Don't try to immediately say:

"I'll use Kafka, Redis, Kubernetes, MongoDB, microservices..."

Instead ask:

What does the system need?
        ↓
How much traffic?
        ↓
What are the bottlenecks?
        ↓
What solution solves those bottlenecks?

Architecture should come from requirements.

Day 1 — What you should remember

If the interviewer gives you any HLD problem, start with:

1. Clarify requirements
2. Functional requirements
3. Non-functional requirements
4. Estimate traffic/storage
5. Define APIs
6. Define data model
7. Draw basic architecture
8. Explain request flow
9. Scale the system
10. Discuss failures
11. Identify bottlenecks
12. Explain trade-offs

And remember these definitions:

Scalability  → Handle more load
Availability → System remains accessible
Reliability  → System behaves correctly despite failures
Performance  → Latency + throughput
Consistency   → What data different readers see
Durability    → Data survives failures
Maintainability → Easy to change/debug

For Day 1, don't worry about designing a perfect system. Your main goal is to build the habit of going from requirements → estimation → architecture → bottlenecks → trade-offs.