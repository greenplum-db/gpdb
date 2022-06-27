# Creating and Managing Tables 

Greenplum Database tables are similar to tables in any relational database, except that table rows are distributed across the different segments in the system. When you create a table, you specify the table's distribution policy.

## Creating a Table 

The `CREATE TABLE` command creates a table and defines its structure. When you create a table, you define:

-   The columns of the table and their associated data types. See [Choosing Column Data Types](#topic27).
-   Any table or column constraints to limit the data that a column or table can contain. See [Setting Table and Column Constraints](#topic28).
-   The distribution policy of the table, which determines how Greenplum Database divides data is across the segments. See [Choosing the Table Distribution Policy](#topic34).
-   The way the table is stored on disk. See [Choosing the Table Storage Model](ddl-storage.html).
-   The table partitioning strategy for large tables. See [Creating and Managing Databases](ddl-database.html).

### Choosing Column Data Types 

The data type of a column determines the types of data values the column can contain. Choose the data type that uses the least possible space but can still accommodate your data and that best constrains the data. For example, use character data types for strings, date or timestamp data types for dates, and numeric data types for numbers.

For table columns that contain textual data, specify the data type `VARCHAR` or `TEXT`. Specifying the data type `CHAR` is not recommended. In Greenplum Database, the data types `VARCHAR` or `TEXT` handles padding added to the data \(space characters added after the last non-space character\) as significant characters, the data type `CHAR` does not. For information on the character data types, see the `CREATE TABLE` command in the *Greenplum Database Reference Guide*.

Use the smallest numeric data type that will accommodate your numeric data and allow for future expansion. For example, using `BIGINT` for data that fits in `INT` or `SMALLINT` wastes storage space. If you expect that your data values will expand over time, consider that changing from a smaller datatype to a larger datatype after loading large amounts of data is costly. For example, if your current data values fit in a `SMALLINT` but it is likely that the values will expand, `INT` is the better long-term choice.

Use the same data types for columns that you plan to use in cross-table joins. Cross-table joins usually use the primary key in one table and a foreign key in the other table. When the data types are different, the database must convert one of them so that the data values can be compared correctly, which adds unnecessary overhead.

Greenplum Database has a rich set of native data types available to users. See the *Greenplum Database Reference Guide* for information about the built-in data types.

### Setting Table and Column Constraints 

You can define constraints on columns and tables to restrict the data in your tables. Greenplum Database support for constraints is the same as PostgreSQL with some limitations, including:

-   `CHECK` constraints can refer only to the table on which they are defined.
-   `UNIQUE` and `PRIMARY KEY` constraints must be compatible with their tableʼs distribution key and partitioning key, if any.

    **Note:** `UNIQUE` and `PRIMARY KEY` constraints are not allowed on append-optimized tables because the `UNIQUE` indexes that are created by the constraints are not allowed on append-optimized tables.

-   `FOREIGN KEY` constraints are allowed, but not enforced.
-   Constraints that you define on partitioned tables apply to the partitioned table as a whole. You cannot define constraints on the individual parts of the table.

#### Check Constraints 

Check constraints allow you to specify that the value in a certain column must satisfy a Boolean \(truth-value\) expression. For example, to require positive product prices:

```
=> CREATE TABLE products 
            ( product_no integer, 
              name text, 
              price numeric CHECK (price > 0) );
```

#### Not-Null Constraints 

Not-null constraints specify that a column must not assume the null value. A not-null constraint is always written as a column constraint. For example:

```
=> CREATE TABLE products 
       ( product_no integer NOT NULL,
         name text NOT NULL,
         price numeric );

```

#### Unique Constraints 

Unique constraints ensure that the data contained in a column or a group of columns is unique with respect to all the rows in the table. The table must be hash-distributed \(not `DISTRIBUTED RANDOMLY`\), and the constraint columns must be the same as \(or a superset of\) the table's distribution key columns. For example:

```
=> CREATE TABLE products 
       ( `product_no` integer `UNIQUE`, 
         name text, 
         price numeric)
`      DISTRIBUTED BY (``product_no``)`;

```

#### Primary Keys 

A primary key constraint is a combination of a `UNIQUE` constraint and a `NOT NULL` constraint. The table must be hash-distributed \(not `DISTRIBUTED RANDOMLY`\), and the primary key columns must be the same as \(or a superset of\) the table's distribution key columns. If a table has a primary key, this column \(or group of columns\) is chosen as the distribution key for the table by default. For example:

```
=> CREATE TABLE products 
       ( `product_no` integer `PRIMARY KEY`, 
         name text, 
         price numeric)
`      DISTRIBUTED BY (``product_no``)`;

```

#### Foreign Keys 

Foreign keys are not supported. You can declare them, but referential integrity is not enforced.

Foreign key constraints specify that the values in a column or a group of columns must match the values appearing in some row of another table to maintain referential integrity between two related tables. Referential integrity checks cannot be enforced between the distributed table segments of a Greenplum database.

### Choosing the Table Distribution Policy 

All Greenplum Database tables are distributed. When you create or alter a table, you optionally specify `DISTRIBUTED BY` \(hash distribution\) or `DISTRIBUTED RANDOMLY` \(round-robin distribution\) to determine the table row distribution.

**Note:** The Greenplum Database server configuration parameter `gp_create_table_random_default_distribution` controls the table distribution policy if the DISTRIBUTED BY clause is not specified when you create a table.

For information about the parameter, see "Server Configuration Parameters" of the *Greenplum Database Reference Guide*.

Consider the following points when deciding on a table distribution policy.

-   **Even Data Distribution** — For the best possible performance, all segments should contain equal portions of data. If the data is unbalanced or skewed, the segments with more data must work harder to perform their portion of the query processing. Choose a distribution key that is unique for each record, such as the primary key.
-   **Local and Distributed Operations** — Local operations are faster than distributed operations. Query processing is fastest if the work associated with join, sort, or aggregation operations is done locally, at the segment level. Work done at the system level requires distributing tuples across the segments, which is less efficient. When tables share a common distribution key, the work of joining or sorting on their shared distribution key columns is done locally. With a random distribution policy, local join operations are not an option.
-   **Even Query Processing** — For best performance, all segments should handle an equal share of the query workload. Query workload can be skewed if a table's data distribution policy and the query predicates are not well matched. For example, suppose that a sales transactions table is distributed on the customer ID column \(the distribution key\). If a predicate in a query references a single customer ID, the query processing work is concentrated on just one segment.

#### Declaring Distribution Keys 

`CREATE TABLE`'s optional clauses `DISTRIBUTED BY` and `DISTRIBUTED RANDOMLY` specify the distribution policy for a table. The default is a hash distribution policy that uses either the `PRIMARY KEY` \(if the table has one\) or the first column of the table as the distribution key. Columns with geometric or user-defined data types are not eligible as Greenplum distribution key columns. If a table does not have an eligible column, Greenplum distributes the rows randomly or in round-robin fashion.

To ensure even distribution of data, choose a distribution key that is unique for each record. If that is not possible, choose `DISTRIBUTED RANDOMLY`. For example:

```
=> CREATE TABLE products
`                        (name varchar(40),
                         prod_id integer,
                         supplier_id integer)
             DISTRIBUTED BY (prod_id);
`
```

```
=> CREATE TABLE random_stuff
`                        (things text,
                         doodads text,
                         etc text)
             DISTRIBUTED RANDOMLY;
`
```

**Important:** A Primary Key is always the Distribution Key for a table. If no Primary Key exists, but a Unique Key exists, this is the Distribution Key for the table.

