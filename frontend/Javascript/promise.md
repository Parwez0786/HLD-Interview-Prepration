# Day 5 — JavaScript Promises

Promises are very important for JavaScript interviews, especially for frontend/React roles.

The main idea:

> A Promise represents the eventual result of an asynchronous operation.

For example:

```javascript
const promise = fetch("/users");
```

The `fetch()` does not immediately give you the users. It gives you a Promise that will eventually either:

- succeed → fulfilled
- fail → rejected

---

## Table of Contents

- [1. Promise States](#1-promise-states)
- [2. new Promise()](#2-new-promise)
- [3. .then()](#3-then)
- [4. .catch()](#4-catch)
- [5. .finally()](#5-finally)
- [6. Promise Chaining](#6-promise-chaining)
- [7. async](#7-async)
- [8. await](#8-await)
- [9. Does await block JavaScript?](#9-does-await-block-javascript)
- [10. Handling Errors with async/await](#10-handling-errors-with-asyncawait)
- [11. Promise.all()](#11-promiseall)
- [12. What happens if ONE Promise fails inside Promise.all()?](#12-what-happens-if-one-promise-fails-inside-promiseall)
- [13. Promise.all() preserves order](#13-promiseall-preserves-order)
- [14. Promise.allSettled()](#14-promiseallsettled)
- [15. Promise.race()](#15-promiserace)
- [16. Promise.any()](#16-promiseany)
- [17. What if ALL promises reject in Promise.any()?](#17-what-if-all-promises-reject-in-promiseany)
- [18. The Most Important Comparison](#18-the-most-important-comparison)
- [19. Real Interview Example](#19-real-interview-example)
- [20. Important Interview Trap](#20-important-interview-trap)
- [21. One Interview Question You Should Be Able to Answer](#21-one-interview-question-you-should-be-able-to-answer)
- [Day 5 — Must Know Flow](#-day-5--must-know-flow)

---

## 1. Promise States

A Promise has 3 states:

```text
             Promise
                |
        -------------------
        |                 |
    Fulfilled          Rejected
     success             error
        |
     Settled
```

Initially:

**Pending**

Then it becomes either:

```text
Pending → Fulfilled
```

or

```text
Pending → Rejected
```

Once settled, it cannot change again.

---

## 2. new Promise()

You can manually create a Promise:

```javascript
const promise = new Promise((resolve, reject) => {

    // asynchronous work

    resolve("Success");

});
```

The constructor receives a function called the executor.

```javascript
(resolve, reject) => {
    
}
```

You get two functions:

### resolve()

Used when operation succeeds.

```javascript
resolve("Data received");
```

### reject()

Used when operation fails.

```javascript
reject("Something went wrong");
```

Example:

```javascript
const promise = new Promise((resolve, reject) => {

    const success = true;

    if (success) {
        resolve("Payment successful");
    } else {
        reject("Payment failed");
    }

});
```

---

## 3. .then()

`.then()` handles the successful result.

```javascript
promise.then((result) => {
    console.log(result);
});
```

Complete example:

```javascript
const promise = new Promise((resolve, reject) => {

    resolve("Success");

});

promise.then((result) => {
    console.log(result);
});
```

Output:

```text
Success
```

Think:

```text
resolve()
   ↓
.then()
```

---

## 4. .catch()

`.catch()` handles rejection/error.

```javascript
const promise = new Promise((resolve, reject) => {

    reject("Something went wrong");

});

promise.catch((error) => {
    console.log(error);
});
```

Output:

```text
Something went wrong
```

Think:

```text
reject()
   ↓
.catch()
```

---

## 5. .finally()

`finally()` runs whether the Promise succeeds or fails.

```javascript
const promise = fetch("/users");

promise
    .then((data) => {
        console.log(data);
    })
    .catch((error) => {
        console.log(error);
    })
    .finally(() => {
        console.log("Request completed");
    });
```

Typical use:

```javascript
showLoadingSpinner();

fetch("/users")
    .then(...)
    .catch(...)
    .finally(() => {
        hideLoadingSpinner();
    });
```

Because you want to hide the loader regardless of success/failure.

---

## 6. Promise Chaining

This is extremely important.

```javascript
fetchUser()
    .then((user) => {
        return fetchOrders(user.id);
    })
    .then((orders) => {
        console.log(orders);
    })
    .catch((error) => {
        console.log(error);
    });
```

Flow:

```text
fetchUser()
     ↓
   then()
     ↓
fetchOrders()
     ↓
   then()
     ↓
   result
```

The important rule:

> Whatever you return from `.then()` becomes the input to the next `.then()`.

Example:

```javascript
Promise.resolve(10)
    .then((value) => {
        return value * 2;
    })
    .then((value) => {
        console.log(value);
    });
```

Output:

```text
20
```

---

## 7. async

`async` makes a function return a Promise.

```javascript
async function getData() {
    return "Hello";
}
```

Even though we return a string:

```javascript
return "Hello";
```

the function actually returns:

```text
Promise<string>
```

So:

```javascript
getData().then((result) => {
    console.log(result);
});
```

Output:

```text
Hello
```

### Interview question

> What does an async function always return?

Answer:

> An async function always returns a Promise. If it returns a normal value, that value is automatically wrapped in a fulfilled Promise.

---

## 8. await

`await` waits for a Promise's result inside an async function.

Instead of:

```javascript
getUser()
    .then((user) => {
        console.log(user);
    });
```

we can write:

```javascript
async function getData() {

    const user = await getUser();

    console.log(user);
}
```

This is much easier to read.

---

## 9. Does await block JavaScript?

This is a very common interview question.

No.

`await` pauses the execution of the current async function, but it does not block the JavaScript thread/event loop.

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

At:

```javascript
await Promise.resolve();
```

the async function pauses.

JavaScript continues executing:

```javascript
console.log("C");
```

Then the continuation of the async function runs as a microtask.

---

## 10. Handling Errors with async/await

Use `try`/`catch`.

```javascript
async function getUser() {

    try {

        const response = await fetch("/user");

        console.log(response);

    } catch (error) {

        console.log("Error:", error);

    }

}
```

Equivalent Promise version:

```javascript
fetch("/user")
    .then((response) => {
        console.log(response);
    })
    .catch((error) => {
        console.log(error);
    });
```

---

## 11. Promise.all()

This is one of the most important methods.

Suppose you need:

- User
- Orders
- Portfolio

and all three requests can happen simultaneously.

```javascript
const userPromise = fetchUser();
const ordersPromise = fetchOrders();
const portfolioPromise = fetchPortfolio();
```

Use:

```javascript
const results = await Promise.all([
    userPromise,
    ordersPromise,
    portfolioPromise
]);
```

Result:

```javascript
[
    user,
    orders,
    portfolio
]
```

### Important

`Promise.all()` runs/waits for multiple promises concurrently.

It resolves only when all promises succeed.

---

## 12. What happens if ONE Promise fails inside Promise.all()?

This is your interview question.

Suppose:

```javascript
const p1 = Promise.resolve("User");
const p2 = Promise.reject("Orders failed");
const p3 = Promise.resolve("Portfolio");

Promise.all([p1, p2, p3])
    .then((results) => {
        console.log(results);
    })
    .catch((error) => {
        console.log(error);
    });
```

Output:

```text
Orders failed
```

Because:

> `Promise.all()` rejects as soon as any input Promise rejects.

The overall result becomes:

```text
Promise.all()
     |
     |--- p1 ✅
     |--- p2 ❌
     |--- p3 ✅
          ↓
      REJECTED
```

### VERY IMPORTANT INTERVIEW POINT

`Promise.all()` does not cancel the other promises.

For example:

```javascript
const p1 = fetch("/users");
const p2 = fetch("/orders");
const p3 = fetch("/portfolio");

await Promise.all([p1, p2, p3]);
```

If `p2` fails, `Promise.all()` rejects.

But `p1` and `p3` are not automatically cancelled.

They may continue running.

So say in interview:

> "Promise.all() rejects when the first input promise rejects, but it doesn't automatically cancel the other operations."

---

## 13. Promise.all() preserves order

Suppose:

```javascript
const p1 = new Promise(resolve =>
    setTimeout(() => resolve("A"), 3000)
);

const p2 = new Promise(resolve =>
    setTimeout(() => resolve("B"), 1000)
);
```

Then:

```javascript
const result = await Promise.all([p1, p2]);

console.log(result);
```

Output:

```javascript
["A", "B"]
```

Even though B finished first.

The output order follows the input order, not completion order.

---

## 14. Promise.allSettled()

Sometimes you don't want one failure to destroy the entire operation.

Use:

```javascript
Promise.allSettled()
```

Example:

```javascript
const p1 = Promise.resolve("User");
const p2 = Promise.reject("Orders failed");
const p3 = Promise.resolve("Portfolio");

const results = await Promise.allSettled([
    p1,
    p2,
    p3
]);

console.log(results);
```

Result conceptually:

```javascript
[
    {
        status: "fulfilled",
        value: "User"
    },
    {
        status: "rejected",
        reason: "Orders failed"
    },
    {
        status: "fulfilled",
        value: "Portfolio"
    }
]
```

### Difference

`Promise.all()`:

```text
One fails
   ↓
Entire Promise.all rejects
```

`Promise.allSettled()`:

```text
One fails
   ↓
Wait for everyone
   ↓
Give result of every Promise
```

---

## 15. Promise.race()

`Promise.race()` returns the result of the first Promise to settle.

"Settle" means:

- fulfilled OR rejected

Example:

```javascript
const p1 = new Promise(resolve => {
    setTimeout(() => resolve("A"), 3000);
});

const p2 = new Promise(resolve => {
    setTimeout(() => resolve("B"), 1000);
});

const result = await Promise.race([p1, p2]);

console.log(result);
```

Output:

```text
B
```

because `p2` finished first.

But if the first Promise rejects:

```javascript
const p1 = Promise.reject("Error");

const p2 = new Promise(resolve => {
    setTimeout(() => resolve("Success"), 1000);
});

Promise.race([p1, p2])
    .catch(console.log);
```

Output:

```text
Error
```

So:

> `race()` cares about the first Promise to settle, whether success or failure.

---

## 16. Promise.any()

`Promise.any()` is different.

It waits for the first successful Promise.

Example:

```javascript
const p1 = Promise.reject("Server 1 failed");

const p2 = new Promise(resolve => {
    setTimeout(() => resolve("Server 2"), 1000);
});

const p3 = new Promise(resolve => {
    setTimeout(() => resolve("Server 3"), 2000);
});

const result = await Promise.any([p1, p2, p3]);

console.log(result);
```

Output:

```text
Server 2
```

Even though `p1` rejected first.

Why?

Because `Promise.any()` ignores rejected Promises until it finds a fulfilled one.

---

## 17. What if ALL promises reject in Promise.any()?

Then:

```javascript
Promise.any([
    Promise.reject("A"),
    Promise.reject("B"),
    Promise.reject("C")
]);
```

rejects with:

**AggregateError**

It contains the individual errors.

---

## 18. The Most Important Comparison

Memorize this table:

| Method | Success condition | Failure condition |
| --- | --- | --- |
| `Promise.all()` | All fulfill | Any one rejects |
| `Promise.allSettled()` | Always returns after all settle | Never rejects because of input promises |
| `Promise.race()` | First promise to settle | First promise to reject |
| `Promise.any()` | First promise to fulfill | All reject |

Easy memory trick:

```text
ALL
↓
Everyone must succeed

ALL SETTLED
↓
I want everyone's result

RACE
↓
First result — success OR failure

ANY
↓
First SUCCESS
```

---

## 19. Real Interview Example

Imagine your Paytm Money dashboard needs:

1. User Profile
2. Portfolio
3. Bank Accounts

You can do:

```javascript
async function loadDashboard() {

    const [profile, portfolio, bankAccounts] =
        await Promise.all([
            getProfile(),
            getPortfolio(),
            getBankAccounts()
        ]);

    console.log(profile);
    console.log(portfolio);
    console.log(bankAccounts);
}
```

Why `Promise.all()`?

Because these requests are independent.

Instead of:

```javascript
const profile = await getProfile();
const portfolio = await getPortfolio();
const bankAccounts = await getBankAccounts();
```

which is roughly:

```text
Profile
   ↓
Portfolio
   ↓
Bank Accounts
```

we can do:

```text
Profile ───────┐
Portfolio ─────┼──→ All complete
Bank Accounts ─┘
```

This reduces total waiting time when the requests can safely run concurrently.

---

## 20. Important Interview Trap

What is wrong with this?

```javascript
const a = await getA();
const b = await getB();
const c = await getC();
```

If they are independent, you're unnecessarily making them wait sequentially.

Better:

```javascript
const [a, b, c] = await Promise.all([
    getA(),
    getB(),
    getC()
]);
```

But don't blindly use `Promise.all()`.

If:

```javascript
getB()
```

depends on:

```javascript
getA()
```

then sequential execution may be necessary.

---

## 21. One Interview Question You Should Be Able to Answer

**Q: Difference between Promise.all() and Promise.allSettled()?**

> `Promise.all()` succeeds only when all promises fulfill and rejects as soon as one rejects. `Promise.allSettled()` waits for every promise to settle and returns the status and result of each promise, regardless of success or failure.

**Q: Difference between Promise.race() and Promise.any()?**

> `Promise.race()` returns the first promise that settles, whether fulfilled or rejected. `Promise.any()` returns the first fulfilled promise and only rejects if all promises reject.

**Q: Does Promise.all() cancel remaining promises when one fails?**

> No. `Promise.all()` rejects immediately from the caller's perspective, but it does not automatically cancel the other underlying operations.

**Q: What does async return?**

> An async function always returns a Promise.

**Q: Does await block the JavaScript thread?**

> No. It pauses the current async function until the Promise settles, while the JavaScript event loop can continue executing other work.

---

## 🔥 Day 5 — Must Know Flow

You should now be able to explain this without looking:

```text
Promise
  │
  ├── pending
  │
  ├── fulfilled ──→ then()
  │
  └── rejected ───→ catch()
                         │
                       finally()
```

And:

```text
Promise.all()
→ all must succeed

Promise.allSettled()
→ wait for everyone

Promise.race()
→ first to settle

Promise.any()
→ first to succeed
```

The most important interview trap is:

```javascript
Promise.all([A, B, C])
```

If B rejects:

```text
A ── may continue
B ── ❌
C ── may continue

Promise.all()
      ↓
   rejects
```

It doesn't cancel A and C automatically.
