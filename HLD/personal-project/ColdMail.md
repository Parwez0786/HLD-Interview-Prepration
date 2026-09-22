# ColdMail — Personal Project HLD Interview Prep

For an HLD interview, don't explain this project as a list of files. Explain it as:

**requirements → architecture → request flow → data model → feature implementation → trade-offs → failure handling → scaling → security → interviewer follow-ups.**

---

## Table of Contents

- [1. 60-Second Project Explanation](#1-first-60-second-project-explanation)
- [2. High-Level Architecture](#2-high-level-architecture)
- [3. Why React + Express?](#3-why-react--express)
- [4. Why MongoDB?](#4-why-mongodb)
- [5. Multi-Tenancy](#5-multi-tenancy)
- [6. Authentication Architecture](#6-authentication-architecture)
- [7–12. Tokens, Refresh, Axios, AsyncLocalStorage](#7-why-not-localstorage)
- [13–31. Compose, Enrichment, Gmail, Bulk Send](#13-compose-architecture)
- [32–48. Templates, Resumes, LLM, Tailoring](#32-template-library)
- [49–62. Job Intake, Rate Limiting, Scaling](#49-job-intake)
- [63–77. Security, Trade-offs, Rapid-Fire](#63-security-cross-questions)
- [Part 2 — Features and Database Schema](#part-2--every-feature-and-database-schema)
- [Complete Interview Answer](#60-one-complete-interview-answer-to-memorize)

---

## 1. First: 60-Second Project Explanation

If interviewer says "Explain your project", start here:

> I built a ColdMail platform that helps users automate personalized job outreach.
>
> The system allows a user to upload resumes, create email templates, provide recruiter emails or LinkedIn profiles, enrich missing information using LLMs, match resumes/templates against a job description, personalize emails, and finally save them as Gmail drafts.
>
> Architecturally, it is a React SPA + Node.js/Express backend + MongoDB + Gmail IMAP + LLM providers.
>
> The backend follows a layered approach: React → Express routes → services → stores → external systems.
>
> Authentication uses short-lived access JWTs with rotating HTTP-only refresh tokens. User data is scoped using the authenticated userId.
>
> For AI functionality, I created a unified LLM abstraction so application code doesn't depend directly on Gemini or Groq. The same interface can select the provider and model.
>
> For email sending, I intentionally save emails as Gmail drafts using IMAP instead of directly sending them. This gives the user an opportunity to review the generated email before sending.
>
> For resume tailoring, I parse the LaTeX resume, calculate deterministic JD-match scores locally, use an LLM only for generating suggestions, and safely apply approved changes using brace-balanced parsing and backups.
>
> The major design decisions were around security, tenant isolation, AI reliability, Gmail integration, and avoiding unnecessary infrastructure for the current scale.

That's your opening.

---

## 2. High-Level Architecture

```text
                    ┌──────────────────┐
                    │     React SPA    │
                    │                  │
                    │ Compose           │
                    │ Templates         │
                    │ Resume Library    │
                    │ JD Match          │
                    │ Resume Tailor     │
                    │ Draft Log        │
                    └────────┬─────────┘
                             │
                         HTTPS / API
                             │
                             ▼
                  ┌─────────────────────┐
                  │ Express API Server  │
                  │                     │
                  │ Auth Middleware     │
                  │ AI Middleware       │
                  │ Rate Limiter        │
                  │ Validation          │
                  │ Routes              │
                  └─────────┬───────────┘
                            │
             ┌──────────────┼──────────────┐
             │              │              │
             ▼              ▼              ▼
        ┌─────────┐    ┌──────────┐   ┌──────────┐
        │ MongoDB │    │ LLM Layer│   │ Gmail    │
        │         │    │          │   │ IMAP     │
        │ Users   │    │ Gemini   │   │          │
        │ Templates│   │ Groq     │   │ Drafts   │
        │ Resumes │    └──────────┘   └──────────┘
        │ Logs    │
        └─────────┘
```

---

## 3. Why React + Express?

**Why React?** The application has multiple interactive screens: Compose, Templates, Resume library, JD matching, Resume tailoring, Draft logs.

There is also a lot of client-side state: selected template, selected resume, recipients, preview, AI provider, tailoring state. React makes this state management straightforward.

**Why Express?** The application is primarily an API-driven backend. It provides middleware, routing, authentication, request validation, error handling, and file upload handling.

For the current scale, Express is simpler than introducing a heavier framework.

---

## 4. Why MongoDB?

**Interviewer:** Why MongoDB instead of PostgreSQL?

The data is mostly document-oriented and has flexible fields.

Recipients can contain arbitrary CSV fields:

```json
{
  "email": "john@abc.com",
  "name": "John",
  "company": "ABC",
  "role": "SDE",
  "location": "Bangalore"
}
```

Different CSV files can contain different columns. Instead of constantly changing relational schemas, MongoDB allows:

```json
{
  "extra": {
    "experience": "3 years",
    "linkedin": "...",
    "location": "Bangalore"
  }
}
```

**Trade-off:** MongoDB gives flexibility, but relational databases would provide stronger constraints, transactions, joins, and relational consistency. For this application's document-heavy data, MongoDB was reasonable.

---

## 5. Multi-Tenancy

The application is multi-user. User A must never see User B's templates.

```text
authenticated user
       ↓
requireAuth
       ↓
userId
       ↓
store automatically adds userId
```

```javascript
find({ userId: currentUserId })

insertOne({ ...template, userId })

replaceOne({ id, userId })
```

**Important:** `userId` is **not** taken from the request body.

**Bad:** `PUT /templates` with `{ "userId": "user123" }` — a malicious user could change it.

**Instead:** `const userId = requireCurrentUserId();` comes from the authenticated session.

**Cross-question:** What if someone modifies the URL from `/templates/A` to `/templates/B`?

The database operation itself contains `{ id: "B", userId: authenticatedUserId }`. So even if B exists, the query doesn't match another user's record.

This provides an important layer of tenant isolation.

---

## 6. Authentication Architecture

```text
Login
  │
  ▼
bcrypt password verification
  │
  ▼
Create refresh token record
  │
  ▼
Generate:
   Access JWT
   Refresh JWT
  │
  ├── Access → client memory
  │
  └── Refresh → httpOnly cookie
```

**Why two tokens?**

| Token | Lifetime | Purpose |
| --- | --- | --- |
| Access token | 15 minutes | API calls |
| Refresh token | long-lived | Get a new access token |

If the access token gets stolen, its lifetime is limited.

---

## 7. Why Not localStorage?

Because localStorage is accessible to JavaScript. If the application has an XSS vulnerability, malicious JavaScript could read the token.

Instead:

- Access token → memory
- Refresh token → HTTP-only cookie

The refresh cookie cannot be directly read by JavaScript.

---

## 8. Why Refresh-Token Rotation?

Suppose refresh token R1 is stolen.

Normal refresh: R1 → R2. If R1 remains valid, attacker can continue using it.

Instead: R1 → revoked, R2 → created.

| jti | revoked |
| --- | --- |
| R1 | true |
| R2 | false |

Now if R1 appears again: possible token theft → `revokeAllForUser()`.

This is called **refresh token reuse detection**.

---

## 9. Why JWT + Database?

**Interviewer:** JWT is stateless. Why are you storing refresh tokens in MongoDB?

I intentionally keep access tokens stateless but make refresh sessions stateful.

| Token | Storage | Enables |
| --- | --- | --- |
| Access | JWT only | No DB lookup |
| Refresh | JWT + database JTI | Revocation, rotation, session tracking, reuse detection, logout-all, password-change invalidation |

This is a good hybrid approach.

---

## 10. Axios Refresh Flow

```text
API request
   ↓
401
   ↓
Axios interceptor
   ↓
refresh token API
   ↓
new access token
   ↓
retry original request
```

**Important issue:** What if 10 API requests simultaneously receive 401?

Without protection: 10 requests → 10 refresh requests.

Instead, `refreshPromise` is shared:

```text
Request A ─┐
Request B ─┼──> same refreshPromise
Request C ─┘
```

Only one refresh request happens. This is a nice concurrency detail to mention.

---

## 11. Why AsyncLocalStorage?

Instead of passing `generateEmail(userId, aiProvider, ...)` everywhere, request context stores `userId`, AI provider, and AI model.

It reduces parameter passing across many layers.

**Trade-off:** Hidden request state can make code harder to understand and test. Use it only for truly cross-cutting request context: authenticated user, request ID, AI provider.

---

## 12. Why Multer Rebound Was Required?

Multer handles multipart uploads using callbacks. Some asynchronous context can be lost across those boundaries.

So after attachment processing: `runWithUser(req.user.id, () => next())` re-establishes the user context.

This prevents accidental queries without tenant context.

---

## 13. Compose Architecture

Compose supports Mail ID, CSV, and LinkedIn — but they eventually converge into a common recipient model.

```text
Mail ID
   │
CSV ├──→ Recipient
   │
LinkedIn
   │
   ▼
Personalization
   │
   ▼
Template rendering
   │
   ▼
Draft creation
```

This reduces duplicate sending logic.

---

## 14. Mail ID Enrichment

Input: `john.smith@google.com`

The system parses the email, then asks the LLM for the likely name.

**But you don't completely trust AI.** Fallback:

```text
john.smith
   ↓
split "."
   ↓
john + smith
   ↓
John Smith
```

LLM calls can fail, timeout, hit quota, or return invalid data. So the application remains useful even if AI fails.

**Important principle:** AI should enhance the application, not become a single point of failure.

---

## 15. LinkedIn Enrichment

Input: `linkedin.com/in/john-smith-123456`

Normalize URL → `john-smith` → `John Smith` → generate possible email patterns.

Then: candidate generation → dedupe → MX lookup → confidence filtering → sorted candidates.

---

## 16. Why MX Lookup?

**Does MX lookup prove that the email exists?** No.

MX lookup only tells us the domain has a mail server capable of receiving email. It does **not** guarantee `john.smith@google.com` is an actual mailbox.

MX lookup is a lightweight **domain-level validation signal**, not mailbox verification.

---

## 17. Why Cache LinkedIn Enrichment?

Email-domain enrichment repeatedly sees the same company. Email patterns don't change frequently.

`company + domain` → 10-minute cache reduces LLM calls, latency, and cost.

**Trade-off:** Current cache is a `Map` local to one server. For horizontal scaling I'd use **Redis**.

---

## 18. CSV Architecture

CSV is parsed on the client using PapaParse.

**Why client-side?** Reduces backend CPU, immediate validation, better UX, no need to upload entire CSV just to parse it.

Required: `email`. Other columns become `extra`.

```json
{
  "email": "a@x.com",
  "extra": {
    "company": "Google",
    "role": "SDE",
    "location": "Bangalore"
  }
}
```

This allows dynamic personalization.

---

## 19. Template Rendering

Template: `Hello {{name}}, I am interested in {{company}}.`

Server uses **Handlebars**. Client uses a small **regex renderer**.

**Why two renderers?** Client doesn't need the complete Handlebars package. It only needs `{{key}}` replacement. This reduces frontend bundle size.

---

## 20. Why Not Use Handlebars Everywhere?

The server needs a proper template engine for production rendering, while the client only needs lightweight preview rendering. The server is the **source of truth**. There is also `/api/preview` to verify server-side rendering.

**Trade-off:** Two rendering implementations introduce the possibility of subtle differences. That's why keeping the supported syntax deliberately small is important.

---

## 21. Security of HTML Preview

Generated HTML is displayed in `<iframe sandbox srcdoc="...">`.

Directly doing `element.innerHTML = generatedHTML` could execute unwanted HTML/script content. Sandboxed iframe isolates the preview.

---

## 22. Why Not Send Emails Directly?

This is one of the most important product decisions.

The system creates a **Gmail Draft** instead of sending immediately.

Cold emails are AI-generated. User should be able to Review → Edit → Send.

Therefore the system behaves more like an **email copilot**.

---

## 23. Gmail Draft Architecture

```text
POST /send-email
       ↓
Validate
       ↓
Build variables
       ↓
Render template
       ↓
MailComposer
       ↓
RFC822 email
       ↓
IMAP
       ↓
Gmail Drafts
```

---

## 24. Why IMAP Instead of Gmail API?

IMAP was chosen because the requirement was to append a properly formatted message directly into the user's Drafts mailbox, and IMAP provides the append operation naturally.

**Trade-off:** Gmail API would generally provide OAuth integration, structured Gmail operations, and better provider-specific semantics. IMAP requires mailbox discovery, app password or credentials, and IMAP connection handling.

For a production consumer application, I would strongly consider **Gmail API + OAuth**.

---

## 25. Why MailComposer?

The email needs to become a valid RFC822 message. MailComposer handles From, To, Subject, HTML, Attachments, MIME and creates an RFC822 buffer. Then IMAP appends it.

---

## 26. Attachment Handling

Two possibilities: Resume library **or** Device upload — mutually exclusive.

If both somehow arrive: multipart file > resumeId. The uploaded file wins because it is the more immediate user action.

Then the attachment gets normalized to `DRAFT_ATTACHMENT_FILENAME.pdf` for consistent MIME behavior.

---

## 27. Bulk Sending

UI does `N × /send-email` instead of `1 × /send-bulk`.

Because each recipient needs independent UI state:

```text
John → drafted
Mike → failed
Sarah → drafted
```

One failure doesn't affect others.

---

## 28. Why 250 ms Delay?

Bulk sending has 250 ms between rows. This is a simple throttling mechanism.

Without it, 1000 rapid IMAP connections could cause provider throttling, connection pressure, and rate-limit problems.

**Trade-off:** Fixed delay is primitive. At scale, I'd replace it with **queue + worker + rate limiter**.

---

## 29. Why No Kafka / RabbitMQ?

Excellent HLD cross-question.

The current application doesn't require distributed asynchronous processing at the current scale. Adding Kafka introduces operational complexity, brokers, partitions, consumer management, retries, and monitoring.

For the current workload, Node process + sequential processing is sufficient.

If requirements become 10M emails/day:

```text
API
 ↓
Queue
 ↓
Workers
 ↓
Email provider
```

---

## 30. Failure Handling During Bulk Send

Suppose 1 → success, 2 → success, 3 → Gmail failure, 4 → success.

We don't abort the entire batch. Each row maintains `pending → sending → drafted` or `pending → sending → failed`.

This gives **partial failure isolation**.

---

## 31. Logging

Every send produces `{ "status": "drafted" }` or `{ "status": "failed" }`. The log is user-scoped, newest first.

**Interesting design choice:** If Mongo logging fails, the draft operation doesn't fail.

- Draft creation = **critical**
- Logging = **best effort**

This is an example of separating critical path from observability/audit path.

---

## 32. Template Library

| Field | Purpose |
| --- | --- |
| `id` | Template ID |
| `userId` | Owner |
| `name` | Display name |
| `subject` / `body` | Handlebars template |
| `tags` | Classification |
| `isDefault` | Default for compose |
| `createdAt` / `updatedAt` | Timestamps |

Every query contains `userId`. There should be only one default template per user.

Current implementation: set new default → unset siblings. Application-level enforcement.

**Trade-off:** Race condition is possible if two requests set different defaults concurrently. A stronger design could use transactions or an appropriate constraint.

---

## 33. Resume Storage

Current design: **MongoDB Binary** instead of S3.

Upload: PDF → Multer memory → Mongo Binary.

**Major trade-off:** MongoDB is not ideal for large binary storage at scale.

Better production architecture: MongoDB stores metadata (`resumeId`, `userId`, `s3Key`, `name`, `tags`); S3 stores the PDF.

---

## 34. Why Not S3?

**Good answer:** S3 was considered as the scalable storage architecture, but the current implementation uses MongoDB Binary for simplicity. The application metadata model already separates resume identity from content, so migrating to object storage would mainly require changing the storage layer.

Don't claim S3 is implemented if it isn't.

---

## 35. Resume Auto-Tagging

PDF → base64 → Gemini multimodal → technology tags (Java, Spring Boot, Kafka, AWS, MongoDB).

Groq cannot process the PDF parts used here, so the system **forces Gemini** for multimodal requests. This is a good example of capability-based provider routing.

---

## 36. Unified LLM Architecture

This is probably the **strongest architecture point**.

Application code doesn't call Gemini or Groq directly. Instead:

```javascript
generateStructuredJson({
  systemPrompt,
  userPrompt,
  schema,
  temperature,
  parts
})
```

```text
Feature
  │
  ▼
LLM abstraction
  │
  ├── Gemini adapter
  │
  └── Groq adapter
```

This is basically an Adapter/Strategy-style abstraction.

---

## 37. Why Unified LLM Layer?

Without abstraction, every feature couples to vendors. With abstraction, switching provider doesn't require changing every feature.

---

## 38. Structured JSON

Instead of asking "Give me the candidate name" and parsing arbitrary text, the system requests a JSON schema.

Gemini supports native JSON schema. Groq uses JSON-object mode and puts the schema into the prompt. This reduces parsing failures.

---

## 39. Why No Server-Side AI Retry?

Quota errors shouldn't be retried. Retrying 429 increases latency, quota consumption, and load.

Instead: quota error → 429 → retry hint → client/user decides.

This is a good reliability decision.

---

## 40. JD Matching

The system sends a slim representation: `id`, `name`, `tags` — not the entire resume/template.

LLM context is expensive. Smaller input means lower token cost, lower latency, less noise.

---

## 41. Why Deterministic Scoring for Resume?

Resume tailoring computes unigram similarity + bigram similarity + ATS heuristics **without AI**.

Scoring should be reproducible, cheap, and fast. If you ask an LLM "Score this resume 83/100," the score may change between runs.

So **AI generates suggestions**, while **deterministic code calculates the score**.

This is a very strong architecture decision.

---

## 42. Resume Tailoring Architecture

```text
Resume LaTeX
      ↓
Parser
      ↓
Structured sections
      ↓
Deterministic scoring
      ↓
JD comparison
      ↓
LLM suggestions
      ↓
Suggestion queue
      ↓
User approve/reject/edit
      ↓
Safe LaTeX modification
      ↓
Compile
      ↓
PDF
```

---

## 43. Why Not Let the LLM Rewrite the Whole Resume?

Full AI rewrite can accidentally modify `\documentclass`, `\usepackage`, `\input`, or break `{}`.

The system instead makes controlled changes.

**Principle:** LLM = suggestion generator. Code = enforcement layer.

---

## 44. LaTeX Safety

The parser understands known macros, e.g. `\resumeItem{Built Kafka pipeline}`.

The editor knows brace boundaries. It doesn't blindly run `string.replace()` because nested braces can make that unsafe.

---

## 45. Change Rollback

Before applying changes: `main.tex` → `main.tex.bak`.

If user changes an earlier suggestion: rollback → restore original → reparse → apply approved suggestions again.

This is essentially **event/replay-style state reconstruction**, instead of trying to reverse arbitrary text edits.

---

## 46. Why Replay Instead of Inverse Operations?

Suppose A, B, C applied, then user rejects B. Calculating `reverse(B)` can be complicated because C may have changed the same area.

Instead: original → apply A → skip B → apply C. Deterministic and much safer.

---

## 47. Compile Architecture

```text
CV directory
    ↓
walk files
    ↓
skip temporary files
    ↓
main.tex first
    ↓
flatten
    ↓
rewrite includes
    ↓
rename main.tex → document.tex
    ↓
TeXLive API
    ↓
PDF
```

**Why flatten?** External compilation service may not understand arbitrary local paths.

---

## 48. Path Traversal Protection

Suppose attacker sends `../../etc/passwd`.

The system resolves the path and verifies `resolvedPath` starts with `PROJECT_ROOT`. Paths outside PROJECT_ROOT are rejected.

---

## 49. Job Intake

Agent provides `jobUrl` + pasted JD + company.

```text
fetch
 ↓
20-second timeout
 ↓
remove script/style
 ↓
strip HTML
 ↓
80k character limit
 ↓
LLM extraction
```

If user supplied company: **user company > LLM company**, because explicit user input should have higher priority.

---

## 50. Rate Limiting

Current implementation uses an in-memory limiter (IP → request count).

Sensitive endpoints: `/send-email`, `/send-bulk`, `/enrich/email` — they consume AI quota, create Gmail connections, and can be abused.

**Trade-off:** In-memory rate limiting doesn't work correctly across multiple instances. Production: Load Balancer → Server A/B/C → **Redis rate limiter**.

---

## 51. What Would You Change for Production?

| Current | Production evolution |
| --- | --- |
| React, Node, MongoDB, LLM, IMAP | Load balancer, multiple API servers, Redis, MongoDB, Queue, Workers, email provider |

Then: S3 for resumes, Redis for distributed cache, queue for bulk sending, worker autoscaling, OAuth/Gmail API, centralized logs, metrics, tracing, distributed rate limiting.

---

## 52. What Are the Biggest Bottlenecks?

| Bottleneck | Problem | Solution |
| --- | --- | --- |
| 1. IMAP | N emails → N IMAP operations | Queue + workers + connection reuse + rate limiting |
| 2. LLM | Slow, expensive, rate-limited | Cache, batching, smaller prompts, structured output, fallback, provider routing |
| 3. MongoDB Binary | Large PDFs increase DB size | S3 |
| 4. In-memory state | Map cache, memory token, in-memory rate limiter don't work across replicas | Redis |

---

## 53. What Happens If MongoDB Goes Down?

| Feature | Behavior |
| --- | --- |
| Templates | Request fails — Mongo is source of truth |
| Logging | Draft still succeeds; log is lost — logging is best effort |
| AI enrichment | Fallback algorithm where available |

The system has different availability priorities.

---

## 54. What Happens If AI Provider Goes Down?

| Feature | Behavior |
| --- | --- |
| Email-name extraction | Local extraction fallback |
| Resume scoring | No AI required |
| Suggestions | Request fails |
| Multimodal PDF | Force Gemini |

The system doesn't treat all AI failures identically.

---

## 55. What Happens If Gmail Fails After Rendering?

Render → Gmail/IMAP → failure → `status = failed`, HTTP 502. The log records the failure.

The email isn't incorrectly marked as drafted if IMAP failed.

---

## 56. What If the Request Succeeds But Response Is Lost?

Server creates Gmail draft → network failure → client gets timeout. If client retries, same email could be created twice.

**Current limitation:** No strong end-to-end idempotency.

**Production:** Client sends `Idempotency-Key`. Server stores `userId + idempotencyKey`. Same key → return previous result.

This is a very good improvement to mention proactively.

---

## 57. Why `/send-email` Instead of `/send-bulk` in UI?

One API call for 1000 emails is more efficient from request overhead, but UI needs per-recipient state.

For high-scale: `POST /bulk-job` → `jobId` → queue → workers → `GET /bulk-job/:id` or WebSocket/SSE for progress.

---

## 58. Current vs Scalable Architecture

| Current | Scalable |
| --- | --- |
| React → Express → MongoDB → IMAP | React → API Gateway → Load Balancer → Stateless APIs → Redis → MongoDB → Kafka/SQS → Workers → Gmail API |
| Small team, low traffic, fast development | High traffic, large bulk ops, independent scaling, retries |

---

## 59. Why Not Microservices?

The current system has several logical modules, but traffic and team size don't justify deploying each independently.

Logical modules: Auth, Templates, Resume, Enrichment, Tailoring, Email — without physically deploying six services.

This is a **modular monolith**. Advantages: simpler deployment, easier debugging, less network overhead, easier transactions, lower infrastructure cost. Extract when individual modules need independent scaling.

---

## 60. How Would You Split Into Microservices?

API Gateway → Auth, Template, Resume, Enrichment, AI Gateway (Gemini/Groq), Email Service → Queue → Workers.

But I would **not start here**.

---

## 61. Why Agent CLI?

Two consumers: React SPA and Agent CLI. Instead of duplicating business logic, the agent calls the same APIs: job-intake, names, jd-match, send-bulk.

One source of truth.

---

## 62. Important Current Gap: CLI Authentication

Be honest. The agent's fetch doesn't include `Authorization: Bearer ...`. Therefore it only works if the API is open or authentication is added.

**Interview answer:** This is a known gap. For production I'd use a short-lived service token or OAuth/device authorization flow.

Don't hide this if interviewer finds it.

---

## 63. Security Cross-Questions

| Question | Answer |
| --- | --- |
| How do you prevent XSS? | Sandboxed iframe, template rendering rules, HTML sanitization, CSP, avoid unsafe DOM injection |
| How do you prevent path traversal? | Resolve path → verify inside PROJECT_ROOT → reject otherwise |
| How do you protect passwords? | bcrypt, 10 rounds. Never store plaintext |
| How do you protect refresh token? | HTTP-only, Secure, SameSite, restricted path, rotation, database revocation |
| How do you isolate users? | Authenticated userId + every DB query scoped by userId |

---

## 64. AI Security Questions

Treat LLM output as **untrusted input**. Never allow LLM to execute code, modify arbitrary filesystem paths, or modify LaTeX packages.

```text
LLM suggestion
 ↓
validation
 ↓
allowlist
 ↓
apply
```

---

## 65. Why Cap Input Sizes?

| Input | Cap |
| --- | --- |
| JD | 20k chars |
| Job page | 80k chars |
| Template | 6k chars |
| CSV | 50 emails |
| Library | 200 items |

This protects memory, LLM token usage, request latency, and abuse. It is both a reliability and cost-control mechanism.

---

## 66. What If 1 Million Recipients Are Uploaded?

Current implementation isn't designed for that.

Redesign: CSV upload → object storage → create bulk job → queue → workers → progress store. Workers process 10k chunks. Progress: processed, success, failed, remaining.

---

## 67. What If 10 Million Users Use the Application?

First make API servers stateless. Load balancer → API1/API2/API3. Move state to Redis, MongoDB, S3, Queue. Then horizontally scale each layer independently.

---

## 68. Database Indexing Questions

| Collection | Indexes |
| --- | --- |
| Users | `email` unique |
| Templates | `userId + id`, `userId + updatedAt` |
| Resumes | `userId + id` |
| Logs | `userId + createdAt` |
| Refresh tokens | `jti`, `userId`, `expiresAt` (potential TTL) |

---

## 69. Why nanoid?

Used for template IDs, refresh JTI, suggestion IDs.

Benefits: compact, random, URL-safe, doesn't expose sequential database IDs. Sequential IDs reveal ordering and can make enumeration easier.

---

## 70. Resume Tailor Session Cache

Session key: `userId:kind:id`. TTL: 7 days.

Tailoring is multi-step: start → suggestions → approve/reject → edit → compile. We need state between requests.

Memory cache → speed. MongoDB → persistence.

---

## 71. Why MongoDB + Memory Cache?

Memory cache alone would disappear after deployment, restart, or crash. Mongo remains the durable source.

Cache miss → Mongo → populate cache.

---

## 72. What Happens When Two Users Modify the Same Template?

Templates are user-owned. For concurrent updates by the same user, current read-modify-write has a race (lost update).

**Production:** optimistic locking with a `version` field: `WHERE id = X AND version = 1`, then increment to 2.

---

## 73. How Would You Monitor This System?

**Metrics:** API latency, 5xx/4xx, LLM latency, LLM quota failures, draft success/failure rate, Mongo latency, queue depth.

**Logs:** Structured JSON with `requestId`, `userId`, `endpoint`, `latency`, `status`.

**Tracing:** Request → API → Mongo → LLM → IMAP. OpenTelemetry would be useful.

---

## 74. What Is the Biggest Architectural Trade-off?

The project intentionally chooses **simplicity over premature scalability**.

Current implementation avoids Kafka, Redis, S3, workers, and microservices because they add operational complexity.

Express + MongoDB + in-memory cache + IMAP is enough for the current scale. But the code boundaries make future extraction possible.

That's a strong HLD answer.

---

## 75. Interviewer Rapid-Fire Cross Questions

| Interviewer question | Short answer |
| --- | --- |
| Why MongoDB? | Flexible document structure and dynamic CSV fields |
| Why not PostgreSQL? | Relational DB gives stronger constraints but isn't necessary for current document-heavy model |
| Why JWT? | Stateless short-lived access authentication |
| Why refresh DB? | Revocation and token reuse detection |
| Why HTTP-only cookie? | Prevent JS from directly reading refresh token |
| Why access token memory? | Reduce persistence/XSS exposure |
| Why rotate refresh token? | Prevent reuse of stolen tokens |
| Why AsyncLocalStorage? | Cross-cutting request context |
| Why React? | Rich interactive state |
| Why Express? | Simple API/middleware architecture |
| Why IMAP? | Direct draft append to mailbox |
| Why not Gmail API? | IMAP was simpler; Gmail API/OAuth is better production direction |
| Why not send directly? | User should review AI-generated email |
| Why individual send calls? | Independent per-recipient UI state |
| Why 250ms delay? | Basic throttling |
| Why no queue? | Current scale doesn't justify it |
| What at 10M emails? | Queue + workers + distributed rate limiter |
| Why Mongo binary? | Simplicity at current scale |
| What for large PDFs? | S3/object storage |
| Why AI abstraction? | Provider independence |
| Why deterministic JD score? | Reproducibility and low cost |
| Why LLM suggestions? | Natural language optimization |
| How protect LaTeX? | Parser + allowlist + backups |
| Why replay suggestions? | Safer than inverse edits |
| How prevent tenant leakage? | Every store query scopes by userId |
| How prevent path traversal? | Resolved path must remain under project root |
| How handle AI failure? | Fallback where possible + structured errors |
| How handle Gmail failure? | Mark failed and return 502 |
| How handle duplicate retry? | Current gap; production use idempotency keys |
| How scale rate limiter? | Redis |
| How scale cache? | Redis |
| How scale API? | Stateless horizontal replicas |
| How scale bulk? | Queue + workers |
| Why not microservices? | Modular monolith is simpler at current scale |
| How split later? | Auth, enrichment, email, resume services |

---

## 76. Five Questions You Should Proactively Mention

1. **"Why didn't you use Kafka?"** Current workload doesn't justify it. If bulk volume grows, I'd introduce a queue and workers.

2. **"Why MongoDB for PDFs?"** Simplicity. At larger scale, binary content to S3, metadata in MongoDB.

3. **"Why two LLM providers?"** Flexibility, cost/performance, fallback. Multimodal → Gemini.

4. **"How do you prevent AI from damaging the resume?"** AI never directly controls filesystem or arbitrary LaTeX. Suggestions + deterministic validation + controlled editor.

5. **"What would you change for production?"** S3 + Redis + queue/workers + Gmail OAuth/API + idempotency + distributed rate limiting + observability + horizontal API scaling.

That is probably the most important answer to memorize.

---

## 77. Best 5-Minute Architecture Answer

If the interviewer gives you only 5 minutes:

```text
1. Problem
   ↓
2. Architecture
   ↓
3. Authentication
   ↓
4. Core email flow
   ↓
5. AI architecture
   ↓
6. Resume tailoring
   ↓
7. Database / multi-tenancy
   ↓
8. Scalability
   ↓
9. Trade-offs
```

Finish with:

> The main design principle was to keep the core application simple and synchronous where the workload is small, while isolating the expensive or asynchronous parts such as AI enrichment, bulk email, and file processing so they can later move behind queues and workers.

---

# Part 2 — Every Feature and Database Schema

You should be able to explain every major feature from **UI → API → service → database → external system**, then explain the schema and relationships.

---

## 1. Complete ColdMail Architecture

```text
                         React SPA
                            │
                     Axios / REST APIs
                            │
                            ▼
                 ┌─────────────────────┐
                 │   Express Backend   │
                 │ Auth / AI / Rate    │
                 │ Limiter / Validation│
                 └──────────┬──────────┘
                            │
        ┌───────────────────┼────────────────────┐
        ▼                   ▼                    ▼
    MongoDB              AI Layer             Gmail
        │              Gemini / Groq          IMAP
        ├── Users
        ├── Refresh Tokens
        ├── Templates
        ├── Resumes
        ├── Sent Logs
        └── Tailor Sessions
```

---

## 2. Database Schema

The main collections are: `users`, `refresh_tokens`, `templates`, `resumes`, `sent_log`, `tailor_sessions`.

---

## 3. `users` Collection

Purpose: authentication and user profile.

```json
{
  "_id": "ObjectId",
  "id": "usr_123",
  "email": "user@gmail.com",
  "name": "Parwez",
  "passwordHash": "$2b$10$...",
  "createdAt": "...",
  "updatedAt": "..."
}
```

| Field | Purpose |
| --- | --- |
| `id` | Application-level user ID |
| `email` | Login identity |
| `name` | User name |
| `passwordHash` | bcrypt password |
| `createdAt` / `updatedAt` | Account timestamps |

**Index:** `email` UNIQUE — login does email → find user → `bcrypt.compare()`.

---

## 4. `refresh_tokens` Collection

```json
{
  "_id": "ObjectId",
  "jti": "abc123xyz",
  "userId": "usr_123",
  "revoked": false,
  "expiresAt": "...",
  "createdAt": "..."
}
```

**Relationship:** User 1 ───────── N RefreshTokens (laptop, phone, browser).

**Indexes:** `jti`, `userId`, `expiresAt` (potential TTL so expired sessions disappear).

---

## 5. Authentication Database Flow

**Login:** `POST /auth/login` → users → bcrypt → refresh_tokens → JTI → Access JWT + Refresh JWT.

**Refresh:**

```text
refresh cookie
     ↓
verify JWT
     ↓
extract JTI
     ↓
refresh_tokens.find(jti)
     ↓
revoked?
   /     \
 yes      no
  │        │
 revoke    rotate
 all       token
 sessions
```

---

## 6–8. `templates` Collection and APIs

```json
{
  "id": "tpl_123",
  "userId": "usr_123",
  "name": "SDE Referral",
  "subject": "Referral for {{role}} at {{company}}",
  "body": "Hi {{name}}, ...",
  "tags": ["referral", "software-engineer", "java"],
  "isDefault": true,
  "createdAt": "...",
  "updatedAt": "..."
}
```

**Relationship:** User 1 ───────── N Templates.

Every query uses authenticated `userId`. APIs: `POST/GET/PUT/DELETE /templates`, `POST /templates/suggest-tags`.

Create: validate → normalize tags → nanoid → add userId → MongoDB.

Update: read-modify-write (`find` → modify → `replaceOne`).

Delete: always `id + userId`, not just `id`.

---

## 9. Default Template

A user should have one default template. Set T2 default → T1/T3 false.

Application-level. Potential race if two requests set different defaults. For stronger guarantees: transaction or uniqueness strategy.

---

## 10–14. `resumes` Collection

```json
{
  "id": "resume_123",
  "userId": "usr_123",
  "name": "Parwez_Resume.pdf",
  "content": "Binary(...)",
  "mimeType": "application/pdf",
  "size": 523421,
  "tags": ["java", "spring-boot", "kafka"],
  "tailoredFor": { "jdHash": "...", "preview": "...", "scoreDelta": {} },
  "createdAt": "...",
  "updatedAt": "..."
}
```

**Upload:** React multipart → Multer memory → validate PDF, size ≤ 10 MB → optional AI tagging → MongoDB Binary.

**Why `list()` excludes PDF content:** `GET /resumes` projects `{ content: 0 }`. `GET /resumes/:id` streams the PDF. Good optimization.

**Download:** Bearer token required. GET → Mongo → PDF bytes → Blob → `URL.createObjectURL()`.

**Replace:** same resume ID + new PDF. References don't break.

---

## 15–16. `sent_log` Collection

```json
{
  "id": "log_123",
  "userId": "usr_123",
  "email": "recruiter@google.com",
  "name": "John",
  "company": "Google",
  "subject": "SDE Referral",
  "status": "drafted",
  "messageId": "imap-12345",
  "error": null,
  "createdAt": "..."
}
```

Status: `drafted` or `failed`.

**Best-effort logging:** If Gmail draft succeeds but Mongo log fails, the user should not see "Email failed." Critical = Gmail draft. Non-critical = Mongo log. Log failure is swallowed.

---

## 17–19. `tailor_sessions` Collection

Contains `kind` (`resume` or `template`), `targetId`, `jd`, `original`, `suggestions[]` (id, target, originalText, suggestion, status), `scores` (before/after), `expiresAt`.

**Flow:** `POST /tailor/session` → read resume → parse LaTeX → baseline score → LLM suggestions → suggestion queue → MongoDB. Then approve / reject / edit.

---

## 20–25. Resume and Template Tailoring Detail

Parser extracts known macros. Score = unigram + bigram + ATS heuristics (action verb, numbers, percentage, bullet count, length) — **not AI**.

LLM targets restricted: `paragraph:1`, `subject`. Must not modify HTML tags, styles, links, lists, `{{tokens}}`. AI output is untrusted input.

---

## 26–30. Enrichment

**Email names:** `POST /enrich/names` → LLM → restore original order → fallback (split `.` `_` `-`, strip digits, skip aliases like hr/sales/support).

**LinkedIn → email:** parse slug → name → domain → LLM patterns → dedupe → MX lookup → confidence filter.

Candidates are **not** a separate collection. Generated for the current workflow. Avoids persisting inaccurate inferred data.

---

## 31–33. Job Intake, JD Match, AI Layer

Job URL fetch with 20s timeout, strip scripts, 80k cap, LLM extraction. User company > AI company.

JD match sends slim `{ id, name, tags }`. Server checks returned IDs exist in request Set so LLM cannot invent IDs.

No AI provider collection. `X-AI-Provider` / `X-AI-Model` + AsyncLocalStorage → `generateStructuredJson()` → adapter.

---

## 34. Complete Feature → Database Mapping

| Feature | Main storage |
| --- | --- |
| Authentication | `users` |
| Sessions | `refresh_tokens` |
| Templates | `templates` |
| Resume library | `resumes` |
| Draft history | `sent_log` |
| Resume tailoring | `tailor_sessions` |
| Email / LinkedIn enrichment | Temporary response / cache |
| JD matching | Temporary request / response |
| AI provider selection | Request context |
| Theme | Browser local state |
| Access token | Browser memory |
| Refresh token | HTTP-only cookie |

---

## 35. Complete API Architecture

```text
/api
├── /auth          signup, login, refresh, logout, me, password
├── /templates     GET, POST, PUT, DELETE, suggest-tags
├── /resumes       GET, POST, PUT, DELETE, /:id
├── /enrich        names, email, jd-match, job-intake
├── /tailor        session, decide, compile
├── /send-email
├── /send-bulk
├── /preview
└── /log
```

---

## 36. Request Pipeline

```text
Client → Express → Helmet → CORS → JSON parser → Cookie parser
      → AI context → Authentication → User context → Rate limiter
      → Route → Service → Store / External API → Response
```

That's the architecture you should draw.

---

## 37. Compose Full Flow

1. User uploads CSV (`email,company,role`).
2. Browser parses CSV.
3. User chooses Template + Resume.
4. Each row becomes `{ email, extra: { company, role } }`.
5. `POST /send-email`.
6. Backend validates.
7. Resolve attachment (uploaded file wins over resumeId).
8. Build variables.
9. Render `{{name}}`, `{{company}}`, `{{role}}`.
10. Create RFC822 message.
11. Connect to Gmail IMAP.
12. Find Drafts mailbox.
13. Append `\Draft`.
14. Log `drafted`.
15. Return `{ status, messageId }`.

---

## 38. Why Gmail Drafts Instead of Your Own Email Database?

Database stores metadata/log. Gmail stores the actual email draft. Avoids building your own inbox. **Source of truth for the actual email is Gmail.**

---

## 39. Database Relationship Diagram

```text
                         ┌───────────────┐
                         │     USERS     │
                         │ id PK         │
                         │ email UNIQUE  │
                         └───────┬───────┘
                                 │
             ┌───────────────────┼──────────────────┐
             ▼                   ▼                  ▼
   REFRESH_TOKENS          TEMPLATES            RESUMES
   jti, userId             id, userId           id, userId
   revoked, expiresAt      subject, body        content, tags
                           tags, isDefault            │
                                                      ▼
                                              TAILOR_SESSIONS
                                              kind, targetId
                                              suggestions, scores

                         SENT_LOG
                         email, status, messageId
```

---

## 40. Is This Actually Relational?

**No.** It's MongoDB. When you write `userId FK` on a whiteboard, clarify: `userId` is a **logical reference**, not a database-enforced foreign key. The application enforces it.

---

## 41–44. Store Factory and Collection Design

`createCollection("templates")` standardizes multi-tenant `create/list/update` with `userId`.

Resumes and tailor sessions don't use the same store (binary content, kind, TTL). Abstraction only where behavior is shared.

**Why not one giant users document?** Document growth, large updates, concurrency, poor querying, Mongo 16MB limit, hard independent indexing.

**Why not one generic `items` collection?** Every query needs `type + userId`; validation becomes complicated. Separate collections = clearer schema, better indexes, simpler code.

---

## 45. How Would You Index the Database?

| Collection | Indexes |
| --- | --- |
| `users` | unique(`email`) |
| `refresh_tokens` | unique(`jti`), `userId`, `expiresAt` TTL |
| `templates` | `userId + updatedAt` |
| `resumes` | `userId + updatedAt` |
| `sent_log` | `userId + createdAt` |
| `tailor_sessions` | `userId + kind + targetId`, `expiresAt` TTL |

---

## 46–49. Duplicates, Lost Updates, Idempotency

Template names are **not** globally unique. Uniqueness is `userId + templateId`. Optional: unique(`userId`, `name`) if product required it.

Current send has a retry gap: timeout → retry → two drafts. Production: `Idempotency-Key` with unique index on `userId + idempotencyKey`. Enforce **before** creating the draft.

Concurrent template writes: lost-update. Production: optimistic locking (`version`) → 409 Conflict.

---

## 50–58. Scaling Cross-Questions

**Resume storage at scale:** Mongo metadata + `s3Key` → S3. Download via signed URL.

**AI at scale:** API → AI job queue → workers → Gemini/Groq + Redis cache for repeated enrichment (`company + domain`).

**LLM rate limits:** provider quota → rate limiter → queue → workers. Exponential backoff for transient failures. Don't retry quota exhaustion aggressively.

**Gmail rate limits:** Queue → worker → distributed rate limiter. 429 → backoff. Permanent errors → failed + DLQ.

**Partial bulk failure:** Don't rollback 90 successful drafts. Each recipient is independent. Store `jobId`, `recipientId`, `status`, `error`, `messageId`. Retry only failures.

**Server crash during bulk:** Current in-process loop loses remaining recipients. Production: bulk job → queue → per-recipient messages. Worker crash → message available again.

**Why queue instead of cron?** Cron = "run every hour." Queue = "process these 100,000 jobs" with retries, ack, concurrency, backpressure, worker scaling.

**Observability:** `requestId`, `userId`, endpoint, latency, status; AI provider/model/tokens; email recipient/draft status; P50/P95/P99, 5xx, AI failure rate, queue depth, Mongo latency.

**Deploy:** Internet → Load Balancer → Node.js → MongoDB / Redis / AI APIs. Static React on CDN.

---

## 59. What Is Your Biggest Weakness?

Don't say "Everything is scalable."

> The current implementation is intentionally optimized for simplicity rather than high-volume distributed processing. The main scaling limitations are in-memory rate limiting/cache, MongoDB binary storage, synchronous IMAP draft creation, and lack of a queue for bulk jobs.

Then immediately: Redis + S3 + Queue/Workers + Gmail API/OAuth + Idempotency.

---

## 60. One Complete Interview Answer to Memorize

If interviewer asks: "Explain the architecture and implementation of your ColdMail project."

> The system is a modular monolith consisting of a React SPA and Node.js/Express backend, backed by MongoDB. The major modules are authentication, templates, resume management, enrichment, JD matching, resume tailoring and email draft creation.
>
> On every request, middleware first establishes the AI context and then authenticates the user. Authentication uses a 15-minute access JWT stored only in memory and a rotating HTTP-only refresh token stored in a cookie. Refresh tokens have a JTI stored in MongoDB, which allows revocation and reuse detection.
>
> For multi-tenancy, every persistent entity contains a userId, but the important part is that userId always comes from the authenticated request context rather than the request body. The store layer automatically adds userId to queries, which prevents cross-user data access.
>
> Templates are stored in a templates collection, resumes in a resumes collection, sent email results in sent_log, authentication sessions in refresh_tokens, and multi-step resume/template tailoring workflows in tailor_sessions.
>
> For compose, the application supports email IDs, CSV and LinkedIn inputs. CSV is parsed in the browser, email IDs are enriched using an LLM with a deterministic fallback, and LinkedIn profiles are converted into candidate email patterns followed by domain MX validation.
>
> Once the user selects a template and resume, the backend builds a variable map containing recipient information and CSV fields, renders the subject and body, creates an RFC822 message using MailComposer and appends it to Gmail's Drafts mailbox using IMAP. I intentionally save drafts instead of directly sending because AI-generated emails should be reviewed by the user.
>
> For AI, I created a provider-independent interface called generateStructuredJson. Features don't directly depend on Gemini or Groq. The abstraction handles structured JSON output, model selection and multimodal requests. Multimodal PDF processing is routed to Gemini because the current Groq integration doesn't support the required PDF parts.
>
> Resume tailoring is intentionally split into deterministic and AI parts. The parser extracts known LaTeX structures, deterministic scoring calculates JD similarity and ATS signals, and the LLM generates suggestions. Suggestions are then approved, rejected or edited by the user. Before modifying LaTeX, the system creates backups and performs brace-balanced changes. If an earlier suggestion changes, the system restores the original file and replays all currently approved suggestions instead of trying to reverse arbitrary text edits.
>
> The current system favors simplicity. It uses MongoDB binary storage, an in-memory cache and rate limiter, and synchronous IMAP operations. If traffic increases significantly, I would move PDFs to S3, use Redis for distributed cache and rate limiting, introduce a queue and workers for bulk email and AI processing, use idempotency keys for retries, and move from IMAP credentials toward Gmail OAuth/API.
>
> So the overall design principle is: keep the current system simple, isolate business modules, treat AI as an untrusted suggestion layer, enforce security and tenant isolation in the backend, and introduce distributed infrastructure only where scale requires it.
