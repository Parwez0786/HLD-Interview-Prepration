# Food Delivery System — HLD Interview

Assume the interviewer asks:

> Design a food delivery system like Swiggy / Zomato / Uber Eats where customers can search restaurants, place orders, pay, and track delivery.

A good answer should **not** immediately jump to Kafka or Redis Geo. Start by clarifying requirements.

---

## Table of Contents

- [1. Start with Requirements](#1-start-with-requirements)
- [2. High-Level Architecture](#2-high-level-architecture)
- [3. Main Services](#3-main-services)
- [4. Search Service](#4-search-service)
- [5. Cart Service](#5-cart-service)
- [6. Order Service](#6-order-service)
- [7. Order Database Schema](#7-order-database-schema)
- [8. Order Placement Flow](#8-order-placement-flow)
- [9. Why Kafka?](#9-why-kafka)
- [10. Payment Design](#10-payment-design)
- [11. Idempotency](#11-idempotency)
- [12. Restaurant Order Processing](#12-restaurant-order-processing)
- [13. Delivery Partner Assignment](#13-delivery-partner-assignment)
- [14. Delivery Assignment Algorithm](#14-delivery-assignment-algorithm)
- [15. Same Driver Selected Twice](#15-what-if-two-orders-select-the-same-driver)
- [16. Real-Time Delivery Tracking](#16-real-time-delivery-tracking)
- [17. Why WebSocket?](#17-why-websocket)
- [18. Notification Service](#18-notification-service)
- [19. Database Choice](#19-database-choice)
- [20. Database Scaling](#20-database-scaling)
- [21. Sharding](#21-sharding)
- [22. Restaurant Search Scalability](#22-restaurant-search-scalability)
- [23. Handling Peak Traffic](#23-handling-peak-traffic)
- [24. Kafka Partitioning](#24-kafka-partitioning)
- [25. Exactly-Once Problem](#25-exactly-once-problem)
- [26. Outbox Pattern](#26-outbox-pattern)
- [27. Restaurant Acceptance Timeout](#27-restaurant-acceptance-timeout)
- [28. Payment Succeeds but Restaurant Rejects](#28-payment-succeeds-but-restaurant-rejects)
- [29. Restaurant Is Offline](#29-restaurant-is-offline)
- [30. Food Item Becomes Unavailable](#30-food-item-becomes-unavailable)
- [31. Cart Price Manipulation](#31-cart-price-manipulation)
- [32. Cancellation](#32-cancellation)
- [33. Order Status Consistency](#33-order-status-consistency)
- [34. API Design](#34-api-design)
- [35. API Idempotency](#35-api-idempotency)
- [36. Security](#36-security)
- [37. Rate Limiting](#37-rate-limiting)
- [38. Failure Handling](#38-failure-handling)
- [39. Observability](#39-observability)
- [40. Caching Strategy](#40-caching-strategy)
- [41. CAP Theorem](#41-cap-theorem-follow-up)
- [42. Disaster Recovery](#42-disaster-recovery)
- [43. Delivery Location Scaling](#43-delivery-location-scaling)
- [44. Nearby Restaurant Problem](#44-nearby-restaurant-problem)
- [45. ETA Calculation](#45-eta-calculation)
- [46. Surge / High Demand](#46-surge--high-demand)
- [47. Multi-Restaurant Cart](#47-multi-restaurant-cart)
- [48. Saga Pattern](#48-saga-pattern)
- [49. Most Important Cross-Questions](#49-most-important-interviewer-cross-questions)
- [50. Final Architecture](#50-final-architecture-id-draw-in-interview)

---

## 1. Start with Requirements

I would first clarify the scope.

### Functional requirements

We need three main actors:

**Customer**

- Search restaurants
- View menu
- Add items to cart
- Place order
- Make payment
- Track order
- Cancel order where allowed
- Rate restaurant / delivery partner

**Restaurant**

- Manage menu
- Accept / reject orders
- Update food preparation status
- Mark order ready

**Delivery Partner**

- Go online / offline
- Receive delivery request
- Accept / reject delivery
- Pick up order
- Deliver order
- Update location

### Non-functional requirements

- High availability
- Low latency for restaurant / search APIs
- Scalable during lunch / dinner peaks
- Strong consistency for payment and order state
- Eventual consistency is acceptable for tracking / search
- Fault tolerance
- Idempotent APIs
- Secure payment

---

## 2. High-Level Architecture

I would divide the system into multiple services.

```text
                    ┌──────────────┐
                    │   Customer   │
                    │     App      │
                    └──────┬───────┘
                           │
                           ▼
                    ┌──────────────┐
                    │ API Gateway  │
                    └──────┬───────┘
                           │
       ┌───────────────────┼────────────────────┐
       │                   │                    │
       ▼                   ▼                    ▼
 ┌───────────┐      ┌────────────┐      ┌─────────────┐
 │ Restaurant│      │   Search   │      │   User      │
 │  Service  │      │  Service   │      │  Service    │
 └─────┬─────┘      └─────┬──────┘      └─────────────┘
       │                  │
       ▼                  ▼
 Restaurant DB       Elasticsearch


       ┌─────────────────────────────┐
       │       Order Service         │
       └─────────────┬───────────────┘
                     │
                     ▼
                  Kafka
                     │
       ┌─────────────┼──────────────────┐
       │             │                  │
       ▼             ▼                  ▼
 Payment       Restaurant         Delivery
 Service        Service            Service
       │             │                  │
       ▼             ▼                  ▼
 Payment DB     Restaurant DB      Redis / Location DB

                     │
                     ▼
              Notification Service
```

---

## 3. Main Services

### 1. User Service

Responsible for user profile, address, authentication, and user preferences.

```text
User
 ├── userId
 ├── name
 ├── phone
 └── addresses
```

### 2. Restaurant Service

Responsible for restaurant information, menu, item availability, and restaurant open/close status.

```text
Restaurant
 ├── restaurantId
 ├── name
 ├── latitude
 ├── longitude
 ├── status
 └── cuisine

MenuItem
 ├── itemId
 ├── restaurantId
 ├── name
 ├── price
 ├── available
 └── category
```

---

## 4. Search Service

This is an important scalability point.

I would **not** query the restaurant database directly for every search.

Instead:

```text
Restaurant DB
      │
      │ CDC / Kafka
      ▼
 Elasticsearch
      │
      ▼
 Search Service
```

Search can support: Pizza, Biryani, Chinese, McDonald's, Restaurants near me.

For location-based search, Elasticsearch geo queries can find restaurants within a radius.

**Interview follow-up:** Why Elasticsearch?

> Restaurant search is read-heavy and requires text search, filtering and geo queries. A relational database can handle basic queries, but Elasticsearch is better suited for these search requirements and can scale search independently.

---

## 5. Cart Service

Cart is usually temporary data, so I would use Redis.

```text
cart:userId
    |
    ├── restaurantId
    ├── itemId
    ├── quantity
    └── price snapshot
```

Example: `cart:123` — restaurantId = 456, Pizza → quantity 2, Coke → quantity 1.

**Why Redis?**

- Very fast
- High read / write volume
- Temporary data
- TTL can automatically remove abandoned carts

---

## 6. Order Service

This is the **core service**.

**Order states:**

```text
CREATED
   ↓
PAYMENT_PENDING
   ↓
PAYMENT_SUCCESS
   ↓
RESTAURANT_ACCEPTED
   ↓
PREPARING
   ↓
READY_FOR_PICKUP
   ↓
PICKED_UP
   ↓
OUT_FOR_DELIVERY
   ↓
DELIVERED
```

**Failure states:** `PAYMENT_FAILED`, `RESTAURANT_REJECTED`, `CANCELLED`, `DELIVERY_FAILED`.

I would implement this as a **state machine** so that invalid transitions are not allowed.

For example, `DELIVERED → PREPARING` should not be possible.

---

## 7. Order Database Schema

### `orders`

| Column | Purpose |
| --- | --- |
| `order_id` | Primary key |
| `user_id` | Customer |
| `restaurant_id` | Restaurant |
| `delivery_partner_id` | Assigned driver |
| `address_id` | Delivery address |
| `status` | State machine status |
| `subtotal` | Item total |
| `delivery_fee` | Delivery charge |
| `tax` | Tax |
| `discount` | Discount |
| `total_amount` | Final amount |
| `created_at` / `updated_at` | Timestamps |

### `order_items`

| Column | Purpose |
| --- | --- |
| `order_item_id` | Primary key |
| `order_id` | Parent order |
| `item_id` | Menu item |
| `item_name` | **Snapshot** |
| `quantity` | Quantity |
| `price` | **Snapshot** |

**Important:** I would store `item_name` and `price` inside `order_items`.

**Why?** Suppose Pizza was ₹200 when the customer ordered it. Tomorrow the restaurant changes it to ₹250. The old order must still show **Pizza ₹200**.

Therefore we store the **price snapshot**.

---

## 8. Order Placement Flow

This is a very important interview flow.

```text
Customer:
Add to cart
     ↓
Checkout
     ↓
Create Order
     ↓
Payment
     ↓
Restaurant accepts
     ↓
Find delivery partner
     ↓
Food preparation
     ↓
Delivery
     ↓
Delivered
```

More technically:

```text
Customer
   |
   | POST /orders
   ▼
Order Service
   |
   | Validate cart
   |
   | Create order
   ▼
Payment Service
   |
   | Payment
   ▼
Kafka
   |
   ├── Restaurant Service
   ├── Delivery Service
   └── Notification Service
```

---

## 9. Why Kafka?

I would use Kafka for asynchronous communication.

Example events:

- `order.created`
- `order.payment.success`
- `order.restaurant.accepted`
- `order.food.ready`
- `order.delivery.assigned`
- `order.delivered`

Instead of Order → Restaurant → Delivery → Notification **synchronously**, we publish an event:

```text
Order Service
      |
      ▼
Kafka
 ┌────┼──────┬────────────┐
 ▼    ▼      ▼            ▼
Restaurant Delivery Notification Analytics
```

**Follow-up: Why not REST?**

REST is good when we need an immediate response.

Kafka is better when:

- Multiple services need the same event
- Processing can happen asynchronously
- We need buffering
- We need retry
- We need decoupling

---

## 10. Payment Design

Payment requires **stronger consistency**.

```text
Customer
   ↓
Order Service
   ↓
Create PAYMENT_PENDING
   ↓
Payment Service
   ↓
Payment Gateway
   ↓
Success / Failure
```

Suppose payment succeeds but our server crashes before updating the order. This is a classic failure case.

**Solution:** Use payment transaction ID, idempotency key, payment status reconciliation, and webhook from payment provider.

Example fields: `payment_id`, `order_id`, `idempotency_key`, `status`, `provider_transaction_id`.

---

## 11. Idempotency

**Interviewer:** What happens if customer clicks Pay twice?

We should not create two payments / orders.

Client sends `Idempotency-Key: abc123`.

Payment service stores `abc123 → payment_789`.

If the same request comes again with `abc123`, we return the previous result.

---

## 12. Restaurant Order Processing

When payment succeeds:

```text
payment.success
       ↓
Kafka
       ↓
Restaurant Service
       ↓
Notify restaurant
       ↓
Accept / Reject
```

Restaurant can respond `ACCEPTED` or `REJECTED`.

If rejected:

```text
Restaurant rejected
       ↓
Order cancelled
       ↓
Refund
       ↓
Notification
```

---

## 13. Delivery Partner Assignment

This is one of the most important follow-ups.

We have restaurant location + available delivery partners. We need to find a nearby delivery partner.

I would maintain active delivery partner locations in Redis.

```text
Driver 1 → (28.61, 77.20)
Driver 2 → (28.62, 77.21)
Driver 3 → (28.70, 77.30)
```

We can use a geo index:

```text
GEOADD drivers longitude latitude driverId
```

Then `GEORADIUS` / `GEOSEARCH` to find nearby drivers.

---

## 14. Delivery Assignment Algorithm

**Basic version:**

```text
Restaurant
    ↓
Find drivers within 2 km
    ↓
Filter online drivers
    ↓
Filter available drivers
    ↓
Rank drivers
    ↓
Send request
    ↓
Driver accepts
```

Ranking can consider: distance, ETA, current workload, vehicle type, driver rating.

---

## 15. What If Two Orders Select the Same Driver?

This is a **concurrency problem**.

Suppose Order A → Driver 10 and Order B → Driver 10 happen simultaneously.

We need **atomic assignment**.

**One approach:** Redis `SETNX` on `driver:10:assignment`. Only one request succeeds.

**Or** a database conditional update:

```sql
UPDATE drivers
SET status = 'BUSY'
WHERE driver_id = 10
AND status = 'AVAILABLE';
```

Then check affected rows.

- `affected rows = 1` → assignment succeeded
- `affected rows = 0` → someone else already assigned the driver

---

## 16. Real-Time Delivery Tracking

Customer wants: Where is my delivery partner?

We don't want the mobile app to continuously poll the server.

Instead:

```text
Driver App
    ↓
Location Service
    ↓
Redis
    ↓
WebSocket
    ↓
Customer App
```

Driver sends location every few seconds: `driverId`, `latitude`, `longitude`, `timestamp`.

Redis stores the **latest location**.

---

## 17. Why WebSocket?

Normal REST: Customer → Server "Where is driver?" every few seconds. This creates unnecessary requests.

With WebSocket, Customer ↔ Server. Server can push: Driver moved, Driver arrived, Order status changed — in real time.

---

## 18. Notification Service

Notifications can be asynchronous.

Events: `order.created`, `payment.success`, `restaurant.accepted`, `food.ready`, `driver.assigned`, `order.delivered`.

Notification service consumes them and can send Push, SMS, Email, WhatsApp.

We use Kafka so notification failure **doesn't block** order processing.

---

## 19. Database Choice

I would use different databases for different requirements.

| Requirement | Technology |
| --- | --- |
| Orders | PostgreSQL / MySQL |
| Users | PostgreSQL / MySQL |
| Restaurant / Menu | PostgreSQL |
| Cart | Redis |
| Driver location | Redis Geo |
| Search | Elasticsearch |
| Events | Kafka |
| Analytics | Data warehouse |
| Cache | Redis |

**Follow-up: Why SQL for orders?**

Because orders require transactions, strong consistency, relationships, and reliable state changes.

---

## 20. Database Scaling

Suppose we have millions of orders.

Initially: Order Service → Primary DB.

Later:

```text
             ┌── Read Replica 1
             │
Order Service├── Read Replica 2
             │
             └── Primary
```

Writes go to primary. Read-heavy queries go to replicas.

---

## 21. Sharding

If one database is no longer sufficient, we can shard orders.

Possible shard key: `user_id` or `order_id`.

```text
Shard 1 → users 1–10M
Shard 2 → users 10M–20M
Shard 3 → users 20M–30M
```

But we should **not shard too early**.

---

## 22. Restaurant Search Scalability

Search is highly read-heavy.

We can cache popular searches ("pizza + Bangalore", "biryani + Hyderabad") using Redis.

```text
Customer
   ↓
Search Service
   ↓
Redis
   │
   └── cache miss
          ↓
    Elasticsearch
```

---

## 23. Handling Peak Traffic

Food delivery has predictable spikes: 12 PM–2 PM, 7 PM–10 PM.

We should horizontally scale services.

```text
              Load Balancer
                    |
       ┌────────────┼────────────┐
       ▼            ▼            ▼
 Order-1         Order-2       Order-3
```

Kafka also acts as a **buffer**.

If restaurant processing becomes slow:

```text
Order Service
     ↓
Kafka
     ↓
Restaurant consumers
```

Orders aren't immediately lost.

---

## 24. Kafka Partitioning

Suppose `orders` topic has 12 partitions.

We could partition by `restaurantId`. This is useful because events for the same restaurant maintain ordering within the partition.

```text
Restaurant 101 → Partition 3

order.created
payment.success
restaurant.accepted
food.ready
```

**Follow-up: What if partition becomes hot?**

If one restaurant receives huge traffic, partitioning only by restaurant ID can create a hot partition.

We can choose a better partitioning strategy depending on the workload, or isolate exceptionally high-volume entities.

---

## 25. Exactly-Once Problem

**Interviewer:** Kafka guarantees exactly-once, right?

I would say:

> Kafka can provide exactly-once semantics in specific Kafka-to-Kafka transactional workflows, but an end-to-end food delivery workflow involving databases and external payment systems cannot simply assume exactly-once delivery. I would design consumers to be idempotent.

For example `eventId = 12345`. Store processed events in `processed_events` (`event_id`, `processed_at`). If the event comes again, ignore it.

---

## 26. Outbox Pattern

Very important HLD follow-up.

Suppose Order Service does DB update + Kafka publish.

What if DB update succeeds and Kafka publish fails? Now order exists but event doesn't.

**Solution: Transactional Outbox**

Order DB has `orders` and `outbox_events`.

Inside one DB transaction:

```text
INSERT order
INSERT outbox_event
COMMIT
```

Then a publisher reads `outbox_events` → Kafka.

This prevents losing the event.

---

## 27. Restaurant Acceptance Timeout

Suppose restaurant doesn't respond. We don't want the order to stay WAITING forever.

We can use `order.acceptance_deadline` and a scheduled worker.

Example: Restaurant has 2 minutes. If timeout:

```text
WAITING
   ↓
AUTO_CANCELLED
   ↓
REFUND
```

---

## 28. Payment Succeeds but Restaurant Rejects

This is another common follow-up.

```text
Payment SUCCESS
       ↓
Restaurant REJECTED
       ↓
Order CANCELLED
       ↓
Refund initiated
```

Refund should also be **idempotent**: `refund_id`, `order_id`, `payment_id`, `status`.

---

## 29. Restaurant Is Offline

Suppose customer places order while restaurant suddenly goes offline. We should not blindly accept.

Restaurant Service maintains: `OPEN`, `CLOSED`, `BUSY`, `TEMPORARILY_UNAVAILABLE`.

Before checkout we check availability.

But this is not enough because restaurant can go offline **after** checkout. Therefore restaurant still gets the order and can reject it.

---

## 30. Food Item Becomes Unavailable

Suppose customer adds Paneer Pizza, then restaurant marks it unavailable before checkout.

At checkout:

```text
Cart
 ↓
Validate latest menu
 ↓
Check availability
 ↓
Create order
```

We should **not** trust the cart price / availability blindly.

---

## 31. Cart Price Manipulation

Suppose frontend sends Pizza = ₹10 even though actual price is ₹300.

**Never trust frontend price.**

At checkout:

```text
Cart item IDs
      ↓
Restaurant / Menu Service
      ↓
Fetch current price
      ↓
Calculate total on backend
```

Backend is the **source of truth**.

---

## 32. Cancellation

Cancellation depends on state.

| State | Cancellable? |
| --- | --- |
| CREATED | YES |
| PAYMENT_PENDING | YES |
| PREPARING | MAYBE |
| READY | MAYBE |
| PICKED_UP | NO |
| DELIVERED | NO |

Business rules decide refund amount.

---

## 33. Order Status Consistency

Suppose restaurant sends PREPARING, then due to retry: ACCEPTED.

We shouldn't move **backward**.

We can maintain allowed transitions:

```text
ACCEPTED → PREPARING
PREPARING → READY
READY → PICKED_UP
PICKED_UP → DELIVERED
```

Service validates every transition.

---

## 34. API Design

| Area | APIs |
| --- | --- |
| Search | `GET /restaurants?lat=...&lng=...&cuisine=pizza` |
| Restaurant | `GET /restaurants/{restaurantId}` |
| Menu | `GET /restaurants/{restaurantId}/menu` |
| Cart | `POST /cart/items`, `GET /cart`, `DELETE /cart/items/{itemId}` |
| Order | `POST /orders`, `GET /orders/{orderId}`, `POST /orders/{orderId}/cancel` |
| Delivery | `POST /delivery/{orderId}/accept`, `POST /delivery/{orderId}/pickup`, `POST /delivery/{orderId}/deliver` |

---

## 35. API Idempotency

For `POST /orders` we should support `Idempotency-Key`, because the customer may click twice or the network may retry.

Example: `Idempotency-Key: xyz123` → same key → same order response.

---

## 36. Security

I would mention:

- JWT / OAuth authentication
- HTTPS
- Authorization based on role
- Don't store card details
- Payment handled by payment provider
- Rate limiting
- Input validation
- Encrypt sensitive data
- Audit logs

**Roles:** `CUSTOMER`, `RESTAURANT`, `DELIVERY_PARTNER`, `ADMIN`.

---

## 37. Rate Limiting

Suppose malicious client sends 1000 requests/sec.

API Gateway can apply 100 requests/min/user using Redis-based rate limiting.

For example: `userId + API` as the rate-limit key.

---

## 38. Failure Handling

| Failure | Response |
| --- | --- |
| **Kafka unavailable** | Use retries and ensure events remain safely persisted through the outbox |
| **Redis unavailable** | Cart: temporary degradation. Driver location: show last known location |
| **Elasticsearch unavailable** | Search can degrade temporarily or use a fallback strategy |
| **Payment service unavailable** | Order remains `PAYMENT_PENDING` rather than falsely marking it failed / successful |

---

## 39. Observability

**Logs:** `orderId`, `userId`, `restaurantId`, `requestId`.

**Metrics:** orders/sec, payment success rate, order latency, restaurant acceptance rate, delivery assignment time, Kafka consumer lag.

**Distributed tracing:**

```text
API Gateway
   ↓
Order Service
   ↓
Payment Service
   ↓
Kafka
   ↓
Restaurant Service
```

Use a common `traceId` to follow one order across services.

---

## 40. Caching Strategy

**Good candidates:** Restaurant details, Menu, Popular restaurants, Popular searches, User addresses.

**Avoid aggressively caching:** Payment status, Order status, Driver availability — unless cache invalidation / consistency is handled carefully.

---

## 41. CAP Theorem Follow-up

**Interviewer:** Where do you prefer consistency and where availability?

**Strong consistency** for: Payment, Order creation, Money / refund, Order state transitions.

**Eventual consistency** for: Search results, Restaurant ratings, Analytics, Driver location, Recommendation system.

---

## 42. Disaster Recovery

```text
Primary Region
       ↓
Replica / Backup Region
```

Database backups: daily full backup + continuous WAL / binlog.

Kafka replication: RF = 3. If one broker fails, another replica can take over.

---

## 43. Delivery Location Scaling

Suppose we have 1 million active delivery partners and each sends location every 5 seconds.

That's approximately:

```text
1,000,000 / 5 = 200,000 location updates/sec
```

This becomes a separate scaling problem.

We shouldn't write every location update to PostgreSQL.

Instead:

```text
Driver
  ↓
Location Service
  ↓
Kafka / Redis
  ↓
Latest location
```

Only important historical location data is persisted asynchronously.

---

## 44. Nearby Restaurant Problem

**Interviewer:** How will you find restaurants within 5 km?

Use geo indexing. For example Elasticsearch `geo_point`. Query: restaurants within 5 km.

We can also use PostGIS if PostgreSQL is being used heavily for geo queries.

---

## 45. ETA Calculation

Initially:

```text
ETA = restaurant preparation time + driver travel time
```

Driver travel time can come from a maps / routing provider.

At scale we can build our own ETA model using: historical delivery time, traffic, distance, restaurant preparation time, weather, time of day.

---

## 46. Surge / High Demand

During dinner: Orders ↑, Drivers ↓.

We can dynamically increase delivery fee or provide incentives to drivers.

But pricing / business rules should be **isolated** from core Order Service. Create a **Pricing Service**.

---

## 47. Multi-Restaurant Cart

**Interviewer:** Can one cart contain items from multiple restaurants?

I would initially **not** support it.

**Reason:** Food preparation and delivery become much more complicated.

A cart belongs to **one restaurant**.

If user adds item from another restaurant: "Your cart contains items from Restaurant A. Do you want to clear the cart?"

This keeps order processing simpler.

---

## 48. Saga Pattern

Food delivery is a **distributed transaction**.

We cannot have one DB transaction across Order, Payment, Restaurant, Delivery.

Instead, use a saga-like workflow.

```text
Create Order
    ↓
Payment Success
    ↓
Restaurant Accept
    ↓
Delivery Assigned
```

If restaurant rejects:

```text
Restaurant Rejected
       ↓
Cancel Order
       ↓
Refund Payment
       ↓
Notify Customer
```

Each service performs its own transaction and compensating action.

---

## 49. Most Important Interviewer Cross-Questions

These are the questions I would prepare especially well.

### Basic

**Q1. Why microservices?**

Because different components have different scaling and deployment requirements.

**Q2. Why Kafka?**

Asynchronous communication, buffering, decoupling and retries.

**Q3. Why Redis?**

Low-latency temporary / high-frequency data such as carts and driver locations.

**Q4. Why Elasticsearch?**

Fast text, filtering and geo-based restaurant search.

### Order

**Q5. How do you prevent duplicate orders?**

Idempotency key.

**Q6. Payment succeeds but order update fails?**

Payment webhook + reconciliation + idempotent processing.

**Q7. Restaurant rejects after payment?**

Cancel order and initiate idempotent refund.

**Q8. How do you prevent invalid order status changes?**

State machine + allowed transitions.

### Delivery

**Q9. How do you find nearest driver?**

Redis Geo / geo-indexed location service.

**Q10. Two orders select same driver?**

Atomic assignment using Redis lock / SETNX or conditional DB update.

**Q11. How does customer get live location?**

Driver → Location Service → Redis → WebSocket → Customer.

**Q12. Driver location is 10 seconds old?**

Show last known location and timestamp; don't pretend it's real-time.

### Kafka

**Q13. What if consumer crashes?**

Kafka retains the message and consumer can resume from its offset.

**Q14. Duplicate event?**

Idempotent consumer using event ID / order ID.

**Q15. Message ordering?**

Partition by an appropriate key, such as order ID when order-level ordering is required.

**Q16. Kafka event published but DB update fails?**

Consumer retries; operations must be idempotent.

**Q17. DB succeeds but Kafka publish fails?**

Transactional Outbox.

### Database

**Q18. SQL or NoSQL for orders?**

SQL because orders need transactions and consistency.

**Q19. How scale orders database?**

Read replicas → partitioning / sharding when necessary.

**Q20. What should be the shard key?**

Depends on access pattern; user ID is useful for user-centric history, while order ID gives more even distribution. I'd choose after understanding query patterns.

---

## 50. Final Architecture I'd Draw in Interview

If the interviewer gives me only 5–10 minutes to draw, I would draw this:

```text
                         CUSTOMER
                            |
                            ▼
                      API GATEWAY
                            |
       ┌────────────────────┼────────────────────┐
       │                    │                    │
       ▼                    ▼                    ▼
   Restaurant            Search               User
    Service              Service             Service
       │                    │
       ▼                    ▼
 Restaurant DB        Elasticsearch

                            │
                            ▼
                       Cart Service
                            │
                          Redis
                            │
                            ▼
                       Order Service
                            │
                            ▼
                       Order DB
                            │
                            ▼
                          Kafka
             ┌──────────────┼───────────────┐
             │              │               │
             ▼              ▼               ▼
         Payment       Restaurant       Delivery
         Service        Consumer         Service
             │                              │
             ▼                              ▼
       Payment Gateway                 Redis Geo
                                            │
                                            ▼
                                         WebSocket
                                            │
                                            ▼
                                         Customer

                     Kafka
                       │
                       ▼
                Notification Service
                       │
              ┌────────┼────────┐
              ▼        ▼        ▼
            Push      SMS      Email
```

### The key story to remember

If you get stuck during the interview, remember this flow:

> **Search → Cart → Order → Payment → Restaurant → Delivery → Tracking → Notification**

And the technologies:

| Technology | Role |
| --- | --- |
| SQL | Transactions / orders |
| Redis | Cart / location / cache |
| Kafka | Async events |
| Elasticsearch | Search / geo search |
| WebSocket | Live tracking |
| Outbox | DB + Kafka reliability |
| Idempotency | Duplicate prevention |
| State machine | Valid order transitions |
| Saga | Distributed workflow / refunds |

That gives you a strong base for almost every Food Delivery HLD follow-up.
