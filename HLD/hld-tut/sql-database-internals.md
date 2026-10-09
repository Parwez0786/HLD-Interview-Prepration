# SQL Database Internals

For an SDE-1/backend interview, don't just memorize definitions. You should understand what happens inside the database when a query runs.

---

## 1. B-Tree Index

A B-Tree index is a tree-based data structure used to find rows efficiently without scanning the entire table.

Example:

```sql
CREATE INDEX idx_user_id
ON users(user_id);
```

Suppose we have:

**users**

| user_id | name |
| --- | --- |
| 10 | A |
| 20 | B |
| 30 | C |
| 40 | D |
| 50 | E |
| ... | ... |

### Without index

```text
Query
 ↓
Scan row 1
 ↓
Scan row 2
 ↓
Scan row 3
 ↓
...
```

### With B-Tree

```text
             40
           /    \
       20,30    50,60
      /   \      /   \
    ...   ...  ...   ...
```

The database can navigate toward the required value.

### Complexity

Approximately:

- Search: O(log N)
- Insert: O(log N)
- Delete: O(log N)

### Why B-Tree is commonly used

It supports:

```sql
WHERE id = 10

WHERE id > 10

WHERE id BETWEEN 10 AND 50

ORDER BY id

MIN(id)

MAX(id)
```

**Interview answer:**

> A B-Tree index keeps indexed values in sorted order and allows the database to locate rows in logarithmic time instead of scanning the entire table. It is especially useful for equality, range queries and ordering.

---

## 2. Hash Index

A hash index uses a hash function to locate a value.

Conceptually:

```text
user_id
   ↓
hash(user_id)
   ↓
bucket
   ↓
row
```

For example:

```text
hash(101) → bucket 5
hash(202) → bucket 2
```

It is very efficient for exact equality lookups:

```sql
WHERE user_id = 101
```

But hash indexes are not naturally suited for:

```sql
WHERE user_id > 100
```

because hash values aren't ordered.

### B-Tree vs Hash

| Feature | B-Tree | Hash |
| --- | --- | --- |
| Equality | ✅ | ✅ |
| Range | ✅ | ❌ |
| ORDER BY | ✅ | ❌ |
| BETWEEN | ✅ | ❌ |
| Sorted data | ✅ | ❌ |
| Typical use | General purpose | Equality lookup |

**Interview answer:**

> Hash indexes are useful for exact-match lookups, while B-Tree indexes are more general because they support equality, range queries and ordering.

---

## 3. Composite Index

A composite index contains multiple columns.

```sql
CREATE INDEX idx_user_status_date
ON orders(user_id, status, created_at);
```

The order is important.

Think of it like:

```text
(user_id)
   ↓
(status)
   ↓
(created_at)
```

### Query

```sql
SELECT *
FROM orders
WHERE user_id = 100
AND status = 'SUCCESS';
```

This can use the composite index efficiently.

But:

```sql
WHERE status = 'SUCCESS'
```

may not efficiently use the index because `user_id` is the first column.

### Leftmost Prefix Rule

For:

```sql
INDEX(user_id, status, created_at)
```

These are generally useful:

```sql
WHERE user_id = ?

WHERE user_id = ?
AND status = ?

WHERE user_id = ?
AND status = ?
AND created_at = ?
```

But:

```sql
WHERE status = ?
```

doesn't benefit in the same way from the leading portion of the index.

### Interview question

**Q:** Why does column order matter in composite indexes?

**Answer:**

> Because the database organizes the index according to the column order. The first column creates the primary ordering, so queries that don't constrain the leading column may not be able to efficiently navigate the index.

---

## 4. Covering Index

A covering index contains all the columns required by a query.

Suppose:

```sql
CREATE INDEX idx_user_status_name
ON users(user_id, status, name);
```

Query:

```sql
SELECT name, status
FROM users
WHERE user_id = 100;
```

The database can potentially get everything it needs directly from the index.

```text
Query
 ↓
Index
 ↓
Result
```

Instead of:

```text
Query
 ↓
Index
 ↓
Find row location
 ↓
Table
 ↓
Fetch columns
 ↓
Result
```

This avoids additional table lookups.

**Interview answer:**

> A covering index contains all columns needed by a query, allowing the database to answer the query directly from the index without accessing the base table.

---

## 5. Query Execution

Consider:

```sql
SELECT name
FROM users
WHERE age > 25;
```

Conceptually the database does:

```text
SQL Query
   ↓
Parser
   ↓
Query Analyzer
   ↓
Optimizer
   ↓
Execution Plan
   ↓
Storage Engine
   ↓
Rows
```

The important part is the **query optimizer**.

It decides things like:

- Should I use an index?
- Which index?
- Which join algorithm?
- Which table should I access first?
- Should I sort?
- Should I scan the table?

---

## 6. Query Optimization

Suppose:

```sql
SELECT *
FROM users
WHERE email = 'abc@gmail.com';
```

If there are 10 million rows:

### Without index

```text
10 million rows
       ↓
Full table scan
       ↓
Find matching email
```

Potentially expensive.

### With index

```text
email index
     ↓
Find email
     ↓
row location
     ↓
fetch row
```

