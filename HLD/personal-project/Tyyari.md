# Tyyari — Personal Project Interview Prep

Tyyari is a gateway-fronted, domain-split SDE interview-preparation platform. This note covers architecture, tradeoffs, and interview answers.

---

## Table of Contents

- [1. First Understand the Project](#1-tyyari--first-understand-the-project)
- [2. 30-Second Interview Explanation](#2-30-second-interview-explanation)
- [3. Overall Architecture](#3-overall-architecture)
- [Data Model — Mongo, Redis, and What Is Not Stored](#data-model--mongo-redis-and-what-is-not-stored)
- [4. Why Microservices?](#4-why-did-you-choose-microservices)
- [5. Why Not a Modular Monolith?](#5-but--interviewer-will-attack-your-microservices-decision)
- [6. API Gateway](#6-api-gateway--understand-it-deeply)
- [7. Why API Gateway?](#7-why-api-gateway)
- [8. What Happens When a Request Comes?](#8-what-happens-when-a-request-comes)
- [9. Why Send X-User-Id?](#9-cross-question-why-send-x-user-id)
- [10. Auth Service](#10-auth-service)
- [11. Authentication Flow](#11-authentication-flow)
- [12. Why Two Tokens?](#12-why-two-tokens)
- [13. Why Hash Refresh Tokens?](#13-why-hash-refresh-tokens)
- [14. Refresh Token Rotation](#14-refresh-token-rotation)
- [15. Why JWT If You Still Need Redis?](#15-cross-question-why-jwt-if-you-still-need-redis)
- [16. Premium JWT Problem](#16-premium-jwt-problem)
- [17. User Service](#17-user-service)
- [18. Content Service](#18-content-service)
- [19. Why One Question Collection?](#19-why-one-question-collection)
- [20. Why MongoDB?](#20-why-mongodb)
- [21. Why Not PostgreSQL?](#21-cross-question-why-not-postgresql)
- [22. Redis](#22-redis--very-important)
- [23. Cache-Aside](#23-explain-cache-aside)
- [24. Why Cache Questions?](#24-why-cache-questions)
- [25. What If Redis Goes Down?](#25-cross-question-what-happens-if-redis-goes-down)
- [26. Why Kafka?](#26-kafka--why-do-we-need-it)
- [27. Synchronous vs Asynchronous](#27-synchronous-vs-asynchronous)
- [28. Account Deletion](#28-account-deletion--excellent-interview-flow)
- [29. What Is DLT?](#29-what-is-dlt)
- [30. Why Kafka Instead of REST?](#30-cross-question-why-kafka-instead-of-rest)
- [31. Mongo Save Succeeds, Kafka Fails](#31-but-now-interviewer-attacks)
- [32. Outbox Pattern](#32-what-is-outbox-pattern)
- [33. Admin Service](#33-admin-service--interesting-architecture)
- [34. Why BFF?](#34-why-bff)
- [35. Is Admin Really a Microservice?](#35-cross-question-is-admin-service-really-a-microservice)
- [36. Code Runner](#36-code-runner--very-important)
- [37. Why Not Execute Code Inside Spring Boot?](#37-why-not-execute-code-inside-spring-boot)
- [38. How Does the Runner Work?](#38-how-does-runner-work)
- [39. Is the Runner Secure?](#39-interviewer-is-your-code-runner-secure)
- [40. Frontend Architecture](#40-frontend-architecture)
- [41. Why Two Frontend Apps?](#41-why-two-frontend-apps)
- [42. Practice Submission Flow](#42-practice-submission-flow)
- [43. Why Calculate Progress?](#43-why-calculate-progress-instead-of-storing-it)
- [44. Isn't Calculating Progress Expensive?](#44-cross-question-isnt-calculating-progress-expensive)
- [45. OA vs Practice](#45-oa-vs-practice)
- [46. Security Architecture](#46-security-architecture)
- [47. What Is IDOR?](#47-what-is-idor)
- [48. Premium Bypass](#48-premium-bypass)
- [49–55. Failure and Scaling](#49-what-happens-if-content-service-goes-down)
- [56. CAP Theorem](#56-cap-theorem-question)
- [57. Most Important Weakness Questions](#57-most-important-weakness-questions)
- [58–63. Deep Cross-Questions](#58-very-deep-cross-questions)
- [64. Biggest Architectural Weaknesses](#64-biggest-architectural-weaknesses--memorize-these)
- [65. What Would You Improve?](#65-what-would-you-improve)
- [66. Interview Deep-Dive Structure](#66-your-interview-deep-dive-structure)
- [67. The 15 Questions to Prepare First](#67-the-15-questions-i-would-prepare-first)
- [68. One-Minute Architecture Answer](#68-one-minute-final-architecture-answer)
- [69. Honest Engineer Answer](#69-your-strongest-honest-engineer-answer)
- [Part 2 — 15 Interview-Ready Answers](#part-2--15-interview-ready-answers)
- [Final Cheat Sheet](#final-your-complete-interview-cheat-sheet)

---

## 1. Tyyari — First Understand the Project

### What problem does Tyyari solve?

Imagine a platform like a combination of:

| Platform | Capability |
| --- | --- |
| LeetCode | DSA |
| InterviewBit | Interview preparation |
| System-design practice | HLD / LLD |
| Frontend playground | Frontend coding |
| Assessment platform | Timed OAs |
| Admin CMS | Managing questions, users and billing |

Tyyari puts these capabilities into **one platform**.

**A candidate can:**

- Register / login
- Browse questions
- Practice DSA / HLD / LLD / frontend / CS
- Write code
- Run code
- Submit solutions
- Track progress
- Practice company-specific questions
- Attempt OAs
- Use premium content

**Admins can:**

- Create questions
- Edit questions
- Publish / unpublish questions
- Manage users
- Manage premium access
- Manage catalog
- Maintain audit logs

**The key architectural decision is:**

> The browser never directly communicates with backend microservices. Everything goes through the API Gateway.

That is one of the most important things you should remember.

---

## 2. 30-Second Interview Explanation

If the interviewer says:

> Tell me about your project.

Say something like:

> I worked on Tyyari, an SDE interview-preparation platform where users can practice DSA, HLD, LLD, frontend coding, CS fundamentals and online assessments.
>
> The system follows a domain-based microservices architecture. We have an API Gateway as the single public entry point, and behind it we have separate Auth, User, Content and Admin services.
>
> Auth handles authentication, authorization-related account information and premium billing, User handles profiles, submissions and progress, Content manages questions and the practice catalog, while Admin acts as a BFF/facade for staff operations.
>
> We use MongoDB for persistence, Redis for caching, rate limiting and session revocation, and Kafka for asynchronous events such as user registration and account deletion.
>
> For code execution, we deliberately keep the runner outside the Java service because user-submitted code is untrusted and needs a separate isolation boundary.
>
> The major tradeoff is that microservices introduce network calls and eventual consistency, but they give us better domain isolation and independent scaling.

This matches the architecture described in your document.

---

## 3. Overall Architecture

Think about the architecture in **five layers**.

```text
                    USERS
                      |
              ----------------
              |              |
         Candidate        Admin
         React App       React App
          :3000            :3001
              \              /
               \            /
                REST + JWT
                     |
                     ▼
              API GATEWAY
                 :8080
                     |
       --------------------------------
       |          |          |        |
       ▼          ▼          ▼        ▼
     AUTH       USER      CONTENT    ADMIN
    :8081      :8082       :8083     :8084
       |          |          |        |
       ▼          ▼          ▼        ▼
    auth_db     user_db   content_db admin_db

                     |
              ----------------
              |       |      |
            Mongo    Redis   Kafka
                              |
                              ▼
                         Async Events

                     |
                Code Runner
                   :2000
```

The architecture uses **Java 21, Spring Boot, Spring Cloud Gateway, MongoDB, Redis, Kafka, React/Vite, Monaco and Excalidraw**.

---

## Data Model — Mongo, Redis, and What Is Not Stored

Each service owns its own Mongo database. Redis is shared. Kafka, JWT, avatars, mail, and code runs are **not** Mongo collections.

### `auth_db` — Auth `:8081`

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `users` | Identity + billing + 2FA | `email` unique, `passwordHash`, `role` (`USER` / `ADMIN` / `EDITOR`), `status` (`ACTIVE` / `DISABLED` / `DELETING`), `emailVerified`, `provider`, `googleSub`, `githubId`, `premium`, `premiumUntil`, `stripeCustomerId`, `totpSecret`, `totpEnabled`, `createdAt`, `updatedAt` |
| `refresh_tokens` | Long-lived login (hashed, 7 days) | `userId`, `tokenHash` unique, `expiresAt`, `device`, `revoked`, `createdAt` |
| `email_verification_tokens` | Verify email | `userId`, `tokenHash`, `expiresAt`, `createdAt` |
| `password_reset_tokens` | Forgot password | `userId`, `tokenHash`, `expiresAt`, `used`, `createdAt` |
| `payments` | Stripe Premium | `userId`, `provider`, `providerRef` unique, `status`, `stripeStatus`, `paymentIntentId`, `refundId`, `refundedAt`, `amount`, `currency`, `createdAt`, `updatedAt` |

### `user_db` — User `:8082`

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `profiles` | Onboarding / public profile | `userId` unique, `name`, `avatar`, `bio`, `githubUrl`, `linkedinUrl`, `experience`, `currentRole`, `targetRole`, `skills[]`, `onboarded` |
| `goals` | Interview targets | `userId` unique, `targetCompanies[]`, `targetRole`, `targetDate`, `dailyGoalMinutes` |
| `preferences` | UI prefs | `userId` unique, `preferredLanguage`, `theme`, `emailNotifications`, `difficultyPreference` |
| `submissions` | Practice + OA answers | `uniqueKey` unique, `userId`, `scope` (`PRACTICE` / `OA`), `questionId`, `questionType`, `assessmentSetId`, `language`, `view`, `files[]`, `canvas`, `math`, `quizScore` / `quizTotal` / `quizAnswers`, `submittedAt` |

`files[]` is **embedded** (`id`, `type`, `name`, `content`) — not a separate collection.

### `content_db` — Content `:8083`

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `questions` | All practice types | `type`, `subType`, `title`, `slug` unique, `description`, `difficulty`, `topics[]`, `companies[]`, `tags[]`, `constraints[]`, `examples[]`, `testcases[]`, `starterFiles[]`, `quiz[]`, `hints[]`, `editorial`, `editorialVideoUrl`, `acceptedCode[]`, `reviewStatus`, `reviewer`, `reviewNote`, `scheduledPublishAt`, `isPublished`, `premium`, `createdBy` |
| `question_sheets` | Curated lists | `slug` unique, `title`, `description`, `type`, `difficulty`, `companies[]`, `questionSlugs[]`, `isPublished` |
| `assessment_sets` | Timed OA | `slug` unique, `title`, `description`, `durationMinutes`, `difficulty`, `companies[]`, `questionSlugs[]`, `isPublished` |
| `roadmaps` | Learn paths | `slug` unique, `role`, `title`, `blurb`, `weeks[]`, `isPublished` |
| `companies` | Catalog | `name`, `slug` unique, `logo`, `active` |
| `topics` | Catalog | `name`, `slug` unique, `category` |
| `tags` | Catalog | `name`, `slug` unique |
| `categories` | Topic groups | `name`, `slug` unique |

**Embedded (not collections):** `examples` (input / output / explanation), `testcases`, `starterFiles`, `quiz` (prompt / options / `answerIndex`), `weeks` → `items` (`kind` / `slug` / `type` / `title`).

### `admin_db` — Admin `:8084`

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `audit_logs` | Staff actions | `actorId`, `action`, `detail`, `createdAt` |

Admin **does not own** users or questions. It reads/writes the others over HTTP (`RestClient`).

### Redis `:6379` — not a table, shared keys

| Key | Writer | TTL / use |
| --- | --- | --- |
| `session:block:{userId}` | Auth (gateway reads) | ~15 min — kill JWT after logout / password change / delete |
| `totp:login:{token}` | Auth | 5 min — 2FA challenge |
| `rate_limit:{id}` | Gateway | 1 min — API rate limit |
| `question:{id}` | Content | ~30 min cache |
| `companies:all`, `topics:*`, `tags:all` | Content | catalog cache |

### What is not a database

| Thing | Where it lives |
| --- | --- |
| Access JWT | Client cookie / memory, not stored |
| Avatars | Disk / volume on user-service |
| Kafka events | Transient (`user-events`, `content-events`, `audit-events`) |
| Mail | Mailpit (dev), not persisted in Mongo |
| Code run | Piston `:2000`, no DB |

---

## 4. Why Did You Choose Microservices?

This is a very likely interview question.

Don't answer: "Because microservices are scalable." That's weak.

Instead explain the actual reasons.

### Reason 1 — Different scaling requirements

Content has:

```text
Millions of question reads
        ↓
Few question writes
```

Authentication has a different traffic pattern.

Code execution is completely different:

- CPU intensive
- Memory intensive
- Untrusted code
- Long-running operations

So these components shouldn't necessarily scale together.

The document explicitly identifies catalog reads, authentication traffic and runner CPU/isolation as different scaling requirements.

### Reason 2 — Failure isolation

Suppose the Code Runner crashes.

You don't want login, profile, and questions to fail too.

Instead:

| Component | Status |
| --- | --- |
| Runner | ❌ |
| Login | ✅ |
| Profile | ✅ |
| Questions | ✅ |

This is one of your strongest arguments for separating the runner.

### Reason 3 — Data isolation

Authentication contains sensitive information:

- Password hashes
- Refresh tokens
- TOTP secrets

while content contains:

- Questions
- Topics
- Companies
- Tags

Putting everything into one service increases the blast radius of bugs or unauthorized access.

---

## 5. BUT — Interviewer Will Attack Your Microservices Decision

**Question:** Why didn't you build a modular monolith?

This is actually a very important question because your architecture document openly admits that a modular monolith could be better for an early product.

**Answer:**

> For a small team and early-stage product, I would actually prefer a modular monolith because microservices introduce operational complexity, network failures and eventual consistency. We chose the microservice structure primarily for domain isolation, independent scaling and learning, and especially to isolate the untrusted code runner. If I were building this from scratch with a two-person team, I'd probably start with a modular monolith and extract the runner and possibly authentication first.

That's a much stronger answer than blindly defending microservices.

---

## 6. API Gateway — Understand It Deeply

The gateway is:

```text
Client
   ↓
API Gateway
   ↓
Microservices
```

The client doesn't directly call `auth:8081`, `user:8082`, or `content:8083`.

Instead:

```text
Client
   ↓
:8080
   ↓
appropriate service
```

The gateway handles:

- JWT validation
- CORS
- Rate limiting
- Correlation ID
- Routing
- Coarse authorization
- User identity headers

The project specifically uses **Spring Cloud Gateway**.

---

## 7. Why API Gateway?

Interviewer: Why not let frontend directly call each microservice?

**Answer:** Because then the frontend needs to know every service URL. That creates tight coupling.

```text
Frontend
    |
    ▼
gateway
    |
    ├── auth
    ├── user
    ├── content
    └── admin
```

Now backend topology can change without changing the frontend.

---

## 8. What Happens When a Request Comes?

Suppose user asks:

```http
GET /api/v1/questions/two-sum
Authorization: Bearer <JWT>
```

| Step | What happens |
| --- | --- |
| 1 | Request reaches API Gateway `:8080` |
| 2 | Gateway extracts JWT and verifies signature, expiry, issuer |
| 3 | Gateway extracts claims: `sub`, `role`, `premium` |
| 4 | Gateway injects `X-User-Id`, `X-User-Role`, `X-User-Premium` |
| 5 | Request is routed to Content Service |
| 6 | Content service checks whether question is premium |

If locked:

- Premium user → full DTO
- Free user → restricted DTO

The important point is that premium authorization isn't trusted from the frontend. The **server strips locked fields**.

---

## 9. Cross-Question: Why Send X-User-Id?

Because downstream services need to know which user is making this request.

Instead of every service decoding the JWT again, the gateway extracts the identity and forwards it.

For example:

```text
JWT

sub = 12345
role = USER
premium = true
```

Gateway converts that into:

```text
X-User-Id: 12345
X-User-Role: USER
X-User-Premium: true
```

### Important security issue

Interviewer: Can't attacker send `X-User-Id` manually?

Excellent question.

**Answer:**

> The gateway overwrites these headers after validating the JWT. Services should never trust client-supplied X-User-* headers from the public internet.

That exact security consideration is documented.

---

## 10. Auth Service

Auth service owns:

- Registration
- Login
- Email verification
- Password reset
- Google OAuth
- GitHub OAuth
- TOTP 2FA
- Refresh tokens
- Stripe checkout
- Premium entitlement
- Account deletion events

The project deliberately puts billing here because premium is considered an **account entitlement**.

### `auth_db` collections

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `users` | Identity + billing + 2FA | `email` unique, `passwordHash`, `role` (`USER` / `ADMIN` / `EDITOR`), `status` (`ACTIVE` / `DISABLED` / `DELETING`), `emailVerified`, `provider`, `googleSub`, `githubId`, `premium`, `premiumUntil`, `stripeCustomerId`, `totpSecret`, `totpEnabled`, `createdAt`, `updatedAt` |
| `refresh_tokens` | Long-lived login (hashed, 7 days) | `userId`, `tokenHash` unique, `expiresAt`, `device`, `revoked`, `createdAt` |
| `email_verification_tokens` | Verify email | `userId`, `tokenHash`, `expiresAt`, `createdAt` |
| `password_reset_tokens` | Forgot password | `userId`, `tokenHash`, `expiresAt`, `used`, `createdAt` |
| `payments` | Stripe Premium | `userId`, `provider`, `providerRef` unique, `status`, `stripeStatus`, `paymentIntentId`, `refundId`, `refundedAt`, `amount`, `currency`, `createdAt`, `updatedAt` |

---

## 11. Authentication Flow

User enters email and password.

Frontend: `POST /login`

Gateway: validate JWT? There isn't a JWT yet, so request reaches Auth service.

Auth service:

```text
find user
   ↓
verify password hash
   ↓
check account status
   ↓
check 2FA
   ↓
generate access token
   ↓
generate refresh token
```

| Token | Lifetime |
| --- | --- |
| Access token | ~15 minutes |
| Refresh token | ~7 days |

The architecture uses **JWT access tokens** and **hashed opaque refresh tokens**.

---

## 12. Why Two Tokens?

This is a classic interview question.

**Access token** — short-lived (15 min), used frequently:

- GET question
- POST submission
- GET profile

**Refresh token** — longer-lived (7 days), used to get another access token.

**Why?** Suppose the access token gets stolen.

If lifetime is 15 minutes, the damage window is limited.

If the access token lived for 7 days, the attacker could potentially use it for a much longer time.

---

## 13. Why Hash Refresh Tokens?

Interviewer: Why don't you store refresh tokens directly?

Suppose the database contains `refreshToken = ABC123`.

If the DB leaks: attacker → `ABC123` → authenticate.

Instead:

```text
refreshToken
      ↓
    hash
      ↓
stored in DB
```

If the database leaks, the attacker doesn't immediately obtain the usable refresh token.

The architecture explicitly stores refresh hashes and supports rotation.

---

## 14. Refresh Token Rotation

Suppose Refresh Token A is used.

Server generates Refresh Token B and invalidates A.

Now attacker tries to use A again. That's a **refresh-token reuse signal**.

The project responds by **deleting all refresh tokens** for that user.

---

## 15. Cross-Question: Why JWT If You Still Need Redis?

This is tricky.

JWT gives you **stateless authentication**.

But you still need a way to immediately revoke sessions.

For example, user clicks "Sign out everywhere."

Existing JWTs could remain valid for up to 15 minutes.

Redis stores `session:block:{userId}`. Gateway checks this and blocks the session.

So:

```text
JWT
+
Redis revocation
```

gives you mostly stateless authentication while still allowing emergency / session-wide revocation.

---

## 16. Premium JWT Problem

Interviewer: Admin upgrades a user to Premium. What happens?

Database immediately says `premium = true`.

But existing JWT might contain `premium = false`.

```text
Old JWT
    ↓
premium=false
```

until it expires / is refreshed.

The project explicitly recognizes this as a tradeoff.

**Better production solutions:**

- Make access tokens shorter
- Force refresh after entitlement change
- Store entitlement separately
- Maintain entitlement version
- Use centralized authorization service

But each has tradeoffs.

---

## 17. User Service

User service represents **"the candidate as a person."**

It owns:

- Profile
- Avatar
- Goals
- Preferences
- Submissions
- Progress
- Streak
- Last question
- Question counts

This separation is important:

| Service | Question it answers |
| --- | --- |
| **Auth** | Can this person authenticate? |
| **User** | What has this person done? |

The project documentation explicitly makes this distinction.

### `user_db` collections

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `profiles` | Onboarding / public profile | `userId` unique, `name`, `avatar`, `bio`, `githubUrl`, `linkedinUrl`, `experience`, `currentRole`, `targetRole`, `skills[]`, `onboarded` |
| `goals` | Interview targets | `userId` unique, `targetCompanies[]`, `targetRole`, `targetDate`, `dailyGoalMinutes` |
| `preferences` | UI prefs | `userId` unique, `preferredLanguage`, `theme`, `emailNotifications`, `difficultyPreference` |
| `submissions` | Practice + OA answers | `uniqueKey` unique, `userId`, `scope` (`PRACTICE` / `OA`), `questionId`, `questionType`, `assessmentSetId`, `language`, `view`, `files[]`, `canvas`, `math`, `quizScore` / `quizTotal` / `quizAnswers`, `submittedAt` |

`files[]` is **embedded** (`id`, `type`, `name`, `content`) — not a separate collection.

---

## 18. Content Service

This is basically your **catalog engine**.

It owns:

- Questions
- Companies
- Topics
- Tags
- Categories
- Sheets
- OA sets
- Roadmaps

One interesting design choice: `questions` is a **single collection**.

Instead of `dsa_questions`, `hld_questions`, `lld_questions`, etc., you have:

```text
questions

with type:
  DSA
  HLD
  LLD
  CS
  FRONTEND
  OA
```

This is called a **discriminator document model**.

### `content_db` collections

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `questions` | All practice types | `type`, `subType`, `title`, `slug` unique, `description`, `difficulty`, `topics[]`, `companies[]`, `tags[]`, `constraints[]`, `examples[]`, `testcases[]`, `starterFiles[]`, `quiz[]`, `hints[]`, `editorial`, `editorialVideoUrl`, `acceptedCode[]`, `reviewStatus`, `reviewer`, `reviewNote`, `scheduledPublishAt`, `isPublished`, `premium`, `createdBy` |
| `question_sheets` | Curated lists | `slug` unique, `title`, `description`, `type`, `difficulty`, `companies[]`, `questionSlugs[]`, `isPublished` |
| `assessment_sets` | Timed OA | `slug` unique, `title`, `description`, `durationMinutes`, `difficulty`, `companies[]`, `questionSlugs[]`, `isPublished` |
| `roadmaps` | Learn paths | `slug` unique, `role`, `title`, `blurb`, `weeks[]`, `isPublished` |
| `companies` | Catalog | `name`, `slug` unique, `logo`, `active` |
| `topics` | Catalog | `name`, `slug` unique, `category` |
| `tags` | Catalog | `name`, `slug` unique |
| `categories` | Topic groups | `name`, `slug` unique |

**Embedded (not collections):** `examples` (input / output / explanation), `testcases`, `starterFiles`, `quiz` (prompt / options / `answerIndex`), `weeks` → `items` (`kind` / `slug` / `type` / `title`).

---

## 19. Why One Question Collection?

Imagine admin wants "Show all questions."

With separate collections you'd have to combine DSA + HLD + LLD + CS + Frontend + OA.

With one collection: `questions.find(...)` — easy pagination and filtering.

**Disadvantage:** different question types have different fields.

| Type | Fields |
| --- | --- |
| DSA | code, testcases, constraints |
| HLD | requirements, architecture |

Therefore the document becomes **sparse**. Validation has to happen in service logic.

The document explicitly identifies this tradeoff.

---

## 20. Why MongoDB?

This is another likely cross-question.

**Your answer:**

> Questions contain nested structures such as examples, files, quiz data and weekly roadmap structures. Mongo's document model lets us represent these naturally without creating a large number of relational tables and joins.

Don't say: "MongoDB is faster than SQL." That's too generic.

The architecture itself says Mongo was selected because questions are nested, while acknowledging weaker analytics compared with relational databases.

---

## 21. Cross-Question: Why Not PostgreSQL?

**Good answer:**

Postgres would be attractive if we needed:

- Complex joins
- Strong relational constraints
- Financial transactions
- Complex analytics

Mongo works well here because:

```text
Question
 ├── examples
 ├── constraints
 ├── test cases
 ├── hints
 └── metadata
```

can naturally live together.

However, for billing-heavy systems or sophisticated analytics, I'd consider PostgreSQL.

---

## 22. Redis — Very Important

Your Redis is **not** your primary database.

It is used for:

| Key | Writer | TTL / use |
| --- | --- | --- |
| `session:block:{userId}` | Auth (gateway reads) | ~15 min — kill JWT after logout / password change / delete |
| `totp:login:{token}` | Auth | 5 min — 2FA challenge |
| `rate_limit:{id}` | Gateway | 1 min — API rate limit |
| `question:{id}` | Content | ~30 min cache |
| `companies:all`, `topics:*`, `tags:all` | Content | catalog cache |

The architecture explicitly lists these uses. Redis is **cache-aside**, not the source of truth.

---

## 23. Explain Cache-Aside

Suppose user requests `GET /questions/two-sum`.

Content service:

```text
Check Redis
    |
    ├── HIT → return
    |
    └── MISS
          ↓
       MongoDB
          ↓
       Redis SET
          ↓
       return
```

This is the **cache-aside pattern**.

---

## 24. Why Cache Questions?

Suppose 10,000 users request Two Sum.

| Without cache | With cache |
| --- | --- |
| 10,000 Mongo reads | 1 Mongo read + 9,999 Redis reads |

This reduces database load and improves latency.

---

## 25. Cross-Question: What Happens If Redis Goes Down?

Very important.

For question caching:

```text
Redis ❌
     ↓
MongoDB
```

The system can still retrieve questions, although slower.

But for rate limiting and session blocking, behavior needs to be carefully defined.

For a production system, I'd decide whether failure should be **fail-open** or **fail-closed** depending on the operation.

For security-sensitive session revocation, fail-open has security implications.

---

## 26. Kafka — Why Do We Need It?

Don't say: "Kafka is faster."

Instead:

> We use Kafka where the producer shouldn't have to wait for downstream consumers.

Example: User registered.

Auth service doesn't need to synchronously call User service.

```text
Auth
 |
 | USER_REGISTERED
 ↓
Kafka
 |
 ↓
User service
```

User service creates the profile asynchronously.

---

## 27. Synchronous vs Asynchronous

### Synchronous — GET question

Client needs the answer now.

```text
Browser
 ↓
Gateway
 ↓
Content
 ↓
Mongo
 ↓
Response
```

### Asynchronous — USER_REGISTERED

Doesn't necessarily need immediate completion.

```text
Auth
 ↓
Kafka
 ↓
User
```

The architecture uses **synchronous HTTP** for immediate CRUD-style operations and **Kafka** for fan-out / retry-oriented events.

---

## 28. Account Deletion — Excellent Interview Flow

This is one of the best flows to explain.

Suppose admin deletes a user.

| Step | Action |
| --- | --- |
| 1 | Admin locks user: `status = DELETING` |
| 2 | Session is blocked: `session:block:{userId}` |
| 3 | Publish `USER_DELETE_REQUESTED` |
| 4 | Kafka distributes the event |
| 5 | Failures are retried |
| 6 | Unprocessable messages go to DLT |

```text
                 Kafka
                   |
          ------------------
          |                |
        Auth              User
          |                |
     delete auth       delete profile
```

This is called **choreography**, rather than centralized orchestration.

---

## 29. What Is DLT?

**DLT = Dead Letter Topic.**

Suppose `USER_DELETE_REQUESTED` is consumed by User Service.

Processing fails: attempt 1 ❌, attempt 2 ❌, attempt 3 ❌.

Instead of endlessly retrying:

```text
Kafka
 ↓
DLT
```

Now engineers can investigate / reprocess it.

---

## 30. Cross-Question: Why Kafka Instead of REST?

Interviewer: Why not Auth directly call User Service?

Possible architecture:

```text
Auth
 |
 | POST /profile
 ↓
User
```

**Problem:** If User service is down, registration fails.

With Kafka: Auth → Kafka. Auth can finish its own transaction while User service processes later.

| REST | Kafka |
| --- | --- |
| Immediate consistency | Eventual consistency |

---

## 31. But Now Interviewer Attacks

> What if Mongo save succeeds but Kafka publish fails?

This is one of the most important weaknesses in your project.

Current implementation:

```text
Mongo save
   ↓
Kafka publish
```

Possible failure:

```text
Mongo save ✅
Kafka ❌
```

Now the database says user created, but the event was never published.

The architecture explicitly acknowledges this and says a production implementation should use a **transactional outbox** or Mongo change streams.

**Interview answer:**

> The current implementation publishes after the Mongo write, so there is a crash window where the database commit can succeed but Kafka publishing can fail. That's a known weakness. In production I'd use an outbox pattern so the business write and event record are committed together, and a publisher reliably forwards the event to Kafka.

Excellent answer.

---

## 32. What Is Outbox Pattern?

Instead of Mongo → Kafka, do:

```text
Mongo transaction
      |
      ├── user document
      |
      └── outbox event
```

Both are persisted.

Then:

```text
Outbox publisher
      ↓
Kafka
```

If Kafka is down, the outbox event remains and the publisher retries later.

---

## 33. Admin Service — Interesting Architecture

Admin service is not another major business domain.

It acts as a **BFF / Backend-for-Frontend**.

```text
Admin UI
   ↓
Admin Service
   |
   ├── Auth internal API
   ├── User internal API
   └── Content internal API
```

Admin service aggregates staff functionality.

The architecture specifically says admin has mainly audit logs of its own and calls other services' internal APIs.

### `admin_db` collections

| Collection | Purpose | Key fields |
| --- | --- | --- |
| `audit_logs` | Staff actions | `actorId`, `action`, `detail`, `createdAt` |

Admin **does not own** users or questions. It reads/writes the others over HTTP (`RestClient`).

---

## 34. Why BFF?

Without BFF, Admin UI talks to Auth, User, Content, etc. Frontend knows internal service topology.

With BFF:

```text
Admin UI
   ↓
Admin BFF
   ↓
internal services
```

The frontend has **one staff API**.

---

## 35. Cross-Question: Is Admin Service Really a Microservice?

Great question.

**Answer:**

> It is a separately deployed service, but conceptually it is a BFF/facade rather than an independent core business domain. It owns audit logs and aggregates staff operations.

This demonstrates that you understand architecture rather than simply repeating "four microservices."

---

## 36. Code Runner — Very Important

This is probably one of the strongest system-design portions of your project.

You have **Code Runner :2000**.

Java services do **not** execute user code.

Instead:

```text
Browser
   ↓
Vite proxy
   ↓
Code Runner
```

The architecture deliberately separates the runner because user code is **untrusted**.

---

## 37. Why Not Execute Code Inside Spring Boot?

Imagine malicious code: `while(true) {}` or code consuming huge memory.

If executed inside your backend:

```text
Spring Boot
   |
   └── user code
```

then malicious code can affect CPU, memory, application threads, and server availability.

Instead:

```text
Spring Boot
       |
       X
       
Browser → Runner
```

Runner is a **separate trust boundary**.

---

## 38. How Does Runner Work?

Conceptually:

```text
User code
   ↓
temporary directory
   ↓
compiler/interpreter
   ↓
execute
   ↓
timeout
   ↓
capture output
   ↓
return result
```

Current implementation has: temporary directory, spawn, timeout, file-size caps.

But the document explicitly states this is **not production-grade isolation**.

---

## 39. Interviewer: Is Your Code Runner Secure?

Do **NOT** say "Yes."

Say:

> It's isolated enough for a lab environment, but I wouldn't call it production-safe against a determined attacker. The current implementation isn't using gVisor, Firecracker or Kata Containers. For production I'd use stronger sandboxing, disable networking, apply CPU and memory cgroups, enforce process limits and run workers with minimal privileges.

That's a very strong answer because you're honestly acknowledging the limitation.

---

## 40. Frontend Architecture

There are two React/Vite applications:

| App | Contents |
| --- | --- |
| `frontend/web` (candidate) | Monaco, Excalidraw, XYFlow |
| `frontend/admin` | CMS, user management, billing |

| State | Library |
| --- | --- |
| Server state | TanStack Query |
| Client state | Zustand |

Editors are **code-split** so users don't download the huge Monaco bundle on the landing page.

---

## 41. Why Two Frontend Apps?

Interviewer: Why not one React application?

**Answer:** Two apps provide Candidate UX ≠ Staff UX.

**Benefits:**

- Separate bundles
- Separate routes
- Reduced accidental privilege exposure
- Independent deployment

**Tradeoff:**

- Duplicate authentication
- Duplicate components
- Duplicate runner integration

A future refactor could extract shared packages.

---

## 42. Practice Submission Flow

Suppose user solves Two Sum.

Frontend sends: `PUT /users/me/submissions`

Gateway: JWT validation

User service: save submission

Progress is **not** stored as a separate percentage.

Instead:

```text
completed questions
        ∩
sheet questions
```

determines sheet completion.

This is a very important design choice.

---

## 43. Why Calculate Progress Instead of Storing It?

Suppose you store `sheetCompletion = 70%`.

Now user solves another question. You have to update submission + progress percentage.

Potential inconsistency: submission = 20 questions, completion = 65%.

Instead, **submissions are the source of truth**.

```text
Submissions
     ↓
calculate progress
```

This avoids maintaining duplicate state.

---

## 44. Cross-Question: Isn't Calculating Progress Expensive?

Yes.

Suppose 1 million submissions, and the dashboard requests progress every time. You may need expensive reads.

Then I'd introduce **materialized progress** or an **event-driven progress projection**:

```text
SubmissionCreated
       ↓
Kafka
       ↓
Progress Consumer
       ↓
Progress DB
```

So current architecture prioritizes correctness / single source of truth, while at larger scale you could introduce a read model.

---

## 45. OA vs Practice

This is a very likely question.

### Practice

```text
Question
 ↓
submit
 ↓
progress
 ↓
sheet completion
```

### OA

```text
Assessment
 ↓
timer
 ↓
question
 ↓
submission
 ↓
assessment score
```

The project deliberately uses a different unique key: **user × set × question** for OA submissions so OA completion doesn't incorrectly mark practice-sheet progress.

---

## 46. Security Architecture

You should know these threats.

| Threat | Current defense |
| --- | --- |
| Stolen JWT | Short TTL |
| Stolen refresh token | Hash + rotation |
| CSRF | Bearer tokens |
| Premium bypass | Server-side DTO stripping |
| IDOR | Published filtering |
| Code injection | Runner isolation |
| Rate abuse | Redis |
| Fake X-User headers | Gateway overwrites |

These are explicitly identified in the architecture document.

---

## 47. What Is IDOR?

Interviewer: What if I change question ID?

For example: `GET /questions/123` becomes `GET /questions/124`.

Server must ensure that the requested resource is actually accessible.

For unpublished questions, `published = false` should not be exposed through the public API.

---

## 48. Premium Bypass

Never do this and assume security is solved:

```javascript
if (user.isPremium) {
   showEditorial();
}
```

That's only UI logic.

A malicious user could call `GET /questions/123` directly.

Therefore backend must enforce:

```text
free user
   ↓
remove locked fields
```

The project explicitly uses **server-side DTO stripping**.

---

## 49. What Happens If Content Service Goes Down?

This is a fantastic failure question.

Current behavior: Content ❌ means Practice ❌.

But Auth ✅ and Login ✅, because authentication is isolated.

That demonstrates why **domain separation** exists.

---

## 50. What If Kafka Goes Down?

For synchronous operations (`GET question`), things can still work.

For events (`USER_DELETE_REQUESTED`), they can't be immediately delivered.

The system can retry and eventually use DLT.

However, there can be temporary inconsistency:

```text
Auth: DELETING
User: still exists
```

This is acceptable as **eventual consistency** if carefully handled.

---

## 51. What If Mongo Goes Down?

This is more serious. Mongo is the primary persistence layer.

So Mongo ❌ means many write/read operations fail.

For production, we'd use a Mongo replica set and potentially Mongo Atlas with backups, monitoring and failover.

The current Compose setup is **intentionally not high-availability** infrastructure.

---

## 52. Current Architecture vs Production Architecture

This distinction will make you sound experienced.

### Current / local

- 1 Mongo process
- 1 Redis
- 1 Kafka broker
- 1 runner

### Production

```text
                    Load Balancer
                         |
              ---------------------
              |                   |
          Gateway 1           Gateway 2
              |                   |
        -------------------------------
        |        |        |           |
      Auth     User    Content      Admin
       |         |        |           |
       ▼         ▼        ▼           ▼
    Mongo     Mongo    Mongo       Mongo
       |
     Redis Cluster
       |
   Kafka Cluster
   3+ brokers
       |
   Runner Queue
       |
  ----------------
  |      |       |
Worker Worker Worker
```

The architecture document itself gives a 10× scaling plan involving gateway replicas, Redis Cluster, Kafka replication, Mongo scaling/sharding and multiple runner workers.

---

## 53. How Would You Scale Code Execution?

**Current:** Browser → Runner

**At scale:**

```text
Browser
 ↓
API
 ↓
Execution Queue
 ↓
-----------------------
|         |           |
Worker1  Worker2    Worker3
```

**Why queue?** Because execution can be CPU intensive, slow, and bursty.

If 1 million users submit code simultaneously, you don't want unlimited processes spawning.

Instead: **queue + worker pool + per-user quota**.

---

## 54. How Would You Scale Content Service?

**Current:** Mongo + Redis

**At larger scale:**

```text
Redis
   ↓
Mongo indexes
   ↓
OpenSearch
```

The architecture specifically proposes subscribing to content-events and indexing into OpenSearch if search grows.

---

## 55. Why Not Elasticsearch From Day One?

Because complexity.

If current filtering is type, difficulty, company, topic — Mongo indexes may be enough.

Introducing Elasticsearch means Mongo + Kafka + Elasticsearch. Now you have another distributed system to operate.

**Better:** Start simple and introduce search infrastructure when query complexity/latency justifies it.

---

## 56. CAP Theorem Question

Interviewer: Is Tyyari CP or AP?

Don't say: "Tyyari is CP."

The architecture explicitly says it isn't globally one or the other.

**Better:**

> Different domains have different consistency priorities. Authentication is consistency-oriented because incorrect identity or entitlement information is dangerous, while the content catalog is more availability-oriented because cached catalog data can tolerate some staleness.

---

## 57. Most Important Weakness Questions

These are the questions I would expect an interviewer to ask after reading this architecture.

| Q | Answer |
| --- | --- |
| **Q1. Why microservices?** | Domain isolation, different scaling requirements, failure isolation, independent ownership. But modular monolith would be simpler for a small team. |
| **Q2. Why Mongo?** | Nested question documents and flexible schemas fit document storage. |
| **Q3. Why Redis?** | Cache-aside for read-heavy content, rate limiting and session revocation. |
| **Q4. Why Kafka?** | Asynchronous fan-out and eventual processing for registration/deletion/content events. |
| **Q5. Why Gateway?** | Single public entry point, centralized routing, JWT validation, CORS and rate limiting. |
| **Q6. Why JWT?** | Stateless short-lived access authentication. |
| **Q7. Why refresh token?** | Avoid requiring users to login every 15 minutes. |
| **Q8. Why hash refresh tokens?** | Reduce impact of database compromise. |
| **Q9. What if Kafka fails?** | Event processing is delayed/retried; DLT handles messages that repeatedly fail. |
| **Q10. What if DB write succeeds but Kafka fails?** | Current implementation has a crash window. Production improvement: transactional outbox/change streams. |
| **Q11. What if Redis fails?** | Cache misses fall back to Mongo, but rate limiting/session-revocation semantics require explicit fail-open/fail-closed decisions. |
| **Q12. What if Content Service fails?** | Practice functionality fails, but authentication remains available due to service isolation. |
| **Q13. Why separate code runner?** | Untrusted code should not execute inside the API JVM. |
| **Q14. Is your runner production secure?** | No. Current isolation is suitable for a lab but needs stronger sandboxing such as gVisor/Firecracker, no network, resource limits and stronger process isolation. |
| **Q15. Why two frontend applications?** | Separate candidate/staff UX and bundles, at the cost of duplicated code. |

---

## 58. Very Deep Cross-Questions

**Q: What happens if two users submit the same question simultaneously?**

Mongo needs an appropriate unique/indexing strategy depending on submission semantics.

For OA, the project specifically uses **user × set × question** as a unique key.

For practice, multiple submissions may be meaningful, so you wouldn't necessarily make `user × question` unique.

---

## 59. What Happens If User Clicks Submit Twice?

Potential: Request A and Request B arrive simultaneously.

You need to determine whether submissions are:

| Style | Meaning |
| --- | --- |
| **Idempotent** | Same request → same result |
| **Append-only** | Every submission is stored |

For OA, uniqueness constraints can protect against duplicate logical submissions.

For production APIs, I'd also use an **Idempotency-Key** for operations where duplicate execution is harmful.

---

## 60. What Happens If Admin Publishes a Question But Cache Contains Old Data?

Current strategy:

```text
Admin publish
     ↓
Mongo update
     ↓
Redis eviction
```

If eviction fails, the old value could remain until TTL expires.

The architecture uses cache eviction plus TTL, and explicitly accepts the possibility of **stale catalog data**.

---

## 61. Why Not Make Redis the Source of Truth?

Because Redis is a cache.

If Redis crashes, we shouldn't lose questions, users, submissions.

Therefore:

```text
Mongo = source of truth
Redis = performance layer
```

This is a phrase worth memorizing:

> Redis is cache-aside, not cache-as-source-of-truth.

---

## 62. What Is Correlation ID?

Suppose request `GET question` goes through Gateway → Content → Mongo.

Logs from three systems need to be connected.

Gateway creates `correlationId = abc-123`.

Then logs contain:

```text
Gateway  abc-123
Content  abc-123
Mongo    abc-123
```

Now debugging becomes much easier.

The gateway generates a correlation ID for every request.

---

## 63. Why Internal APIs?

Content has public APIs and `/internal/v1/**`. Admin uses internal APIs.

**Why?** Suppose admin wants to publish an unpublished question. The normal candidate endpoint should never allow this.

```text
Candidate
 ↓
Gateway
 ↓
Public Content API

Admin
 ↓
Gateway
 ↓
Admin
 ↓
Internal Content API
```

Internal APIs must **not** be exposed through the public gateway.

---

## 64. Biggest Architectural Weaknesses — Memorize These

Your architecture document itself identifies these.

| # | Weakness | Detail |
| --- | --- | --- |
| 1 | Infrastructure isn't truly distributed | 1 Mongo, 1 Kafka, 1 Redis — logically separated but not HA |
| 2 | No Outbox | Potential event loss |
| 3 | JWT premium staleness | Premium changes aren't immediately reflected |
| 4 | Weak runner isolation | Not production-grade |
| 5 | Runner execute API isn't authenticated | Potential abuse problem |
| 6 | Admin audit isn't transactional | Content can succeed while audit fails |
| 7 | Content events have limited consumers | Future-oriented event architecture |
| 8 | Gateway routes grow | Every new public resource requires route configuration |
| 9 | User IDs are plain strings | No shared ID abstraction |

These weaknesses are not things you should hide. If the interviewer finds them, **admitting them and proposing improvements is much stronger**.

---

## 65. What Would You Improve?

This is probably the most valuable final interview question.

Say:

> My first improvements would be strengthening the code execution sandbox, introducing an outbox for reliable Kafka publishing, moving billing into its own service if payment complexity grows, adding circuit breakers around the Admin BFF, and moving refresh tokens to more secure httpOnly cookies in production.

The architecture already identifies several of these as future improvements/tradeoffs.

---

## 66. Your Interview Deep-Dive Structure

When interviewer asks "Explain the architecture," don't randomly jump between technologies.

Follow this order:

```text
1. Problem
      ↓
2. High-level architecture
      ↓
3. Gateway
      ↓
4. Auth
      ↓
5. User
      ↓
6. Content
      ↓
7. Admin BFF
      ↓
8. Redis
      ↓
9. Kafka
      ↓
10. Code runner
      ↓
11. Database
      ↓
12. Security
      ↓
13. Failure handling
      ↓
14. Scaling
      ↓
15. Tradeoffs
```

This gives the interviewer a clear mental model.

---

## 67. The 15 Questions I Would Prepare First

If you have limited preparation time, master these:

1. Explain Tyyari architecture.
2. Why microservices instead of monolith?
3. Why MongoDB instead of PostgreSQL?
4. Why Redis and exactly where is it used?
5. Explain cache-aside.
6. Why Kafka?
7. Explain USER_REGISTERED flow.
8. Explain account deletion using Kafka.
9. What happens if Kafka fails?
10. What is the Outbox Pattern and why do you need it?
11. Explain JWT + refresh-token architecture.
12. Why is the code runner separated?
13. Is the code runner production secure?
14. How would you scale Tyyari to 10× traffic?
15. What are the weaknesses of your architecture?

If you can answer these naturally, you can handle a large portion of the architecture discussion.

---

## 68. One-Minute Final Architecture Answer

Memorize the structure, not every word:

> Tyyari is an SDE interview-preparation platform supporting DSA, HLD, LLD, frontend coding, CS fundamentals and online assessments.
>
> At a high level, we have two React applications, one for candidates and one for administrators. Both communicate through a Spring Cloud API Gateway, which is the only public backend entry point. The gateway validates JWTs, handles CORS and rate limiting, adds the authenticated user's identity and routes requests to the appropriate service.
>
> We have four main backend services. Auth owns authentication, OAuth, 2FA, refresh tokens and premium billing. User owns profiles, submissions and progress. Content owns the question catalog, companies, topics, sheets, OAs and roadmaps. Admin is a BFF/facade that aggregates staff operations and maintains audit logs.
>
> MongoDB is used as the primary datastore with logical databases per service. Redis is used for question/catalog caching, rate limiting and session revocation. Kafka handles asynchronous events such as user registration, account deletion and content mutations.
>
> For code execution, we keep the runner outside the Java service because user-submitted code is untrusted. The current runner uses basic process and timeout isolation, but production would require stronger sandboxing.
>
> The main tradeoff is that microservices introduce network failures and eventual consistency, but they provide domain isolation and allow components such as the code runner and content service to scale independently.

---

## 69. Your Strongest "Honest Engineer" Answer

If the interviewer pushes very hard, this is the mindset you want:

1. "This is what the current system does."
2. "This is why we chose it."
3. "Here is the tradeoff."
4. "Here is what I would change at production scale."

For example:

Interviewer: Why don't you have an Outbox?

**Weak:** "We didn't need it."

**Strong:**

> The current implementation publishes after Mongo persistence, so there is a potential event-loss window. For the current scale it was acceptable, but I recognize it isn't reliable enough for production. I would introduce an outbox so the database state and event record are committed atomically.

That style is much more convincing.

Your uploaded architecture document itself emphasizes this "honest tradeoff" approach and explicitly recommends a modular monolith for a small early-stage team, while identifying the missing outbox, JWT staleness and runner isolation as weaknesses.

### The single line to remember

> Tyyari is a gateway-fronted, domain-split interview platform: Auth handles identity and billing, User handles progress, Content handles the catalog, Admin acts as a staff BFF, Redis handles fast ephemeral concerns, Kafka handles asynchronous domain events, MongoDB is the source of truth, and the untrusted code runner is isolated from the Java backend.

If you can explain every word in that sentence, you're in a strong position for the project/system-design portion of an SDE interview.

---

# Part 2 — 15 Interview-Ready Answers

Below are interview-ready answers to all 15 questions, based primarily on your Tyyari architecture document. Format: **what to say → how it works technically → likely cross-questions**.

---

## 1. Explain Tyyari Architecture

**Interview answer:**

> Tyyari is an SDE interview-preparation platform where users can practice DSA, HLD, LLD, frontend coding, CS fundamentals and online assessments.
>
> We have two React applications — one for candidates and one for administrators. Both communicate with the backend through an API Gateway, so the microservices are not directly exposed to the browser.
>
> Behind the gateway, we have four main services: Auth Service, User Service, Content Service and Admin Service.
>
> Auth Service handles authentication, OAuth, 2FA, refresh tokens and premium billing. User Service manages profiles, submissions and progress. Content Service manages questions, companies, topics, sheets, OAs and roadmaps. Admin Service acts as a BFF/facade for staff operations and maintains audit logs.
>
> MongoDB is used as the primary database, Redis is used for caching, rate limiting and session revocation, and Kafka is used for asynchronous events such as user registration and account deletion.
>
> Code execution is handled by a separate runner because user-submitted code is untrusted and shouldn't execute inside the Java backend.

```text
                  Candidate / Admin
                         |
                     React App
                         |
                      REST/JWT
                         |
                         ▼
                  API Gateway :8080
                         |
       ┌─────────────────┼─────────────────┐
       ↓                 ↓                 ↓
     Auth              User             Content
    :8081              :8082              :8083
       ↑                 ↑                 ↑
       └──────────────── Admin :8084 ─────┘
                         |
                       Mongo
                         +
                       Redis
                         +
                       Kafka

Candidate
   |
   └──────→ Code Runner :2000
```

**Cross-question: Why is the gateway necessary?**

> It gives us a single public entry point and centralizes concerns such as JWT validation, CORS, rate limiting, correlation IDs and routing.

---

## 2. Why Microservices Instead of Monolith?

**Interview answer:**

> We separated services primarily because the domains have different responsibilities, scaling requirements and failure characteristics.
>
> For example, Content Service is heavily read-oriented, Auth has security-sensitive operations, and the code runner is CPU-intensive and handles untrusted code.
>
> Separating them means a failure in the code runner doesn't bring down authentication, and we can scale content independently from authentication.
>
> However, I wouldn't claim microservices are always better. For a small early-stage product, a modular monolith would actually be simpler. The current architecture is a growth and learning-oriented choice.

**Cross-question: What is the disadvantage?**

> Network latency, distributed failures, eventual consistency, deployment complexity, duplicated DTOs and more complicated debugging.

**Cross-question: If you redesigned it?**

> I'd probably start with a modular monolith, extract the code runner immediately because of its security boundary, and extract Auth or Content later when independent scaling becomes necessary.

Don't say ❌ "Microservices are faster."

Say ✅ "Microservices provide isolation and independent scaling, at the cost of distributed-system complexity."

---

## 3. Why MongoDB Instead of PostgreSQL?

**Interview answer:**

> The main reason was the structure of our content. Questions contain nested data such as examples, files, quiz information and other type-specific fields. MongoDB's document model maps naturally to this structure.
>
> We also have different question types such as DSA, HLD, LLD, CS, frontend and OA. Instead of maintaining completely separate relational structures, we use a unified questions collection with a type discriminator.
>
> PostgreSQL would be a strong choice if the dominant requirement were complex joins, relational constraints or financial analytics. But for the question catalog, MongoDB provided a simpler document-oriented model.

**Cross-question: Isn't PostgreSQL more reliable?**

> Reliability isn't the main distinction here. Both can be reliable. The decision was primarily about data modeling and access patterns.

**Cross-question: What is the weakness of MongoDB here?**

> Complex analytics and relational constraints become harder. If we needed extensive reporting or transactional financial workflows, PostgreSQL could be a better fit.

**Cross-question: Why one questions collection?**

```json
{ "type": "DSA", ... }
{ "type": "HLD", ... }
{ "type": "LLD", ... }
```

This makes search, pagination, filtering, and admin listing simpler.

The downside is sparse/type-specific fields and validation at the service layer.

---

## 4. Why Redis and Exactly Where Is It Used?

This is a very important question for your project.

**Interview answer:**

> Redis isn't our primary database. We use it for fast, temporary or frequently accessed data.
>
> In Tyyari, there are four major uses: question caching, catalog caching, rate limiting and session revocation.

| Use | Key | Purpose |
| --- | --- | --- |
| 1. Question cache | `question:{id}` | Frequently accessed questions |
| 2. Catalog cache | companies, topics, tags | Read frequently, changes infrequently |
| 3. Rate limiting | `rate_limit:{id}` | Gateway tracks requests and prevents abuse |
| 4. Session blocking | `session:block:{userId}` | Supports "sign out everywhere" |

**Cross-question: Why Redis instead of MongoDB for caching?**

> Redis is an in-memory key-value store optimized for extremely fast access and operations such as counters and TTLs. MongoDB is our durable source of truth, while Redis is a performance and control layer.

**Cross-question: What happens if Redis goes down?**

For cached content: Redis ❌ → MongoDB. We can fall back, although latency increases.

For rate limiting and session revocation, we need explicit failure semantics because those are security/control functions.

**Cross-question: Is Redis the source of truth?**

Absolutely not.

> MongoDB is the source of truth. Redis is cache-aside and ephemeral state.

The architecture explicitly calls this "cache-aside, not cache-as-source-of-truth."

---

## 5. Explain Cache-Aside

This is extremely important.

Suppose `GET /questions/two-sum`.

**First request:**

```text
Browser
   ↓
Gateway
   ↓
Content Service
   ↓
Redis
   ↓
MISS
   ↓
MongoDB
   ↓
Redis SET
   ↓
Response
```

**Second request:** Redis HIT → Response. Mongo isn't touched.

**Why?** Because content is read-heavy: **Reads >>> Writes**. Caching reduces MongoDB load.

**What happens when question changes?**

```text
Mongo UPDATE
     ↓
Redis EVICT
```

Next request: Redis MISS → Mongo → Redis SET. This is **cache invalidation**.

The current architecture uses explicit eviction plus TTLs.

**Cross-question: What if eviction fails?**

Then Redis may return stale data until TTL expires. That's one of the known tradeoffs in the project.

---

## 6. Why Kafka?

**Interview answer:**

> We use Kafka when the producer shouldn't have to synchronously wait for downstream services.
>
> For example, after user registration, Auth Service can publish a USER_REGISTERED event. User Service consumes that event and creates the user's profile.
>
> Similarly, account deletion uses an asynchronous USER_DELETE_REQUESTED event so Auth and User can independently process deletion.

The project uses Kafka for registration, deletion and content mutation events, while keeping immediate CRUD operations synchronous.

| REST | Kafka |
| --- | --- |
| Service A waits for Service B | A doesn't need B to be available immediately |

**Cross-question: Why not Kafka for everything?**

> Kafka adds complexity and eventual consistency. For operations where the client needs an immediate response, such as login, question retrieval or normal CRUD, synchronous HTTP is simpler and appropriate.

---

## 7. Explain USER_REGISTERED Flow

Let's say a new candidate registers.

| Step | Action |
| --- | --- |
| 1 | Frontend: `POST /auth/register` |
| 2 | Gateway routes to Auth Service |
| 3 | Auth validates email/password; password is hashed |
| 4 | Auth saves the user in `auth_db` |
| 5 | Auth publishes `USER_REGISTERED` to Kafka |
| 6 | User Service consumes `USER_REGISTERED` |
| 7 | User Service creates profile, preferences, goals |

```text
              Auth
               |
          save user
               |
               ▼
             Kafka
               |
      USER_REGISTERED
               |
               ▼
          User Service
               |
               ▼
        Create profile
```

**Cross-question: Why not create the profile synchronously?**

You could: Auth → User Service. But then if User Service is down, Registration ❌.

With Kafka: Auth → Kafka → User, the profile can be created later.

**Cross-question: What if Kafka fails?** This leads directly to question 9.

---

## 8. Explain Account Deletion Using Kafka

This is another flow you should be able to draw on a whiteboard.

Suppose admin requests deletion.

| Step | Action |
| --- | --- |
| 1 | Lock account: `status = DELETING` |
| 2 | Block active sessions: `session:block:{userId}` |
| 3 | Publish `USER_DELETE_REQUESTED` |
| 4 | Kafka distributes the event |
| 5 | Retry failures |
| 6 | If still failing → Dead Letter Topic |

```text
                   Kafka
                     |
             USER_DELETE_REQUESTED
                  /       \
                 /         \
                ▼           ▼
             Auth          User
              |             |
        delete auth     delete profile
```

Each service handles its own data. The project uses this **choreography** approach rather than a central orchestrator.

---

## 9. What Happens If Kafka Fails?

This is where you need to distinguish between Kafka unavailable and consumer processing failure.

### Case 1 — Kafka itself is unavailable

```text
Auth
 ↓
Kafka ❌
```

The event cannot immediately be delivered. For important events, production design should ensure the event isn't lost. This is where **Outbox** becomes important.

### Case 2 — Kafka works but User Service is down

```text
Auth → Kafka → User Service ❌
```

The message remains available for consumption. When User Service comes back, it processes the event.

### Case 3 — Consumer keeps failing

attempt 1 ❌, attempt 2 ❌, attempt 3 ❌ → **DLT**

**Cross-question: Does Kafka guarantee exactly-once processing?**

Don't casually say yes.

A safer answer:

> Kafka provides strong delivery and ordering guarantees depending on configuration and consumer semantics, but business-level exactly-once behavior still requires idempotent consumers and appropriate transaction design.

For example, `USER_DELETE_REQUESTED` could theoretically be processed more than once. Therefore deletion handlers should be **idempotent**.

---

## 10. What Is the Outbox Pattern and Why Do You Need It?

This is probably the most important weakness in your architecture.

**Current flow:** MongoDB → save user → publish Kafka event.

Potential problem:

```text
Mongo save      ✅
Application     💥 crash
Kafka publish   ❌

Database = user exists
Kafka = no USER_REGISTERED event
```

User profile may never be created. The architecture explicitly identifies this problem.

**Outbox solution:**

```text
              MongoDB
                 |
       ---------------------
       |                   |
   User record        Outbox record
       |                   |
       ---------------------
                 |
            committed
                 |
                 ▼
          Outbox Publisher
                 |
                 ▼
               Kafka
```

The business record and event record are stored together. Then a background publisher reads `outbox_events` and publishes to Kafka.

If Kafka is down: Mongo ✅, Kafka ❌ — the event still exists in outbox. Publisher retries later.

**Interview answer:**

> Our current implementation publishes after the Mongo write, which creates a crash window between persistence and Kafka publication. The production improvement would be a transactional outbox. We would write the business record and an outbox event in the same transaction, and a separate publisher would reliably publish the event to Kafka.

That's an excellent answer.

---

## 11. Explain JWT + Refresh-Token Architecture

This is another high-frequency interview topic.

Tyyari uses:

| Token | Type | Lifetime |
| --- | --- | --- |
| Access Token | JWT | ~15 minutes |
| Refresh Token | opaque | ~7 days |

The refresh token is stored **hashed** in MongoDB and is **rotatable**.

**Login:**

```text
User
 ↓
email + password
 ↓
Auth Service
 ↓
verify password
 ↓
generate:
    JWT access token
    refresh token
```

JWT might conceptually contain:

```json
{
  "sub": "123",
  "role": "USER",
  "premium": true,
  "exp": "..."
}
```

**API request:**

```text
Browser
 ↓
Authorization: Bearer JWT
 ↓
Gateway
 ↓
validate JWT
 ↓
Content/User/etc.
```

No database lookup is required just to validate the JWT. That's why JWT is useful for the access token.

**Refresh flow:** After 15 minutes, access token expires → 401.

Frontend uses refresh token:

```text
Refresh Token
     ↓
Auth Service
     ↓
verify hashed token
     ↓
rotate refresh token
     ↓
new JWT
```

User doesn't have to login again.

**Why short access-token lifetime?** Security. If access JWT is stolen, the token expires relatively quickly.

**Why hash refresh token?** If database is compromised, attacker gets hashed refresh tokens rather than directly usable tokens.

**Cross-question: Why not store sessions in Redis?**

You could. But current architecture chooses JWT access tokens to keep gateway authentication largely stateless.

Redis is used for session blocking (`session:block:{userId}`) rather than storing every access-token session.

---

## 12. Why Is the Code Runner Separated?

This is one of the strongest architecture decisions.

Imagine Spring Boot executing user code. A malicious user could execute `while(true) {}` or consume huge memory/CPU. They could potentially affect the backend process.

Therefore:

```text
Java Backend
      X
      |
Code Runner
```

The runner becomes a **separate trust boundary**.

The current architecture uses a Piston-compatible runner and deliberately keeps Java away from executing user code.

**Request flow:**

```text
Browser
   ↓
Vite /api/piston
   ↓
Code Runner
   ↓
Compiler
   ↓
Execute
   ↓
Output
```

The Java backend isn't involved in executing the code.

**Cross-question: Why not call runner through API Gateway?**

The current implementation intentionally bypasses the Java gateway:

```text
Browser → Vite proxy → Runner
```

This reduces latency and simplifies the path.

**Tradeoff:** The runner execute endpoint doesn't currently receive normal user identity/authentication from the gateway. The architecture explicitly calls this out as a weakness.

---

## 13. Is the Code Runner Production Secure?

**Answer: No.** And this is actually the answer you should give.

**Current protection:** temporary directory, process spawning, timeout, file-size limits.

But it does not currently use stronger sandboxing such as gVisor, Firecracker, or Kata Containers.

The architecture explicitly states that the current runner isn't production-safe against a determined attacker.

**How would you improve it?**

| # | Improvement | Why |
| --- | --- | --- |
| 1 | Strong sandbox (gVisor / Firecracker) | Isolate native code |
| 2 | CPU limits | Prevent 100% CPU forever |
| 3 | Memory limits | Prevent 10 GB allocation |
| 4 | Process limits | Prevent fork bombs |
| 5 | Network isolation | User code shouldn't access Mongo, Redis, internal services, internet |
| 6 | Non-root execution | Minimal privileges |
| 7 | Filesystem restrictions | Temporary isolated filesystem |
| 8 | Per-user quotas | Free user → X executions/min; Premium → higher quota |

**Interview answer:**

> The current runner is suitable for a lab environment but isn't production-grade. Since arbitrary native code is untrusted, I'd use stronger sandboxing, disable networking, enforce CPU/memory/process limits, run without root privileges and put executions behind a queue and worker pool.

---

## 14. How Would You Scale Tyyari to 10× Traffic?

This question can become a complete system-design round.

Current architecture has: 1 Gateway, 1 Mongo process, 1 Redis, 1 Kafka broker, 1 Code Runner.

The architecture explicitly describes these as **local/dev infrastructure** rather than true HA infrastructure.

### Step 1 — Horizontally scale Gateway

```text
             Load Balancer
              /          \
       Gateway 1       Gateway 2
```

JWT is stateless, so sticky sessions aren't required.

### Step 2 — Scale services independently

- Auth: Auth1 Auth2
- Content: C1 C2 C3 C4 (read-heavy)
- User: U1 U2

### Step 3 — Redis Cluster

Current: Redis 1 → Production: Redis Cluster for better availability and capacity.

### Step 4 — Kafka cluster

Current: Kafka Broker 1, RF = 1 → Production: Kafka 1/2/3, RF = 3.

### Step 5 — Scale code execution

```text
             Queue
               |
      -------------------
      |        |        |
   Worker1  Worker2  Worker3
```

This prevents traffic spikes from overwhelming a single runner.

### Step 6 — Database

For submissions, `userId` could become a shard key if volume becomes extremely large. Content can use replicas and optimized indexes.

### Step 7 — Search

If Mongo filtering becomes insufficient:

```text
content-events
       ↓
Kafka
       ↓
OpenSearch
```

Search query → OpenSearch, while Mongo remains the source of truth.

**Excellent 10× answer:**

> I'd scale horizontally rather than vertically. I'd put multiple gateway instances behind a load balancer, scale Content Service independently because it is read-heavy, move Redis to a cluster, run Kafka with multiple brokers and replication, and move code execution behind a queue with multiple isolated workers. For Mongo, I'd first optimize indexes and replicas and only introduce sharding for high-volume collections such as submissions when necessary.

---

## 15. What Are the Weaknesses of Your Architecture?

This is where you can really impress the interviewer.

Don't defend everything. Say: "There are several known tradeoffs."

### Weakness 1 — Not truly HA infrastructure

1 Mongo, 1 Redis, 1 Kafka. Mongo failure → large part of system affected.

**Improvement:** Production replicas / managed infrastructure.

### Weakness 2 — No Outbox

Mongo save → Kafka publish. Crash between them can lose event.

**Improvement:** Transactional Outbox.

### Weakness 3 — Premium JWT staleness

JWT says `premium=false`. Admin upgrades user: DB `premium=true`. Existing JWT still says false until refresh.

**Improvement:** Shorter token TTL, forced refresh, entitlement versioning or centralized entitlement check.

### Weakness 4 — Runner isn't production-grade

Timeout, temp directory, file-size limit — but no strong sandbox.

**Improvement:** gVisor / Firecracker + no network + CPU/memory/process limits.

### Weakness 5 — Runner isn't authenticated

Browser → Runner doesn't have the normal gateway identity context. This makes abuse/quota enforcement harder.

**Improvement:** Put execution behind an authenticated service/API or issue signed execution requests with quotas.

### Weakness 6 — Admin audit isn't transactional

Content update ✅, Audit insert ❌. Action happened but audit record is missing.

**Improvement:** Outbox/event-driven audit or stronger transactional design.

### Weakness 7 — Event consumers are limited

`content-events` exists for future extensibility, but currently has relatively few consumers.

Interviewer might ask: Why publish events nobody consumes?

**Answer:**

> The event contract is intended to reduce future coupling and allow capabilities such as search or notifications to subscribe later. But I agree that introducing Kafka has an operational cost, so I'd only retain it where there is a clear evolution path.

### Weakness 8 — Gateway becomes a routing bottleneck

As resources grow (`/questions`, `/roadmaps`, `/categories`, `/sheets`, ...), gateway routing configuration grows.

**Improvement:** Cleaner route grouping / service discovery / configuration.

### Weakness 9 — Two frontend applications duplicate code

Candidate (`web`) and Admin (`admin`) duplicate some authentication/runner logic.

**Improvement:** Shared packages:

```text
packages/
   auth/
   api-client/
   runner/
   ui/
```

### Weakness 10 — Eventual consistency

Example: Auth says user exists, User says profile not yet created, because Kafka is asynchronous.

The UI/backend needs to tolerate that temporary state.

---

## Final: Your Complete Interview Cheat Sheet

| Question | Core answer |
| --- | --- |
| Architecture | Gateway + Auth + User + Content + Admin + Runner |
| Why microservices? | Isolation + independent scaling + domain separation |
| Why Mongo? | Nested/flexible question documents |
| Why Redis? | Cache + rate limit + session revocation |
| Cache-aside | Redis → miss → Mongo → Redis |
| Why Kafka? | Async events + decoupling |
| Registration | Auth → Kafka → User creates profile |
| Deletion | Lock → block session → Kafka → Auth/User consumers |
| Kafka failure | Retry/DLT; outbox needed for reliable publishing |
| Outbox | DB record + event committed together |
| JWT | Short-lived stateless access token |
| Refresh | Long-lived opaque token, hashed + rotated |
| Runner | Separate untrusted execution boundary |
| Runner secure? | Not production-grade currently |
| 10× scaling | Horizontal services + Redis cluster + Kafka cluster + runner workers |
| Weaknesses | No outbox, weak runner, JWT staleness, single-node infra, eventual consistency |

### The 5 things you absolutely must be able to draw

1. Overall architecture
2. Login + JWT flow
3. Cache-aside flow
4. USER_REGISTERED Kafka flow
5. Account deletion Kafka flow

If you can draw those five and answer "why?" after every component, you will be able to handle most follow-up questions around this project.

The architecture's own recommended 30-second summary is essentially: **gateway + domain split + JWT + Redis + Kafka + Mongo + isolated runner**, with operational complexity and eventual consistency being the major tradeoffs.
