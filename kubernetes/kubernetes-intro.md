Day 1 — What is Kubernetes?

Let's learn Kubernetes from an SDE-1/HLD interview perspective, starting from the basics.

1. What is Kubernetes?

Kubernetes (K8s) is an open-source container orchestration platform.

In simple words:

Kubernetes automatically manages containers running across multiple machines.

It can:

Start containers
Stop containers
Restart failed containers
Create more containers when traffic increases
Remove unnecessary containers
Distribute traffic
Deploy new versions
Roll back deployments
Provide service discovery
Schedule containers on available machines

Think of it as a manager for containers.

2. Why do we need Kubernetes?

Suppose you have a Spring Boot application.

Without Kubernetes:

User
  ↓
Server
  ↓
Docker
  ├── Container 1
  ├── Container 2
  └── Container 3

Initially this looks fine.

But now problems start.

Problem 1 — Container crashes

Suppose:

Container 2
     ↓
   CRASH

Someone needs to manually restart it.

With Kubernetes:

Container crashes
       ↓
Kubernetes detects it
       ↓
Starts a new container
Problem 2 — Server crashes

Suppose all containers are running on:

Server 1

and Server 1 goes down.

Server 1 ❌
   ↓
All containers ❌

Kubernetes can run workloads across multiple worker nodes:

Worker 1
 ├── Pod
 └── Pod

Worker 2
 ├── Pod
 └── Pod

If one node fails, Kubernetes can reschedule workloads onto healthy nodes, subject to the workload's configuration and available capacity.

3. Traffic increases

Suppose normally:

100 requests/sec

You have:

3 containers

During a sale:

10,000 requests/sec

Three containers may not be enough.

Kubernetes can increase the number of application replicas:

Before:

Pod 1
Pod 2
Pod 3


After:

Pod 1
Pod 2
Pod 3
Pod 4
Pod 5
Pod 6
Pod 7
...

This is called scaling.

4. Deployment problem

Suppose your application currently has:

Version 1

You want to deploy:

Version 2

A naive deployment could be:

Stop V1
   ↓
Deploy V2

Users may experience downtime.

Kubernetes supports deployment strategies such as rolling updates.

For example:

V1 V1 V1 V1
 ↓
V2 V1 V1 V1
 ↓
V2 V2 V1 V1
 ↓
V2 V2 V2 V1
 ↓
V2 V2 V2 V2

So old instances are gradually replaced.

5. Service discovery problem

Suppose you have:

User Service
Payment Service
Order Service

Order Service needs to call Payment Service.

If Payment Service containers keep changing IP addresses:

Payment Pod
IP = 10.0.1.5

Pod crashes

New Payment Pod
IP = 10.0.2.8

Hardcoding IP addresses would be a bad approach.

Kubernetes provides Services that give applications a stable way to reach a group of Pods.

Conceptually:

Order Service
      ↓
Payment Service
      ↓
Payment Pods
 ┌────┼────┐
 ↓    ↓    ↓
Pod  Pod  Pod

We'll study Services in detail later.

6. Docker vs Kubernetes

This is a very common interview question.

Docker

Docker primarily helps you:

Build and run containers.

For example:

docker run my-app

Docker can run:

Container
Container
Container
Kubernetes

Kubernetes helps you:

Manage containers at scale across machines.

For example:

              Kubernetes
                   ↓
        ┌──────────┼──────────┐
        ↓          ↓          ↓
      Node 1     Node 2     Node 3
        ↓          ↓          ↓
      Pods       Pods       Pods
Simple comparison
Docker	Kubernetes
Runs containers	Manages containerized workloads
Container runtime/tooling	Container orchestration
Usually focuses on individual machine/workflows	Designed for clusters
Build/run images	Scheduling, scaling, deployment, recovery
docker run	Desired-state management

Important: Kubernetes does not replace Docker in the sense of "Docker is the thing Kubernetes uses." Modern Kubernetes commonly uses container runtimes such as containerd or CRI-O.

7. What is Container Orchestration?

Orchestration means automatically managing many containers.

Imagine you have:

500 containers
20 servers

Manually managing them would be difficult.

You would need to answer:

Where should each container run?
What if a container crashes?
What if a server crashes?
How many instances do we need?
How do users reach the correct containers?
How do we deploy a new version?
How do we scale?
How do we monitor desired vs actual state?

Kubernetes automates much of this.

So:

Container
   ↓
Docker / container runtime

Many containers
   ↓
Kubernetes
   ↓
Orchestration
8. What is a Kubernetes Cluster?

A Kubernetes cluster is a collection of machines managed by Kubernetes.

It generally contains:

Kubernetes Cluster
│
├── Control Plane
│
├── Worker Node
│
├── Worker Node
└── Worker Node

There are two major concepts:

Control Plane

Makes decisions and manages the cluster.

Worker Nodes

Run your application workloads.

9. Kubernetes Architecture

The basic architecture is:

                    Kubernetes Cluster

              ┌─────────────────────────┐
              │      Control Plane      │
              │                         │
              │     API Server          │
              │     Scheduler           │
              │     Controller Manager  │
              │     etcd                │
              └────────────┬────────────┘
                           │
             ┌─────────────┴─────────────┐
             │                           │
       Worker Node                  Worker Node
       ┌─────────────┐              ┌─────────────┐
       │             │              │             │
       │   Pod       │              │    Pod      │
       │   Pod       │              │    Pod      │
       │             │              │             │
       └─────────────┘              └─────────────┘

Let's understand every component.

10. Control Plane

