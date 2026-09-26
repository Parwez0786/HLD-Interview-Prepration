Docker & Kubernetes for HLD

For HLD interviews, you don't need to become a Kubernetes administrator. You mainly need to understand why Kubernetes exists, what each component does, and how traffic and deployments flow through the system.

1. Containerization
What is containerization?

Containerization packages an application along with its:

Code
Runtime
Libraries
Dependencies
Configuration needed to run

into a container.

Without containers:

Application
   ↓
Needs Java 21
Needs specific libraries
Needs specific OS dependencies
   ↓
"Works on my machine"

With containers:

Docker Container
 ├── Application
 ├── Java
 ├── Libraries
 └── Dependencies

So the same container can run on:

Developer machine
       ↓
Testing
       ↓
Staging
       ↓
Production
Why containers?

Main benefits:

Consistent environment
Fast startup
Isolation
Easy deployment
Easy scaling
Better resource utilization
2. Docker
5

Docker is a platform for building and running containers.

Think:

Dockerfile
    ↓
Docker Image
    ↓
Docker Container
Dockerfile

Example:

FROM eclipse-temurin:21

COPY app.jar app.jar

CMD ["java", "-jar", "app.jar"]

Build:

docker build -t payment-service .

Run:

docker run payment-service
Important distinction

Image = blueprint

Container = running instance of that blueprint

For example:

payment-service image
       ↓
 ┌─────┼─────┐
 ↓     ↓     ↓
Pod1  Pod2  Pod3
3. Kubernetes
7

Docker can run containers.

But imagine you have:

100 containers
20 services
10 machines
Millions of requests

Managing everything manually becomes difficult.

Kubernetes is a container orchestration platform.

It helps with:

Deploying containers
Scaling containers
Restarting failed containers
Service discovery
Load balancing
Rolling deployments
Self-healing
Configuration management

High-level:

                Kubernetes Cluster
                       |
        ┌──────────────┼──────────────┐
        ↓              ↓              ↓
      Node 1         Node 2         Node 3
        ↓              ↓              ↓
      Pods           Pods           Pods
        ↓              ↓              ↓
   Containers      Containers     Containers
4. Pod

A Pod is the smallest deployable unit in Kubernetes.

Usually:

Pod
 └── Container
      └── Spring Boot application

For example:

Pod 1
 └── payment-service

Pod 2
 └── payment-service

Pod 3
 └── payment-service

The three pods are running separate instances of your application.

Why not directly deploy containers?

Kubernetes manages Pods, not individual containers.

A Pod can technically contain multiple containers:

Pod
 ├── Main application container
 └── Sidecar container

But in normal backend applications, one main application container per Pod is common.

5. Deployment

A Deployment manages Pods for you.

Suppose:

replicas: 3

Kubernetes maintains:

Deployment
     ↓
 ┌───┼───┐
 ↓   ↓   ↓
Pod Pod Pod

If one Pod crashes:

Pod 1 ❌

Pod 2 ✅
Pod 3 ✅

Deployment creates another:

Pod 1 ❌
     ↓
New Pod ✅

Pod 2 ✅
Pod 3 ✅

This is part of Kubernetes' self-healing behavior.

Interview answer

A Deployment manages the desired number of Pod replicas and handles updates, replacement of failed Pods, and rollout/rollback of application versions.

6. Service

Pods are temporary.

Their IP addresses can change.

Suppose:

payment Pod
IP = 10.1.1.5

Pod crashes.

New Pod:

IP = 10.1.1.20

If another service directly uses the Pod IP, everything breaks.

So Kubernetes provides a Service.

                Payment Service
                 10.10.0.10
                     |
          ┌──────────┼──────────┐
          ↓          ↓          ↓
        Pod 1      Pod 2      Pod 3

The Service gives clients a stable endpoint.

Example:

payment-service

Other services can call:

http://payment-service

instead of knowing individual Pod IPs.

Service also provides load balancing
Request
   ↓
Service
 ├──→ Pod 1
 ├──→ Pod 2
 └──→ Pod 3
7. Ingress

Now suppose users access your application from the internet.

