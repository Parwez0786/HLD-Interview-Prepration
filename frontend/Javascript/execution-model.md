# Week 1 — JavaScript Day 1: Execution Model

The goal today is to understand what JavaScript does internally when it runs your code. Once this is clear, topics like closures, `this`, promises, and the event loop become much easier.

---

## Table of Contents

- [1. How JavaScript executes code](#1-how-javascript-executes-code)
- [2. Execution Context](#2-execution-context)
- [3. Global Execution Context](#3-global-execution-context)
- [4. Two phases of execution](#4-two-phases-of-execution)
- [5. Call Stack](#5-call-stack)
- [6. Function Execution Context](#6-function-execution-context)
- [7. Lexical Environment](#7-lexical-environment)
- [8. Scope](#8-scope)
- [9. Scope Chain](#9-scope-chain)
- [10. Now the important question: var](#10-now-the-important-question-var)
- [11. Now let](#11-now-let)
- [12. var vs let](#12-var-vs-let)
- [13. What about const?](#13-what-about-const)
- [14. Block Scope](#14-block-scope)
- [15. Function Scope](#15-function-scope)
- [16. Complete example](#16-complete-example)
- [17. Very important interview distinction](#17-very-important-interview-distinction)
- [18. Interview question: Explain this](#18-interview-question-explain-this)
- [19. Interview question: Explain this](#19-interview-question-explain-this-1)
- [20. One mental model to remember](#20-one-mental-model-to-remember)
- [Day 1 — What you should be able to answer](#day-1--what-you-should-be-able-to-answer)

---

## 1. How JavaScript executes code

Consider:

```javascript
var a = 10;

function greet() {
    console.log("Hello");
}

greet();
```

JavaScript doesn't simply execute every line randomly.

At a high level:

```text
JavaScript Code
      ↓
Create Execution Context
      ↓
Memory/Environment Setup
      ↓
Execute Code
      ↓
Call Stack manages function execution
```

The most important concepts are:

- Execution Context
- Call Stack
- Global Execution Context
- Function Execution Context
- Lexical Environment
- Scope
- Scope Chain
- Hoisting

---

## 2. Execution Context

An Execution Context is the environment in which JavaScript code is evaluated and executed.

Think:

> Execution Context = everything JavaScript needs to execute a piece of code.

There are mainly two execution contexts you'll encounter:

### Global Execution Context

Created when JavaScript starts executing your script.

### Function Execution Context

Created every time a function is called.

For example:

```javascript
var a = 10;

function add(x, y) {
    var result = x + y;
    return result;
}

add(5, 10);
```

JavaScript creates:

```text
Global Execution Context
        ↓
    add() called
        ↓
Function Execution Context
```

---

## 3. Global Execution Context

When JavaScript starts your program, it creates the Global Execution Context (GEC).

Example:

```javascript
var name = "Parwez";

function greet() {
    console.log("Hello");
}

console.log(name);
greet();
```

Conceptually:

```text
Global Execution Context

Variables:
    name → "Parwez"

Functions:
    greet → function

Other global information:
    global object
    this
    lexical environment
```

The global context remains active while your script is running.

---

## 4. Two phases of execution

This is extremely important for understanding hoisting.

JavaScript execution can be simplified into two phases:

- Phase 1 → Creation / Initialization
- Phase 2 → Execution

Let's use:

```javascript
console.log(a);

var a = 10;

console.log(a);
```

### Phase 1

JavaScript processes declarations.

Conceptually:

```text
a → undefined
```

It doesn't yet execute:

```javascript
a = 10;
```

### Phase 2

Now statements execute:

```javascript
console.log(a);  // undefined

a = 10;

console.log(a);  // 10
```

So:

**Before execution:**

```text
a → undefined
```

**During execution:**

```text
console.log(a)
       ↓
undefined

a = 10
       ↓
a → 10
```

This behavior is related to hoisting.

---

## 5. Call Stack

The call stack keeps track of which function JavaScript is currently executing.

Example:

```javascript
function first() {
    second();
}

function second() {
    console.log("Hello");
}

first();
```

Initially:

```text
Call Stack

Global
```

Then:

```javascript
first();
```

So:

```text
Call Stack

first()
Global
```

Inside `first()`:

```javascript
second();
```

Now:

```text
Call Stack

second()
first()
Global
```

`second()` finishes:

```text
Call Stack

first()
Global
```

`first()` finishes:

```text
Call Stack

Global
```

Then the program finishes.

### Important interview statement

> The call stack follows LIFO — Last In, First Out.

---

## 6. Function Execution Context

Every time you call a function, JavaScript creates a new execution context for that function.

Example:

```javascript
function add(a, b) {
    var sum = a + b;
    return sum;
}

var result = add(10, 20);
```

When JavaScript reaches:

```javascript
add(10, 20);
```

it creates a function execution context:

```text
Function Execution Context

Parameters:
    a → 10
    b → 20

Local variables:
    sum

Lexical environment:
    ...
```

And puts it on the call stack:

```text
Call Stack

add()
Global
```

When `add()` returns:

```text
Call Stack

Global
```

The function execution context is no longer active.

---

## 7. Lexical Environment

This is one of the most important concepts for interviews.

A lexical environment is the structure JavaScript uses to keep track of variables/functions and their relationship to the surrounding code.

Consider:

```javascript
var x = 10;

function test() {
    var y = 20;

    console.log(x);
    console.log(y);
}
```

The function `test` is lexically inside the global code.

Conceptually:

```text
Global Lexical Environment
│
├── x → 10
└── test → function
       │
       ↓
   Function Lexical Environment
   │
   └── y → 20
```

Because `test()` was created inside the global environment, it can access variables from that surrounding environment.

---

## 8. Scope

Scope determines where a variable can be accessed.

Example:

```javascript
let x = 10;

function test() {
    let y = 20;

    console.log(x); // 10
    console.log(y); // 20
}

test();

console.log(x); // 10
console.log(y); // Error
```

Why can't the global code access `y`?

Because `y` belongs to the scope of `test()`.

```text
Global Scope
│
└── x

Function Scope
│
└── y
```

`test()` can access the outer scope.

The outer scope cannot access variables created inside `test()`.

---

## 9. Scope Chain

Suppose:

```javascript
var a = 10;

function outer() {
    var b = 20;

    function inner() {
        var c = 30;

        console.log(a);
        console.log(b);
        console.log(c);
    }

    inner();
}

outer();
```

When `inner()` executes:

```text
inner scope
    ↓
outer scope
    ↓
global scope
```

For:

```javascript
console.log(b);
```

JavaScript searches:

```text
inner scope
    ↓
Doesn't find b

outer scope
    ↓
Finds b = 20
```

For:

```javascript
console.log(a);
```

JavaScript searches:

```text
inner
 ↓
outer
 ↓
global
 ↓
a = 10
```

This is the scope chain.

### Interview definition

> The scope chain is the chain of lexical environments JavaScript searches when resolving a variable.

---

## 10. Now the important question: var

Consider:

```javascript
console.log(a);

var a = 10;
```

Output:

```text
undefined
```

Why?

Because `var` declarations are initialized to `undefined` during the creation phase.

Conceptually JavaScript behaves like:

```javascript
var a;

console.log(a);

a = 10;
```

So:

**Creation phase:**

```text
a → undefined
```

Then:

**Execution:**

```text
console.log(a)
      ↓
undefined

a = 10
      ↓
a → 10
```

### Important

It is not actually moving the line of code physically to the top.

When people say:

> "var is hoisted."

They mean that the declaration is processed before normal statement execution.

---

## 11. Now let

Consider:

```javascript
console.log(a);

let a = 10;
```

This produces:

```text
ReferenceError
```

Why doesn't it print `undefined`?

Because `let` behaves differently.

The variable exists in the lexical environment, but it isn't initialized with a usable value until execution reaches:

```javascript
let a = 10;
```

The period between entering the scope and reaching the declaration is called the:

**Temporal Dead Zone — TDZ**

Example:

```javascript
console.log(a);

let a = 10;
```

Conceptually:

**Creation phase:**

```text
a → uninitialized
```

Then:

```javascript
console.log(a);
```

JavaScript tries to access `a`.

But:

```text
a → uninitialized
```

Therefore:

```text
ReferenceError
```

Only when execution reaches:

```javascript
let a = 10;
```

does `a` become initialized:

```text
a → 10
```

---

## 12. var vs let

This is a very common interview question.

| Feature | var | let |
| --- | --- | --- |
| Hoisted | Yes | Yes |
| Initial value during setup | undefined | Uninitialized |
| TDZ | No | Yes |
| Access before declaration | undefined | ReferenceError |
| Scope | Function scoped | Block scoped |
| Redeclaration in same scope | Allowed | Not allowed |

### var

```javascript
console.log(a);

var a = 10;
```

Output:

```text
undefined
```

### let

```javascript
console.log(a);

let a = 10;
```

Output:

```text
ReferenceError
```

---

## 13. What about const?

`const` behaves similarly to `let` regarding hoisting and TDZ.

```javascript
console.log(a);

const a = 10;
```

Result:

```text
ReferenceError
```

`const` is also block scoped.

---

## 14. Block Scope

Consider:

```javascript
{
    let a = 10;
    const b = 20;
    var c = 30;
}

console.log(c); // 30

console.log(a); // ReferenceError
console.log(b); // ReferenceError
```

Why?

`let` and `const` are block scoped.

```text
{
    let a
    const b
}
```

They exist only inside that block.

But `var` is not block scoped.

```text
{
    var c
}
```

`c` belongs to the surrounding function/global scope.

---

## 15. Function Scope

Example:

```javascript
function test() {
    var a = 10;
}

console.log(a);
```

Result:

```text
ReferenceError
```

Because `a` belongs to the function's scope.

```text
Global
│
└── test()
      │
      └── a
```

The global scope cannot access `a`.

---

## 16. Complete example

Now let's combine everything.

```javascript
var a = 10;

function outer() {
    var b = 20;

    function inner() {
        var c = 30;

        console.log(a);
        console.log(b);
        console.log(c);
    }

    inner();
}

outer();
```

### Step 1 — Global context

```text
Global Environment

a → 10
outer → function
```

### Step 2 — outer() called

```text
Call Stack

outer()
Global
```

`outer` gets:

```text
b → 20
inner → function
```

### Step 3 — inner() called

```text
Call Stack

inner()
outer()
Global
```

`inner` gets:

```text
c → 30
```

### Step 4 — console.log(a)

Search:

```text
inner
 ↓
outer
 ↓
global
 ↓
a = 10
```

Result:

```text
10
```

### Step 5 — console.log(b)

Search:

```text
inner
 ↓
outer
 ↓
b = 20
```

Result:

```text
20
```

### Step 6 — console.log(c)

Found immediately:

```text
c = 30
```

Result:

```text
30
```

---

## 17. Very important interview distinction

Don't say:

> "JavaScript executes everything line by line."

That's incomplete.

A better answer is:

> JavaScript creates execution contexts, establishes the relevant lexical environments and bindings, and then executes statements. Function calls create new function execution contexts that are managed through the call stack.

---

## 18. Interview question: Explain this

```javascript
console.log(a);

var a = 10;
```

### Good interview answer

> `var` is hoisted. During creation of the execution context, the binding for `a` is created and initialized with `undefined`. When `console.log(a)` executes, `a` therefore contains `undefined`. Later, `a = 10` assigns the value `10`.

---

## 19. Interview question: Explain this

```javascript
console.log(a);

let a = 10;
```

### Good interview answer

> `let` is also hoisted in the sense that its lexical binding is created during environment setup, but it is not initialized to `undefined`. It remains uninitialized in the Temporal Dead Zone until execution reaches the declaration. Accessing it before initialization causes a `ReferenceError`.

This wording is much better than saying:

> "let is not hoisted."

Because technically, that statement is misleading.

---

## 20. One mental model to remember

For every piece of JavaScript, think:

```text
                 JavaScript starts
                        │
                        ↓
             Create Execution Context
                        │
                        ↓
              Create variable bindings
                        │
                        ↓
              Execute statements
                        │
              ┌─────────┴─────────┐
              ↓                   ↓
        Normal code          Function call
                                  │
                                  ↓
                         Create Function EC
                                  │
                                  ↓
                           Push to Call Stack
                                  │
                                  ↓
                              Execute
                                  │
                                  ↓
                              Return
                                  │
                                  ↓
                         Pop from Call Stack
```

And for variable lookup:

```text
Current Lexical Environment
            │
            ↓
     Not found?
            │
            ↓
Outer Lexical Environment
            │
            ↓
     Not found?
            │
            ↓
Outer Lexical Environment
            │
            ↓
         Global
            │
            ↓
     Still not found?
            │
            ↓
      ReferenceError
```

---

## Day 1 — What you should be able to answer

Before moving to Day 2, make sure you can explain these without memorizing:

```javascript
console.log(a);
var a = 10;
```

```javascript
console.log(a);
let a = 10;
```

```javascript
var a = 10;

function test() {
    var b = 20;
    console.log(a);
}

test();
```

```javascript
{
    let x = 10;
    var y = 20;
}

console.log(x);
console.log(y);
```

And especially:

> "What exactly happens in the execution context when JavaScript encounters `var`, `let`, and `const`?"

That question connects hoisting + lexical environment + scope + TDZ, and is a very common JavaScript interview area.
