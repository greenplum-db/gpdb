---
title: About Implicit Text Casting in Greenplum Database 
---

Greenplum Database version 4.3.x is based on PostgreSQL version 8.2. Greenplum Database version 6.x is based on PostgreSQL version 9.4. PostgreSQL 8.3 removed automatic implicit casts between the `text` type and other data types. When you migrate from Greenplum Database version 4.3.x to version 6, this change in behavior might impact existing applications and queries.

For information about how Greenplum Database 6 performs type casts, see [Type Casts](../admin_guide/query/topics/defining-queries.html).

**What is different in Greenplum Database 6**

Greenplum Database 6 does not automatically implicitly cast between text and other data types. Greenplum Database 6 also treats certain automatic implicit casts differently than version 4.3.x, and in some cases does not handle them at all. **Applications or queries that you wrote for Greenplum Database 4.3.x that rely on automatic implicit casting may fail on Greenplum Database version 6.**

\(The term *implicit cast*, when used in the remainder of this section, refers to implicit casts automatically applied by Greenplum Database.\)

-   Greenplum Database 6 has downgraded implicit casts in the to-text type direction; these casts are now treated as assignment casts. A cast from a data type to the text type will continue to work in Greenplum Database 6 if used in assignment contexts.
-   Greenplum Database 6 no longer automatically provides an implicit cast in the to-text type direction that can be used in expression contexts. Additionally, Greenplum Database 6 no longer provides implicit casts in the from-text type direction. When such expressions or assignments are encountered, Greenplum Database 6 returns an error and the following message:

    ```
    HINT:  No operator matches the given name and argument type(s). You might need to add explicit type casts.
    ```

    To illustrate, suppose you create two tables:

    ```
    CREATE TABLE foo (a int) DISTRIBUTED RANDOMLY ;
    CREATE TABLE bar (b text) DISTRIBUTED RANDOMLY ;
    ```

    The following examples demonstrate certain types of text comparison queries that will fail on Greenplum Database 6.

    **Note:** This is not an exhaustive list of failure scenarios.

    -   Queries that reference `text` type and non-text type columns in an expression. In this example query, the comparison expression returns a cast error.

        ```
        SELECT * FROM foo, bar WHERE foo.a = bar.b;
        ERROR:  operator does not exist: integer = text
        LINE 1: SELECT * FROM foo, bar WHERE foo.a = bar.b;
                                                   ^
        HINT:  No operator matches the given name and argument type(s). You might need to add explicit type casts.
        ```

        The updated example casts the `text` type to an `integer` type.

        ```
        SELECT * FROM foo, bar WHERE foo.a = bar.b::int;
        ```

    -   Queries that mix the `text` type and non-text type columns in function and aggregate arguments. In this example, the query that runs the example function `concat` returns a cast error.

        ```
        CREATE FUNCTION concat(TEXT, TEXT) 
        RETURNS TEXT AS $$ 
          SELECT $1 || $2 
        $$ STRICT LANGUAGE SQL;
        
        SELECT concat('a'::TEXT, 2);
        ```

        Adding an explicit cast from `integer` to `text` fixes the issue.

        ```
        SELECT concat('a', 2::text);
        ```

    -   Queries that perform comparisons between a `text` type column and a non-quoted literal such as an `integer`, `number`, `float`, or `oid`. This example query that compares text and non-quoted integer returns an error.

        ```
        SELECT * FROM bar WHERE b = 123;
        ```

        Adding an explicit cast to text fixes the issue.

        ```
        SELECT * FROM bar WHERE b = 123::text;
        ```

    -   Queries that perform comparisons between a `date` type column or literal and an integer-like column \(Greenplum Database internally converts date types to the text type\) . This example query that compares an `integer` column with a literal of type `date` returns an error.

        ```
        SELECT * FROM foo WHERE a = '20130101'::DATE;
        ```

        There is no built-in cast from integer type to `date` type. However, you can explicitly cast an `integer` to `text` and then to `date`. The updated examples use the `cast` and `::` syntax.

        ```
        SELECT * FROM foo WHERE cast(cast(a AS text) AS date)  = '20130101'::date;
        SELECT * FROM foo WHERE (a::text)::date  = '20130101'::date;
        ```


**The only supported workaround for the implicit casting differences between Greenplum Database versions 4.3.x and 6 is to analyze failing applications and queries and update the application or query to use explicit casts to fix the failures.**

If rewriting the application or query is not feasible, you may choose to temporarily work around the change in behaviour introduced by the removal of automatic implicit casts in Greenplum Database 6. There are two well-known workarounds to this PostgreSQL issue:

