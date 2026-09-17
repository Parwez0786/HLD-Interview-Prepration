Kafka Controller — Detailed Interview Tutorial

The Kafka Controller is the component responsible for managing the cluster-level metadata and leadership decisions.

A simple way to remember it:

Kafka brokers store and serve data. The Controller manages the cluster and decides who should be leader.

1. Why do we need a Controller?

Suppose we have 3 Kafka brokers:

Broker 1
Broker 2
Broker 3

And a topic:

orders

Partition 0
Partition 1
Partition 2

Each partition has replicas:

Partition 0
Leader   → Broker 1
Replica  → Broker 2
Replica  → Broker 3

Now suppose Broker 1 crashes.

Kafka needs to answer:

"Who should become the new leader for Partition 0?"

Some component needs to coordinate this decision.

That is one of the Controller's important responsibilities.

2. What is the Kafka Controller?

The Controller is a Kafka cluster component responsible for cluster management.

It manages things such as:

Partition leadership
Leader election
Broker failure detection
Replica state changes
Cluster metadata
Topic/partition changes
Coordinating broker membership

Think:

                    Kafka Cluster
                         |
                    Controller
                         |
        --------------------------------
        |              |               |
   Broker 1        Broker 2        Broker 3
        |              |               |
    partitions      partitions      partitions

The Controller does not normally process your application's messages.

For example:

Producer
   |
   v
Broker
   |
   v
Partition

The Controller is not sitting in the middle of every message.

Instead, it manages the cluster state.

3. Controller responsibilities
Responsibility 1 — Partition Leadership

Every partition has one leader.

Example:

orders-0

Broker 1 → Leader
Broker 2 → Replica
Broker 3 → Replica

Producers and consumers generally interact with the partition leader.

The Controller keeps track of leadership.

4. What happens when a leader fails?

Suppose:

Partition 0

Broker 1 → Leader
Broker 2 → Replica
Broker 3 → Replica

Broker 1 crashes.

Now:

Broker 1 ❌
Broker 2
Broker 3

Kafka needs another leader.

If Broker 2 is an eligible replica, it can become:

Partition 0

Broker 2 → New Leader
Broker 3 → Replica

The Controller coordinates this leadership change.

5. Leader Election

Leader election means:

Selecting another replica to become the leader when the current leader is unavailable.

Example:

Before:

Partition 0
     |
     +---- Broker 1 → Leader
     +---- Broker 2 → Replica
     +---- Broker 3 → Replica

Broker 1 fails:

Broker 1 ❌

Controller detects the failure:

Controller
    |
    | detect failure
    v
Choose eligible replica
    |
    v
Broker 2 becomes leader

Now:

Partition 0

Broker 2 → Leader
Broker 3 → Replica

Clients eventually refresh their metadata and start sending requests to Broker 2.

6. What is an ISR?

This is very important for Controller interviews.

ISR = In-Sync Replicas

Suppose:

Partition 0

Broker 1 → Leader
Broker 2 → ISR
Broker 3 → ISR

All three are sufficiently caught up.

If Broker 1 fails, Kafka can select an eligible ISR replica.

For example:

Broker 1 ❌

Broker 2 → New Leader
Broker 3 → Replica

Why does Kafka prefer ISR?

Because ISR replicas are considered sufficiently synchronized with the leader, reducing the possibility of losing acknowledged data.

7. What if a replica is not in ISR?

Suppose:

Broker 1 → Leader
Broker 2 → ISR
Broker 3 → Not in ISR

Broker 1 crashes.

Normally Kafka prefers:

Broker 2

rather than:

Broker 3

because Broker 2 is in the ISR.

There are configuration and failure scenarios where an out-of-sync replica may be allowed to become leader, but this can involve potential data loss.

Interview answer:

"Kafka generally prefers an in-sync replica for leader election because it is caught up with the leader."

8. Broker Failure Detection

The Controller also monitors broker membership.

Imagine:

Broker 1
Broker 2
Broker 3

Broker 2 suddenly crashes.

Kafka needs to know:

Broker 2 → Dead

Then it can determine which partitions were led by Broker 2 and initiate leadership changes where necessary.

Conceptually:

Broker failure
      ↓
Controller detects
      ↓
Identify affected partitions
      ↓
Select new leaders
      ↓
Update cluster metadata
      ↓
Clients learn new metadata
9. Cluster Metadata

Kafka needs to know things such as:

Which brokers exist?
Which topics exist?
How many partitions?
Which replicas belong to each partition?
Who is the leader?
Which replicas are in ISR?

For example:

Topic: orders

Partition 0
Leader: Broker 1
Replicas: Broker 1,2,3

Partition 1
Leader: Broker 2
Replicas: Broker 2,3,1

This information is cluster metadata.

The Controller manages changes to this cluster state.

10. Controller vs Broker

This distinction is important.

Broker

Main responsibilities:

Produce messages
Consume messages
Store logs
Replicate partition data
Serve client requests
Controller

Main responsibilities:

Manage cluster metadata
Manage partition leadership
Coordinate leader elections
React to broker failures
Manage cluster-level changes

Think:

              Controller
                  |
        "Who should be leader?"
                  |
        ---------------------
        |         |         |
     Broker 1  Broker 2  Broker 3
11. What happens when a broker crashes?

Let's take a complete example.

Initially:

Topic: payment

Partition 0:

Broker 1 → Leader
Broker 2 → Replica
Broker 3 → Replica

Producer:

Producer
   |
   v
Broker 1
   |
   v
Partition 0

Now Broker 1 crashes:

Broker 1 ❌

The Controller detects the broker failure.

It knows:

Partition 0 was led by Broker 1

It selects an eligible replica:

Broker 2

Then:

Partition 0

Broker 2 → New Leader
Broker 3 → Replica