You might have:

/api/users
/api/payments
/api/orders

You don't necessarily want separate public load balancers for every service.

Ingress can act as an HTTP routing layer.

                  Internet
                     ↓
                  Ingress
                /    |     \
               ↓     ↓      ↓
           User Svc Payment Svc Order Svc

For example:

/api/users/*
       ↓
user-service

/api/payments/*
       ↓
payment-service

/api/orders/*
       ↓
order-service

Ingress can also handle things such as:

Host-based routing
Path-based routing
TLS termination
Routing to Services
Simple distinction
Service
    ↓
Exposes Pods inside Kubernetes

Ingress
    ↓
Routes external HTTP/HTTPS traffic
to Services
8. ConfigMap

Applications need configuration.

Example:

DATABASE_HOST=postgres
LOG_LEVEL=INFO
PAYMENT_TIMEOUT=5000

You don't want to hardcode these inside your Docker image.

So Kubernetes provides ConfigMap.

ConfigMap
   ↓
Application Pod

Example:

DATABASE_HOST: postgres
LOG_LEVEL: INFO

The application can read these as environment variables or mounted configuration files.

Important

ConfigMap is for non-sensitive configuration.

9. Secret

Sensitive information should not be stored in a ConfigMap.

For example:

DB_PASSWORD
API_KEY
JWT_SECRET

Use Kubernetes Secret.

Secret
   ↓
Pod
   ↓
Application

Example:

DB_USERNAME = payment
DB_PASSWORD = ********
Interview distinction
ConfigMap	Secret
Normal configuration	Sensitive configuration
DB host	DB password
Log level	API key
Feature flags	Credentials

One important interview nuance: Kubernetes Secrets are designed for sensitive configuration, but by default their values are not magically encrypted merely because the object is called a Secret; production clusters should use appropriate encryption-at-rest and access controls.

10. Horizontal Pod Autoscaler — HPA

Suppose your application normally needs:

3 Pods

Traffic increases:

CPU = 85%

You want:

3 Pods → 6 Pods

HPA automatically changes the number of Pod replicas based on configured metrics.

Traffic increases
       ↓
CPU increases
       ↓
HPA detects it
       ↓
3 Pods → 6 Pods

When traffic decreases:

6 Pods → 3 Pods
Example
minReplicas: 3
maxReplicas: 10
targetCPUUtilization: 60%

Kubernetes can scale between 3 and 10 Pods to maintain the target.

HLD point

HPA provides horizontal scaling.

It does not make one Pod more powerful.

11. Rolling Deployment

Suppose currently:

Version 1

Pod 1 → V1
Pod 2 → V1
Pod 3 → V1

You want to deploy V2.

Instead of stopping everything:

V1 V1 V1
 ↓
V2 V2 V2

Kubernetes gradually replaces Pods.

V1 V1 V1
   ↓
V2 V1 V1
   ↓
V2 V2 V1
   ↓
V2 V2 V2

Users can continue accessing the application during deployment.

Advantage

Minimal downtime.

Risk

During deployment, both versions may temporarily run:

V1 + V2

Therefore, database/API changes should generally be backward compatible during the transition.

12. Blue-Green Deployment

Here you maintain two complete environments.

Blue
V1
 ├── Pod
 ├── Pod
 └── Pod

Green
V2
 ├── Pod
 ├── Pod
 └── Pod

Initially:

Users
  ↓
Blue V1

You deploy and test V2:

Users
  ↓
Blue V1

Green V2

After verification:

Users
  ↓
Green V2

Traffic is switched from Blue to Green.

Advantage

Very fast rollback:

Green V2 ❌

Switch traffic
      ↓

Blue V1 ✅
Disadvantage

You temporarily need infrastructure for both environments.

13. Canary Deployment

Canary means sending a small percentage of traffic to the new version first.

Example:

100% traffic
     ↓
   V1

Deploy V2:

95% → V1
 5% → V2

Monitor:

Error rate
Latency
CPU
Business metrics
Logs

If V2 behaves correctly:

95% → V1
 5% → V2

        ↓

50% → V1
50% → V2

        ↓

0% → V1
100% → V2

If V2 has problems:

V2 ❌

Traffic
  ↓
V1
Why use Canary?

It reduces the blast radius of a bad deployment.

14. Rolling vs Blue-Green vs Canary
Strategy	Basic idea
Rolling	Gradually replace old Pods
Blue-Green	Maintain old and new environments, then switch traffic
Canary	Send a small percentage of traffic to new version

Example:

Rolling
V1 V1 V1
 ↓
V2 V1 V1
 ↓
V2 V2 V1
 ↓
V2 V2 V2
Blue-Green
Blue = V1
Green = V2

Traffic → Blue

Switch

Traffic → Green
Canary
95% → V1
 5% → V2

      ↓

50% → V1
50% → V2

      ↓

100% → V2
15. Put Everything Together

For an HLD interview, imagine you're designing a Payment System.

5

A simplified architecture:

                    Users
                      |
                      ↓
              Load Balancer
                      |
                      ↓
                   Ingress
                      |
          ┌───────────┼───────────┐
          ↓           ↓           ↓
     User Service Payment      Order Service
          |         Service          |
          |            |             |
          ↓            ↓             ↓
       Service      Service       Service
          |            |             |
      ┌───┼───┐    ┌───┼───┐    ┌───┼───┐
      ↓   ↓   ↓    ↓   ↓   ↓    ↓   ↓   ↓
     Pod Pod Pod   Pod Pod Pod   Pod Pod Pod
      |   |   |    |   |   |    |   |   |
      └───┴───┘    └───┴───┘    └───┴───┘

And configuration:

ConfigMap ─────→ Pods
Secret ─────────→ Pods

Autoscaling:

High traffic
     ↓
HPA
     ↓
More Pods

Deployment:

V1
 ↓
Rolling / Canary / Blue-Green
 ↓
V2
16. Most Important HLD Interview Questions

You should be able to answer these clearly:

Q1. Why Kubernetes if Docker already exists?

Docker runs containers. Kubernetes manages containers at scale across multiple machines, providing scheduling, scaling, service discovery, self-healing, and deployment management.

Q2. Why do we need a Service?

Pods are ephemeral and their IP addresses can change. A Service provides a stable endpoint and distributes traffic across matching Pods.

Q3. Deployment vs Pod?

A Pod runs the application container. A Deployment manages the desired number of Pods and handles updates and replacement of failed Pods.

Q4. Service vs Ingress?

A Service exposes a group of Pods through a stable endpoint. Ingress provides HTTP/HTTPS routing from external traffic to different Services.

Q5. ConfigMap vs Secret?

ConfigMap stores normal configuration, while Secret is intended for sensitive configuration such as credentials and API keys.

Q6. How does Kubernetes handle a crashed Pod?
Pod crashes
    ↓
Deployment/Controller detects desired state mismatch
    ↓
New Pod created
    ↓
New Pod becomes Ready
    ↓
Service sends traffic to healthy Pod
Q7. How do you scale your application?
Traffic increases
      ↓
HPA
      ↓
More Pods
      ↓
Service
      ↓
Load distributed across Pods
17. The HLD Mental Model

Remember this flow:

                    INTERNET
                       |
                       ↓
                Load Balancer
                       |
                       ↓
                    Ingress
                       |
              ┌────────┴────────┐
              ↓                 ↓
           Service           Service
              ↓                 ↓
          Deployment        Deployment
              ↓                 ↓
        ┌─────┼─────┐     ┌─────┼─────┐
        ↓     ↓     ↓     ↓     ↓     ↓
       Pod   Pod   Pod    Pod   Pod   Pod
        |     |     |      |     |     |
        └─────┴─────┘      └─────┴─────┘
              ↑                 ↑
         ConfigMap           Secret
              ↑
             HPA

One-line memory trick:

Docker packages the application → Kubernetes manages it → Pod runs it → Deployment manages Pods → Service exposes Pods → Ingress routes traffic → HPA scales Pods → ConfigMap/Secret provide configuration → deployment strategies control how new versions are released.