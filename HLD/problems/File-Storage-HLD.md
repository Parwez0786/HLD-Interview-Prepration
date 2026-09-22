# File Storage System — HLD Interview

Assume the interviewer asks:

> Design a file storage system like Google Drive / Dropbox where users can upload, download, and manage files.

---

## Table of Contents

- [1. Start by Clarifying Requirements](#1-start-by-clarifying-requirements)
- [2. High-Level Architecture](#2-high-level-architecture)
- [3. Why Object Storage?](#3-why-object-storage)
- [4. Upload Flow](#4-upload-flow)
- [5. Why Pre-Signed URLs?](#5-why-pre-signed-urls)
- [6. Large File Upload](#6-large-file-upload)
- [7. Resumable Upload](#7-resumable-upload)
- [8. Download Flow](#8-download-flow)
- [9. Metadata Database](#9-metadata-database)
- [10. Folder Table](#10-folder-table)
- [11. Sharing](#11-sharing)
- [12. Public Sharing](#12-public-sharing)
- [13. Object Storage Key Design](#13-object-storage-key-design)
- [14. Database vs Object Storage](#14-database-vs-object-storage)
- [15. Consistency](#15-consistency)
- [16. Handling Failed Uploads](#16-handling-failed-uploads)
- [17. Delete Flow](#17-delete-flow)
- [18. Why Kafka?](#18-why-kafka)
- [19. Complete Architecture](#19-complete-architecture)
- [20. Caching](#20-caching)
- [21. Database Scaling](#21-database-scaling)
- [22. Sharding](#22-sharding)
- [23. CDN for Downloads](#23-cdn-for-downloads)
- [24. Security](#24-security)
- [25. Virus Scanning](#25-virus-scanning)
- [26. File Deduplication](#26-file-deduplication)
- [27. Versioning](#27-versioning)
- [28. Capacity Estimation](#28-capacity-estimation)
- [29. Availability](#29-availability)
- [30. Durability vs Availability](#30-durability-vs-availability)
- [31. API Design](#31-api-design)
- [32. What Happens if Upload Succeeds but DB Update Fails?](#32-what-happens-if-upload-succeeds-but-db-update-fails)
- [33. What if DB Says File Exists but Object Is Missing?](#33-what-if-db-says-file-exists-but-object-is-missing)
- [34. Rate Limiting](#34-rate-limiting)
- [35. Monitoring](#35-monitoring)
- [36. Final Interview Summary](#36-final-interview-summary)
- [Most Important Follow-ups to Prepare](#most-important-follow-ups-to-prepare)
- [Interviewer's Mental Model](#interviewers-mental-model)
- [HLD Follow-up Answers](#file-storage-system--hld-follow-ups)
- [Control Plane vs Data Plane](#the-most-important-concept-control-plane-vs-data-plane)

---

## 1. Start by Clarifying Requirements

I would first ask:

### Functional requirements

- User can upload a file
- User can download a file
- User can delete a file
- User can list files in a folder
- User can create folders
- User can share files/folders with other users
- Support large files, e.g. up to 10 GB
- Files should be available across devices

### Optional

- File versioning
- Trash / recycle bin
- File search
- Resumable uploads

### Non-functional requirements

- High availability
- Highly durable storage
- Low download latency
- Scalable to millions / billions of files
- Secure access
- Strong consistency for metadata
- Eventual consistency is acceptable for some asynchronous operations
- Resumable uploads for large files

I would explicitly say:

> For this interview, I'll focus on upload, download, metadata management, folders, sharing and large-file support. I'll treat file content and metadata as separate concerns.

That is an important design decision.

---

## 2. High-Level Architecture

The most important idea:

**Do NOT store actual files inside MySQL / PostgreSQL.**

Store:

| What | Where |
| --- | --- |
| File metadata | Database |
| Actual file bytes | Object Storage |

Examples of object storage:

- Amazon S3
- Google Cloud Storage
- Azure Blob Storage

### Architecture

```text
                         ┌───────────────┐
                         │    Client     │
                         │ Web / Mobile  │
                         └───────┬───────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  API Gateway /  │
                        │ Load Balancer   │
                        └────────┬────────┘
                                 │
               ┌─────────────────┼─────────────────┐
               │                 │                 │
               ▼                 ▼                 ▼
        ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
        │ File        │   │ Metadata    │   │ Sharing     │
        │ Service     │   │ Service     │   │ Service     │
        └──────┬──────┘   └──────┬──────┘   └──────┬──────┘
               │                 │                 │
               │                 ▼                 │
               │          ┌─────────────┐          │
               │          │ PostgreSQL  │          │
               │          │ / MySQL     │          │
               │          └─────────────┘          │
               │                                   │
               ▼                                   ▼
        ┌─────────────────────────────────────────────────┐
        │                  Object Storage                  │
        │                    S3 / Blob                     │
        │                                                  │
        │          Actual File Contents                   │
        └─────────────────────────────────────────────────┘
```

---

## 3. Why Object Storage?

This is one of the first things an interviewer may ask.

### Bad approach

```text
Application Server
       ↓
PostgreSQL
       ↓
BLOB
       ↓
10 GB file
```

This creates several problems:

- Database becomes huge
- Expensive storage
- Database backup becomes difficult
- Database I/O gets affected
- Scaling becomes difficult
- Application servers become bottlenecks

### Better approach

```text
             Metadata
                ↓
           PostgreSQL

             File
                ↓
          Object Storage
```

Object storage is designed for:

- Large files
- High durability
- Massive scale
- Cheap storage
- Parallel access
- Lifecycle management

---

## 4. Upload Flow

This is one of the most important parts of the interview.

Suppose the user uploads:

- `resume.pdf`
- size = 20 MB

### Naive approach

```text
Client
  ↓
File Server
  ↓
Object Storage
```

The application server becomes part of the data path.

For large files, this is inefficient.

Instead, use **pre-signed URLs**.

### Upload flow using pre-signed URL

#### Step 1

Client asks:

```http
POST /files/upload/initiate
```

Request:

```json
{
  "fileName": "resume.pdf",
  "size": 20971520,
  "contentType": "application/pdf"
}
```

#### Step 2

File Service generates:

- `fileId = 12345`
- `objectKey = users/101/files/12345`

and generates a pre-signed upload URL.

Response:

```json
{
  "fileId": "12345",
  "uploadUrl": "https://object-storage/..."
}
```

#### Step 3

Client directly uploads:

```text
Client
   │
   │ PUT file
   ▼
Object Storage
```

The backend doesn't transfer 20 MB.

#### Step 4

After successful upload:

```http
POST /files/12345/complete
```

Backend verifies the upload and updates metadata.

```text
UPLOADING
     ↓
UPLOADED
```

---

## 5. Why Pre-Signed URLs?

**Interviewer:** Why don't you send the file through your backend?

**Answer:**

> Because file content is much larger than metadata. If all uploads pass through application servers, network bandwidth and CPU become bottlenecks. With pre-signed URLs, the backend handles authentication and metadata while the client uploads directly to object storage.

This is a very important HLD interview point.

---

## 6. Large File Upload

Suppose the file is **10 GB**.

We don't want:

```text
10 GB → single request
```

Instead use **multipart / chunked upload**.

```text
10 GB file
       ↓
Chunk 1 → 10 MB
Chunk 2 → 10 MB
Chunk 3 → 10 MB
...
Chunk N
```

### Architecture

```text
Client
  │
  ├── Chunk 1 ────────► Object Storage
  ├── Chunk 2 ────────► Object Storage
  ├── Chunk 3 ────────► Object Storage
  ├── Chunk 4 ────────► Object Storage
  │
  ▼
Complete Upload
```

### Advantages

- Parallel upload
- Faster upload
- Resume failed uploads
- Only failed chunks need retry
- Better handling of unstable networks

---

## 7. Resumable Upload

Suppose:

- 1 GB file
- Uploaded: 700 MB
- Then network fails

**Without resumable upload:** start again from 0. Bad.

**With chunking:**

```text
Chunk 1  ✓
Chunk 2  ✓
Chunk 3  ✓
...
Chunk 70 ✓
Chunk 71 ✗
```

Only chunk 71 is retried.

We maintain upload state:

- `uploadId`
- `fileId`
- `uploadedChunks`
- `totalChunks`
- `status`

---

## 8. Download Flow

Client:

```http
GET /files/12345/download
```

Backend:

1. Authenticate user
2. Check authorization
3. Fetch metadata
4. Generate pre-signed download URL
5. Return URL

```text
Client
   │
   │ GET /files/12345/download
   ▼
File Service
   │
   ├── Check Auth
   ├── Check Permission
   └── Generate URL
          │
          ▼
     Object Storage
```

Then:

```text
Client ───────────────► Object Storage
             File
```

Again, the application server doesn't stream the entire file.

---

## 9. Metadata Database

We need to store information about files.

### File table

| Column | Description |
| --- | --- |
| `file_id` | Unique file identifier |
| `user_id` | Owner |
| `folder_id` | Parent folder |
| `name` | Display name |
| `object_key` | Key in object storage |
| `size` | File size in bytes |
| `content_type` | MIME type |
| `checksum` | Integrity hash |
| `status` | Lifecycle state |
| `created_at` | Created timestamp |
| `updated_at` | Updated timestamp |
| `deleted_at` | Soft-delete timestamp |
| `version` | Current version |

### Example

| Field | Value |
| --- | --- |
| `file_id` | 1001 |
| `user_id` | 501 |
| `folder_id` | 20 |
| `name` | `resume.pdf` |
| `object_key` | `501/1001/resume.pdf` |
| `size` | 20 MB |
| `content_type` | `application/pdf` |
| `checksum` | `abc123` |
| `status` | `ACTIVE` |

---

## 10. Folder Table

| Column | Description |
| --- | --- |
| `folder_id` | Unique folder identifier |
| `user_id` | Owner |
| `parent_folder_id` | Parent folder (`NULL` for root) |
| `name` | Folder name |
| `created_at` | Created timestamp |
| `updated_at` | Updated timestamp |

### Example

```text
My Drive
   │
   ├── Documents
   │      ├── Resume.pdf
   │      └── Resume2.pdf
   │
   ├── Photos
   │      ├── image1.jpg
   │      └── image2.jpg
   │
   └── Videos
```

We can represent this using `parent_folder_id`.

---

## 11. Sharing

Suppose:

- User A owns `resume.pdf`
- User A wants to share it with User B

### FilePermission

| Column | Description |
| --- | --- |
| `file_id` | Shared file |
| `user_id` | User who received access |
| `permission` | Access level |
| `created_at` | Created timestamp |
| `expires_at` | Optional expiry |

### Example

| Field | Value |
| --- | --- |
| `file_id` | 1001 |
| `user_id` | 2001 |
| `permission` | `VIEW` |

Permissions could be:

- `VIEW`
- `EDIT`
- `OWNER`

---

## 12. Public Sharing

For a public link:

```text
https://storage.com/share/abc123
```

We should **NOT** expose the actual object-storage key.

### ShareLink

| Column | Description |
| --- | --- |
| `share_id` | Unique share identifier |
| `file_id` | Shared file |
| `token_hash` | Hashed token (never store raw token) |
| `created_by` | Owner who created the link |
| `expires_at` | Expiry time |
| `permission` | Access level |

### Example

| Field | Value |
| --- | --- |
| `share_id` | 5001 |
| `token` | `abcXYZ123` |
| `file_id` | 1001 |
| `expires_at` | ... |

When someone accesses the link:

```text
Share URL
   ↓
File Service
   ↓
Validate token
   ↓
Check expiry
   ↓
Generate pre-signed URL
   ↓
Object Storage
```

---

## 13. Object Storage Key Design

Don't use:

```text
resume.pdf
```

because millions of users may have the same filename.

Instead:

```text
bucket/
   userId/
      fileId/
         version/
             object
```

Example:

```text
files/
   1001/
      98765/
         v1
```

or:

```text
files/1001/98765/abc123
```

The important thing is that the **object key should be unique and independent of filename**.

---

## 14. Database vs Object Storage

A very good interview answer:

| Data | Storage |
| --- | --- |
| File content | Object Storage |
| Filename | SQL DB |
| File size | SQL DB |
| Owner | SQL DB |
| Folder | SQL DB |
| Permissions | SQL DB |
| Object key | SQL DB |
| Checksum | SQL DB |
| File versions | SQL DB + Object Storage |

---

## 15. Consistency

We need to distinguish between metadata and file content.

### Metadata

Should generally be **strongly consistent**.

Example: user deletes `resume.pdf`. The UI should not immediately show it as active.

Use a SQL transaction for metadata updates.

### File content

Object storage provides strong durability / availability characteristics, but our metadata state machine still needs to handle upload failures.

For example:

- `UPLOADING`
- `UPLOADED`
- `FAILED`
- `DELETED`

---

## 16. Handling Failed Uploads

Suppose:

```http
POST /upload/initiate
```

succeeds, but the client never uploads the file.

We now have:

```text
file status = UPLOADING
```

forever.

### Solution

Run a background cleanup job.

```text
Scheduler
    ↓
Find uploads older than X hours
    ↓
Delete incomplete objects
    ↓
Mark metadata FAILED
```

---

## 17. Delete Flow

User:

```http
DELETE /files/123
```

We shouldn't immediately permanently delete the object.

Use **soft delete**.

```text
ACTIVE
   ↓
TRASHED
   ↓
PERMANENTLY_DELETED
```

Database:

```text
deleted_at = timestamp
```

Then asynchronous cleanup:

```text
Kafka
  ↓
Deletion Worker
  ↓
Object Storage
  ↓
Delete object
```

---

## 18. Why Kafka?

Suppose the user deletes 100 files.

We don't want:

```text
DELETE API
   ↓
Delete DB
   ↓
Delete S3
   ↓
Wait
```

Instead:

```text
DELETE API
    ↓
Update DB
    ↓
Publish FileDeleted event
    ↓
Return response
```

Then:

```text
Kafka
  ↓
Deletion Worker
  ↓
Object Storage
```

This makes deletion asynchronous.

Kafka can also be used for:

- Virus scanning
- Thumbnail generation
- Metadata extraction
- Search indexing
- Notifications
- Audit logs

---

## 19. Complete Architecture

Now I would draw this in the interview:

```text
                         ┌──────────────┐
                         │    Client    │
                         └──────┬───────┘
                                │
                                ▼
                       ┌──────────────────┐
                       │ API Gateway / LB │
                       └────────┬─────────┘
                                │
                ┌───────────────┼────────────────┐
                │               │                │
                ▼               ▼                ▼
        ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
        │ File Service │ │ Metadata     │ │ Sharing      │
        │              │ │ Service      │ │ Service      │
        └──────┬───────┘ └──────┬───────┘ └──────┬───────┘
               │                │                │
               │                ▼                │
               │         ┌──────────────┐        │
               │         │ PostgreSQL   │        │
               │         └──────────────┘        │
               │                                 │
               ▼                                 │
       ┌──────────────────┐                      │
       │ Object Storage   │◄─────────────────────┘
       │ S3 / Blob        │
       └──────────────────┘
               ▲
               │
               │ Direct upload/download
               │
            Client


                    ┌──────────────┐
                    │    Kafka     │
                    └──────┬───────┘
                           │
             ┌─────────────┼──────────────┐
             ▼             ▼              ▼
        ┌──────────┐ ┌───────────┐ ┌─────────────┐
        │ Deletion │ │ Virus     │ │ Search      │
        │ Worker   │ │ Scanner   │ │ Indexer     │
        └──────────┘ └───────────┘ └─────────────┘
```

---

## 20. Caching

We can introduce Redis.

### Good candidates

- File metadata
- Permissions
- Share links
- Folder listings

Example:

```text
GET /files/123
       ↓
Redis
       ↓ cache miss
PostgreSQL
```

Be careful with permission caching because stale authorization information can become a security issue.

For highly sensitive permission changes, use **short TTL** or **explicit invalidation**.

---

## 21. Database Scaling

Suppose:

- 100 million users
- 10 billion files

A single PostgreSQL instance won't be enough.

We can use **read replicas**:

```text
                  ┌──────────────┐
                  │   Primary    │
                  └──────┬───────┘
                         │
              ┌──────────┴─────────┐
              ▼                    ▼
        Read Replica 1       Read Replica 2
```

- Writes → primary
- Reads → replicas

---

## 22. Sharding

At very large scale, shard by `user_id`.

For example:

- Shard 1 → users 1–10M
- Shard 2 → users 10M–20M
- Shard 3 → users 20M–30M

Or use consistent hashing:

```text
hash(user_id) → shard
```

This works well because most file operations are user-centric.

---

## 23. CDN for Downloads

For popular files:

```text
Client
   ↓
CDN
   ↓
Object Storage
```

Instead of:

```text
Client
   ↓
Object Storage
```

CDN helps with:

- Lower latency
- Reduced object-storage bandwidth
- Better performance for geographically distributed users

For private files, CDN URLs should be **signed and short-lived**.

---

## 24. Security

This is a major part of file storage design.

### Authentication

Use JWT / OAuth.

### Authorization

Before generating a download URL:

- Does the user own the file?
- **OR** does the user have permission?
- **OR** is it a valid public share link?

Only then generate a signed URL.

### Encryption

**At rest:**

```text
Object Storage
      ↓
Encryption
```

**In transit:** HTTPS / TLS

### Other protections

- Malware / virus scanning
- File type validation
- File size limits
- Rate limiting
- Audit logs
- Signed URLs
- Expiring links

---

## 25. Virus Scanning

Suppose a user uploads `malware.exe`.

We shouldn't immediately make it downloadable.

### Flow

```text
Upload
   ↓
Object Storage
   ↓
Kafka: FileUploaded
   ↓
Virus Scanner
   ↓
SAFE / INFECTED
```

Metadata:

```text
status = SCANNING
```

- If safe: `SCANNING → ACTIVE`
- If malicious: `SCANNING → QUARANTINED`

---

## 26. File Deduplication

Interviewer may ask:

> What if two users upload the exact same 1 GB file?

We can calculate `SHA-256(file)`.

Example:

- File A → hash = `abc123`
- File B → hash = `abc123`

Instead of storing two physical copies, object storage can maintain a single blob and metadata can reference it.

```text
File A ──┐
         ├──► Object abc123
File B ──┘
```

However, deduplication adds complexity around:

- Encryption
- Deletion
- Reference counting
- Privacy / isolation

So I would make it an **optional optimization**, not part of the initial design.

---

## 27. Versioning

Suppose `resume.pdf` is edited.

Don't overwrite immediately.

Store:

```text
file_id = 100
version = 1
object = abc
```

Then:

```text
file_id = 100
version = 2
object = xyz
```

### FileVersion

| Column | Description |
| --- | --- |
| `version_id` | Unique version identifier |
| `file_id` | Parent file |
| `version_number` | Incremental version |
| `object_key` | Object storage key for this version |
| `size` | Version size |
| `checksum` | Integrity hash |
| `created_at` | Created timestamp |

Current version: `version = 2`

Old versions can be deleted according to retention policy.

---

## 28. Capacity Estimation

In an interview, I'd make assumptions.

Suppose:

- 100M users
- 10% daily active = **10M active users/day**

Assume:

- 5 uploads / user / day
- Average file size = 10 MB

### Uploads / day

```text
10M × 5 = 50M files/day
```

### Storage / day

```text
50M × 10 MB = 500M MB ≈ 500 TB/day
```

That is huge. So immediately I would say:

> This scale strongly justifies object storage rather than database storage.

And we need lifecycle policies, compression where applicable, tiered storage, and possibly deduplication.

---

## 29. Availability

We don't want a single object-storage instance.

We want replication across availability zones.

```text
              Object
                │
       ┌────────┼────────┐
       ▼        ▼        ▼
      AZ-1     AZ-2     AZ-3
```

For disaster recovery, depending on requirements:

```text
Region A
   ↓
Region B
```

Cross-region replication can provide disaster recovery.

---

## 30. Durability vs Availability

Interviewer may ask: what's the difference?

| Term | Meaning |
| --- | --- |
| **Availability** | Can I access my file right now? |
| **Durability** | Will my file still exist in the future? |

For file storage, **durability is extremely important**.

You don't want:

```text
Uploaded file
     ↓
Disk failure
     ↓
File lost
```

Object storage handles replication and durability mechanisms for us.

---

## 31. API Design

| Action | Endpoint |
| --- | --- |
| Upload initialization | `POST /v1/files/upload/initiate` |
| Complete upload | `POST /v1/files/{fileId}/complete` |
| Download | `GET /v1/files/{fileId}/download` |
| Delete | `DELETE /v1/files/{fileId}` |
| List files | `GET /v1/folders/{folderId}/files` |
| Create folder | `POST /v1/folders` |
| Share | `POST /v1/files/{fileId}/permissions` |
| Create public link | `POST /v1/files/{fileId}/share-link` |

---

## 32. What Happens if Upload Succeeds but DB Update Fails?

This is a classic distributed-systems problem.

Suppose:

- Object Storage upload ✓
- Database update ✗

Now:

- File exists in object storage
- but DB doesn't know about it

This is an **orphan object**.

### Solution

Store state as `UPLOADING`, then:

```text
Object upload
       ↓
Complete API
       ↓
DB → ACTIVE
```

And run periodic reconciliation:

```text
Object Storage
       ↓
Find objects without metadata
       ↓
Cleanup / recover
```

We can also use object-storage events to notify the backend.

---

## 33. What if DB Says File Exists but Object Is Missing?

Opposite problem:

- DB → `ACTIVE`
- Object Storage → missing

Use:

- Object storage durability
- Replication
- Periodic consistency checker
- Object existence verification
- Restore from backup / replica if necessary

This is a very good follow-up answer.

---

## 34. Rate Limiting

We should prevent a user uploading 1000 files/sec.

Use a Redis-based rate limiter.

```text
User
 ↓
API Gateway
 ↓
Rate Limiter
 ↓
File Service
```

Limits can be:

- 100 upload requests / minute
- 10 GB / day

depending on product requirements.

---

## 35. Monitoring

We should monitor:

### API

- Request rate
- Latency
- Error rate

### Upload

- Upload failures
- Upload duration
- Average file size

### Object Storage

- Storage usage
- Read / write throughput
- Error rate

### Kafka

- Consumer lag
- Failed events

### Database

- CPU
- Connections
- Query latency
- Replication lag

Use:

- Prometheus
- Grafana
- ELK
- CloudWatch

---

## 36. Final Interview Summary

At the end, I would summarize the design like this:

> The key design decision is to separate metadata from file content. File metadata, folders and permissions are stored in a scalable SQL database, while the actual file bytes are stored in object storage such as S3. For uploads and downloads, the backend generates pre-signed URLs so clients communicate directly with object storage, preventing our application servers from becoming bandwidth bottlenecks. Large files use multipart resumable uploads. Kafka handles asynchronous operations such as virus scanning, deletion, indexing and notifications. Redis can cache frequently accessed metadata and permissions. As the system grows, we scale the stateless services horizontally, use database replicas and eventually shard metadata by user ID. Object storage provides high durability, while CDN and signed URLs improve download performance and security.

---

## Most Important Follow-ups to Prepare

If this is an SDE-2 HLD interview, I would expect these:

1. Why object storage instead of database?
2. Why pre-signed URLs?
3. How do you upload a 10 GB file?
4. How do resumable uploads work?
5. What happens if upload succeeds but DB update fails?
6. What happens if DB says uploaded but object is missing?
7. How do you handle concurrent uploads?
8. How do you prevent duplicate files?
9. How do you implement file sharing?
10. How do you implement public links securely?
11. How do you scale PostgreSQL?
12. How would you shard the metadata database?
13. How do you handle file versioning?
14. How do you delete files safely?
15. How do you implement trash / recycle bin?
16. How do you scan uploaded files for malware?
17. How do you support millions of downloads?
18. Where would you use Kafka?
19. Where would you use Redis?
20. How do you ensure durability?
21. How do you handle multi-region disaster recovery?
22. How would you reduce storage cost?
23. How would you design Dropbox-like synchronization?
24. How do you handle concurrent edits to the same file?
25. How do you calculate storage and bandwidth requirements?

---

## Interviewer's Mental Model

The easiest way to remember this system is:

```text
                 FILE STORAGE SYSTEM

                         Client
                           │
                           ▼
                    API Gateway/LB
                           │
                           ▼
                  ┌─────────────────┐
                  │ File Service    │
                  └───────┬─────────┘
                          │
             ┌────────────┴────────────┐
             │                         │
             ▼                         ▼
        Metadata DB               Object Storage
       PostgreSQL/MySQL              S3/Blob
             │                         ▲
             │                         │
             ▼                         │
           Redis                  Direct Upload
             │                         │
             ▼                         │
           Kafka ◄─────────────── Client
             │
       ┌─────┼──────┐
       ▼     ▼      ▼
    Scanner  Delete Search
```

### Core principle

| Component | Role |
| --- | --- |
| **DB** | "What is this file?" |
| **Object Storage** | "Where are the actual bytes?" |
| **Kafka** | "What should happen asynchronously?" |
| **Redis** | "What do we need quickly?" |
| **CDN** | "How do we serve downloads faster?" |

---

# File Storage System — HLD Follow-ups

Below are the 25 follow-up questions with interview-ready answers. The goal is not just to know the answer, but to know how to explain it to the interviewer in simple English.

---

## Follow-up 1. Why object storage instead of database?

**Interview answer:**

> I would store file metadata in PostgreSQL, but I would store the actual file content in object storage such as S3.

### PostgreSQL stores

| Column |
| --- |
| `file_id` |
| `file_name` |
| `size` |
| `owner_id` |
| `object_key` |
| `checksum` |
| `created_at` |

### Actual file

```text
S3
└── users/123/files/456
      └── 10 GB file
```

### Why?

Object storage is designed for:

- Very large files
- Massive storage
- High durability
- High throughput
- Cheap storage
- Horizontal scalability

If we store 10 GB files as DB BLOBs:

```text
Application
     ↓
PostgreSQL
     ↓
10 GB BLOB
```

the database becomes a bottleneck for:

- Storage
- Backup
- Replication
- I/O
- Network bandwidth

**One-line interview answer:**

> Database stores metadata; object storage stores large binary objects.

---

## Follow-up 2. Why pre-signed URLs?

Suppose the user wants to upload a 1 GB file.

### Without pre-signed URL

```text
Client
   ↓ 1 GB
Backend
   ↓ 1 GB
S3
```

Backend has to handle the entire 1 GB.

Now imagine 10,000 concurrent uploads. Huge bandwidth requirement.

### With pre-signed URL

```text
Client
   │
   │ Request upload permission
   ▼
Backend
   │
   │ Generate signed URL
   ▼
Client
   │
   │ Direct upload
   ▼
S3
```

Backend only handles:

- Authentication
- Authorization
- Metadata
- URL generation

S3 handles the actual file transfer.

**Interview answer:**

> Pre-signed URLs allow the client to directly upload or download from object storage without routing large file contents through our backend. This significantly reduces backend bandwidth and improves scalability.

---

## Follow-up 3. How do you upload a 10 GB file?

I would use **multipart upload**.

Break:

```text
10 GB
  ↓
1000 chunks × 10 MB
```

Then:

```text
Client
 ├── Chunk 1 ──→ S3
 ├── Chunk 2 ──→ S3
 ├── Chunk 3 ──→ S3
 ├── Chunk 4 ──→ S3
 └── ...
```

Chunks can be uploaded in parallel.

### Flow

```text
POST /upload/initiate
        ↓
uploadId created
        ↓
Generate signed URLs
        ↓
Upload chunks
        ↓
Complete multipart upload
        ↓
Mark file ACTIVE
```

### Why?

- Parallel upload
- Retry individual chunks
- Resume failed uploads
- Avoid huge single HTTP request

---

## Follow-up 4. How do resumable uploads work?

Suppose:

- 1 GB file
- 100 chunks

Already uploaded:

```text
Chunk 1  ✓
Chunk 2  ✓
...
Chunk 70 ✓
Chunk 71 ✗
...
Chunk 100 ✗
```

Network fails. We don't start again.

Client asks:

```http
GET /uploads/{uploadId}/status
```

Response:

```json
{
  "uploadedChunks": [1, 2, 3, 70]
}
```

Client resumes from chunks **71 → 100**.

**Interview answer:**

> I maintain an upload session identified by `uploadId`. Each chunk has a chunk number or part number. The client can query which chunks have successfully uploaded and retry only the missing chunks.

---

## Follow-up 5. What happens if upload succeeds but DB update fails?

This is a very important distributed-system problem.

Suppose:

- S3 upload ✓
- DB update ✗

Now:

- S3 → file exists
- DB → file doesn't exist

This is called an **orphan object**.

### Solution

Maintain upload state:

```text
INITIATED
    ↓
UPLOADING
    ↓
UPLOADED
    ↓
ACTIVE
```

If DB update fails, the object remains temporarily.

A background reconciliation / cleanup job periodically checks:

```text
S3 objects
     ↓
Find objects without valid metadata
     ↓
Delete after retention period
```

We can also use object-storage events.

### Important

Don't immediately delete the object.

Because the DB request might simply have failed temporarily.

Give it a **grace period**.

---

## Follow-up 6. What happens if DB says uploaded but object is missing?

Opposite problem:

- DB → `ACTIVE`
- S3 → missing

This is more serious because the user believes their file exists.

### Prevention

Use:

- Object storage replication
- Strong durability guarantees
- Checksums
- Background consistency checks
- Cross-region replication for critical data

A reconciliation job can periodically verify:

```text
DB file
   ↓
object_key
   ↓
Does object exist?
```

If missing:

- Mark file = `CORRUPTED`
- or recover it from another replica / version

**Interview answer:**

> The database is the source of truth for metadata, but object storage is the source of truth for file bytes. We periodically reconcile the two systems.

---

## Follow-up 7. How do you handle concurrent uploads?

Consider two requests:

- User A uploads `resume.pdf`
- User A uploads `resume.pdf`

at almost exactly the same time.

We need to define the product behavior.

### Option 1 — Allow both

Generate unique `fileId` and `objectKey` so both are independent files.

### Option 2 — Same logical file

Use a unique constraint such as:

```text
(user_id, folder_id, file_name)
```

if the product doesn't allow duplicate names.

But concurrency can still happen. Use:

- Database unique constraint
- Optimistic locking
- Idempotency keys

### Idempotency

Client sends:

```http
Idempotency-Key: abc123
```

If the same request arrives twice:

- Request 1 → process
- Request 2 → return existing result

**Interview answer:**

> For API-level duplicate requests, I would use idempotency keys. For business-level uniqueness, I would enforce a database unique constraint.

---

## Follow-up 8. How do you prevent duplicate files?

There are two meanings of duplicate.

### A. Duplicate request

Use `Idempotency-Key`.

### B. Same file content

Calculate `SHA-256(file)`.

Example:

- File A → SHA256 = `ABC123`
- File B → SHA256 = `ABC123`

Therefore they contain identical bytes.

We can store:

```text
File
    ↓
content_hash
    ↓
Blob
```

Multiple metadata records can point to the same physical blob.

```text
File A ──┐
         ├──→ Blob ABC123
File B ──┘
```

This is called **deduplication**.

### But be careful

Deduplication introduces complexity around:

- Encryption
- Deletion
- Reference counting
- Privacy

So I'd introduce it only if storage cost justifies the complexity.

---

## Follow-up 9. How do you implement file sharing?

Create a permissions table.

### FilePermission

| Column | Description |
| --- | --- |
| `file_id` | Shared file |
| `user_id` | User granted access |
| `permission` | Access level |
| `created_at` | Created timestamp |
| `expires_at` | Optional expiry |

Example:

| Field | Value |
| --- | --- |
| `file_id` | 101 |
| `user_id` | 200 |
| `permission` | `READ` |

Possible permissions:

- `OWNER`
- `EDITOR`
- `VIEWER`

### Flow

```text
User A
  ↓
Share file
  ↓
Sharing Service
  ↓
FilePermission
  ↓
User B can access
```

During download:

```text
User B
   ↓
Download file
   ↓
Check permission
   ↓
Generate signed URL
```

---

## Follow-up 10. How do you implement public links securely?

Don't expose:

```text
s3.com/my-private-file
```

Instead generate:

```text
https://files.com/share/X7ab92K
```

### ShareLink

| Column | Description |
| --- | --- |
| `share_id` | Unique share identifier |
| `file_id` | Shared file |
| `token_hash` | Hashed token |
| `permission` | Access level |
| `expires_at` | Expiry time |
| `created_by` | Link creator |

When a request arrives:

```text
Share URL
   ↓
Validate token
   ↓
Check expiry
   ↓
Check permissions
   ↓
Generate short-lived signed URL
   ↓
S3
```

### Security

Public links should support:

- Expiration
- Revocation
- Read / write permission
- Rate limiting
- Optional password
- Audit logging

**Very important:** Don't put the permanent S3 credentials in the URL. Use a short-lived signed URL.

---

## Follow-up 11. How do you scale PostgreSQL?

Initially:

```text
Application
     ↓
PostgreSQL
```

As traffic increases, add read replicas:

```text
                ┌── Replica 1
                │
Primary ────────┼── Replica 2
                │
                └── Replica 3
```

- Writes → Primary
- Reads → Replicas

Also:

- Proper indexing
- Connection pooling
- Query optimization
- Partition large tables
- Caching
- Eventually sharding

**Interview answer:**

> I would first scale vertically and optimize queries, then introduce read replicas for read-heavy workloads, and finally shard when a single database can no longer handle the dataset or write throughput.

---

## Follow-up 12. How would you shard the metadata database?

The natural shard key is `user_id`.

For example:

```text
hash(user_id) % N
User 101 → Shard 1
User 102 → Shard 3
User 103 → Shard 2
```

### Why `user_id`?

Most queries are:

- Get my files
- Get my folders
- Get my permissions

So data for a user stays together.

### Important problem

What about sharing?

- User A owns file
- User B accesses file

Cross-shard queries become possible.

Solutions include:

- Permission index
- Separate sharing service / database
- Denormalized access metadata

---

## Follow-up 13. How do you handle file versioning?

Don't overwrite the old object.

Store:

```text
file_id = 101
version = 1
object_key = abc
```

Then:

```text
file_id = 101
version = 2
object_key = xyz
```

### Database

**File**

| Column |
| --- |
| `file_id` |
| `current_version` |

**FileVersion**

| Column |
| --- |
| `version_id` |
| `file_id` |
| `version_number` |
| `object_key` |
| `size` |
| `checksum` |
| `created_at` |

### Architecture

```text
                 File
                  │
        ┌─────────┼─────────┐
        ▼         ▼         ▼
      V1        V2        V3
     S3-A      S3-B      S3-C
```

Benefits:

- Rollback
- Recovery
- Audit history

---

## Follow-up 14. How do you delete files safely?

Don't immediately hard-delete.

Use:

```text
ACTIVE
  ↓
TRASHED
  ↓
DELETED
```

When the user clicks delete:

```text
DB
↓
deleted_at = timestamp
```

Then an asynchronous worker:

```text
Kafka
 ↓
Deletion Worker
 ↓
S3
 ↓
Delete object
```

This avoids blocking the user request on physical deletion.

---

## Follow-up 15. How do you implement trash / recycle bin?

When deleted:

- `status = TRASHED`
- `deleted_at = timestamp`

Example:

- `resume.pdf`
- `status = TRASHED`
- `deleted_at = Aug 20`

User can **Restore**:

```text
TRASHED → ACTIVE
```

After 30 / 60 / 90 days:

```text
TRASHED
   ↓
Background Job
   ↓
Permanent Delete
```

We can use object-storage lifecycle rules for automatic cleanup.

---

## Follow-up 16. How do you scan uploaded files for malware?

Never make the file immediately downloadable.

### Flow

```text
Client
   ↓
S3
   ↓
FileUploaded event
   ↓
Kafka
   ↓
Virus Scanner
   ↓
Scan
```

### State

```text
UPLOADING
     ↓
SCANNING
     ↓
SAFE
```

or:

```text
SCANNING
    ↓
INFECTED
```

If infected: quarantine, and don't generate a download URL.

### Why asynchronous?

Scanning large files can take time.

We don't want:

```text
Upload API
   ↓
Wait 30 seconds
   ↓
Scan
   ↓
Response
```

---

## Follow-up 17. How do you support millions of downloads?

The biggest mistake would be:

```text
Client
 ↓
Backend
 ↓
S3
```

for every download.

Instead:

```text
Client
   ↓
CDN
   ↓
S3
```

Backend only:

- Authenticate
- Authorize
- Generate signed URL

CDN handles actual file delivery.

### Architecture

```text
                  ┌──────────────┐
                  │    Client    │
                  └──────┬───────┘
                         │
                         ▼
                       CDN
                    ↙       ↘
              Cache Hit    Cache Miss
                 │             │
                 │             ▼
                 │             S3
                 │
                 ▼
               Client
```

For private files:

- Signed CDN URLs
- Short expiration
- Authorization before URL generation

---

## Follow-up 18. Where would you use Kafka?

Kafka is useful for operations that don't need to happen synchronously.

For example:

- `FileUploaded`
- `FileDeleted`
- `FileShared`
- `FileVersionCreated`

### Consumers

```text
                    Kafka
                      │
        ┌─────────────┼─────────────┐
        ▼             ▼             ▼
 Virus Scanner    Search Indexer   Notification
        │             │             │
        ▼             ▼             ▼
    Scanner       Elasticsearch     Email
```

Also:

- Thumbnail generation
- Audit logging
- Storage analytics
- Cleanup
- Notifications

### Important

Don't use Kafka for the actual 10 GB file.

Kafka should carry **metadata / events**, not the entire binary file.

---

## Follow-up 19. Where would you use Redis?

Redis is useful for low-latency data.

### Cache file metadata

```text
file:123
 ↓
metadata
```

### Cache permissions

```text
permission:file123:user456
 ↓
VIEW
```

### Cache share links

```text
share:abc123
 ↓
file123
```

### Rate limiting

```text
rate:user123
```

### Session / upload state

```text
upload:xyz
```

But don't use Redis as permanent storage for file metadata.

Redis is primarily a **cache / fast-access layer** here.

---

## Follow-up 20. How do you ensure durability?

We need protection against:

- Disk failure
- Machine failure
- AZ failure
- Region failure

Object storage typically provides replication internally.

For stronger disaster recovery:

```text
Region A
   │
   │ Replication
   ▼
Region B
```

Also:

- Versioning
- Backups
- Cross-region replication
- Checksums
- Periodic integrity checks

### Important distinction

Replication improves availability and durability, but backups / versioning help recovery from accidental deletion or corruption.

---

## Follow-up 21. How do you handle multi-region disaster recovery?

### Architecture

```text
             Global DNS
                 │
       ┌─────────┴─────────┐
       ▼                   ▼
   Region A             Region B
       │                   │
   Services             Services
       │                   │
   Metadata DB          Metadata DB
       │                   │
       └───────┬───────────┘
               │
       Object Replication
```

We can replicate files:

```text
S3 Region A
     ↓
S3 Region B
```

Metadata can use:

- Cross-region replication
- Multi-region database
- Database backups
- Failover

If Region A fails:

```text
Global DNS
    ↓
Region B
```

For a simpler interview design, I'd start with:

> Primary region + replicated disaster-recovery region.

Then discuss active-active only if required.

---

## Follow-up 22. How would you reduce storage cost?

Several approaches.

### 1. Deduplication

Same content → one physical copy.

### 2. Compression

Useful for compressible formats.

Not useful for already compressed:

- JPEG
- MP4
- ZIP

### 3. Storage tiers

| Access pattern | Tier |
| --- | --- |
| Frequently accessed | Hot storage |
| Old files | Cold storage |
| Very old files | Archive storage |

### 4. Lifecycle policies

| Age | Tier |
| --- | --- |
| 0–30 days | Standard |
| 30–180 days | Infrequent Access |
| 180+ days | Archive |

### 5. Delete abandoned uploads

Incomplete multipart uploads can consume storage.

---

## Follow-up 23. How would you design Dropbox-like synchronization?

This is a more advanced question.

We need to detect:

- File created
- File modified
- File deleted

Client maintains:

- `file_id`
- `version`
- `checksum`
- `last_modified`

Server maintains the same metadata.

Example:

- Client version = 5
- Server version = 6

Client knows the server has a newer version and downloads the delta / full file.

### Better approach

Use a change log:

```text
File changes
     ↓
Kafka
     ↓
Sync Service
     ↓
Client notification
```

Client can also periodically ask:

```http
GET /changes?cursor=12345
```

Response:

```json
{
  "changes": [
    {
      "fileId": "101",
      "version": 7,
      "type": "UPDATED"
    }
  ],
  "nextCursor": "12350"
}
```

The cursor is important because the client doesn't have to repeatedly download the entire folder.

---

## Follow-up 24. How do you handle concurrent edits to the same file?

Suppose:

- User A downloads V5
- User B downloads V5

Then:

- User A → edits → V6
- User B → edits → V7

Now we have a conflict.

### Optimistic concurrency

Client sends `baseVersion = 5`.

Server checks: current version = 6.

Mismatch: `5 != 6`. Therefore reject / update conflict.

Response: `409 Conflict`

Client can then:

1. Download latest
2. Merge changes
3. Upload again

### For collaborative editing

If the product requires Google Docs-like real-time collaboration, we'd need more advanced mechanisms:

- Operational Transformation
- CRDT
- Real-time WebSocket communication

But for Dropbox-style file synchronization, **versioning + conflict detection** is generally sufficient.

---

## Follow-up 25. How do you calculate storage and bandwidth requirements?

This is extremely important in HLD interviews.

Let's assume:

- 100M users
- 10% daily active = **10M DAU**
- Each active user uploads **2 files/day**
- Average file size = **10 MB**

### Uploads per day

```text
10M × 2 = 20M files/day
```

### Storage per day

```text
20M × 10 MB = 200M MB ≈ 200 TB/day
```

### Storage per year

```text
200 TB × 365 ≈ 73 PB/year
```

So:

> At this scale, object storage is mandatory; database BLOB storage would be impractical.

### Bandwidth

```text
20M uploads/day × 10 MB = 200 TB/day
```

Average bandwidth:

```text
200 TB / 86,400 seconds ≈ 2.3 GB/sec
```

And that's only average upload bandwidth.

For peak traffic, assume perhaps a **5× peak factor**:

```text
~11.5 GB/sec
```

This demonstrates why we don't want:

```text
Client
 ↓
Backend
 ↓
S3
```

for the file data.

Instead:

```text
Client ─────────────► S3
```

The backend only handles metadata / control-plane operations.

---

## The Most Important Concept: Control Plane vs Data Plane

This is probably the best way to explain the entire design.

### Control plane

Handles:

- Authentication
- Authorization
- File metadata
- Folders
- Permissions
- Upload initialization
- Download URL generation

```text
Client
  ↓
API
  ↓
Services
  ↓
PostgreSQL / Redis
```

### Data plane

Handles actual file bytes.

```text
Client
   ↓
Object Storage / CDN
```

### Combined view

```text
                 FILE STORAGE

             CONTROL PLANE
                  │
                  ▼
             API Gateway
                  │
           ┌──────┴──────┐
           ▼             ▼
       Metadata        Sharing
           │             │
           └──────┬──────┘
                  ▼
             PostgreSQL
                  │
                  ▼
                Redis


               DATA PLANE
                  │
                  ▼
             Object Storage
                  ▲
                  │
              ┌───┴───┐
              │Client │
              └───────┘

           Download → CDN
```

If the interviewer asks:

> What is the single most important design decision in your system?

Say:

> I would separate the control plane from the data plane. The backend manages metadata, authentication, authorization and file lifecycle, while clients transfer the actual file bytes directly through object storage using pre-signed URLs. This prevents application servers and databases from becoming bandwidth bottlenecks.

That's a very strong HLD interview answer.
