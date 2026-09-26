# Day 2 — Kubernetes Architecture

Today the main goal is to understand **who does what** inside Kubernetes and, more importantly, **how a request travels through the cluster**.

---

## 1. Big Picture

A Kubernetes cluster has two major parts:

```
                 Kubernetes Cluster
                        |
          +-------------+-------------+
          |                           |
     Control Plane                 Worker Nodes
     (Brain)                       (Machines)
          |                           |
   +------+-------+             +-----+------+
   |      |       |             |     |      |
 API    etcd   Scheduler      kubelet kube-proxy Runtime
Server          Controller
                Manager
```

Think of it like this:

- **Control Plane** → decides what should happen
- **Worker Node** → actually runs the application

---

## 2. Control Plane

The control plane contains the components that manage the cluster.

Main components:

```
Control Plane
│
├── kube-apiserver
├── etcd
├── kube-scheduler
├── kube-controller-manager
└── cloud-controller-manager
```

---

## 3. kube-apiserver

This is the **entry point** to Kubernetes.

Whenever you use:

```bash
kubectl get pods
kubectl create deployment nginx ...
kubectl delete pod nginx
```

your request goes to the API Server.

Think of it as:

> The receptionist / security gate of Kubernetes.

It handles:

- Authentication
- Authorization
- Validation
- Kubernetes API requests
- Communication with other components

Almost every Kubernetes component communicates through the API Server.

```
kubectl
   |
   v
kube-apiserver
```

### Example

You run:

```bash
kubectl create deployment nginx --image=nginx
```

The request goes:

```
kubectl
   |
   v
API Server
```

The API Server validates the request and processes it.

---

## 4. etcd

etcd is Kubernetes' **distributed key-value database**.

It stores the cluster's desired/current state information, such as:

- Pods
- Deployments
- Services
- ConfigMaps
- Secrets
- Nodes
- Namespaces
- etc.

For example:

```
Deployment:
    name = nginx
    replicas = 3
```

This information is persisted in etcd.

Think:

> **etcd = Kubernetes' database**

**Important interview point:**

- The API Server communicates with etcd.
- Most Kubernetes components don't directly modify etcd.

```
              +--------+
              | kubectl|
              +---+----+
                  |
                  v
           +-------------+
           | API Server  |
           +------+------+
                  |
                  v
             +---------+
             |  etcd   |
             +---------+
```

---

## 5. kube-scheduler

The scheduler decides:

> Which worker node should run a newly created Pod?

Suppose you create:

```
Pod: nginx
```

There are three worker nodes:

| Node   | CPU available |
|--------|---------------|
| Node A | 10%           |
| Node B | 70%           |
| Node C | 40%           |

The scheduler considers things such as:

- Available resources
- CPU/memory requirements
- Node constraints
- Affinity / anti-affinity
- Taints and tolerations
- Other scheduling rules

Then it selects an appropriate node.

```
             Scheduler
                 |
       +---------+---------+
       |         |         |
     Node A    Node B    Node C
                         ^
                         |
                    selected node
```

### Important

The scheduler **doesn't run the container**.

It only decides:

> "This Pod should run on Node B."

---

## 6. kube-controller-manager

This component runs various Kubernetes controllers.

A controller continuously compares:

```
Desired State
      vs
Current State
```

and tries to make them equal.

For example:

You request:

```
replicas = 3
```

But currently:

```
running pods = 2
```

The controller notices:

```
Desired = 3
Current = 2
```

So it creates another Pod.

```
Desired: 3 Pods

Current:
Pod 1
Pod 2

        ↓

Controller creates Pod 3

        ↓

Pod 1
Pod 2
Pod 3
```

This is one of the most important Kubernetes concepts:

> Kubernetes is continuously trying to make actual state match desired state.

---

## 7. cloud-controller-manager

This component integrates Kubernetes with cloud providers.

