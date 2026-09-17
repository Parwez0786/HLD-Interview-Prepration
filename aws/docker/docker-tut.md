# Docker Interview Preparation — Complete Tutorial

---

## Table of Contents

- [1. What is Docker?](#1-what-is-docker)
- [2. Why Docker?](#2-why-docker)
- [3. Docker Architecture](#3-docker-architecture)
- [4. Container vs Image](#4-container-vs-image)
- [5. Image Layers](#5-image-layers)
- [6. Dockerfile](#6-dockerfile)
- [7. Important Dockerfile Instructions](#7-important-dockerfile-instructions)
- [8. FROM](#8-from)
- [9. RUN vs CMD vs ENTRYPOINT](#9-run-vs-cmd-vs-entrypoint)
- [10. CMD vs ENTRYPOINT](#10-cmd-vs-entrypoint)
- [11. EXPOSE](#11-expose)
- [12. Docker Port Mapping](#12-docker-port-mapping)
- [13. Important Docker Commands](#13-important-docker-commands)
- [14. Docker Logs](#14-docker-logs)
- [15. docker exec](#15-docker-exec)
- [16. Container Lifecycle](#16-container-lifecycle)
- [17. Container vs Virtual Machine](#17-container-vs-virtual-machine)
- [18. How Docker Provides Isolation](#18-how-docker-provides-isolation)
- [19. Docker Networking](#19-docker-networking)
- [20. Container-to-Container Communication](#20-container-to-container-communication)
- [21. Docker DNS](#21-docker-dns)
- [22. Volumes](#22-volumes)
- [23. Bind Mount vs Volume](#23-bind-mount-vs-volume)
- [24. Docker Compose](#24-docker-compose)
- [25. Docker Compose Architecture](#25-docker-compose-architecture)
- [26. depends_on](#26-depends_on)
- [27. Health Checks](#27-health-checks)
- [28. Environment Variables](#28-environment-variables)
- [29. Docker Secrets](#29-docker-secrets)
- [30. Multi-Stage Docker Build](#30-multi-stage-docker-build)
- [31. .dockerignore](#31-dockerignore)
- [32. Docker Build Context](#32-docker-build-context)
- [33. Docker Image Tagging](#33-docker-image-tagging)
- [34. Docker Registry](#34-docker-registry)
- [35. Docker Image Optimization](#35-docker-image-optimization)
- [36. Docker Security](#36-docker-security)
- [37. Docker Restart Policies](#37-docker-restart-policies)
- [38. What Happens When a Container Crashes?](#38-what-happens-when-a-container-crashes)
- [39. Docker and Kubernetes](#39-docker-and-kubernetes)
- [40. Docker vs Kubernetes](#40-docker-vs-kubernetes)
- [41. Docker with Spring Boot](#41-docker-with-spring-boot)
- [42. Spring Boot + MySQL with Docker](#42-spring-boot--mysql-with-docker)
- [43. Example Compose Setup](#43-example-compose-setup)
- [44. What Does docker compose down -v Do?](#44-what-does-docker-compose-down--v-do)
- [45. Docker Networking Interview Questions](#45-docker-networking-interview-questions)
- [46. Important Storage Question](#46-important-storage-question)
- [47. Docker Container vs Process](#47-docker-container-vs-process)
- [48. What Happens During docker run?](#48-what-happens-during-docker-run)
- [49. docker run vs docker start](#49-docker-run-vs-docker-start)
- [50. docker create vs docker run](#50-docker-create-vs-docker-run)
- [51. docker stop vs docker kill](#51-docker-stop-vs-docker-kill)
- [52. COPY vs ADD](#52-copy-vs-add)
- [53. ARG vs ENV](#53-arg-vs-env)
- [54. ENTRYPOINT Shell vs Exec Form](#54-entrypoint-shell-vs-exec-form)
- [55. PID 1 in Docker](#55-pid-1-in-docker)
- [56. Docker Resource Limits](#56-docker-resource-limits)
- [57. Docker Health vs Process Running](#57-docker-health-vs-process-running)
- [58. Docker Debugging Scenario](#58-docker-debugging-scenario)
- [59. Scenario: Container Immediately Exits](#59-scenario-container-immediately-exits)
- [60. Scenario: Port Already in Use](#60-scenario-port-already-in-use)
- [61. Scenario: Backend Can't Connect to MySQL](#61-scenario-backend-cant-connect-to-mysql)
- [62. Scenario: Container Is Using Too Much Memory](#62-scenario-container-is-using-too-much-memory)
- [63. docker inspect](#63-docker-inspect)
- [64. docker stats](#64-docker-stats)
- [65. Docker Prune](#65-docker-prune)
- [66. Docker Compose Commands You Must Know](#66-docker-compose-commands-you-must-know)
- [67. Image vs Container vs Volume vs Network](#67-docker-image-vs-container-vs-volume-vs-network)
- [68–72. Question Banks](#68-docker-interview-questions--basic)
- [73. Production Questions](#73-production-questions)
- [74. CI/CD + Docker](#74-cicd--docker)
- [75. Docker + Kafka](#75-docker--kafka)
- [76. Docker + Redis](#76-docker--redis)
- [77–81. Advanced Questions](#77-advanced-question-why-containers-are-fast)
- [82. Real-World Interview Scenario](#82-important-real-world-interview-scenario)
- [83. 15 Most Important Questions](#83-15-most-important-docker-questions-for-your-sde-1-interview)
- [84. Resume Follow-ups](#84-questions-interviewer-may-ask-based-on-your-resume)
- [85. A Strong 2-Minute Docker Explanation](#85-a-strong-2-minute-docker-explanation)
- [86. Docker Preparation Roadmap](#86-your-docker-preparation-roadmap)

---

## 1. What is Docker?

**Interview answer**

> Docker is a containerization platform that packages an application along with its dependencies, configuration, libraries, and runtime into a portable unit called a container.

The main benefit is **consistency**: the same container image can run in development, testing, and production environments.

**Without Docker:**

```text
Developer Machine
    |
    | Java 21
    | Maven
    | MySQL
    | Redis
    |
    v
Application
```

A developer may have Java 21, MySQL 8, Redis 7, while production may have different versions.

**With Docker:**

```text
Docker Image
   |
   +-- Application
   +-- Java Runtime
   +-- Dependencies
   +-- Configuration
   |
   v
Container
```

The environment becomes much more consistent.

---

## 2. Why Docker?

Common interview question: **Why do we need Docker?**

Main problems Docker solves:

### 1. Environment inconsistency

"It works on my machine" — Docker packages the environment with the application.

### 2. Dependency management

Instead of manually installing Java, Node, Redis, MySQL, Kafka, Nginx, you can run containers.

### 3. Isolation

Each container has an isolated process/filesystem/network environment.

### 4. Portability

The same image can run on: Laptop, AWS, Azure, GCP, Kubernetes, CI/CD server.

### 5. Faster deployment

```text
Download image
      ↓
Run container
      ↓
Application starts
```

---

## 3. Docker Architecture

This is extremely important.

```text
                Docker Client
                     |
                     | Docker API
                     v
              Docker Daemon
              (dockerd)
                     |
       +-------------+-------------+
       |             |             |
       v             v             v
    Images       Containers      Networks
       |
       v
    Registry
   Docker Hub
```

### Components

**Docker Client** — when you execute `docker run nginx`, the Docker CLI acts as the client. It communicates with the Docker daemon.

**Docker Daemon** — `dockerd` is responsible for: images, containers, networks, volumes, container lifecycle.

**Docker Registry** — stores Docker images. Examples: Docker Hub, AWS ECR, Google Artifact Registry, Azure Container Registry, GitHub Container Registry.

---

## 4. Container vs Image

One of the most frequently asked questions.

**Image** — a read-only template used to create containers. Example: `mysql:8`.

Think: **Image = Class, Container = Object**.

**Container** — a running/created instance of an image.

```text
MySQL Image
     |
     +---- Container 1
     |
     +---- Container 2
```

You can run multiple containers from the same image.

---

## 5. Image Layers

Docker images are composed of layers.

Example:

```dockerfile
FROM ubuntu
RUN apt update
RUN apt install openjdk-21-jdk
COPY app.jar /app.jar
CMD ["java", "-jar", "/app.jar"]
```

Conceptually:

```text
Application Layer
       ↓
Java Layer
       ↓
Ubuntu Layer
       ↓
Base Layer
```

Each Dockerfile instruction can create a layer.

**Why layers?** Docker can reuse unchanged layers.

Suppose: Layer 1 = Ubuntu, Layer 2 = Java, Layer 3 = Dependencies, Layer 4 = Application.

If only the application changes: Layers 1–3 reused, Layer 4 rebuilt. This makes builds faster.

---

## 6. Dockerfile

A Dockerfile describes how to build an image.

Example Spring Boot application:

```dockerfile
FROM eclipse-temurin:21-jre

WORKDIR /app

COPY target/app.jar app.jar

EXPOSE 8080

ENTRYPOINT ["java", "-jar", "app.jar"]
```

Build: `docker build -t my-spring-app .`

Run: `docker run -p 8080:8080 my-spring-app`

---

## 7. Important Dockerfile Instructions

| Instruction | Purpose |
| --- | --- |
| `FROM` | Base image |
| `RUN` | Execute command during build |
| `COPY` | Copy files |
| `ADD` | Copy files + additional features |
| `WORKDIR` | Set working directory |
| `CMD` | Default command |
| `ENTRYPOINT` | Main executable |
| `EXPOSE` | Documents container port |
| `ENV` | Environment variable |
| `ARG` | Build-time variable |
| `USER` | Run as specific user |
| `VOLUME` | Declare volume |

---

## 8. FROM

```dockerfile
FROM eclipse-temurin:21-jre
```

It specifies the base image.

- Java: `FROM eclipse-temurin:21-jre`
- Node: `FROM node:22`
- Nginx: `FROM nginx`

---

## 9. RUN vs CMD vs ENTRYPOINT

This is a very common interview question.

**RUN** — executed during image building.

```dockerfile
RUN apt-get update
```

```text
docker build
      ↓
RUN executes
      ↓
Image created
```

**CMD** — default command when container starts.

```dockerfile
CMD ["java", "-jar", "app.jar"]
```

CMD can be overridden. Example: `docker run image-name bash`

**ENTRYPOINT** — defines the main executable.

```dockerfile
ENTRYPOINT ["java", "-jar", "app.jar"]
```

Arguments can be appended: `docker run myapp --server.port=9090`

Conceptually: ENTRYPOINT + CMD.

---

## 10. CMD vs ENTRYPOINT

Suppose:

```dockerfile
ENTRYPOINT ["java", "-jar", "app.jar"]
CMD ["--server.port=8080"]
```

Running `docker run myapp` results conceptually in: `java -jar app.jar --server.port=8080`

If `docker run myapp --server.port=9090`, then CMD is replaced: `java -jar app.jar --server.port=9090`

---

## 11. EXPOSE

```dockerfile
EXPOSE 8080
```

**Important:** EXPOSE does **not** actually publish the port. It documents that the application listens on port 8080.

Actual port publishing happens with: `docker run -p 8080:8080 myapp`

---

## 12. Docker Port Mapping

Suppose your Spring Boot application listens on container port 8080.

```bash
docker run -p 9000:8080 myapp
```

Meaning:

```text
Host                         Container

localhost:9000  --------->   8080
```

Syntax: `-p HOST_PORT:CONTAINER_PORT`

---

## 13. Important Docker Commands

Memorize these.

| Task | Command |
| --- | --- |
| Check Docker version | `docker --version` |
| List images | `docker images` or `docker image ls` |
| List running containers | `docker ps` |
| List all containers | `docker ps -a` |
| Run container | `docker run nginx` |
| Run in background | `docker run -d nginx` (`-d` = detached) |
| Give container a name | `docker run --name my-nginx nginx` |
| Port mapping | `docker run -p 8080:80 nginx` |
| Stop | `docker stop my-nginx` |
| Start | `docker start my-nginx` |
| Restart | `docker restart my-nginx` |
| Remove container | `docker rm my-nginx` |
| Force remove | `docker rm -f my-nginx` |
| Remove image | `docker rmi nginx` |

---

## 14. Docker Logs

Very important for debugging.

```bash
docker logs container-name
docker logs -f container-name
```

Useful when:

```text
Container starts
      ↓
Application crashes
      ↓
docker logs
      ↓
Find exception
```

For Spring Boot: `docker logs -f spring-app`

---

## 15. docker exec

Allows you to execute commands inside a running container.

```bash
docker exec -it container-name bash
```

If bash isn't installed: `docker exec -it container-name sh`

Example: `docker exec -it redis redis-cli`

---

## 16. Container Lifecycle

```text
             docker create
                   |
                   v
               CREATED
                   |
             docker start
                   |
                   v
                RUNNING
                /     \
               /       \
           stop        crash
             |            |
             v            v
          STOPPED      EXITED
             |
          start
             |
             v
          RUNNING
```

A stopped container still exists. Therefore `docker ps` may not show it. Use `docker ps -a`.

---

## 17. Container vs Virtual Machine

Very common.

**Virtual Machine**

```text
Hardware
   ↓
Host OS
   ↓
Hypervisor
   ↓
Guest OS
   ↓
Application
```

Each VM has its own OS kernel.

**Docker Container**

```text
Hardware
   ↓
Host OS
   ↓
Docker Engine
   ↓
Container
   ↓
Application
```

Containers share the host OS kernel.

Therefore containers are generally: lighter, faster to start, more resource efficient.

But they provide a different isolation model from VMs.

---

## 18. How Docker Provides Isolation

Docker uses Linux kernel features such as:

**Namespaces** — isolation of process IDs, network, mounts, hostname, users.

Example: Container A has PID 1, 2, 3. Container B has PID 1, 2. Processes are isolated by namespaces.

**cgroups** — control resource usage.

```text
Container A
CPU: 1 core
Memory: 512 MB
```

You can limit resources:

```bash
docker run --memory=512m --cpus=1 myapp
```

---

## 19. Docker Networking

This is very important for backend interviews.

Docker provides networks such as: `bridge`, `host`, `none`.

Default commonly used network: **bridge**.

---

## 20. Container-to-Container Communication

Suppose Spring Boot → MySQL. Both containers are on the same Docker network.

```bash
docker network create app-network

docker run \
  --name mysql \
  --network app-network \
  mysql:8

docker run \
  --name backend \
  --network app-network \
  myapp
```

The backend can connect using `mysql:3306` rather than `localhost:3306`.

**Important interview point:** Inside a container, `localhost` means **the current container itself**. It does not mean another container.

---

## 21. Docker DNS

Docker networks provide service/container name resolution.

```text
backend
   |
   | MySQL hostname = mysql
   v
mysql
```

Docker resolves `mysql` → MySQL container IP.

This is why Compose applications commonly use:

```text
SPRING_DATASOURCE_URL: jdbc:mysql://mysql:3306/app
```

instead of `jdbc:mysql://localhost:3306/app`.

---

## 22. Volumes

Containers are **ephemeral**.

Suppose MySQL stores data inside a container. If you `docker rm mysql`, you can lose the container's writable-layer data.

For persistent data, use volumes.

```text
Container
    |
    v
Volume
    |
    v
Persistent Data
```

```bash
docker volume create mysql-data

docker run \
  --name mysql \
  -v mysql-data:/var/lib/mysql \
  mysql:8
```

Now MySQL data persists independently of the container lifecycle.

---

## 23. Bind Mount vs Volume

**Volume** — managed by Docker. `-v mysql-data:/var/lib/mysql`

Good for: database persistence, production container data.

**Bind mount** — maps a host directory. `-v ./src:/app/src`

Useful during development.

```text
Host
./src
  |
  v
Container
/app/src
```

---

## 24. Docker Compose

Very important for SDE interviews.

Suppose your application requires Spring Boot, MySQL, Redis, Kafka. Running everything manually becomes cumbersome.

Docker Compose allows you to define multiple services.

Example:

```yaml
services:

  backend:
    build: .
    ports:
      - "8080:8080"
    depends_on:
      - mysql
      - redis

  mysql:
    image: mysql:8
    environment:
      MYSQL_ROOT_PASSWORD: root
      MYSQL_DATABASE: app

  redis:
    image: redis:7
```

Start: `docker compose up`

Background: `docker compose up -d`

Stop: `docker compose down`

---

## 25. Docker Compose Architecture

```text
                 Docker Compose
                       |
        +--------------+--------------+
        |              |              |
        v              v              v
     Backend          MySQL         Redis
       8080            3306          6379
        |              |              |
        +--------------+--------------+
                       |
                Docker Network
```

---

## 26. depends_on

```yaml
backend:
  depends_on:
    - mysql
```

This means Compose starts the MySQL service before starting the backend container.

**Important interview trick:** `depends_on` does **not** necessarily mean MySQL is ready to accept connections.

Startup ordering and application readiness are different.

A better setup can use:

```yaml
healthcheck:
  test: ["CMD", "mysqladmin", "ping", "-h", "localhost"]
```

and appropriate dependency/readiness handling.

---

## 27. Health Checks

```dockerfile
HEALTHCHECK \
  --interval=30s \
  --timeout=5s \
  --retries=3 \
  CMD curl -f http://localhost:8080/actuator/health || exit 1
```

Docker can use this to determine whether the container is healthy.

---

## 28. Environment Variables

Never hardcode configuration like `DB_PASSWORD=abc123` inside the image.

Use environment variables:

```bash
docker run \
  -e DB_HOST=mysql \
  -e DB_USER=root \
  -e DB_PASSWORD=secret \
  myapp
```

Spring Boot:

```properties
spring.datasource.url=${DB_URL}
spring.datasource.username=${DB_USERNAME}
spring.datasource.password=${DB_PASSWORD}
```

---

## 29. Docker Secrets

For sensitive production configuration, don't casually put secrets directly into Dockerfiles or images.

In real deployments, secrets may be handled by: AWS Secrets Manager, AWS Parameter Store, Kubernetes Secrets, Vault, Docker Swarm Secrets.

---

## 30. Multi-Stage Docker Build

Very important for Java interviews.

**Bad approach:**

```dockerfile
FROM maven:3.9-eclipse-temurin-21
COPY . .
RUN mvn package
CMD ["java", "-jar", "target/app.jar"]
```

The final image contains build tooling.

**Better:**

```dockerfile
FROM maven:3.9-eclipse-temurin-21 AS build

WORKDIR /app

COPY pom.xml .
COPY src ./src

RUN mvn clean package -DskipTests


FROM eclipse-temurin:21-jre

WORKDIR /app

COPY --from=build /app/target/app.jar app.jar

ENTRYPOINT ["java", "-jar", "app.jar"]
```

Architecture:

```text
Build Stage
   |
   | Maven
   | Source code
   | Dependencies
   |
   v
app.jar
   |
   v
Runtime Stage
   |
   | JRE
   | app.jar
   |
   v
Final Image
```

**Advantages:** smaller image, fewer unnecessary tools, smaller attack surface, faster deployment.

---

## 31. .dockerignore

Similar to `.gitignore`.

Example:

```text
.git
.idea
target
node_modules
*.log
.env
```

Without `.dockerignore`, `docker build` context can include `.git`, `node_modules`, logs, source — making builds slower and unnecessarily including files.

---

## 32. Docker Build Context

When you run `docker build -t myapp .`, the `.` means current directory is the build context.

Docker sends that context to the build process. Therefore `.dockerignore` is important.

---

## 33. Docker Image Tagging

```bash
docker build -t myapp:1.0 .
```

repository = `myapp`, tag = `1.0`.

Run: `docker run myapp:1.0`

Avoid relying entirely on `latest` for production deployments because it is mutable and less explicit.

Prefer immutable/versioned tags, ideally including a commit SHA.

---

## 34. Docker Registry

Typical workflow:

```text
Developer
    |
    v
Dockerfile
    |
docker build
    |
    v
Docker Image
    |
docker push
    |
    v
Registry
    |
docker pull
    |
    v
Production
```

Example:

```bash
docker login
docker tag myapp:1.0 username/myapp:1.0
docker push username/myapp:1.0
```

---

## 35. Docker Image Optimization

**Interview question:** How would you reduce Docker image size?

1. Use smaller base image (JRE instead of full JDK when compilation isn't needed in the runtime image)
2. Multi-stage build — separate build and runtime
3. `.dockerignore` — don't copy unnecessary files
4. Minimize layers where appropriate
5. Don't install unnecessary packages
6. Use specific image tags instead of blindly using `latest`

---

## 36. Docker Security

Important interview topics.

- **Don't run application as root** — use `USER appuser`
- Use trusted/minimal base images
- Keep images updated
- **Don't put secrets in Dockerfile** — bad: `ENV PASSWORD=secret`
- Scan images — organizations commonly use image vulnerability scanners
- Limit privileges — avoid `--privileged` unless specifically required

---

## 37. Docker Restart Policies

Useful in production.

```bash
docker run --restart=unless-stopped myapp
```

Common policies: `no`, `always`, `on-failure`, `unless-stopped`.

Example: `docker run --restart=on-failure:3 myapp` — attempts to restart after failures, with the specified retry behavior.

---

## 38. What Happens When a Container Crashes?

Suppose Spring Boot crashes. The container process exits.

Check: `docker ps -a`, then `docker logs container`.

For automatic recovery: `--restart=on-failure`, or an orchestrator such as Kubernetes can restart/reschedule workloads.

---

## 39. Docker and Kubernetes

Very common SDE question: **Is Docker a replacement for Kubernetes?**

**No.** They solve different problems.

**Docker:** containerization, image building, container runtime/workflow.

**Kubernetes:** container orchestration, scaling, scheduling, service discovery, rolling deployments, self-healing.

```text
Docker Image
     |
     v
Kubernetes
     |
     +---- Pod
     |
     +---- Pod
     |
     +---- Pod
```

---

## 40. Docker vs Kubernetes

| Docker | Kubernetes |
| --- | --- |
| Builds/runs containers | Orchestrates containers |
| Image/container tooling | Cluster management |
| Single machine workflows | Distributed workloads |
| Basic networking | Service discovery/load balancing |
| Basic restart policies | Self-healing/scheduling |
| Compose for multi-container local setups | Production orchestration |

---

## 41. Docker with Spring Boot

A very common practical question for you.

Suppose Spring Boot on port 8080.

Build application: `mvn clean package`

Dockerfile:

```dockerfile
FROM eclipse-temurin:21-jre

WORKDIR /app

COPY target/app.jar app.jar

EXPOSE 8080

ENTRYPOINT ["java", "-jar", "app.jar"]
```

Build: `docker build -t spring-app .`

Run: `docker run -p 8080:8080 spring-app`

Test: `http://localhost:8080`

---

## 42. Spring Boot + MySQL with Docker

```text
              Docker Network
                    |
          +---------+---------+
          |                   |
          v                   v
     Spring Boot            MySQL
       :8080                 :3306
          |
          |
    jdbc:mysql://mysql:3306/app
```

**Important:** Inside Spring Boot, `mysql` is the hostname. Not `localhost`.

---

## 43. Example Compose Setup

```yaml
services:

  backend:
    build: .
    ports:
      - "8080:8080"
    environment:
      DB_HOST: mysql
      DB_PORT: 3306
      DB_NAME: app
      DB_USERNAME: root
      DB_PASSWORD: root
    depends_on:
      - mysql

  mysql:
    image: mysql:8
    environment:
      MYSQL_ROOT_PASSWORD: root
      MYSQL_DATABASE: app
    ports:
      - "3306:3306"
    volumes:
      - mysql-data:/var/lib/mysql

volumes:
  mysql-data:
```

Start: `docker compose up -d`

Check: `docker compose ps`

Logs: `docker compose logs backend`

Stop: `docker compose down`

---

## 44. What Does docker compose down -v Do?

This is an interview trap.

`docker compose down` removes containers/network created by Compose, but named volumes are normally retained.

`docker compose down -v` also removes the Compose-associated volumes.

Therefore **database data may be deleted**.

---

## 45. Docker Networking Interview Questions

**Q1. Why can't container A access container B using localhost?**

Because `localhost` = current container. Use the other container's DNS/service name on a shared network.

**Q2. How do containers communicate?**

Through Docker networks. Example: `backend → mysql:3306`, `backend → redis:6379`.

**Q3. Does every container get an IP?**

Containers attached to Docker networks generally receive network connectivity and an IP address, but applications should usually use service/container DNS names rather than hard-coding container IPs.

---

## 46. Important Storage Question

**What happens to data when a container is deleted?**

Data stored only in the container's writable layer is lost with that container.

Persistent data should use: Docker volume, bind mount, or external database/storage.

---

## 47. Docker Container vs Process

A good interview explanation:

> A container is not a full virtual machine. It is essentially an isolated process or group of processes with filesystem, network, process, and resource isolation provided through OS/kernel mechanisms.

This is a strong answer.

---

## 48. What Happens During docker run?

Suppose `docker run nginx`.

Conceptually:

```text
Docker CLI
   |
   v
Docker daemon
   |
   | Does image exist?
   |
   +---- No
   |      |
   |      v
   |   Pull image
   |
   v
Create container
   |
   v
Configure filesystem
   |
   v
Configure networking
   |
   v
Start container process
```

This is a good architecture question.

---

## 49. docker run vs docker start

**docker run** — creates and starts a new container. `docker run nginx`

**docker start** — starts an existing stopped container. `docker start nginx-container`

---

## 50. docker create vs docker run

`docker create nginx` creates the container but doesn't start it.

`docker run nginx` creates + starts it.

---

## 51. docker stop vs docker kill

**stop** — graceful shutdown. `docker stop container`. Docker sends a termination signal and gives the process time to exit.

**kill** — forceful termination. `docker kill container`. Used when the container isn't responding properly.

---

## 52. COPY vs ADD

**COPY** — simple file copying: `COPY app.jar /app/`

**ADD** — additional behavior, such as handling local tar archives and URL-related semantics.

**Interview answer:** Prefer COPY for ordinary file copying because its behavior is simpler and more explicit; use ADD only when its additional features are actually needed.

---

## 53. ARG vs ENV

**ARG** — available during image build.

```dockerfile
ARG VERSION=1.0
```

Use: `docker build --build-arg VERSION=2.0 .`

**ENV** — available to the image/container runtime environment.

```dockerfile
ENV APP_ENV=production
```

**Important:** ARG → build time. ENV → runtime environment.

---

## 54. ENTRYPOINT Shell vs Exec Form

Preferred:

```dockerfile
ENTRYPOINT ["java", "-jar", "app.jar"]
```

instead of: `ENTRYPOINT java -jar app.jar`

The exec form handles process signals more directly and avoids unnecessary shell behavior.

This matters for graceful shutdown.

---

## 55. PID 1 in Docker

A common advanced question.

The main process inside a container typically becomes PID 1.

```text
Container
   |
   +-- PID 1 → Java
```

PID 1 has special signal-handling responsibilities on Linux.

If your application doesn't handle termination correctly, graceful shutdown can be affected.

This is one reason proper exec-form entrypoints matter.

---

## 56. Docker Resource Limits

```bash
docker run \
  --memory=512m \
  --cpus=1 \
  myapp
```

This helps prevent a container from consuming uncontrolled host resources.

**Interview:** How do you prevent one container from consuming all CPU?

**Answer:** Use resource controls such as CPU and memory limits, backed by Linux cgroups.

---

## 57. Docker Health vs Process Running

Important distinction.

A container can be RUNNING but the application may be unhealthy.

Example: Java process → alive, Database connection → broken, API → returning errors.

Therefore health checks can detect **application-level** health.

---

## 58. Docker Debugging Scenario

**Interview question:** My Spring Boot application works locally but doesn't work inside Docker. How would you debug it?

Answer step-by-step:

1. Check container: `docker ps`
2. Check logs: `docker logs container`
3. Check application port: `docker port container`
4. Check port mapping: `docker run -p 8080:8080 image`
5. Check environment variables: `docker inspect container`
6. Check network connectivity: `docker exec -it container sh`
7. Check database hostname — inside Docker: `mysql`, not `localhost`
8. Check whether Spring Boot is listening on the expected interface/port

---

## 59. Scenario: Container Immediately Exits

**Question:** Container starts and immediately exits. Why?

Containers live as long as their main process runs.

Example: `CMD ["echo", "hello"]` — the command finishes: echo → exit → container exits.

Debug: `docker ps -a`, then `docker logs container`.

---

## 60. Scenario: Port Already in Use

You run `docker run -p 8080:8080 myapp` and get: port is already allocated.

Possible solution: `docker run -p 8081:8080 myapp`

Now: Host 8081 → Container 8080.

---

## 61. Scenario: Backend Can't Connect to MySQL

You have backend + mysql. Backend uses `localhost:3306`.

Problem: `localhost` → backend container, not MySQL.

Use `mysql:3306` if both services are on the same Docker network.

---

## 62. Scenario: Container Is Using Too Much Memory

Check resource usage: `docker stats`

Example:

```text
CONTAINER    CPU %    MEM USAGE
backend      80%      900MB
mysql        10%      300MB
redis        5%       100MB
```

Then investigate: application memory, garbage collection, memory leak, traffic, queries, caching, container limits.

---

## 63. docker inspect

Very useful debugging command: `docker inspect container`

Can show: Network, IP, Mounts, Environment, Ports, Configuration, State.

---

## 64. docker stats

Shows real-time resource usage: `docker stats`

Useful for: CPU, Memory, Network I/O, Block I/O, PIDs.

---

## 65. Docker Prune

Unused Docker objects can consume disk space.

`docker system df` shows Docker disk usage.

Cleanup: `docker system prune`

Be careful: pruning can remove unused Docker resources. Don't blindly run it on production machines.

---

## 66. Docker Compose Commands You Must Know

```bash
docker compose up
docker compose up -d
docker compose down
docker compose ps
docker compose logs
docker compose logs -f backend
docker compose restart backend
docker compose build
docker compose pull
```

---

## 67. Docker Image vs Container vs Volume vs Network

| Object | Purpose |
| --- | --- |
| Image | Application template |
| Container | Running/created instance |
| Volume | Persistent data |
| Network | Container communication |
| Registry | Stores images |

Architecture:

```text
                 Registry
                    |
                    | pull
                    v
                  Image
                    |
                    | run
                    v
               Container
              /    |     \
             /     |      \
            v      v       v
        Volume   Network   Process
```

---

## 68. Docker Interview Questions — Basic

You should be able to answer:

What is Docker? Why Docker? What is containerization? What is a Docker image? What is a container? Image vs container? Docker vs VM? What is Dockerfile? What is Docker Hub? What is Docker Registry? What is Docker daemon? What is Docker CLI? What is Docker Compose? What is a Docker volume? What is Docker network?

---

## 69. Dockerfile Questions

Prepare: What is FROM? What is RUN? What is COPY? COPY vs ADD? What is WORKDIR? CMD vs ENTRYPOINT? What is EXPOSE? What is ENV? ARG vs ENV? What is `.dockerignore`? What is multi-stage build? How do you reduce image size? Why use JRE instead of JDK in runtime? Why use exec-form ENTRYPOINT?

---

## 70. Networking Questions

Prepare: How do containers communicate? What is Docker bridge network? What is port mapping? What does `-p 8080:8080` mean? Why doesn't localhost work between containers? How does Docker DNS work? What is host networking? What is network isolation? How do you connect two containers? How do you troubleshoot network connectivity?

---

## 71. Storage Questions

Prepare: Why do containers need volumes? What happens to container data after deletion? Volume vs bind mount? How do you persist MySQL data? How do you back up a Docker volume? What is ephemeral storage?

---

## 72. Docker Compose Questions

Prepare: What is Docker Compose? Why use Compose? What is services? What is `depends_on`? What are Compose networks? What are Compose volumes? `docker compose up` vs `down`? What does `down -v` do? How do services communicate? How do you configure environment variables?

---

## 73. Production Questions

These are important for an experienced SDE-1 candidate.

**Q1. How would you deploy a Dockerized Spring Boot application?**

```text
Developer
    |
    v
Git
    |
    v
CI/CD
    |
    v
Docker Build
    |
    v
Image
    |
    v
Container Registry
    |
    v
Deployment Platform
    |
    v
Containers
```

The deployment platform could be: Kubernetes, ECS, EKS, EC2, etc.

---

## 74. CI/CD + Docker

Typical pipeline:

```text
Git Push
   |
   v
CI
   |
   +-- Run tests
   |
   +-- Build application
   |
   +-- Build Docker image
   |
   +-- Security scan
   |
   +-- Push image
   |
   v
Container Registry
   |
   v
Deploy
```

Example image tag: `myapp:commit-a81f23d`

This is better for traceability than relying only on `latest`.

---

## 75. Docker + Kafka

Since your interview background includes Kafka, this can be asked.

```text
             Docker Network
                  |
       +----------+----------+
       |                     |
       v                     v
   Spring Boot             Kafka
       |                     |
       | produce             |
       +-------------------->|
                             |
                             v
                          Consumer
```

Kafka may itself require multiple supporting services depending on the Kafka distribution/configuration.

For local development, Docker Compose is commonly used to run the Kafka environment.

---

## 76. Docker + Redis

```yaml
services:

  backend:
    build: .
    depends_on:
      - redis

  redis:
    image: redis:7
```

Spring configuration: `REDIS_HOST=redis`, `REDIS_PORT=6379`

Not `REDIS_HOST=localhost` inside the backend container.

---

## 77. Advanced Question: Why Containers Are Fast?

Containers don't need to boot a complete guest OS.

**VM:** Application → Guest OS → Hypervisor

**Container:** Application → Container isolation → Host kernel

Therefore startup can be much faster.

---

## 78. Advanced Question: Are Containers Completely Isolated?

No.

Containers provide process/resource/filesystem/network isolation, but they **share the host kernel**.

Therefore containers are not identical to VMs in isolation architecture.

This is an important nuanced answer.

---

## 79. Advanced Question: What if the Host Machine Dies?

If the Docker host dies, containers on that host also become unavailable.

Docker itself doesn't magically provide distributed failover.

For high availability, use an orchestration/deployment system such as Kubernetes/ECS and multiple hosts.

---

## 80. Advanced Question: How Do You Scale Docker Containers?

Suppose traffic ↑. You can run multiple application instances:

```text
             Load Balancer
              /    |    \
             /     |     \
            v      v      v
         App-1   App-2   App-3
```

Docker can run multiple containers, while an orchestrator can automate: scheduling, scaling, health checks, restart, load balancing/service discovery, rolling deployments.

---

## 81. Advanced Question: What Is Immutable Infrastructure?

Instead of modifying a running server manually (SSH → install package → change config), you create a new image:

```text
New Code
   ↓
New Image
   ↓
New Container
```

This makes deployments more reproducible.

---

## 82. Important Real-World Interview Scenario

**Question:** You have a Spring Boot service, Redis and MySQL. How would you Dockerize the system?

**Answer:**

```text
                    Docker Network
                          |
          +---------------+---------------+
          |               |               |
          v               v               v
      Spring Boot       Redis           MySQL
       :8080            :6379           :3306
          |
          |
       Application
```

**Steps:**

1. Create Dockerfile for Spring Boot
2. Create Compose file
3. Create services: backend, redis, mysql
4. Use environment variables
5. Use Docker network
6. Use MySQL volume
7. Add health checks
8. Build: `docker compose build`
9. Start: `docker compose up -d`
10. Monitor: `docker compose logs -f`

---

## 83. 15 Most Important Docker Questions for Your SDE-1 Interview

If time is limited, master these first:

1. What is Docker?
2. Docker vs VM?
3. Image vs container?
4. Dockerfile?
5. CMD vs ENTRYPOINT?
6. RUN vs CMD?
7. Port mapping?
8. Container networking?
9. Why localhost doesn't work between containers?
10. Docker volumes?
11. Docker Compose?
12. Multi-stage Docker builds?
13. How do you debug a crashing container?
14. How do you reduce Docker image size?
15. How would you Dockerize a Spring Boot + MySQL application?

---

## 84. Questions Interviewer May Ask Based on Your Resume

Given your backend experience, I would particularly prepare for these follow-ups.

### Docker + Spring Boot

How did you containerize your Spring Boot application? What base image did you use and why? Why JRE instead of JDK? How did you pass configuration? How did you connect the application to MySQL? How did you expose the application?

### Docker + Kafka

How did you run Kafka locally? How did your Spring Boot application communicate with Kafka inside Docker? What hostname did you use? How would you run multiple Kafka brokers? How would you persist Kafka data?

### Docker + Redis

How did your application connect to Redis? Why can't you use localhost? How would you persist Redis data?

### Production

How do you deploy Docker containers? How do you perform zero-downtime deployment? What happens if a container crashes? How do you monitor containers? How do you limit CPU and memory? How do you handle secrets? How do you roll back a deployment?

---

## 85. A Strong 2-Minute Docker Explanation

If the interviewer says: **"Explain Docker."**

You can say:

> Docker is a containerization platform that packages an application and its dependencies into a portable image, which can then be instantiated as containers.
>
> A Docker image is an immutable template, while a container is a running instance of that image.
>
> Docker uses a client-daemon architecture. The Docker CLI communicates with the Docker daemon, which manages images, containers, networks and volumes.
>
> Containers provide isolation using Linux kernel features such as namespaces and cgroups, while sharing the host kernel, which generally makes them lighter than virtual machines.
>
> We define how an image is built using a Dockerfile, and for multi-container applications we can use Docker Compose.
>
> For example, for a Spring Boot application, I can create a Docker image containing the application JAR and Java runtime, expose port 8080, and run it with `-p 8080:8080`. If the application needs MySQL and Redis, I can use Docker Compose and put all services on a common Docker network, where the application connects to MySQL using the service name rather than localhost.
>
> For production, images can be pushed to a container registry and deployed using platforms such as Kubernetes or ECS.

That is a very solid SDE-1 answer.

---

## 86. Your Docker Preparation Roadmap

I recommend learning it in this order:

| Day | Topic |
| --- | --- |
| Day 1 | Docker fundamentals |
| Day 2 | Docker commands |
| Day 3 | Dockerfile |
| Day 4 | Networking |
| Day 5 | Volumes + storage |
| Day 6 | Docker Compose |
| Day 7 | Spring Boot + Docker |
| Day 8 | MySQL + Redis + Docker |
| Day 9 | Kafka + Docker |
| Day 10 | Docker debugging |
| Day 11 | Docker security |
| Day 12 | Docker + CI/CD |
| Day 13 | Docker + Kubernetes |
| Day 14 | Mock interview |

### Most important for your SDE-1 round

Don't just memorize Docker commands. Be able to build and explain this complete setup from scratch:

```text
                  Client
                    |
                    v
              Spring Boot
               Container
                    |
        +-----------+-----------+
        |                       |
        v                       v
      Redis                   MySQL
    Container                Container
        |                       |
        |                       v
        |                    Volume
        |
        v
      Kafka
    Container(s)
```

And explain:

> Dockerfile → image → container → network → volume → environment variables → health check → logs → debugging → registry → deployment.

That chain covers a large portion of the Docker questions likely to arise in an SDE-1 backend interview.
