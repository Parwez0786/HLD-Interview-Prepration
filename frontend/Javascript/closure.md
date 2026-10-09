# Day 2 — Closures in JavaScript

Closures are extremely important for frontend JavaScript interviews. You should be able to explain them both conceptually and by tracing execution.

---

## Table of Contents

- [1. First understand the example](#1-first-understand-the-example)
- [2. But counter() has finished. What happens to count?](#2-but-counter-has-finished-what-happens-to-count)
- [3. Now c() is called](#3-now-c-is-called)
- [4. Second call](#4-second-call)
- [5. The key interview answer](#5-the-key-interview-answer)
- [6. What exactly is a closure?](#6-what-exactly-is-a-closure)
- [7. Very important: count is NOT copied](#7-very-important-count-is-not-copied)
- [8. Why doesn't JavaScript garbage collect count?](#8-why-doesnt-javascript-garbage-collect-count)
- [9. Closures give us private variables](#9-closures-give-us-private-variables)
- [10. Multiple closures have separate state](#10-multiple-closures-have-separate-state)
- [11. Closure + loop — very important interview question](#11-closure--loop--very-important-interview-question)
- [12. Closure and lexical scope](#12-closure-and-lexical-scope)
- [13. Closure vs Scope — interview distinction](#13-closure-vs-scope--interview-distinction)
- [14. Common frontend uses of closures](#14-common-frontend-uses-of-closures)
- [15. Interview mental model](#15-interview-mental-model)
- [Questions you should be able to answer without hesitation](#-questions-you-should-be-able-to-answer-without-hesitation)
- [One-line definition to memorize](#one-line-definition-to-memorize)

---

## 1. First understand the example

```javascript
function counter() {
    let count = 0;

    return function () {
        return ++count;
    };
}

const c = counter();

console.log(c()); // 1
console.log(c()); // 2
```

Let's execute this step by step.

### Step 1: counter() is called

```javascript
const c = counter();
```

JavaScript creates a function execution context for `counter`.

Inside it:

```javascript
let count = 0;
```

So we have:

```text
counter execution context
        |
        └── count = 0
```

Then this function is created:

```javascript
function () {
    return ++count;
}
```

Notice something important:

This inner function uses `count`, even though `count` belongs to `counter()`.

Then `counter()` returns the inner function.

So:

`c`

now points to:

```javascript
function () {
    return ++count;
}
```

---

## 2. But counter() has finished. What happens to count?

Normally, you might think:

> `counter()` has finished, so its local variable `count` should disappear.

But JavaScript sees that the returned function still references `count`.

Therefore, JavaScript keeps the required lexical environment alive.

Conceptually:

```text
c
│
▼
inner function
│
│ references
▼
Lexical Environment
│
└── count = 0
```

This combination of:

**function + its surrounding lexical environment**

is called a closure.

---

## 3. Now c() is called

```javascript
console.log(c());
```

The inner function executes:

```javascript
return ++count;
```

It needs to find `count`.

JavaScript searches:

```text
Local scope of inner function
        ↓
Outer lexical environment
        ↓
count = 0
```

Then:

```javascript
++count
```

changes:

```text
count = 0
```

to:

```text
count = 1
```

So:

```javascript
c(); // 1
```

---

## 4. Second call

Now:

```javascript
console.log(c());
```

Again the same inner function executes.

But `count` is not recreated.

It is still:

```text
count = 1
```

Therefore:

```javascript
++count
```

becomes:

```text
count = 2
```

Output:

```text
1
2
```

---

## 5. The key interview answer

If interviewer asks:

> Why does `count` remain alive even after `counter()` has finished?

Say:

> `count` remains alive because the returned inner function forms a closure over the lexical environment of `counter()`. Since the inner function still has a reference to `count`, JavaScript keeps that environment reachable instead of garbage collecting it. Every time we call `c()`, the function accesses and modifies the same `count` variable.

That's a strong interview answer.

---

## 6. What exactly is a closure?

A closure is commonly explained as:

> A function together with access to the variables from its surrounding lexical scope, even after that outer function has finished executing.

Example:

```javascript
function outer() {
    let x = 10;

    function inner() {
        console.log(x);
    }

    return inner;
}

const fn = outer();

fn(); // 10
```

Here:

```text
outer()
  |
  ├── x = 10
  |
  └── inner()
         |
         └── references x
```

After `outer()` finishes:

```text
outer() execution finished
```

But:

```text
fn → inner function → x
```

So `x` remains accessible.

---

## 7. Very important: count is NOT copied

This is a common interview trap.

When we do:

```javascript
const c = counter();
```

JavaScript does not make a copy like:

```javascript
c.count = 0;
```

Instead, the function maintains access to the same variable.

So:

```javascript
c();
c();
c();
```

results in:

```text
count = 0
   ↓
c()
count = 1
   ↓
c()
count = 2
   ↓
c()
count = 3
```

---

## 8. Why doesn't JavaScript garbage collect count?

Garbage collection removes objects/data that are no longer reachable.

Initially:

```text
c
↓
inner function
↓
lexical environment
↓
count
```

`count` is still reachable through `c`.

Therefore, the environment containing `count` cannot simply be garbage collected.

If later:

```javascript
c = null;
```

and there are no other references to the inner function/environment, then that closure can eventually become eligible for garbage collection.

---

## 9. Closures give us private variables

This is one of the most useful applications.

```javascript
function counter() {
    let count = 0;

    return function () {
        return ++count;
    };
}
```

There is no direct access like:

```javascript
c.count
```

because `count` is private to the closure.

We can only modify it through the returned function:

```javascript
const c = counter();

console.log(c()); // 1
console.log(c()); // 2
console.log(c()); // 3
```

This is similar to encapsulation/private state.

---

## 10. Multiple closures have separate state

This is another very common interview question.

```javascript
function counter() {
    let count = 0;

    return function () {
        return ++count;
    };
}

const c1 = counter();
const c2 = counter();

console.log(c1()); // 1
console.log(c1()); // 2

console.log(c2()); // 1
console.log(c2()); // 2
```

Why?

Because we called `counter()` twice.

Conceptually:

```text
c1
 ↓
Closure #1
 ↓
count = 0


c2
 ↓
Closure #2
 ↓
count = 0
```

They have separate lexical environments.

Therefore:

```text
c1 → 1 → 2

c2 → 1 → 2
```

---

## 11. Closure + loop — very important interview question

Consider:

```javascript
for (var i = 0; i < 3; i++) {
    setTimeout(() => {
        console.log(i);
    }, 1000);
}
```

Output:

```text
3
3
3
```

Why?

Because `var` is function-scoped. The callbacks close over the same `i` variable.

After the loop:

```text
i = 3
```

Then callbacks execute and all read:

```text
i = 3
```

With `let`:

```javascript
for (let i = 0; i < 3; i++) {
    setTimeout(() => {
        console.log(i);
    }, 1000);
}
```

Output:

```text
0
1
2
```

Why?

`let` creates a separate binding for each iteration, so each callback closes over its corresponding iteration's `i`.

---

## 12. Closure and lexical scope

Remember this connection:

```text
Lexical Scope
      ↓
Function can access variables
from where it was defined
      ↓
Closure
      ↓
That access can continue
after outer function finishes
```

Example:

```javascript
let global = 100;

function outer() {
    let x = 10;

    return function inner() {
        let y = 20;

        console.log(global);
        console.log(x);
        console.log(y);
    };
}
```

Inside `inner()`:

- `y` → local variable
- `x` → outer lexical environment
- `global` → global environment

This is the scope chain.

---

## 13. Closure vs Scope — interview distinction

### Scope

Scope answers:

> Where can I access this variable?

### Closure

Closure answers:

> How can a function continue accessing variables from its surrounding scope after that surrounding function has finished?

---

## 14. Common frontend uses of closures

You should know these examples:

### 1. Counters

```javascript
function counter() {
    let count = 0;

    return () => ++count;
}
```

### 2. Data privacy

```javascript
function user() {
    let password = "secret";

    return {
        checkPassword: () => password
    };
}
```

### 3. Event handlers

```javascript
function setupButton(id) {
    const buttonId = id;

    button.addEventListener("click", () => {
        console.log(buttonId);
    });
}
```

The callback remembers `buttonId`.

### 4. setTimeout / async callbacks

```javascript
function greet(name) {
    setTimeout(() => {
        console.log("Hello", name);
    }, 1000);
}
```

The callback closes over `name`.

### 5. Function factories

```javascript
function multiplyBy(x) {
    return function (y) {
        return x * y;
    };
}

const double = multiplyBy(2);

console.log(double(5)); // 10
```

Here `double` remembers:

```text
x = 2
```

---

## 15. Interview mental model

Whenever you see:

```javascript
function outer() {
    let x = 10;

    return function inner() {
        console.log(x);
    };
}
```

Immediately think:

```text
outer()
  │
  ├── x = 10
  │
  └── inner()
        │
        └── references x
              │
              ▼
       closure created
```

Then:

```javascript
const fn = outer();
```

means:

```text
fn
 ↓
inner()
 ↓
outer's lexical environment
 ↓
x = 10
```

Even though:

```text
outer() execution is finished
```

`x` can still be accessed because `fn` keeps the closure reachable.

---

## 🔥 Questions you should be able to answer without hesitation

- What is a closure?
- Why does `count` survive after `counter()` finishes?
- Is `count` copied or is the same variable reused?
- Why do `c1` and `c2` have independent counters?
- How do closures provide data privacy?
- How are closures related to lexical scope?
- Why does `var` produce `3 3 3` in the loop example?
- Why does `let` produce `0 1 2`?
- Can a closure cause a memory leak?
- Where are closures used in real frontend applications?

---

## One-line definition to memorize

> A closure is a function that retains access to variables from its lexical environment even after the outer function has finished executing.
