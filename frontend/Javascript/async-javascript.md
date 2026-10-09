# Day 4 — Async JavaScript

Exactly. Day 4 — Async JavaScript is one of the most important JavaScript interview topics. Let's build the execution model from the ground up.

---

## Table of Contents

- [1. The mental model](#1-the-mental-model)
- [2. First: Call Stack](#2-first-call-stack)
- [3. What are Web APIs?](#3-what-are-web-apis)
- [4. Callback Queue](#4-callback-queue)
- [5. Microtask Queue](#5-microtask-queue)
- [6. The Event Loop](#6-the-event-loop)
- [7. Your important example](#7-your-important-example)
- [8. Step 3 — Promise](#8-step-3--promise)
- [9. Step 4 — console.log("D")](#9-step-4--consolelogd)
- [10. Now the Event Loop gets involved](#10-now-the-event-loop-gets-involved)
- [11. Then B](#11-then-b)
- [12. The most important rule](#12-the-most-important-rule)
- [13. Interview trap: setTimeout(..., 0)](#13-interview-trap-settimeout-0)
- [14. Interview trap: Promise vs setTimeout](#14-interview-trap-promise-vs-settimeout)
- [15. Another important example](#15-another-important-example)
- [16. Microtasks can create a problem](#16-microtasks-can-create-a-problem)
- [17. async/await also uses Promises](#17-asyncawait-also-uses-promises)
- [18. Very important distinction](#18-very-important-distinction)
- [19. Browser vs Node.js](#19-browser-vs-nodejs)
- [20. Your Day 4 interview cheat sheet](#20-your-day-4-interview-cheat-sheet)

---

## 1. The mental model

When JavaScript runs asynchronous code, think of these components:

```text
                JavaScript Runtime

              ┌─────────────────┐
              │    Call Stack    │
              └────────┬────────┘
                       │
                       │ async operation
                       ↓
              ┌─────────────────┐
              │    Web APIs     │
              │                 │
              │ setTimeout      │
              │ DOM events      │
              │ fetch           │
              └────────┬────────┘
                       │
             ┌─────────┴──────────┐
             ↓                    ↓
      Callback Queue        Microtask Queue
      (Macrotask)           (Higher priority)
             │                    │
             └─────────┬──────────┘
                       ↓
                  Event Loop
                       │
                       ↓
                  Call Stack
```

The Event Loop decides when queued callbacks can enter the Call Stack.

---

## 2. First: Call Stack

JavaScript executes synchronous code using the Call Stack.

Example:

```javascript
console.log("A");

console.log("B");

console.log("C");
```

Execution:

```text
Call Stack
────────────

console.log("A")
      ↓
print A
      ↓
removed

console.log("B")
      ↓
print B
      ↓
removed

console.log("C")
      ↓
print C
      ↓
removed
```

Output:

```text
A
B
C
```

JavaScript executes this synchronously, one operation at a time.

---

## 3. What are Web APIs?

Things like:

- `setTimeout()`
- `fetch()`
- DOM events

are provided by the runtime environment, such as the browser.

They aren't JavaScript language features themselves.

For example:

```javascript
setTimeout(() => {
    console.log("Hello");
}, 1000);
```

JavaScript doesn't sit inside the Call Stack waiting for one second.

Instead:

```text
Call Stack
    │
    │ setTimeout()
    ↓
Web API
    │
    │ waits 1000 ms
    ↓
Callback Queue
```

After the timer finishes, its callback becomes eligible to be processed.

---

## 4. Callback Queue

Consider:

```javascript
setTimeout(() => {
    console.log("B");
}, 0);
```

Many beginners think:

> 0 ms means execute immediately.

That's incorrect.

It means:

> Don't wait intentionally before making the callback eligible for execution.

The callback still needs to wait until JavaScript's Call Stack is empty and the Event Loop can move it onto the stack.

So:

```text
setTimeout
    ↓
Web API
    ↓
Callback Queue
    ↓
Event Loop
    ↓
Call Stack
```

---

## 5. Microtask Queue

Promises use the Microtask Queue.

Example:

```javascript
Promise.resolve().then(() => {
    console.log("C");
});
```

The callback passed to `.then()` goes into the Microtask Queue.

So we have two important queues:

**Microtask Queue**

- `Promise.then()`
- `Promise.catch()`
- `Promise.finally()`
- `queueMicrotask()`

**Callback / Task Queue**

- `setTimeout()`
- `setInterval()`
- some event callbacks

The important rule is:

> Microtasks are processed before the next regular task/callback.

This is why Promises often execute before `setTimeout(..., 0)`.

---

## 6. The Event Loop

The Event Loop continuously checks whether JavaScript can execute another queued task.

Simplified:

```text
Is Call Stack empty?
       │
       ↓
      YES
       │
       ↓
Are there Microtasks?
       │
       ↓
Execute ALL available Microtasks
       │
       ↓
Then take a task from Callback/Task Queue
       │
       ↓
Put callback onto Call Stack
```

The key interview rule:

```text
Synchronous code
      ↓
Microtasks
      ↓
Tasks / Macrotasks
```

---

## 7. Your important example

```javascript
console.log("A");

setTimeout(() => console.log("B"), 0);

Promise.resolve().then(() => console.log("C"));

console.log("D");
```

The output is:

```text
A
D
C
B
```

Let's understand exactly why.

### Step 1 — console.log("A")

```javascript
console.log("A");
```

This is synchronous.

It goes directly onto the Call Stack.

```text
Call Stack
──────────
console.log("A")
```

Output:

```text
A
```

Then it is removed.

### Step 2 — setTimeout

```javascript
setTimeout(() => console.log("B"), 0);
```

The timer callback does not execute immediately.

It is handled by the runtime's timer mechanism.

Conceptually:

```text
Call Stack
    │
    ↓
setTimeout
    │
    ↓
Timer/Web API
    │
    ↓
Callback/Task Queue
```

Eventually:

```text
Callback Queue:
┌───────────────┐
│ console.log B │
└───────────────┘
```

But it cannot execute yet because synchronous JavaScript is still running.

---

## 8. Step 3 — Promise

Now:

```javascript
Promise.resolve().then(() => console.log("C"));
```

The Promise is already resolved.

Its `.then()` callback is scheduled as a microtask.

So:

```text
Microtask Queue
┌───────────────┐
│ console.log C │
└───────────────┘
```

At this point conceptually we have:

```text
Microtask Queue
    C

Callback Queue
    B
```

---

## 9. Step 4 — console.log("D")

```javascript
console.log("D");
```

Again, synchronous.

So it executes immediately.

Output:

```text
A
D
```

Now all synchronous code has finished.

The Call Stack becomes empty.

---

## 10. Now the Event Loop gets involved

At this point:

```text
Call Stack
    EMPTY

Microtask Queue
    C

Callback Queue
    B
```

What does JavaScript do?

It processes the Microtask Queue first.

So:

```javascript
console.log("C");
```

executes.

Output:

```text
A
D
C
```

---

## 11. Then B

After the microtask queue has been drained, the Event Loop can take the callback from the normal task/callback queue.

```javascript
console.log("B");
```

Output:

```text
A
D
C
B
```

Therefore:

```text
A
D
C
B
```

---

## 12. The most important rule

Memorize this:

```text
                 JavaScript
                     │
             ┌───────┴────────┐
             ↓                ↓
        Synchronous        Asynchronous
             │                │
             ↓                ↓
        Call Stack       Runtime/Web APIs
                              │
                    ┌─────────┴─────────┐
                    ↓                   ↓
              Microtask Queue      Task Queue
                    │                   │
                    └────────┬──────────┘
                             ↓
                         Event Loop
```

And the execution priority is approximately:

1. Synchronous Call Stack
2. Microtask Queue
3. Task/Callback Queue

---

## 13. Interview trap: setTimeout(..., 0)

Suppose interviewer asks:

> Does `setTimeout(fn, 0)` execute immediately?

Answer:

> No. 0 means the callback has no additional timer delay requirement. It still has to wait until the current synchronous execution finishes, and microtasks that are pending are processed before the next task.

---

## 14. Interview trap: Promise vs setTimeout

Question:

```javascript
setTimeout(() => console.log("timeout"), 0);

Promise.resolve().then(() => console.log("promise"));
```

Output:

```text
promise
timeout
```

Why?

```text
Promise callback
      ↓
Microtask Queue
      ↓
processed first

setTimeout callback
      ↓
Task Queue
      ↓
processed afterward
```

---

## 15. Another important example

```javascript
console.log("1");

setTimeout(() => {
    console.log("2");
}, 0);

Promise.resolve().then(() => {
    console.log("3");
});

Promise.resolve().then(() => {
    console.log("4");
});

console.log("5");
```

Output:

```text
1
5
3
4
2
```

Why?

First synchronous:

```text
1
5
```

Then all microtasks:

```text
3
4
```

Then task:

```text
2
```

So:

```text
1 → 5 → 3 → 4 → 2
```

---

## 16. Microtasks can create a problem

Consider:

```javascript
function test() {
    Promise.resolve().then(test);
}

test();

setTimeout(() => {
    console.log("timeout");
}, 0);
```

The Promise keeps adding another microtask.

Conceptually:

```text
Microtask
   ↓
test()
   ↓
new Microtask
   ↓
test()
   ↓
new Microtask
   ↓
...
```

Because the runtime keeps draining microtasks before moving to the next task, the timer may be starved.

This is why excessive microtask scheduling can prevent other tasks from getting a chance to run.

---

## 17. async/await also uses Promises

This is extremely important for interviews.

Example:

```javascript
async function test() {
    console.log("A");

    await Promise.resolve();

    console.log("B");
}

test();

console.log("C");
```

Output:

```text
A
C
B
```

Why?

Before `await`:

```javascript
console.log("A");
```

runs synchronously.

Then:

```javascript
await Promise.resolve();
```

causes the rest of the async function to continue later as a microtask.

Meanwhile:

```javascript
console.log("C");
```

runs synchronously.

So:

```text
A
C
B
```

---

## 18. Very important distinction

Don't say:

> "JavaScript is asynchronous."

A better interview answer is:

> JavaScript execution itself is single-threaded and synchronous by default. Asynchronous behavior is enabled by the runtime through mechanisms such as timers, Web APIs, Promises, and the Event Loop.

This is a much stronger answer.

---

## 19. Browser vs Node.js

One small correction to the simplified diagram:

When people say "Web APIs", they're usually talking about the browser runtime.

For example:

```text
Browser
├── JavaScript Engine
├── DOM APIs
├── Timer APIs
├── Fetch APIs
└── Event Loop
```

Node.js has a different runtime architecture, using Node APIs and libuv, but the fundamental interview concept of:

```text
Call Stack
→ asynchronous runtime
→ queues
→ Event Loop
```

is still useful.

---

## 20. Your Day 4 interview cheat sheet

### Call Stack

Executes JavaScript code synchronously.

### Web APIs / Runtime APIs

Handle asynchronous operations such as timers, network requests, and events.

### Callback/Task Queue

Callbacks waiting to be executed as tasks.

### Microtask Queue

Contains things such as:

- `Promise.then()`
- `Promise.catch()`
- `Promise.finally()`
- `queueMicrotask()`

### Event Loop

Coordinates when queued work can be executed by the Call Stack.

### Most important ordering

```text
Synchronous code
        ↓
Microtasks
        ↓
Next task
```

### Your question

```javascript
console.log("A");

setTimeout(() => console.log("B"), 0);

Promise.resolve().then(() => console.log("C"));

console.log("D");
```

Answer:

```text
A
D
C
B
```

The one-line explanation to memorize:

> A and D are synchronous, the Promise callback goes to the Microtask Queue, and the setTimeout callback goes to the Task Queue; after synchronous code finishes, microtasks are processed before the next task.
