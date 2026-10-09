# Instagram — HLD Interview Preparation

For an HLD / System Design interview, Instagram is an excellent problem because it combines almost every important backend concept: high traffic, fan-out, caching, Kafka, media processing, real-time systems, sharding, recommendations, and storage.

---

## Table of Contents

- [Part 1 — Core HLD](#part-1--core-hld)
  - [1. Clarify Requirements](#1-clarify-requirements)
  - [2. Scale Estimation](#2-scale-estimation)
  - [3. High-Level Architecture](#3-high-level-architecture)
  - [4. Feed Generation](#4-feed-generation--most-important)
  - [5. Hybrid Feed Generation](#5-hybrid-feed-generation)
  - [6–11. Feed Storage, Pagination, Ranking, Kafka, Media, CDN](#6-feed-storage)
  - [12–22. Storage, Sharding, Caching, Likes, Notifications](#12-object-storage-vs-database)
  - [23–31. Failures, End-to-End Flows, Trade-offs](#27-failure-scenario--kafka-down)
  - [32–34. Interview Plan and Questions](#32-most-important-interview-trade-offs)
- [Part 2 — Celebrity / Hybrid Fan-out Deep Dive](#part-2--celebrity-hybrid-fan-out-deep-dive)
- [Part 3 — Celebrity Cross-Questions](#part-3--celebrity-feed-cross-questions)
- [Part 4 — Schema Design and API Contracts](#part-4--schema-design-and-api-contracts)

---

# Part 1 — Core HLD

## What we will design

- Large-scale traffic
- Feed generation
- Media processing
- Real-time communication
- Location
- Recommendation
- Storage
- Caching
- Messaging

---

## 1. Clarify Requirements

In an interview, don't immediately start drawing Kafka and databases.

Start with:

> I'll first clarify the functional and non-functional requirements, then estimate scale, define APIs, design the high-level architecture, and finally deep dive into feed generation, media processing, messaging, and scalability.

### Functional requirements

For our simplified Instagram:

- User signup / login
- Follow / unfollow users
- Upload photo / video
- View home feed
- Like / comment
- Search users
- Stories / reels
- Direct messaging
- Notifications
- Location-based content
- Recommendations

### Non-functional requirements

- High availability
- Low latency
- Horizontal scalability
- Eventual consistency where acceptable
- Durable media storage
- Fault tolerance
- Idempotency
- Security / privacy

**Feed:** target P99 feed latency roughly **200–500 ms**.

**Upload:** upload itself can be asynchronous; processing doesn't have to block the user for the entire pipeline.

---

## 2. Scale Estimation

Suppose:

| Metric | Scale |
| --- | --- |
| Daily active users | 500M |
| Posts / day | 100M |
| Feed views / day | 5B |
| Likes / comments / day | 10B |
| Average post media size | 2 MB |

### Media storage

```text
100M × 2 MB  ≈  200 TB/day
```

That's enormous. Therefore we should **not** store media directly inside MySQL / PostgreSQL.

```text
Application
     |
     v
Object Storage
     |
     +---- Photos
     +---- Videos
     +---- Thumbnails
```

Examples: **S3 / GCS / Blob Storage**.

---

## 3. High-Level Architecture

A good interview diagram:

```text
                   ┌───────────────┐
                   │    Clients    │
                   │ Android/iOS/Web│
                   └───────┬───────┘
                           |
                           v
                    ┌─────────────┐
                    │ CDN / WAF   │
                    └──────┬──────┘
                           |
                           v
                    ┌─────────────┐
                    │ Load Balancer│
                    └──────┬──────┘
                           |
                    ┌──────v───────┐
                    │ API Gateway  │
                    └──────┬───────┘
                           |
          ┌────────────────┼────────────────┐
          |                |                |
          v                v                v
    User Service      Feed Service     Post Service
          |                |                |
          v                v                v
       User DB         Feed Cache       Post DB
                           |
                           v
                         Kafka
                           |
          ┌────────────────┼────────────────┐
          |                |                |
          v                v                v
   Notification      Recommendation   Media Processing
      Service             Service          Service
                                             |
                                             v
                                      Object Storage
```

Additional services:

- Messaging Service
- Location Service
- Search Service
- Like Service
- Comment Service
- Story Service
- Notification Service

---

## 4. Feed Generation — MOST IMPORTANT

This is probably the most important Instagram HLD topic.

Suppose user **A** follows **B, C, D, E**. B creates post **P1**. A should eventually see P1 in their feed.

There are two major approaches.

### Approach 1 — Fan-out on Read

When A opens Instagram:

```text
Feed Service
      |
      +---- Get following list
      |
      +---- Get posts of B
      +---- Get posts of C
      +---- Get posts of D
      +---- Get posts of E
      |
      v
    Merge
      |
      v
   Ranking
      |
      v
    Feed
```

**Problem:** suppose a celebrity has 100 million followers. Every follower opening their feed causes queries. Very expensive.

### Approach 2 — Fan-out on Write

When B creates a post:

```text
B creates P1
      |
      v
Post Service
      |
      v
Kafka
      |
      v
Fanout Workers
      |
      ├── User A feed
      ├── User C feed
      ├── User D feed
      ├── User E feed
      └── ...
```

We pre-compute the feed. Then:

```text
User opens Instagram
        |
        v
Feed Service
        |
        v
Redis
        |
        v
Feed
```

Very fast reads.

### Fan-out on Write has a problem

Celebrity (e.g. Taylor Swift) with **100M followers**:

```text
1 post
     ↓
100M feed writes
```

That's terrible. So we use a **hybrid** approach.

---

## 5. Hybrid Feed Generation

| User type | Strategy |
| --- | --- |
| Normal users | Fan-out on Write |
| Celebrities / high-follower users | Fan-out on Read |

```text
Normal user
     |
     v
Kafka
     |
     v
Fanout workers
     |
     v
Redis feed

Celebrity Post
      |
      v
Post DB
      |
      v
When user requests feed
      |
      v
Fetch celebrity posts
```

Then combine:

```text
Precomputed Feed
       +
Celebrity Posts
       +
Recommended Posts
       |
       v
     Ranking
       |
       v
     Result
```

This is a very strong interview answer.

---

## 6. Feed Storage

Don't store `user_id → entire feed JSON` in Redis. Store **post IDs**.

```text
feed:user:123

[9821, 9820, 9812, 9771, ...]
```

```text
Feed Service
      |
      v
Redis
      |
      v
Post IDs
      |
      v
Post Service
      |
      v
Post DB / Cache
```

Redis is excellent because feed reads are `READ → READ → READ` and usually require the latest N posts.

---

## 7. Feed Pagination

Never return 100 posts. Use **cursor pagination**.

**Bad:**

```http
GET /feed?page=5
```

**Better:**

```http
GET /feed?cursor=9821&limit=20
```

```json
{
  "posts": [],
  "nextCursor": "9750"
}
```

Offset pagination becomes expensive at large scale.

---

## 8. Feed Ranking

Chronological feed is simple: newest posts first. An Instagram-like feed should rank posts.

Possible signals: relationship strength, likes, comments, shares, recency, user interests, content type, previous engagement.

```text
score =
    relationshipScore
    + engagementScore
    + freshnessScore
    + interestScore
```

```text
Candidate Generation
        |
        v
Ranking Model
        |
        v
Top N posts
```

---

## 9. Kafka in Instagram

Kafka becomes the event backbone.

```text
User creates post
       |
       v
Post Service
       |
       v
Kafka
       |
       ├── Feed Service
       ├── Notification Service
       ├── Recommendation Service
       ├── Analytics Service
       └── Moderation Service
```

Topic: `post-created`

```json
{
  "postId": "P123",
  "userId": "U456",
  "timestamp": 1720000000
}
```

This decouples services.

---

## 10. Media Processing

User uploads a 4K video. We should **not** upload 500 MB through the API server and process it there — that ties up application servers.

Instead:

```text
Client
   |
   v
API Server
   |
   | Generate pre-signed URL
   v
Object Storage

Client ───────────────> S3
```

Then:

```text
S3
 |
 | upload event
 v
Kafka
 |
 v
Media Processing Service
 |
 ├── Resize
 ├── Compress
 ├── Generate thumbnails
 ├── Transcode video
 ├── Generate different resolutions
 └── Content moderation
 |
 v
S3
 |
 v
CDN
```

---

## 11. Why CDN?

A popular Reel requested by 10 million users should not hit the application server.

```text
User
 |
 v
CDN
 |
 +-- Cache HIT → media
 |
 +-- Cache MISS
          |
          v
       S3
```

Benefits:

- Lower latency
- Reduced origin traffic
- Lower bandwidth cost
- Better global performance

---

## 12. Object Storage vs Database

**Q: Why don't you store images in MySQL?**

> Media is large, immutable, and requires huge storage and bandwidth. Object storage is designed for large blobs and provides durability and scalability. The database should store metadata such as post ID, user ID, media URL, caption, and timestamps.

**Post DB (metadata):** `post_id`, `user_id`, `caption`, `media_url`, `created_at`, `location_id`

**Actual file:** S3, e.g. `/posts/2026/10/P123/video.mp4`

---

## 13. Real-Time Communication

Instagram has DMs, typing indicators, online status, message delivery, and read receipts.

```text
Client
  |
  v
WebSocket Gateway
  |
  v
Messaging Service
  |
  +---- Redis
  |
  +---- Kafka
  |
  +---- Message DB
```

HTTP polling (`client → server` repeatedly) creates unnecessary traffic. WebSocket keeps a persistent connection:

```text
Client <================> Server
```

---

## 14. Sending a Message

User A sends `"Hi"`:

```text
A
 |
 | WebSocket
 v
WebSocket Gateway
 |
 v
Messaging Service
 |
 +---- Store message
 |
 +---- Kafka
 |
 v
Delivery Service
 |
 v
B
```

```json
{
  "messageId": "M123",
  "conversationId": "C10",
  "senderId": "A",
  "receiverId": "B",
  "text": "Hi",
  "timestamp": 123456
}
```

---

## 15. What if B is Offline?

We store in **Message DB** and possibly maintain `unread_count`.

```text
B connects
 |
 v
Messaging Service
 |
 v
Fetch undelivered messages
 |
 v
Send to B
```

A push notification can also be sent.

---

## 16. Message Ordering

If A sends `"Hello"`, `"How are you?"`, `"Where are you?"`, we need ordering.

Generate `messageId` + `sequenceNumber`, or use a server-side timestamp / monotonic sequence per conversation.

Partition Kafka by `conversationId`:

```text
Same conversation
      |
      v
Same Kafka partition
```

which helps preserve ordering.

---

## 17. Location

Instagram can support location tags, nearby content, location-based recommendations, and geotagged posts.

Use a geospatial index. Example user: `lat = 23.18`, `lng = 79.98`. Nearby search: posts within 5 km.

Possible technologies: Redis GEO, Elasticsearch / OpenSearch geo queries, PostgreSQL PostGIS.

At very large scale: **GeoHash**.

```text
lat/lng
   |
   v
Geohash
   |
   v
Nearby cells
```

Then query only relevant cells.

---

## 18. Recommendation System

```text
Candidate Generation
          |
          v
       Ranking
          |
          v
      Filtering
          |
          v
       Results
```

### Candidate generation sources

- People you follow
- Similar users
- Popular posts
- Trending content
- Location
- User interests
- Previously viewed content

Maybe millions of candidates. We cannot rank millions synchronously.

```text
1M candidates
      ↓
Candidate filtering
      ↓
10K candidates
      ↓
ML ranking
      ↓
100 candidates
      ↓
Top 20
```

---

## 19. Recommendation Events

Every interaction becomes an event: view, like, comment, share, save, follow, skip, watch_time.

```text
User watches Reel for 20 seconds
       |
       v
Kafka
       |
       v
Recommendation pipeline
       |
       v
Feature Store
       |
       v
ML model
```

This allows recommendations to continuously adapt.

---

## 20. Storage Architecture

Don't use one database for everything.

```text
                    Instagram
                        |
       ┌────────────────┼────────────────┐
       |                |                |
     Users             Posts          Messages
       |                |                |
       v                v                v
   SQL/NoSQL          NoSQL            NoSQL
```

| Data | Store | Why |
| --- | --- | --- |
| Users | MySQL / PostgreSQL | Consistency-sensitive |
| Posts | Cassandra / DynamoDB / distributed NoSQL | Huge scale, high write volume, horizontal scaling |
| Media | S3 | Large blobs |
| Search | Elasticsearch / OpenSearch | Full-text / geo |
| Cache | Redis | Hot reads |

---

## 21. Database Sharding

`posts` can have billions of rows. One database won't scale indefinitely.

Shard based on `user_id`: `hash(user_id) % N`

```text
User 101 → Shard 1
User 102 → Shard 7
User 103 → Shard 3
```

Think carefully about celebrity users because one user can become a **hotspot**.

---

## 22. Caching Strategy

| Object | Key |
| --- | --- |
| User profile | `user:123` |
| Post | `post:456` |
| Feed | `feed:123` |
| Recommendation | `recommendation:123` |
| Session | `session:abc` |

---

## 23. Cache-Aside Pattern

```text
Application
     |
     v
Redis
     |
   HIT?
   /   \
 Yes    No
 |       |
Return   DB
         |
         v
       Redis
         |
         v
       Return
```

```java
Post post = redis.get(postId);

if (post == null) {
    post = postRepository.findById(postId);
    redis.set(postId, post);
}

return post;
```

---

## 24. Cache Invalidation

**Q: What happens when a post changes?**

`post:123` is cached. User changes caption. We update the DB, then `DEL post:123` (or update cache).

For immutable media / posts, caching is much easier.

---

## 25. Likes

A popular post with 10M likes should **not** repeatedly update one database row:

```sql
UPDATE posts
SET likes = likes + 1
WHERE id = 123;
```

That row becomes a hotspot.

```text
Like event
     |
     v
Kafka
     |
     v
Like workers
     |
     v
Distributed storage
```

Counters can be aggregated asynchronously.

For user-specific like state, `userId + postId` must be **idempotent**: `UNIQUE(user_id, post_id)` or an equivalent distributed key.

---

## 26. Notification System

A likes B's post — don't synchronously call the notification service.

```text
Like Service
     |
     v
Kafka
     |
     v
Notification Service
     |
     ├── Push notification
     ├── In-app notification
     └── Email (if required)
```

This prevents notification failures from affecting likes.

---

## 27. Failure Scenario — Kafka Down

**Q: What if Kafka goes down?**

Kafka is critical for asynchronous processing, but the synchronous user operation should not necessarily fail if the event can be safely persisted first.

**Transactional Outbox Pattern:**

```text
API
 |
 v
DB Transaction
 |
 v
Outbox Table
 |
 v
Outbox Publisher
 |
 v
Kafka
```

In one DB transaction:

```sql
INSERT post
INSERT outbox_event
```

Both succeed / fail together. Then:

```text
Outbox Worker
     |
     v
Kafka
```

This prevents: DB updated, Kafka event lost.

---

## 28. Production Failure Scenario

**Q: Feed suddenly becomes slow. How do you debug?**

**Step 1 — Check metrics:** P50, P95, P99, RPS, CPU, Memory

**Step 2 — Check service health:** Feed Service, Redis, Post DB, Kafka

**Step 3 — Check Redis:** cache hit ratio, latency, connections, CPU, memory, evictions

**Step 4 — Check Kafka:** consumer lag, partition imbalance, broker health

**Step 5 — Distributed tracing:**

```text
Request
  |
  v
API Gateway
  |
  v
Feed Service
  |
  +---- Redis 15ms
  |
  +---- Post DB 200ms  <-- problem
```

Then inspect DB: slow queries, locks, connection pool, CPU, indexes.

---

## 29. End-to-End Feed Request

Memorize this flow:

```text
User opens Instagram
        |
        v
     CDN/WAF
        |
        v
  Load Balancer
        |
        v
   API Gateway
        |
        v
    Feed Service
        |
        v
   Redis Feed Cache
        |
        ├── HIT → Post IDs
        |
        └── MISS
             |
             v
        Feed Storage → Post IDs
             |
             v
    Candidate Generation
             |
             v
         Ranking
             |
             v
       Top 20 Posts
             |
             v
        Post Cache
             |
             v
           CDN
             |
             v
          Client
```

---

## 30. End-to-End Post Upload

```text
Client
   |
   | request upload URL
   v
Post Service
   |
   v
Pre-signed URL
   |
   v
Client
   |
   | direct upload
   v
Object Storage
   |
   v
Kafka
   |
   v
Media Processing
   |
   ├── resize
   ├── compress
   ├── transcode
   ├── thumbnail
   └── moderation
   |
   v
Object Storage
   |
   v
CDN
```

Then:

```text
Post Created
     |
     v
Kafka
     |
     ├── Feed Fanout
     ├── Recommendation
     ├── Notification
     └── Analytics
```

---

## 31. End-to-End DM

```text
User A
  |
  | WebSocket
  v
WebSocket Gateway
  |
  v
Messaging Service
  |
  ├── Message DB
  |
  └── Kafka
        |
        v
   Delivery Service
        |
        v
   User B WebSocket
```

If B is offline: Message DB + Push Notification. When B reconnects: fetch undelivered messages.

---

## 32. Most Important Interview Trade-offs

| Problem | Solution |
| --- | --- |
| Huge feed reads | Precomputed feed |
| Celebrity fan-out | Hybrid fan-out |
| Huge media | Object storage |
| Global media delivery | CDN |
| Video processing | Async workers |
| Async communication | Kafka |
| Fast reads | Redis |
| Billions of posts | Sharding |
| Search | Elasticsearch / OpenSearch |
| Real-time chat | WebSocket |
| Offline messages | Persistent message DB |
| Recommendations | Candidate + ranking |
| Location search | Geo index |
| Event loss | Outbox |
| Duplicate events | Idempotency |
| Kafka failure | Retry + DLQ |
| Feed pagination | Cursor |
| Hot users / posts | Sharding + caching |
| Observability | Logs + metrics + traces |

---

## 33. How I'd Present This in a 60-Minute Interview

| Time | Focus |
| --- | --- |
| 0–5 min | Requirements + assumptions |
| 5–10 min | Scale estimation |
| 10–20 min | High-level architecture |
| 20–30 min | Feed generation: fan-out-on-write, fan-out-on-read, hybrid, Redis, Kafka, ranking |
| 30–38 min | Media pipeline: S3, pre-signed URL, Kafka, workers, CDN |
| 38–45 min | Messaging: WebSocket, Kafka, message DB, offline delivery, ordering |
| 45–50 min | Recommendation + location |
| 50–55 min | Storage + caching + sharding |
| 55–60 min | Failures + monitoring + trade-offs |

---

## 34. Questions Interviewer Will Likely Ask

### Feed

- Why fan-out on write?
- Why not fan-out on read?
- How do you handle celebrities?
- What happens if Redis goes down?
- How do you paginate?
- How do you rank posts?
- How do you prevent duplicate posts?
- How do you handle a user following 50K people?

### Kafka

- How many partitions?
- What is your partition key?
- What happens when a consumer crashes?
- How do you handle duplicate events?
- What happens when Kafka is unavailable?
- Do you need exactly-once processing?

### Media

- Why S3 instead of DB?
- Why CDN?
- How do you process 4K videos?
- What if video processing fails?
- How do you retry?
- How do you prevent duplicate processing?

### Messaging

- Why WebSocket?
- What happens when the user is offline?
- How do you maintain ordering?
- How do you scale WebSocket servers?
- What happens when a WebSocket server crashes?

### Database

- SQL or NoSQL?
- How do you shard?
- What is your partition key?
- How do you handle hot partitions?
- How do you maintain consistency?

### Production

- Feed latency suddenly increases — debug it.
- Redis CPU reaches 95% — what do you do?
- Kafka consumer lag is increasing — why?
- DB connections are exhausted — how do you investigate?
- How do you monitor the entire system?

---

## The core architecture to remember

If you forget everything during the interview, remember this:

```text
                         ┌─────────────┐
                         │   Client    │
                         └──────┬──────┘
                                |
                         CDN / Load Balancer
                                |
                         ┌──────v──────┐
                         │ API Gateway │
                         └──────┬──────┘
                                |
        ┌───────────────┬───────┼────────┬───────────────┐
        |               |       |        |               |
        v               v       v        v               v
      User            Feed     Post   Messaging    Recommendation
     Service         Service  Service   Service        Service
        |               |       |        |
        |               v       v        v
        |             Redis   DB/S3    WebSocket
        |               |
        |               v
        |             Kafka
        |               |
        └───────────────┼──────────────────────────┐
                        |                          |
                        v                          v
                 Fanout Workers              Notifications
                        |
                        v
                   Feed Cache
```

```text
Post → S3 → Kafka → Media Workers → S3 → CDN
```

For your interview, the **single most important deep dive is Feed Generation**. Be able to explain fan-out-on-write vs fan-out-on-read, celebrity problem, Redis feed storage, Kafka partitioning, ranking, pagination, cache failure, duplicate events, and consistency without looking at notes.

---

# Part 2 — Celebrity / Hybrid Fan-out Deep Dive

This is the celebrity problem in Instagram feed design, and it is one of the most important HLD interview questions.

**The key idea:**

> Do **not** push a celebrity's post into millions of followers' feed caches. Instead, store the celebrity post once and merge it into each follower's feed at read / ranking time.

---

## 1. Why normal fan-out fails

Celebrity C creates Post P1 for 10 million followers.

With normal fan-out-on-write:

```text
P1
 |
 +--> Follower 1 Redis feed
 +--> Follower 2 Redis feed
 +--> Follower 3 Redis feed
 ...
 +--> Follower 10,000,000 Redis feed
```

That's **10 million writes** for one post. If the celebrity posts 10 times/day:

```text
10M × 10 = 100M feed writes/day
```

This creates huge Kafka traffic, huge Redis writes, hotspots, expensive storage, and large fan-out latency. So we don't do that.

---

## 2. Hybrid fan-out

**Normal user:**

```text
Post → Kafka → Fan-out workers → Followers' feed caches
```

**Celebrity:**

```text
Celebrity Post → Post DB → Redis / Post Cache
```

No millions of feed-cache writes.

Then when a follower opens Instagram:

```text
Follower opens feed
       |
       v
Feed Service
       |
       ├── Get precomputed feed
       ├── Get celebrity posts
       ├── Get recommended posts
       |
       v
     Ranking
       |
       v
     Top 20
```

---

## 3. Concrete example

Celebrity **C**, 10 million followers, creates **P100**.

We store in Post DB (`authorId = C`, media URL, timestamp, …) and cache `post:P100` in Redis.

We **do not** do:

```text
feed:user1  ← P100
feed:user2  ← P100
...
feed:user10000000 ← P100
```

Instead, maintain the relationship `C → followers` in a scalable follow graph.

---

## 4. Now follower opens feed

User **U123** follows A, B, **C (celebrity)**, D, E.

Precomputed feed from Redis `feed:U123`:

```text
[A-post-5, D-post-8, B-post-10, E-post-2]
```

Feed Service also gets recent posts from high-fanout accounts U123 follows: `C → P100`.

Candidates: A-post-5, D-post-8, B-post-10, E-post-2, C-post-100.

```text
             Candidate Posts
                    |
                    v
             Ranking Service
                    |
        ┌───────────┼───────────┐
        |           |           |
     freshness   relevance   engagement
        |           |           |
        └───────────┼───────────┘
                    |
                    v
                 Top 20
```

The celebrity post gets into the user's feed **without writing it into 10 million feeds**.

---

## 5. But won't this cause millions of reads?

Excellent interviewer question.

If 10 million followers open Instagram, would we query the celebrity's posts 10 million times? Yes, if implemented naively.

That's why we cache celebrity posts aggressively:

```text
Redis
celebrity_posts:C
       |
       v
[P100, P99, P98, P97...]
```

And actual media is behind CDN:

```text
Celebrity Post → CDN → S3
```

The system is not repeatedly hitting the database.

---

## 6. Don't query every followed user

If a user follows 2,000 accounts, we don't want 2,000 database queries.

Maintain a high-fanout / celebrity account list.

User follows: A (normal), B (normal), **C (celebrity)**, D (normal), **E (celebrity)**.

```text
highFanoutFollowing(U123)
       ↓
[C, E]
```

It only checks recent posts from C and E, then combines them with the precomputed feed.

---

## 7. The architecture

This is the architecture I would draw in an interview:

```text
                    Celebrity
                        |
                    Creates P100
                        |
                        v
                  Post Service
                        |
                ┌───────┴────────┐
                |                |
                v                v
             Post DB           Kafka
                |                |
                v                v
             Redis         Recommendation
          celebrity cache       etc.
                |
                v
             CDN
```

Notice: Kafka does **not** fan out P100 to 10 million feed caches.

Instead:

```text
                 Follower opens feed
                         |
                         v
                    Feed Service
                         |
            ┌────────────┼────────────┐
            |            |            |
            v            v            v
      Precomputed     Celebrity    Recommended
         Feed          Posts          Posts
            |            |            |
            └────────────┼────────────┘
                         v
                      Ranking
                         |
                         v
                       Top 20
```

---

## 8. What exactly is stored in Redis?

**Normal users:**

```text
feed:U123  →  [P91, P82, P71, P66, P60]     (post IDs)
```

**Celebrity:**

```text
celebrity_posts:C  →  [P100, P99, P98, P97]  (recent post IDs)
```

---

## 9. Pull only recent posts

A celebrity may have 50,000 posts. We don't fetch all 50,000. We only need something like the **latest 20–100 posts**. Ranking then selects the relevant ones.

---

## 10. What if the celebrity post becomes extremely viral?

P100, 100M followers, 50M users open Instagram. CDN + Redis become critical.

| Layer | Store |
| --- | --- |
| Metadata | Redis `post:P100` |
| Media | CDN → `P100.jpg` / `P100.mp4` |

```text
50M users
    |
    v
   CDN
    |
    +---- cache HIT → P100 media
```

Application servers don't need to serve the actual large video repeatedly.

---

## 11. Ranking still matters

You don't necessarily want every celebrity post at the top.

Candidates: P100 (celebrity), P200 (friend A), P300 (friend B), recommended posts.

Ranking considers: relationship, recency, engagement, user interest, previous interactions, content type.

- User who frequently interacts with the celebrity → P100 score = HIGH
- Someone who never interacts with the celebrity → P100 score = LOW

The celebrity post is shown appropriately rather than blindly pushed to everyone.

---

## 12. Recommended posts vs following

There are two paths:

**Following celebrity**

```text
Celebrity → Post → Celebrity-post cache → Feed candidate generation → Ranking → Follower feed
```

**Not following celebrity**

```text
Celebrity → Post → Recommendation pipeline → Candidate pool → Ranking → User feed
```

The recommendation system can decide that a celebrity's post is relevant even for users who don't follow them. This distinction is very important.

---

## 13. Interview-ready answer

**Q: How would you handle a celebrity with 10 million followers?**

> I would use a hybrid fan-out strategy. For normal users, I would use fan-out-on-write and precompute follower feeds in Redis because the follower count is manageable. For celebrity or high-fanout users, I would avoid writing the post into millions of follower feeds. Instead, I would store the post once in the post store and maintain a cached list of recent posts for that celebrity. When a follower requests their feed, the Feed Service retrieves the precomputed feed, fetches recent posts from high-fanout accounts they follow, adds recommendation candidates, and sends everything to the ranking layer. This avoids millions of writes while keeping read latency low. The actual media would be served through a CDN, so even if the post goes viral, application servers and the database aren't serving the media directly.

### One-line principle

- **Normal user** → push the post to followers.
- **Celebrity** → don't push to millions; pull the celebrity's recent posts during feed generation and rank them.
- **Media** → CDN, not application servers.
- **Recommendations** → separate candidate-generation + ranking pipeline.

---

# Part 3 — Celebrity Feed Cross-Questions

For an SDE-2 HLD interview, once you propose the hybrid approach, the interviewer will usually drill into scale, Redis, Kafka, consistency, ranking, hot keys, failures, and latency.

---

### 1. Why can't we simply fan-out to all 10M followers?

If one celebrity has 10M followers, one post would create 10M feed writes. Frequent posts create huge write amplification, Kafka traffic, Redis writes, storage pressure, and processing latency — plus a hotspot around the celebrity. For high-fanout users, prefer fan-out-on-read or hybrid.

### 2. Then aren't you shifting the problem from writes to reads?

Yes, but reads are much easier to optimize: cache the celebrity's recent posts centrally and serve media through CDN. Instead of 10M feed writes, we have a small number of writes and many cache-efficient reads.

**Follow-up:** 10M users can still read it.

Correct — that's why I don't query the database for every user. Recent post IDs and metadata are cached in Redis; media is cached at the CDN.

### 3. What exactly is stored in Redis?

```text
feed:user123           → [P91, P82, P71, P65]
celebrity_posts:user456 → [P100, P99, P98, P97]
```

The celebrity's feed isn't copied into every follower's Redis list.

### 4. How do you identify a celebrity?

Don't say: "If followers > 1 million." That's too simplistic.

> I'd use a dynamic high-fanout classification based on follower count, post frequency, and fan-out cost. For example, we can maintain a threshold such as 100K or 1M followers, but the threshold should be configurable and based on system load.

```text
followerCount > threshold
        OR
estimatedFanoutCost > threshold
        →  NORMAL | HIGH_FANOUT
```

### 5. What if someone suddenly crosses the threshold?

100K followers → 1M followers. Classification can be recalculated asynchronously. Once the account becomes high-fanout, **new posts** use the celebrity path. We don't need to migrate all historical feed entries immediately.

### 6. What happens if a celebrity has 100M followers?

Definitely wouldn't fan-out to 100M users. Write the post once to the post store, publish as an event, cache as a high-fanout user's recent post, pull during candidate generation. Media through CDN.

### 7. How do you prevent Redis from becoming a bottleneck?

Redis Cluster, sharding, replicas where appropriate, multiple cache nodes, TTL, avoid huge values, avoid single hot keys where possible.

### 8. What if `celebrity_posts:123` becomes a HOT KEY?

Excellent SDE-2 question. 10M users hitting the same Redis key.

Avoid relying on a single Redis node/key. Replicate hot data across multiple cache replicas or use key replication (`celebrity_posts:123:0`, `:1`, `:2`) and select replicas using hashing / load balancing. Read-heavy, relatively infrequent writes → replication is a good trade-off.

```text
                 Celebrity posts
                       |
        ┌──────────────┼──────────────┐
        ↓              ↓              ↓
      Redis-1        Redis-2        Redis-3
        ↑              ↑              ↑
        └────── Read traffic ──────────┘
```

### 9. What if the celebrity posts again?

Update the celebrity's recent-post cache: insert P101 at the head, evict old entries beyond the configured window.

```text
Before: [P100, P99, P98]
After:  [P101, P100, P99]
```

### 10. What if Redis is down?

Redis is a cache; source of truth remains the database. On cache miss or Redis failure, Feed Service can fall back to the post store, with rate limiting and degraded functionality.

Don't say "every request goes to DB" — that could overload the DB.

```text
Redis failure
     ↓
Fallback cache / replica
     ↓
DB only if necessary
     ↓
Circuit breaker + rate limit
```

### 11. What if Redis and DB are both slow?

Fail gracefully rather than allowing feed requests to block indefinitely. Return a partially populated feed from previously cached candidates or recommended content. Use strict timeouts, circuit breakers, and monitor P95 / P99 latency.

### 12. How do you guarantee that every follower sees the celebrity post?

This is a trick question. **Don't promise that.**

> For a feed system, I wouldn't guarantee that every follower sees every post. The feed is ranked and personalized. The requirement is that eligible posts are available as candidates, and the ranking system decides whether and where they appear.

**Available ≠ guaranteed to display.**

### 13. Is this strongly consistent?

No. The feed is generally **eventually consistent**. A newly created celebrity post may take a short time to become available. That's acceptable because feed freshness usually has a small tolerance.

### 14. User opens feed 1 second after the celebrity posts?

The post creation event should update the celebrity-post cache asynchronously. If near-real-time visibility is required, Feed Service can also query a very recent-post buffer or a strongly consistent post store for the latest few seconds. Freshness path without fan-out to millions.

```text
Normal path        → Redis
Very recent posts  → Recent-post buffer → Merge
```

### 15. Why use Kafka here?

Kafka decouples post creation from downstream processing.

```text
Post Service → Kafka → Feed / Recommendation / Notification / Analytics / Moderation
```

Post creation doesn't need to synchronously wait for all consumers.

### 16. What partition key would you use?

For `post-created`, we could partition by `authorId` if we want ordering of posts from the same author.

> The partition key depends on the consumer's requirement. For author-level ordering, `authorId` is reasonable. For feed fan-out work, we may instead partition by follower / user buckets to distribute work.

### 17. Won't the celebrity become a hot Kafka partition?

Yes. That's a potential hotspot. For extremely high-fanout users, avoid making all downstream fan-out work dependent on one author partition. Separate celebrity processing into a different pipeline, or use a bucketed key such as `authorId + bucketId` where strict ordering isn't required for the fan-out work.

### 18. How would you handle 100M followers efficiently?

Don't iterate 100M followers and write 100M feeds.

```text
Post Created
     |
     v
Celebrity Post Store
     |
     v
Central recent-post cache
     |
     v
Followers pull candidate
     |
     v
Ranking
```

### 19. How do you know whether a user follows the celebrity?

Following Service / social graph. Example: `following:user123 → [C, A, B, D]`. Relationship data itself needs partitioning / caching.

### 20. Would you query the following table for every feed request?

No. Cache the following graph or store it in a scalable graph / NoSQL representation. Feed Service should retrieve relevant followee IDs efficiently, preferably from a cached relationship set or precomputed user profile data.

### 21. What if the user follows 50,000 people?

We cannot fetch recent posts from all 50K users.

Don't treat every followee equally. Candidate generation uses multiple sources and limits. For normal followees, rely on precomputed feed entries. Only pull dynamically from high-fanout accounts or other special candidate sources. Bound the candidate set before ranking.

```text
50K followees
      ↓
Precomputed candidates
      +
High-fanout candidates
      +
Recommendations
      ↓
Bounded candidate set
```

### 22. How many candidates do you rank?

Don't say "all posts."

> Candidate generation should produce a bounded set, for example hundreds or a few thousand candidates depending on the system. We then apply cheaper filtering before sending the remaining candidates to the expensive ranking model.

```text
Potential content → 100K → Candidate filtering → 5K → Ranking → 100 → Top 20
```

### 23. What if the celebrity post is deleted?

Post store is the source of truth. Mark deleted / tombstoned, publish a deletion event. Consumers invalidate caches; ranking / candidate layer filters it out.

```text
Delete P100 → Post DB → Kafka → Redis invalidation / Recommendation removal / Search removal
```

### 24. What if the delete event is processed twice?

Downstream consumers should be **idempotent**. Deleting an already-deleted cache entry should produce the same final state.

**At-least-once delivery + idempotent consumer.**

### 25. What if Kafka sends the post event twice?

Deduplicate using `eventId` / `postId`. Example: `processed:eventId`, or enforce idempotency in the target datastore.

### 26. What if Feed Service crashes during ranking?

Ranking is a read-time operation, so there is no permanent data loss. The request can retry or another instance can handle it. Use load balancing, timeouts, and circuit breakers.

### 27. What if ranking service is completely down?

Degrade gracefully. Fall back to a simpler ranking strategy (recency + engagement) instead of failing the entire feed.

```text
ML Ranking → failure → Fallback ranking (recency + engagement)
```

This is **graceful degradation**.

### 28. What if the CDN is down?

Use multiple CDN origins / providers where justified, and ultimately fall back to object storage. Don't suddenly expose object storage to massive traffic — rate limiting and origin protection are important.

### 29. Why not just fan out slowly via Kafka?

Kafka can handle high event throughput, but the problem isn't only Kafka. The downstream work is the issue: millions of Redis writes, storage operations, network traffic, and cache entries. **Kafka doesn't eliminate that write amplification.**

### 30. What if the celebrity posts 100 times in one minute?

Don't keep all of them as feed candidates. Recent-post cache keeps latest N; ranking selects. Also apply frequency controls, deduplication, content diversity, ranking limits.

Example: don't show 10 consecutive posts from the same celebrity.

### 31. How do you prevent feed domination by one celebrity?

Ranking / product requirement. Use diversity constraints + per-author frequency cap + ranking penalty.

Example: maximum 1–2 posts from the same author within a feed window. The exact number is a product decision.

### 32. What happens when a user unfollows the celebrity?

```text
10:00 → User follows C
10:05 → P100 enters candidate pool
10:10 → User unfollows C
```

If feed was precomputed, old entries may still exist.

At read time, validate eligibility for dynamically sourced candidates, or maintain invalidation events for important relationship changes. Apply a final authorization / filtering step before returning the feed.

```text
Candidate → Eligibility check → Still follows celebrity?
   ├── YES → return
   └── NO  → filter
```

### 33. What if a private account posts?

Privacy / authorization must be checked **before** a post becomes a feed candidate. Visible only to authorized followers. Never rely solely on cache keys for authorization.

```text
private account → followers only
```

### 34. What if Redis contains a private post after the user unfollows?

**Cache is not the authorization source of truth.** Perform authorization / visibility validation. Also invalidate cached candidate relationships when access changes.

### 35. What metrics would you monitor?

| Area | Metrics |
| --- | --- |
| Feed | request rate, P50 / P95 / P99, cache hit ratio, candidate count, ranking latency |
| Kafka | consumer lag, throughput, partition skew, rebalance rate |
| Redis | CPU, memory, evictions, hit ratio, hot keys, latency, connections |
| DB | CPU, QPS, slow queries, connection pool, replication lag |
| CDN | cache hit ratio, origin requests, bandwidth, error rate, latency |

### 36. Walk-through scenario

**"A celebrity with 50M followers posts a video. Within 30 seconds, 20M users open Instagram. Walk me through exactly what happens."**

> The post is uploaded directly to object storage using a pre-signed URL. Media processing generates different resolutions and thumbnails asynchronously. The post metadata is stored in the post database and the post-created event is published to Kafka. Because the author is classified as high-fanout, we don't fan out the post to 50M follower feeds. Instead, we update the celebrity's recent-post cache. When followers request their feeds, the Feed Service combines their precomputed feed with a bounded set of recent high-fanout posts and recommendation candidates. The ranking service selects the final posts. The video itself is served through the CDN, so 20M users don't hit our application servers or database for the media. We monitor cache hit rate, CDN hit rate, feed P99, Kafka lag and Redis hot-key behavior.

---

## The 5 concepts you MUST be able to defend

1. **Hybrid Fan-out** — Normal → Fan-out Write; Celebrity → Fan-out Read
2. **Redis** — Precomputed feeds + celebrity recent posts
3. **CDN** — Actual image / video delivery
4. **Kafka** — Async event distribution
5. **Ranking** — Merge + personalize + limit celebrity frequency

**Most important distinction:**

> Kafka solves asynchronous event distribution; it does **not** magically solve the 10-million-write fan-out problem.

That is exactly the kind of follow-up distinction an SDE-2 interviewer may look for.

---

# Part 4 — Schema Design and API Contracts

I would not put everything into one database. Different access patterns:

| Data | Store |
| --- | --- |
| Users / relationships | SQL / distributed KV |
| Posts | Distributed NoSQL |
| Feed | Redis |
| Media | Object Storage |
| Messages | NoSQL |
| Likes | NoSQL / distributed KV |
| Comments | NoSQL |
| Search | Elasticsearch / OpenSearch |

---

## 1. User Schema

```text
users
-------------------------
user_id           BIGINT PK
username          VARCHAR UNIQUE
name              VARCHAR
bio               VARCHAR
profile_image_url VARCHAR
is_private        BOOLEAN
follower_count    BIGINT
following_count   BIGINT
created_at        TIMESTAMP
updated_at        TIMESTAMP
```

**Indexes:** `UNIQUE(username)`, `INDEX(created_at)`

Maintain counters separately if they become extremely high-write fields.

---

## 2. Follow Relationship

```text
follows
-------------------------
follower_id      BIGINT
followee_id      BIGINT
created_at       TIMESTAMP

PRIMARY KEY(follower_id, followee_id)
INDEX(followee_id, follower_id)
```

**Why two access patterns?**

- When user opens feed: `follower_id` → who does this user follow?
- When normal user posts: `followee_id` → who follows this user?

At very large scale, potentially maintain separate distributed views:

```text
following:{userId}
followers:{userId}
```

---

## 3. Post Schema

```text
posts
-------------------------
post_id             BIGINT PK
author_id           BIGINT
caption             TEXT
media_type          ENUM
media_url           VARCHAR
thumbnail_url       VARCHAR
visibility          ENUM
location_id         BIGINT NULL
created_at          TIMESTAMP
updated_at          TIMESTAMP
status              ENUM
```

Example: `post_id = 10001`, `author_id = 500`, `media_type = VIDEO`, `visibility = PUBLIC`, `status = ACTIVE`.

**Index:** `INDEX(author_id, created_at DESC)` — useful for getting latest posts of a celebrity.

---

## 4. Media Schema

Keep media metadata separate from the post.

```text
media
-------------------------
media_id             BIGINT PK
post_id              BIGINT
media_type           ENUM
original_url         VARCHAR
processed_url        VARCHAR
thumbnail_url        VARCHAR
width                INT
height               INT
duration_ms          BIGINT
processing_status    ENUM
created_at           TIMESTAMP
```

Actual files are **not** stored in this table. They live in object storage:

```text
Object Storage
      |
      ├── original
      ├── 360p
      ├── 720p
      ├── 1080p
      └── thumbnails
```

---

## 5. Celebrity / High-Fanout Schema

Don't hard-code "celebrity." Create a classification.

```text
high_fanout_users
-------------------------
user_id              BIGINT PK
follower_count       BIGINT
fanout_mode          ENUM
updated_at           TIMESTAMP
```

Example: `user_id = 500`, `follower_count = 50,000,000`, `fanout_mode = READ`

| Type | `fanout_mode` |
| --- | --- |
| Normal | `WRITE` |
| Celebrity | `READ` |

---

## 6. Redis Feed Schema

For normal users: `feed:{userId}`

Use a Redis Sorted Set:

```text
ZADD feed:1001 1728392000 P900
ZREVRANGE feed:1001 0 19    # latest 20 posts
```

---

## 7. Celebrity Post Cache

```text
celebrity_posts:{userId}

ZADD celebrity_posts:500 timestamp postId
ZREVRANGE celebrity_posts:500 0 49    # latest 50 posts
```

---

## 8. Recommendation Cache

```text
recommendations:{userId}  →  [P700, P701, P702, P810]
```

Candidate posts generated by the recommendation system.

---

## 9. Like Schema

Don't do `posts.like_count++` for every like at massive scale.

```text
post_likes
-------------------------
post_id       BIGINT
user_id       BIGINT
created_at    TIMESTAMP

PRIMARY KEY(post_id, user_id)
```

`user + post` is unique, so liking twice doesn't create duplicate likes.

---

## 10. Comment Schema

```text
comments
-------------------------
comment_id       BIGINT PK
post_id          BIGINT
user_id          BIGINT
text             TEXT
created_at       TIMESTAMP
parent_id        BIGINT NULL
status           ENUM

INDEX(post_id, created_at DESC)
```

Efficiently fetch comments by `post_id`.

---

## 11. Message Schema

```text
conversations
-------------------------
conversation_id    BIGINT PK
created_at         TIMESTAMP
updated_at         TIMESTAMP

conversation_members
-------------------------
conversation_id    BIGINT
user_id            BIGINT
PRIMARY KEY(conversation_id, user_id)

messages
-------------------------
message_id         BIGINT
conversation_id    BIGINT
sender_id          BIGINT
message_type       ENUM
content            TEXT
created_at         TIMESTAMP
sequence_number    BIGINT
status             ENUM

PRIMARY KEY(conversation_id, sequence_number)
```

`conversation_id` + `sequence_number` give message ordering.

---

## 12. Location Schema

```text
locations
-------------------------
location_id       BIGINT PK
name              VARCHAR
latitude          DOUBLE
longitude         DOUBLE
geohash           VARCHAR
```

At scale, use Redis GEO, PostGIS, or Elasticsearch / OpenSearch depending on requirements.

---

## 13. Notification Schema

```text
notifications
-------------------------
notification_id     BIGINT
user_id             BIGINT
actor_id            BIGINT
type                ENUM
entity_id           BIGINT
is_read             BOOLEAN
created_at          TIMESTAMP
```

Example: `actor_id = 200`, `user_id = 100`, `type = LIKE`, `entity_id = 5000` → User 200 liked your post 5000.

---

## 14. Feed API Contract

This is the most important API.

```http
GET /v1/feed?cursor=eyJpZCI6IjkwMCJ9&limit=20
Authorization: Bearer <token>
```

```json
{
  "data": [
    {
      "postId": "900",
      "author": {
        "userId": "500",
        "username": "celebrity",
        "profileImageUrl": "https://cdn..."
      },
      "media": {
        "type": "VIDEO",
        "url": "https://cdn.../900.mp4",
        "thumbnailUrl": "https://cdn.../900.jpg"
      },
      "caption": "New video!",
      "likeCount": 1200000,
      "commentCount": 23000,
      "createdAt": "2026-10-08T10:00:00Z"
    }
  ],
  "nextCursor": "eyJpZCI6Ijg4MCJ9",
  "hasMore": true
}
```

### How Feed API works internally

```text
GET /feed
     |
     v
Feed Service
     |
     ├───────────────┐
     |               |
     v               v
Redis Feed       High-Fanout
     |              Posts
     |               |
     └───────┬───────┘
             |
             v
     Recommendation Candidates
             |
             v
        Deduplication
             |
             v
          Ranking
             |
             v
        Top 20 posts
             |
             v
       Post Metadata
             |
             v
          Response
```

---

## 15. Create Post API

For media upload, separate upload initialization from post creation.

### Step 1 — Request upload URL

```http
POST /v1/media/upload-url
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "fileName": "reel.mp4",
  "contentType": "video/mp4",
  "sizeBytes": 25000000
}
```

```json
{
  "uploadId": "UP123",
  "uploadUrl": "https://object-storage...",
  "expiresIn": 900
}
```

Client then uploads directly to object storage.

### Step 2 — Create post

```http
POST /v1/posts
Authorization: Bearer <token>
Idempotency-Key: 8f91...
Content-Type: application/json
```

```json
{
  "uploadId": "UP123",
  "caption": "My new reel",
  "visibility": "PUBLIC",
  "locationId": "LOC123"
}
```

```json
{
  "postId": "P10001",
  "status": "PROCESSING",
  "createdAt": "2026-10-08T10:20:00Z"
}
```

Return `PROCESSING` because video processing is asynchronous.

---

## 16. Follow / Unfollow API

```http
POST /v1/users/{userId}/follow
Authorization: Bearer <token>
```

```json
{ "following": true }
```

```http
DELETE /v1/users/{userId}/follow
Authorization: Bearer <token>
```

---

## 17. Like API

```http
POST /v1/posts/{postId}/like
Authorization: Bearer <token>
Idempotency-Key: <key>
```

```json
{ "postId": "P10001", "liked": true }
```

```http
DELETE /v1/posts/{postId}/like
Authorization: Bearer <token>
```

---

## 18. Comment API

```http
POST /v1/posts/{postId}/comments
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{ "text": "Amazing!" }
```

```json
{
  "commentId": "C1001",
  "postId": "P10001",
  "text": "Amazing!",
  "createdAt": "2026-10-08T10:30:00Z"
}
```

```http
GET /v1/posts/{postId}/comments?cursor=abc&limit=20
```

---

## 19. Get User Profile

```http
GET /v1/users/{userId}
Authorization: Bearer <token>
```

```json
{
  "userId": "500",
  "username": "celebrity",
  "name": "Celebrity",
  "profileImageUrl": "https://cdn...",
  "followerCount": 50000000,
  "followingCount": 1000,
  "isFollowing": true
}
```

---

## 20. DM API

```http
POST /v1/conversations/{conversationId}/messages
Authorization: Bearer <token>
```

```json
{
  "clientMessageId": "CMSG-123",
  "type": "TEXT",
  "content": "Hello"
}
```

```json
{
  "messageId": "M10001",
  "conversationId": "C100",
  "sequenceNumber": 1245,
  "status": "SENT",
  "createdAt": "2026-10-08T10:40:00Z"
}
```

---

## 21. WebSocket Contract

```text
wss://api.instagram.com/v1/ws
```

Client sends:

```json
{
  "type": "SEND_MESSAGE",
  "conversationId": "C100",
  "clientMessageId": "CMSG123",
  "content": "Hello"
}
```

Server ACK:

```json
{
  "type": "MESSAGE_ACK",
  "messageId": "M1001",
  "sequenceNumber": 100
}
```

Receiver:

```json
{
  "type": "NEW_MESSAGE",
  "messageId": "M1001",
  "conversationId": "C100",
  "senderId": "200",
  "content": "Hello",
  "sequenceNumber": 100
}
```

---

## 22. Kafka Event Contracts

**`post-created`**

```json
{
  "eventId": "E123",
  "eventType": "POST_CREATED",
  "postId": "P100",
  "authorId": "U500",
  "createdAt": "2026-10-08T10:00:00Z"
}
```

**`post-deleted`**

```json
{
  "eventId": "E124",
  "eventType": "POST_DELETED",
  "postId": "P100",
  "authorId": "U500"
}
```

**`post-liked`**

```json
{
  "eventId": "E125",
  "eventType": "POST_LIKED",
  "postId": "P100",
  "userId": "U200"
}
```

---

## 23. Celebrity Flow With Schema + API

Celebrity **U500**, 50M followers, `fanout_mode = READ`.

Calls `POST /v1/posts` → Post **P100**, `author_id = U500`.

Stored in `posts`. Event published: Kafka `post-created`.

Because `fanout_mode = READ`, we **don't** execute 50M Redis writes. Instead:

```text
ZADD celebrity_posts:U500 timestamp P100
```

Follower calls `GET /v1/feed?limit=20`. Feed Service:

1. Redis → user's precomputed feed
2. Redis → high-fanout posts
3. Recommendation → candidates
4. Deduplicate
5. Authorization
6. Ranking
7. Top 20

P100 is included if ranking determines that it belongs in the user's feed.

---

## 24. Idempotency

For write APIs such as `POST /posts`, `POST /like`, `POST /messages`, support:

```http
Idempotency-Key: <unique-client-key>
```

Network timeout: client POSTs like, server succeeds, response is lost, client retries.

- Without idempotency: LIKE, LIKE (duplicate)
- With the same Idempotency-Key: return previous result

This is a very good SDE-2 point.

---

## 25. Final Interview Architecture

Put this on the board:

```text
                       CLIENT
                          |
                          v
                    API Gateway
                          |
          ┌───────────────┼────────────────┐
          |               |                |
          v               v                v
      User Service    Post Service    Feed Service
          |               |                |
          v               v                v
      User DB          Post DB           Redis
          |               |                |
          |               v                |
          |              Kafka             |
          |               |                |
          |       ┌───────┼────────┐       |
          |       |       |        |       |
          |       v       v        v       |
          |     Media  Feed     Recomm.    |
          |   Workers  Worker    Service    |
          |       |                       |
          |       v                       |
          |      S3                       |
          |       |                       |
          |       v                       |
          |      CDN <--------------------┘
          |
          v
      Follow Store


              MESSAGING
                  |
                  v
          WebSocket Gateway
                  |
                  v
          Messaging Service
             |          |
             v          v
         Message DB   Kafka
```

### Core schema decision

| What | Where |
| --- | --- |
| Post | Post DB |
| Media | Object Storage |
| Feed | Redis |
| Relationships | Follow Store |
| Messages | Message DB |
| Events | Kafka |
| Search | Search Index |
| Actual media delivery | CDN |

### Core API principle

- **Synchronous API** handles the user-facing operation.
- **Kafka** handles asynchronous downstream work.
- **Redis** handles hot / read-heavy data.
- **Object storage + CDN** handles media.
- **Database** remains the source of truth.