```
Kubernetes
     |
     v
Cloud Controller Manager
     |
     +---- AWS
     +---- Azure
     +---- GCP
```

It can handle cloud-specific resources such as:

- Load balancers
- Cloud nodes
- Routes
- Cloud volumes

For example, when you create:

```yaml
kind: Service
spec:
  type: LoadBalancer
```

in a cloud environment, Kubernetes can interact with the cloud provider to provision an external load balancer.

---

## 8. Worker Node

Worker nodes are the machines where your applications actually run.

A worker node typically has:

```
Worker Node
│
├── kubelet
├── kube-proxy
└── Container Runtime
```

And of course, your application **Pods** run there.

---

## 9. kubelet

The kubelet is the agent running on **every worker node**.

Its job is basically:

> "Make sure the Pods assigned to this node are actually running."

Suppose the scheduler says:

> Run nginx Pod on Node A

The kubelet on Node A receives the relevant Pod specification through the Kubernetes control mechanisms and makes sure the Pod is created.

It talks to the container runtime.

```
API Server
     |
     v
Node A
     |
  kubelet
     |
     v
Container Runtime
     |
     v
Container
```

The kubelet also monitors containers and reports status back to Kubernetes.

---

## 10. Container Runtime

The container runtime is responsible for actually running containers.

Examples include runtimes such as:

- containerd
- CRI-O

The kubelet communicates with the runtime through the **Container Runtime Interface (CRI)**.

```
kubelet
   |
   | CRI
   v
containerd
   |
   v
Container
```

So:

```
Scheduler
    ↓
selects Node
    ↓
kubelet
    ↓
container runtime
    ↓
container starts
```

---

## 11. kube-proxy

kube-proxy is associated with Kubernetes **Service networking**.

Suppose you have:

```
Service
   |
   +---- Pod 1
   +---- Pod 2
   +---- Pod 3
```

The Service provides a **stable virtual endpoint** while Pods can come and go.

kube-proxy helps implement the networking rules that direct Service traffic toward the appropriate Pods.

Modern Kubernetes networking can use different implementations, and some clusters replace or supplement kube-proxy with other networking mechanisms.

**For interview purposes:**

> kube-proxy helps implement Kubernetes Service networking and traffic forwarding.

---

## 12. Complete Example

Let's say you run:

```bash
kubectl create deployment nginx --image=nginx --replicas=3
```

Now let's follow the request.

### Step 1 — kubectl

```
kubectl
   |
   | HTTP request
   v
API Server
```

kubectl sends the request to the Kubernetes API Server.

### Step 2 — API Server

The API Server:

- Authenticates the request
- Authorizes it
- Validates it
- Processes the Kubernetes object

Then the desired state is stored in etcd.

```
kubectl
   |
   v
API Server
   |
   v
etcd

Deployment:
nginx
replicas = 3
```

### Step 3 — Controller

The controller notices that the Deployment requires Pods.

```
Desired:
3 Pods

Current:
0 Pods
```

So controllers create the required Pod objects.

```
Deployment
    |
    v
ReplicaSet
    |
    v
3 Pods
```

### Step 4 — Scheduler

The newly created Pods don't initially have a node assigned.

Scheduler notices them:

```
Pod 1 → no node
Pod 2 → no node
Pod 3 → no node
```

It evaluates available worker nodes and assigns Pods to suitable nodes.

For example:

```
Pod 1 → Node A
Pod 2 → Node B
Pod 3 → Node A
```

### Step 5 — kubelet

On Node A:

- kubelet sees that Pods assigned to Node A need to run.

On Node B:

- kubelet does the same.

### Step 6 — Container Runtime

The kubelet asks the container runtime to create/run the containers.

```
kubelet
   |
   v
containerd
   |
   v
nginx container
```

Now nginx is running.

---

## 13. The Correct Mental Model

Your original flow is useful, but there is one important correction.

You wrote:

```
kubectl
   ↓
API Server
   ↓
etcd
   ↓
Scheduler
   ↓
Worker Node
   ↓
kubelet
   ↓
Container Runtime
   ↓
Container
```

Don't think of it as one straight synchronous pipeline.

A better mental model is:

```
                    CONTROL PLANE

kubectl
   |
   v
API Server
   |
   +-----------> etcd
   |
   +-----------> Controllers
   |                 |
   |                 v
   |            Create/maintain
   |                Pods
   |
   +-----------> Scheduler
                     |
                     v
                Selects Node
                     |
                     v
              Worker Node
                     |
                  kubelet
                     |
                     v
             Container Runtime
                     |
                     v
                 Container
```

And then status flows back:

```
Container
    |
    v
kubelet
    |
    v
API Server
    |
    v
etcd / Kubernetes state
```

---

## 14. Most Important Interview Concept: Desired vs Actual State

This is the heart of Kubernetes.

Suppose you declare:

```yaml
replicas: 3
```

Kubernetes interprets this as:

> **Desired State = 3 Pods**

If one Pod crashes:

```
Desired = 3
Current = 2
```

The controller notices the difference.

```
Controller
    |
    | "We need one more Pod"
    v
Create Pod
    |
    v
Scheduler
    |
    v
Worker Node
    |
    v
kubelet
    |
    v
Container Runtime
    |
    v
New Container
```

Eventually:

```
Desired = 3
Current = 3
```

This reconciliation process is continuously happening.

---

## 15. Who Does What?

| Component                 | Simple responsibility                          |
|---------------------------|------------------------------------------------|
| kube-apiserver            | Entry point / API                              |
| etcd                      | Stores cluster state                           |
| kube-scheduler            | Chooses node for Pod                           |
| controller-manager        | Makes actual state match desired state         |
| cloud-controller-manager  | Cloud-provider integration                     |
| kubelet                   | Makes sure Pods run on its node                |
| kube-proxy                | Helps implement Service networking             |
| Container Runtime         | Runs containers                                |

### Easy way to remember

```
API Server     → Talk
etcd           → Store
Scheduler      → Choose
Controller     → Reconcile
kubelet        → Run/manage Pods on node
kube-proxy     → Service networking
Runtime        → Run containers
```

---

## 16. Interview Cross-Questions

After learning this, you should be able to answer:

### Q1. What happens when you run `kubectl apply`?

Explain:

```
kubectl
 → API Server
 → authentication / authorization / validation
 → state persisted
 → controllers reconcile
 → scheduler assigns node
 → kubelet
 → container runtime
 → container
```

### Q2. What happens if a Pod crashes?

Explain:

```
Pod crashes
   ↓
kubelet detects failure
   ↓
runtime / kubelet attempts recovery
   ↓
if desired replica count is no longer satisfied,
controller creates / reconciles replacement Pod
```

### Q3. Does scheduler create containers?

**No.**

Scheduler only selects a suitable node.

### Q4. Does kubelet choose the node?

**No.**

Scheduler chooses the node.

### Q5. Does kubelet directly talk to etcd?

**Normally, no.** Kubernetes components generally interact through the API Server.

### Q6. Where is Kubernetes state stored?

**etcd.**

### Q7. Who actually runs the container?

The **container runtime**, instructed by the kubelet.

### Q8. What happens if the API Server goes down?

Existing containers can continue running on nodes, but normal cluster management / control operations that depend on the API Server are disrupted.

---

## Day 2 takeaway

If you remember only this, remember:

```
             CONTROL PLANE
                  |
        +---------+---------+
        |         |         |
    API Server  Scheduler  Controller
        |
       etcd
        |
        v
   "What should happen?"
        |
        v
     Worker Node
        |
      kubelet
        |
  Container Runtime
        |
        v
    Container
```

- **Control Plane** decides and reconciles.
- **Worker Nodes** execute.

That distinction is the foundation for understanding Kubernetes architecture.