Cluster metadata is updated.

The producer may initially try Broker 1 and get a metadata/leader-related error.

It refreshes metadata:

Producer
   |
   | refresh metadata
   v
Broker 2
   |
   v
Partition 0

The application can continue.

12. Historical Architecture — ZooKeeper

Older Kafka versions used ZooKeeper for cluster coordination.

Architecture:

                ZooKeeper
                    |
              Kafka Controller
                    |
       ---------------------------
       |            |            |
    Broker 1     Broker 2     Broker 3

ZooKeeper helped Kafka coordinate things such as:

Broker membership
Controller election
Cluster metadata coordination

There was still a Kafka Controller, but ZooKeeper was an external coordination system.

13. Problems with the ZooKeeper architecture

Kafka had two separate systems:

Kafka
+
ZooKeeper

This increased operational complexity.

You had to manage:

Kafka cluster
      +
ZooKeeper cluster

There were also separate mechanisms for maintaining Kafka's metadata and coordination state.

Kafka eventually moved toward a self-contained architecture.

That architecture is called:

KRaft

KRaft = Kafka Raft

It uses the Raft consensus protocol to manage Kafka's metadata.

14. What is KRaft?

KRaft allows Kafka to manage its own metadata without requiring ZooKeeper.

Instead of:

Kafka
  |
ZooKeeper

Modern Kafka can use:

Kafka
  |
KRaft metadata quorum

Some Kafka nodes participate in the metadata quorum.

These are called controllers in KRaft mode.

15. KRaft Architecture

Simplified:

             KRaft Controller Quorum

          Controller 1
               |
        -----------------
        |               |
 Controller 2       Controller 3
        |
        |
   Metadata Log
        |
        v
   Kafka Brokers

The controllers use the Raft consensus mechanism to maintain consistent metadata.

One controller acts as the active leader for metadata operations, while the others provide quorum participation.

16. Why multiple controllers?

Because having only one controller would create a single point of failure.

Suppose:

Controller 1 → Active
Controller 2 → Standby
Controller 3 → Standby

Controller 1 fails:

Controller 1 ❌

The remaining controllers can elect another controller as leader.

For example:

Controller 2 → New active controller

This is based on the Raft consensus mechanism.

17. KRaft metadata log

One of the important KRaft concepts is the metadata log.

Kafka stores cluster metadata changes in a replicated log.

For example:

Metadata Log

1. Create topic orders
2. Create partition orders-0
3. Broker 1 registered
4. Broker 2 registered
5. Leader = Broker 1
6. Leader = Broker 2

This allows the controller quorum to maintain a consistent view of cluster metadata.

18. ZooKeeper vs KRaft
Feature	ZooKeeper architecture	KRaft
External ZooKeeper	Required	Not required
Metadata management	Kafka + ZooKeeper	Kafka itself
Consensus/coordination	ZooKeeper	Raft
Operational components	Kafka + ZooKeeper	Kafka
Metadata storage	ZooKeeper-based coordination plus Kafka state	Kafka metadata log
Modern Kafka architecture	Historical	Current direction

The key interview statement:

KRaft removes Kafka's dependency on ZooKeeper by using a Kafka-native metadata quorum based on Raft.

19. Controller Quorum vs Broker

In KRaft, it is useful to distinguish:

Controller
   ↓
Manages metadata and cluster state

Broker
   ↓
Stores and serves application data

A Kafka deployment can have nodes that perform controller and broker roles depending on the deployment configuration.

For production setups, controller and broker responsibilities can also be separated onto dedicated nodes.

20. Important interview question
Q: Does the Controller handle every Kafka message?

No.

The Controller manages cluster-level operations.

Normal message flow is:

Producer
   |
   v
Partition Leader
   |
   v
Replicas

The Controller is not involved in every produce/consume request.

21. Another important question
Q: What happens if the Controller fails?

In modern KRaft Kafka:

Active Controller
       ↓
      fails
       ↓
Controller quorum
       ↓
New controller leader elected
       ↓
Cluster management continues

The metadata quorum provides high availability for controller leadership.

22. Controller vs Partition Leader

This is a common confusion.

They are not the same thing.

Controller

Manages cluster-level decisions.

Controller
    |
    +---- Partition 0 → Leader Broker 1
    +---- Partition 1 → Leader Broker 2
    +---- Partition 2 → Leader Broker 3
Partition Leader

Handles client requests for a specific partition.

Partition 0
    |
Broker 1 → Leader

So:

Controller decides/coordinates who should lead. Partition leader serves the partition's data traffic.

23. Interview-ready answer

If interviewer asks:

"What is Kafka Controller?"

You can say:

"Kafka Controller is responsible for managing cluster-level metadata and coordination. It handles things like broker membership, partition leadership, and leader elections. For example, if a broker hosting the leader of a partition fails, the controller detects the failure and coordinates election of an eligible replica as the new leader. Historically Kafka used ZooKeeper for cluster coordination and controller election, but modern Kafka uses KRaft, where Kafka maintains a metadata quorum using the Raft consensus protocol, removing the ZooKeeper dependency."

24. One-line memory trick

Remember:

BROKER
→ stores and serves data

PARTITION LEADER
→ handles requests for that partition

CONTROLLER
→ manages cluster and leadership

KRAFT
→ Kafka manages its own metadata using Raft

ZOOKEEPER
→ historical external coordination system

And the most important flow:

Broker fails
     ↓
Controller detects failure
     ↓
Affected partition identified
     ↓
Eligible ISR replica selected
     ↓
New partition leader
     ↓
Metadata updated
     ↓
Clients refresh metadata
     ↓
Traffic goes to new leader

This is the core Controller + KRaft story you should be able to explain in an SDE-1 HLD/Kafka interview.