-   Re-create the implicit casts \(described in [Readding implicit casts in PostgreSQL 8.3](http://petereisentraut.blogspot.com/2008/03/readding-implicit-casts-in-postgresql.html)\).
-   Create missing operators \(described in [Problems and workaround recreating implicit casts using 8.3+](http://blog.ioguix.net/postgresql/2010/12/11/Problems-and-workaround-recreating-casts-with-8.3+.html)\).

The workaround to re-create the implicit casts is not recommended as it breaks concatenation functionality. With the create missing operators workaround, you create the operators and functions that implement the comparison expressions that are failing in your applications and queries.

## <a id="temp_workaround"></a>Workaround: Manually Creating Missing Operators 

**Warning:** Use this workaround only to aid migration to Greenplum Database 6 for evaluation purposes. Do not use this workaround in a production environment.

When you create an operator, you identify the data types of the left operand and the right operand. You also identify the name of a function that Greenplum Database invokes to evaluate the operator expression between the specified data types. The operator function evaluates the expression by performing either to-text or from-text conversion using the INPUT/OUTPUT methods of the data types involved. By creating operators for each \(text type, other data type\) and \(other data type, text type\) combination, you effectively implement the casts that are missing in Greenplum Database 6.

To implement this workaround, complete the following tasks **after** you install Greenplum Database 6:

1.  Identify and note the names of the Greenplum 6 databases in which you want to create the missing operators. Consider applying this workaround to all databases in your Greenplum Database deployment.
2.  Identify a schema in which to create the operators and functions. Use a schema other than `pg_catalog` to ensure that these objects are included in a `pg_dump` or `gpbackup` of the database. This procedure will use a schema named `cast_fix` for illustrative purposes.
3.  Review the blog entry [Problems and workaround recreating implicit casts using 8.3+](http://blog.ioguix.net/postgresql/2010/12/11/Problems-and-workaround-recreating-casts-with-8.3+.html). The blog discusses this temporary workaround to the casting issue, i.e. creating missing operators. It also references a SQL script that you can run to create a set of equality \(`=`\) operators and functions for several text and other data type comparisons.
4.  Download the [8.3 operator workaround.sql](https://gist.github.com/ioguix/4dd187986c4a1b7e1160) script referenced on the blog page, noting the location to which the file was downloaded on your local system.
5.  The `8.3 operator workaround.sql` script creates the equality operators and functions. Open the script in the editor of your choice, and examine the contents. For example, using the `vi` editor:

    ```
    vi 8.3 operator workaround.sql
    ```

    Notice that the script creates the operators and functions in the `pg_catalog` schema.

6.  Replace occurrences of `pg_catalog` in the script with the name of the schema that you identified in Step 2, and then save the file and exit the editor. \(You will create this schema in an upcoming step if the schema does not already exist.\) For example:

    ```
    :s/pg_catalog/cast_fix/g
    :wq
    ```

7.  Analyze your failing queries, identifying the operators and from-type and to-type data type comparisons that are the source of the failures. Compare this list to the contents of the `8.3 operator workaround.sql` script, and identify the minimum set of additional operators and left\_type/right\_type expression combinations that you must support.
8.  For each operator and left\_type/right\_type combination that you identify in the previous step, add `CREATE` statements for the following *objects* to the `8.3 operator workaround.sql` script:
    1.  *Create the function that implements the left\_type operator right\_type comparison.* For example, to create a function that implements the greater than \(`>`\) operator for text \(left\_type\) to integer \(right\_type\) comparison:

        ```
        CREATE FUNCTION cast_fix.textgtint(text, integer)
        RETURNS boolean
        STRICT IMMUTABLE LANGUAGE SQL AS $$ 
          SELECT textin(int4out($2)) > $1;
        $$;
        ```

        Be sure to schema-qualify the function name.

    2.  *Create the operator*. For example, to create a greater than \(`>`\) operator for text \(left\_type\) to integer \(right\_type\) type comparison that specifies the function you created above:

        ```
        CREATE OPERATOR cast_fix.> (PROCEDURE=cast_fix.textgtint, LEFTARG=text, RIGHTARG=integer, COMMUTATOR=OPERATOR(cast_fix.>))
        ```

        Be sure to schema-qualify the operator and function names.

    3.  You must create another operator and function if you want the operator to work in reverse \(i.e. using the example above, if you want a greater than operator for integer \(left\_type\) to text \(right\_type\) comparison.\)
9.  For each database that you identified in Step 1, add the missing operators. For example:

    1.  Connect to the database as an administrative user. For example:

        ```
        $ psql -d database1 -U gpadmin
        ```

    2.  Create the schema if it does not already exist. For example:

        ```
        CREATE SCHEMA cast_fix;
        ```

    3.  Run the script. For example, if you downloaded the file to the `/tmp` directory:

        ```
        \i '/tmp/8.3 operator workaround.sql'
        ```

    You must create the schema and run the script for every new database that you create in your Greenplum Database cluster.

10. Identify and note the names of the users/roles to which you want to provide this capability. Consider exposing this to all roles in your Greenplum Database deployment.
11. For each role that you identified in Step 10, add the schema to the role's `search_path`. For example:

    ```
    SHOW search_path;
    ALTER ROLE bill SET search_path TO existing_search_path, cast_fix;
    ```

    If required, also grant schema-level permissions to the role.


