# Cloud Architecture — HLD Interview Explanation

For HLD interviews, you don't need to memorize every AWS feature. You should understand what problem each service solves, when to use it, and how services fit together.

A common architecture looks like:

```text
                    Internet
                       |
                 Route 53 / DNS
                       |
                Load Balancer
                       |
              +--------+--------+
              |                 |
           EC2/ECS            EC2/ECS
              |                 |
              +--------+--------+
                       |
          +------------+------------+
          |            |            |
         RDS         S3          SQS/SNS
          |
       Multi-AZ
```

---

## Table of Contents

- [1. AWS Fundamentals](#1-aws-fundamentals)
- [2. EC2](#2-ec2)
- [3. ECS](#3-ecs)
- [4. EKS](#4-eks)
- [5. Lambda](#5-lambda)
- [6. S3](#6-s3)
- [7. RDS](#7-rds)
- [8. DynamoDB](#8-dynamodb)
- [9. SQS](#9-sqs)
- [10. SNS](#10-sns)
- [11. VPC](#11-vpc)
- [12. Load Balancer](#12-load-balancer)
- [13. Multi-AZ](#13-multi-az)
- [14. Auto Scaling](#14-auto-scaling)
- [Putting Everything Together](#putting-everything-together)
- [What to Say in an HLD Interview](#what-to-say-in-an-hld-interview)
- [Most Important Interview Comparisons](#most-important-interview-comparisons)

---

## 1. AWS Fundamentals

AWS provides cloud infrastructure/services that you can provision instead of managing physical servers yourself.

Important concepts:

### Region

A geographical location containing multiple Availability Zones.

Example:

```text
Mumbai Region
   |
   +-- AZ-1
   +-- AZ-2
   +-- AZ-3
```

### Availability Zone

A separate infrastructure location inside a region.

You use multiple AZs to avoid a single infrastructure failure taking down the application.

### Example

Instead of:

```text
EC2 → RDS
```

use:

```text
             Load Balancer
              /          \
           EC2-A        EC2-B
            AZ-1         AZ-2
               \         /
                 RDS
              Multi-AZ
```

---

## 2. EC2

EC2 = virtual server.

You get a virtual machine where you can run your application.

For example:

```text
EC2
 |
 +-- Java
 +-- Spring Boot
 +-- Kafka client
 +-- Redis client
```

Your Spring Boot application can run directly on EC2.

### When to use EC2?

Use it when you want:

- Control over the server
- Custom OS configuration
- Custom networking
- Long-running applications
- Specific CPU/memory requirements

### Scaling

You can run:

```text
EC2 #1
EC2 #2
EC2 #3
```

behind a load balancer.

---

## 3. ECS

ECS = Elastic Container Service.

It runs Docker containers.

Instead of:

```text
EC2
 |
 +-- Java application
```

you can have:

```text
ECS
 |
 +-- Docker container
       |
       +-- Spring Boot
```

ECS can run containers using:

- EC2 instances
- AWS Fargate

### Fargate

Fargate lets you run containers without managing the underlying EC2 servers.

Interview answer:

> ECS is AWS's managed container orchestration service. It is useful when we want to deploy and scale Dockerized applications.

---

## 4. EKS

EKS = Elastic Kubernetes Service.

It is AWS-managed Kubernetes.

If your organization already uses Kubernetes:

```text
EKS
 |
 +-- Pods
      |
      +-- Java service
      +-- Payment service
      +-- Notification service
```

### ECS vs EKS

| ECS | EKS |
| --- | --- |
| AWS container orchestration | Managed Kubernetes |
| Simpler | More complex |
| AWS-specific | Kubernetes ecosystem |
| Easier to operate | More flexibility |

Interview question:

Why not use EKS everywhere?

Because Kubernetes introduces additional operational complexity. If you don't need Kubernetes capabilities, ECS can be simpler.

---

## 5. Lambda

Lambda = serverless function.

You provide code and AWS executes it when triggered.

Example:

```text
S3 upload
    ↓
Lambda
    ↓
Process image
```

Another example:

```text
API request
    ↓
API Gateway
    ↓
Lambda
```

You don't manage servers.

Good for:

- Event processing
- Small background tasks
- Scheduled jobs
- S3 events
- Lightweight APIs

Not always ideal for:

- Long-running workloads
- Applications requiring persistent processes
- Heavy compute with predictable continuous traffic

---

## 6. S3

S3 = object storage.

Store things like:

- Images
- Videos
- PDFs
- Documents
- Backups
- CSV files
- Logs

Example:

```text
User
 |
Upload PDF
 |
S3
```

Your database usually stores:

- `document_id`
- `user_id`
- `s3_object_key`

rather than storing the actual PDF in the database.

### Important

S3 is object storage, not a normal filesystem or relational database.

---

## 7. RDS

RDS = managed relational database.

It supports databases such as:

- PostgreSQL
- MySQL
- MariaDB
- Oracle
- SQL Server

Example:

```text
Spring Boot
     |
     ↓
    RDS
     |
 PostgreSQL
```

Use RDS when you need:

- SQL
- Transactions
- ACID properties
- Relationships
- Joins
- Strong relational modeling

---

## 8. DynamoDB

DynamoDB = AWS managed NoSQL database.

It is particularly useful for applications requiring very high-scale key-value/document access.

Example:

```text
UserId → UserProfile
```

or:

```text
OrderId → Order
```

Typical access pattern:

Get order by orderId

rather than complicated joins.

### RDS vs DynamoDB

| RDS | DynamoDB |
| --- | --- |
| Relational | NoSQL |
| SQL | Key-value/document |
| Joins | Designed around access patterns |
| Complex relationships | High-scale predictable access |
| Transactions supported | Transactions supported, but different model |

Interview principle:

> Choose DynamoDB based on access patterns, not simply because the application needs "a NoSQL database."

---

## 9. SQS

SQS = message queue.

It allows one service to send work to another asynchronously.

Example:

```text
Order Service
     |
     | message
     ↓
    SQS
     |
     ↓
Payment Worker
```

Instead of:

```text
Order → Payment
```

synchronously, you can do:

```text
Order → SQS → Payment Worker
```

### Why?

Suppose Payment Service is temporarily slow.

Without queue:

```text
Order Service
     ↓
Payment Service ❌
```

With queue:

```text
Order Service
     ↓
SQS
     ↓
Payment Service
```

The messages wait until the consumer can process them.

### Important interview concepts

- Visibility timeout
- Dead-letter queue
- Message retention
- Long polling
- At-least-once delivery

Therefore, consumers should generally be idempotent.

---

## 10. SNS

SNS = publish/subscribe messaging service.

One event can be delivered to multiple subscribers.

Example:

```text
              SNS
               |
       +-------+-------+
       |       |       |
      SQS    Lambda   Email
```

Suppose:

```text
OrderCreated
```

You want:

- Inventory Service
- Notification Service
- Analytics Service

to receive the event.

SNS can publish the event to multiple subscribers.

### SQS vs SNS

Think:

- **SQS** → queue/work distribution
- **SNS** → broadcast/fan-out

They can also be combined:

```text
                  SNS
               OrderCreated
               /          \
             SQS          SQS
              |            |
         Inventory      Notification
```

---

## 11. VPC

VPC = your private network inside AWS.

Think of it like your own network.

```text
                 VPC
                  |
       +----------+----------+
       |                     |
 Public Subnet          Private Subnet
       |                     |
 Load Balancer          Application
                             |
                            RDS
```

### Public subnet

Resources can have internet connectivity through appropriate routing.

Typical:

- Internet-facing Load Balancer

### Private subnet

Typically used for internal resources:

- EC2
- ECS
- RDS

You generally don't want your database directly exposed to the public internet.

---

## 12. Load Balancer

A load balancer distributes incoming requests across multiple servers.

```text
              Load Balancer
              /     |      \
             /      |       \
          EC2-1   EC2-2    EC2-3
```

Without it:

```text
Users
  |
One EC2
```

That server can become a bottleneck or single point of failure.

With it:

```text
Users
  |
Load Balancer
 /    |    \
EC2  EC2   EC2
```

Benefits:

- Traffic distribution
- Health checks
- High availability
- Scaling
- Failure handling

---

## 13. Multi-AZ

Multi-AZ = deploy across multiple Availability Zones.

Instead of:

```text
AZ-1
 |
EC2
 |
RDS
```

use:

```text
             Load Balancer
              /          \
           AZ-1          AZ-2
            |              |
          EC2            EC2
            \              /
             \            /
                RDS
              Multi-AZ
```

If one AZ has an infrastructure problem, the application can continue operating through another AZ, assuming the relevant components are properly designed for failover.

### Important distinction

Multi-AZ ≠ Multi-Region

**Multi-AZ**

```text
Mumbai
 ├── AZ-1
 ├── AZ-2
 └── AZ-3
```

versus:

**Multi-Region**

```text
Mumbai
   +
Singapore
```

Multi-region is generally used for stronger disaster-recovery/geographic-resilience requirements.

---

## 14. Auto Scaling

Auto Scaling automatically changes the number of application instances based on demand or configured policies.

Example:

Normal traffic

```text
EC2 EC2
```

Traffic increases:

```text
EC2 EC2 EC2 EC2
```

Traffic decreases:

```text
EC2 EC2
```

Typical architecture:

```text
              Load Balancer
                   |
            Auto Scaling Group
             /      |      \
           EC2     EC2     EC2
```

Example rule:

```text
CPU > 70%
     ↓
Launch more instances
```

Later:

```text
CPU < 30%
     ↓
Terminate unnecessary instances
```

---

## Putting Everything Together

Imagine you're designing an e-commerce application.

```text
                       Internet
                           |
                      Load Balancer
                           |
                +----------+----------+
                |                     |
              ECS-A                 ECS-B
                |                     |
                +----------+----------+
                           |
                    Application Logic
                           |
          +----------------+----------------+
          |                |                |
         RDS              S3              SQS
          |                |                |
       Orders           Images        Background Jobs
                                           |
                                         Worker
```

For notifications:

```text
Order Service
      |
      ↓
     SNS
   /     \
  SQS    Lambda
   |       |
Email    Analytics
```

Network:

```text
                    VPC
                     |
          +----------+----------+
          |                     |
     Public Subnet         Private Subnet
          |                     |
    Load Balancer          ECS / RDS
```

Scaling:

```text
                 Load Balancer
                       |
                Auto Scaling
                  /   |   \
                ECS  ECS  ECS
```

High availability:

```text
              Region
          /             \
       AZ-1             AZ-2
        |                 |
      App               App
        \                 /
         \               /
             Database
             Multi-AZ
```

---

## What to Say in an HLD Interview

If asked:

> Why AWS?

You can say:

> AWS provides managed infrastructure services for compute, storage, databases, networking and messaging. Instead of managing physical infrastructure ourselves, we can use services such as ECS or EC2 for compute, RDS for relational data, S3 for object storage, SQS for asynchronous processing and load balancers for distributing traffic.

If asked:

> How would you design a highly available backend?

Say:

> I would deploy application instances across multiple Availability Zones behind a load balancer. I would use Auto Scaling to handle changing traffic. For persistent data, I would use a Multi-AZ database. S3 can handle object storage, while SQS can decouple asynchronous workloads. I would keep the database and application servers in private subnets where appropriate and expose only the required entry points.

---

## Most Important Interview Comparisons

Memorize these:

| Service | Meaning |
| --- | --- |
| EC2 | Virtual machine |
| ECS | Container orchestration |
| EKS | Kubernetes |
| Lambda | Serverless function |
| S3 | Object storage |
| RDS | Relational database |
| DynamoDB | NoSQL database |
| SQS | Queue |
| SNS | Pub/Sub / fan-out |
| VPC | Private network |
| LB | Distribute traffic |
| Multi-AZ | High availability across AZs |
| AutoScale | Automatically adjust capacity |

And the most useful mental model:

```text
             USERS
               |
        Load Balancer
               |
        ECS / EC2 / EKS
               |
       +-------+-------+
       |       |       |
      RDS     S3      SQS
                       |
                     Worker
                       |
                      SNS
                    /     \
                  SQS    Lambda
```

This is enough to connect most of the AWS topics you'll encounter in an SDE-1 HLD interview.