Much less work when the index is selective.

---

## 7. Full Table Scan

A full table scan means:

The database reads the table's rows/pages and checks them against the query condition rather than using a suitable index.

Example:

```sql
SELECT *
FROM users
WHERE city = 'Bhopal';
```

If there is no useful index:

```text
Table
 ↓
Row 1 → no
Row 2 → no
Row 3 → yes
Row 4 → no
...
```

Complexity is roughly:

```text
O(N)
```

But full table scan is not always bad.

If the query needs a large percentage of the table:

```sql
SELECT *
FROM users;
```

using an index could actually be worse because the database would have to perform many index lookups and then fetch many rows.

So the optimizer may deliberately choose a table scan.

---

## 8. Index Scan

With an index:

```sql
SELECT *
FROM users
WHERE user_id = 500;
```

The database can:

```text
Index
 ↓
user_id = 500
 ↓
row pointer / row location
 ↓
table row
```

Instead of examining every row.

### Important

An index doesn't automatically make every query faster.

The optimizer considers:

- selectivity
- table size
- statistics
- index size
- query conditions
- estimated cost

---

## 9. Joins

Suppose:

**users**

- id
- name

**orders**

- id
- user_id
- amount

Query:

```sql
SELECT u.name, o.amount
FROM users u
JOIN orders o
ON u.id = o.user_id;
```

The database has to find matching rows between the two tables.

Common join algorithms include:

### Nested Loop Join

Conceptually:

```text
For each user
    find matching orders
```

Useful when one side is small or when an efficient index exists.

### Hash Join

Conceptually:

```text
Build hash table from one side
             ↓
Scan other side
             ↓
Lookup matching keys
```

Good for equality joins, especially when appropriate indexes aren't available.

### Sort-Merge Join

Conceptually:

```text
Sort table A
Sort table B
     ↓
Merge them
```

Useful when inputs are already sorted or sorting is otherwise worthwhile.

---

## 10. Connection Pooling

Opening a database connection is expensive.

### Without connection pooling

```text
Request 1
 ↓
Create DB connection
 ↓
Query
 ↓
Close connection

Request 2
 ↓
Create DB connection
 ↓
Query
 ↓
Close connection
```

This creates unnecessary overhead.

### With a connection pool

```text
             Connection Pool
          ┌────┬────┬────┬────┐
          │ C1 │ C2 │ C3 │ C4 │
          └────┴────┴────┴────┘
             ↑         ↑
          Request    Request
```

Application:

```text
Request
   ↓
Get connection from pool
   ↓
Execute query
   ↓
Return connection
   ↓
Connection remains available
```

The connection is returned to the pool, not normally physically closed after every request.

### Example

Suppose:

```text
Pool size = 10
```

and:

```text
100 requests arrive
```

Only around 10 can actively use those pooled connections at once, depending on configuration and workload.

The remaining requests wait for a connection to become available.

---

## Very Important Interview Cross-Questions

### Q1. Why not create an index on every column?

Because indexes:

- consume disk space
- slow down INSERT
- slow down UPDATE
- slow down DELETE
- require maintenance

Every write may need corresponding index updates.

### Q2. Why might the database ignore an index?

Possible reasons:

1. Query returns a large percentage of rows
2. Index has poor selectivity
3. Function applied to indexed column
4. Type conversion prevents efficient use
5. Leading column of composite index isn't constrained
6. Optimizer estimates table scan is cheaper
7. Statistics are stale

### Q3. What is a selective index?

An index is more useful when it significantly reduces the number of candidate rows.

Example:

```sql
gender = 'M'
```

may be low-selectivity if 50% of the table is male.

But:

```sql
email = 'abc@gmail.com'
```

is usually highly selective because it may identify one row.

### Q4. Index vs primary key?

A primary key is a logical database constraint identifying a row uniquely.

An index is a data structure used to speed up access.

A primary key is commonly backed by an index, but the concepts are different.

### Q5. What happens when you run EXPLAIN?

You ask the database to show its planned execution strategy.

For example:

```sql
EXPLAIN
SELECT *
FROM users
WHERE email = 'abc@gmail.com';
```

You can inspect things such as:

- access method
- chosen index
- estimated rows
- join strategy
- cost

For interview purposes, remember:

> EXPLAIN helps us understand how the database plans to execute our query and whether it is using an efficient access path.

---

## The Mental Model You Should Remember

When an interviewer gives you a slow SQL query, think in this order:

```text
1. What is the query doing?
          ↓
2. How many rows are involved?
          ↓
3. Is there a suitable index?
          ↓
4. Is the index column order correct?
          ↓
5. Is the query doing a full table scan?
          ↓
6. What joins are happening?
          ↓
7. Is the join using an efficient strategy?
          ↓
8. What does EXPLAIN show?
          ↓
9. Can I reduce rows earlier?
          ↓
10. Can I use a covering/composite index?
```

---

## One-line summary for interviews

> SQL performance mainly comes down to how efficiently the database can locate and process the required rows. Indexes reduce unnecessary scanning, the optimizer chooses an execution plan, joins determine how tables are combined, and connection pooling reduces the overhead of repeatedly creating database connections.