The Control Plane is essentially the brain/control layer of the Kubernetes cluster.

It decides:

What should be running in the cluster, and helps make the actual state match the desired state.

Important components:

Control Plane
│
├── API Server
├── Scheduler
├── Controller Manager
└── etcd
11. API Server

The API Server is the main entry point into Kubernetes.

When you run:

kubectl get pods

kubectl communicates with the Kubernetes API Server.

Conceptually:

kubectl
   ↓
API Server
   ↓
Kubernetes

Other Kubernetes components also communicate through the API.

Think of API Server as:

The gateway through which Kubernetes objects are accessed and managed.

12. etcd

etcd is a distributed key-value store used by Kubernetes to store cluster state.

For example, Kubernetes needs to keep information about:

Pods
Nodes
Deployments
Services
Configurations
Cluster state

Conceptually:

Kubernetes
    ↓
  etcd
    ↓
Cluster state

A simple mental model:

etcd = Kubernetes' persistent cluster-state database

Do not think of it as your application's MySQL/PostgreSQL database.

13. Scheduler

Suppose you create a Pod:

Pod
 ↓
Needs to run somewhere

Which worker node should run it?

That's one of the scheduler's responsibilities.

For example:

Worker 1 → CPU almost full
Worker 2 → Available
Worker 3 → Available

Scheduler may select an appropriate node based on scheduling constraints and available resources.

Conceptually:

Pod
 ↓
Scheduler
 ↓
Worker Node 2
14. Controller Manager

Kubernetes uses controllers to continuously compare:

Desired State
      vs
Current State

Example:

You say:

I want 3 replicas.

Desired state:

3 Pods

But currently:

2 Pods

A controller notices the difference and works toward:

2 Pods
   ↓
Create another Pod
   ↓
3 Pods

This is one of the most important Kubernetes concepts:

Kubernetes is largely based on desired state and reconciliation.

15. Worker Node

A worker node is a machine where application workloads run.

For example:

Worker Node
│
├── Pod
├── Pod
└── Pod

Important node components include:

Worker Node
│
├── kubelet
├── container runtime
└── kube-proxy

We'll study these separately.

16. What is a Pod?

This is extremely important.

A Pod is the smallest deployable unit in Kubernetes.

Most commonly:

Pod
 ↓
Container

Example:

Pod
└── Spring Boot Container

But a Pod can contain multiple closely related containers:

Pod
├── Application Container
└── Sidecar Container

Containers inside the same Pod share certain resources and networking.

For now, remember:

Pod = Kubernetes' basic execution unit for your application containers.

17. Full flow

Let's connect everything.

Suppose you want:

3 replicas of Payment Service

You submit a Kubernetes configuration.

kubectl
   ↓
API Server
   ↓
Desired state stored in cluster state
   ↓
Controller
   ↓
Creates/maintains Pods
   ↓
Scheduler selects nodes
   ↓
Worker Nodes
   ↓
kubelet
   ↓
Container runtime
   ↓
Containers start

Simplified:

                 Control Plane
          ┌────────────────────────┐
          │ API Server              │
          │ Scheduler               │
          │ Controllers             │
          │ etcd                    │
          └───────────┬────────────┘
                      │
             ┌────────┴────────┐
             ↓                 ↓
        Worker Node       Worker Node
        ┌───────────┐     ┌───────────┐
        │   Pod     │     │   Pod     │
        │ Container │     │ Container │
        └───────────┘     └───────────┘
18. The most important Kubernetes idea

If you remember only one concept from Day 1, remember this:

Desired State

You tell Kubernetes:

I want:
3 replicas
of my Payment Service

Kubernetes continuously tries to make reality match that requirement.

Desired State
     │
     │ reconcile
     ↓
Actual State

Example:

Desired = 3 Pods

Actual = 3 Pods
        ↓
      Good


One Pod crashes

Desired = 3
Actual  = 2

        ↓
Controller
        ↓
Create Pod
        ↓
Actual = 3

This reconciliation loop is fundamental to understanding Kubernetes.

19. Kubernetes solves our original problems
Problem	Kubernetes concept
Container crashes	Self-healing/reconciliation
Server/node failure	Rescheduling/workload distribution
Traffic increases	Scaling
Deployment downtime	Rolling updates
Changing Pod IPs	Services/service discovery
Many containers	Orchestration
Need to specify desired deployment	Kubernetes objects/configuration
Need to choose where workloads run	Scheduler
20. Interview answer: "What is Kubernetes?"

A good SDE-1 answer:

Kubernetes is an open-source container orchestration platform used to deploy, manage, scale, and maintain containerized applications across a cluster of machines. It provides features such as scheduling, service discovery, scaling, self-healing, and rolling deployments. Its architecture mainly consists of a control plane that manages the cluster and worker nodes that run application workloads in Pods.

That's enough for the basic interview question.

Day 1 mental model

Remember this hierarchy:

Kubernetes Cluster
│
├── Control Plane
│   ├── API Server
│   ├── Scheduler
│   ├── Controller Manager
│   └── etcd
│
└── Worker Nodes
    │
    ├── Pod
    │   └── Container
    │
    ├── Pod
    │   └── Container
    │
    └── Pod
        └── Container

And the most important relationship:

Control Plane
      ↓
Manages cluster
      ↓
Worker Nodes
      ↓
Pods
      ↓
Containers
What to learn next

For Day 2, the natural next step is:

Pod → Container → ReplicaSet → Deployment → Service

because these are the objects you'll use constantly when deploying an application to Kubernetes.