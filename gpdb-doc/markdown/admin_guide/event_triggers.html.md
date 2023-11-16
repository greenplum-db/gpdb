---
title: Creating and Using Event Triggers
---

Greenplum Database provides event triggers. Unlike regular triggers, which are attached to a single table and capture only DML events, event triggers are global to a particular database and are capable of capturing DDL events.

You can write event trigger functions in any procedural language that includes event trigger support, or in C, but not in plain SQL.


## <a id="about"></a>About Event Trigger Behavior

An event trigger fires whenever the event with which it is associated occurs in the database in which the trigger is defined. The only supported events are `ddl_command_start`, `ddl_command_end`, `table_rewrite`, and `sql_drop`. Greenplum Database may add support for additional events in future releases.

The `ddl_command_start` event occurs just before the execution of a `CREATE`, `ALTER`, `DROP`, `COMMENT`, `GRANT`, or `REVOKE` command. Greenplum Database performs no check whether the affected object exists or doesn't exist before the event trigger fires. As an exception, however, this event does not occur for DDL commands targeting shared objects — databases, roles, and tablespaces — or for commands targeting event triggers themselves. The event trigger mechanism does not support these object types. `ddl_command_start` also occurs just before the execution of a `SELECT INTO` command, since this is equivalent to `CREATE TABLE AS`.

The `ddl_command_end` event occurs just after the execution of this same set of commands. To obtain more details on the DDL operations that took place, use the set-returning function `pg_event_trigger_ddl_commands()` (see [Event Trigger Functions](#functions)). Note that the trigger fires after the actions have taken place (but before the transaction commits), and thus the system catalogs can be read as already changed.

The `sql_drop` event occurs just before the `ddl_command_end` event trigger for any operation that drops database objects. To list the objects that have been dropped, use the set-returning function `pg_event_trigger_dropped_objects()` (see [Event Trigger Functions](#functions)). Note that the trigger is invoked after the objects have been deleted from the system catalogs, so it's not possible to look them up anymore.

The `table_rewrite` event occurs just before a table is rewritten by some actions of the commands `ALTER TABLE` and `ALTER TYPE`. While other control statements are available to rewrite a table, like `CLUSTER` and `VACUUM`, the `table_rewrite` event is not triggered by these.

> *Note** Greenplum Database does not fire a `table_rewrite` event for `ALTER TABLE ... SET WITH (REORGANIZE=true)` and `ALTER TABLE ... REPACK BY` commands.

Event triggers (like other functions) cannot be run in an aborted transaction. If a DDL command fails with an error, any associated `ddl_command_end` triggers will not be run. Conversely, if a `ddl_command_start` trigger fails with an error, no further event triggers will fire, and no attempt will be made to run the command itself. Similarly, if a `ddl_command_end` trigger fails with an error, the effects of the DDL statement will be rolled back, just as they would be in any other case where the containing transaction aborts.

You create an event trigger using the command [CREATE EVENT TRIGGER](../ref_guide/sql_commands/CREATE_EVENT_TRIGGER.html). In order to create an event trigger, you must first create a function with the special return type `event_trigger`. This function need not (and may not) return a value; the return type serves merely as a signal that the function is to be invoked as an event trigger.

If more than one event trigger is defined for a particular event, they will fire in alphabetical order by trigger name.

A trigger definition can also specify a `WHEN` condition so that, for example, a `ddl_command_start` trigger can be fired only for particular commands which the user wishes to intercept. A common use of such triggers is to restrict the range of DDL operations which users may perform.

## <a id="matrix"></a>Event Trigger Command Firing Matrix

The following table lists all commands for which Greenplum Database supports event triggers:

<table class="table" summary="Event Trigger Support by Command Tag" border="1">
        <colgroup>
          <col />
          <col />
          <col />
          <col />
          <col />
        </colgroup>
        <thead>
          <tr>
            <th>Command Tag</th>
            <th><code class="literal">ddl_command_start</code></th>
            <th><code class="literal">ddl_command_end</code></th>
            <th><code class="literal">sql_drop</code></th>
            <th><code class="literal">table_rewrite</code></th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td align="left"><code class="literal">ALTER AGGREGATE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER COLLATION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER CONVERSION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER DEFAULT PRIVILEGES</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER DOMAIN</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER EXTENSION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER FOREIGN DATA WRAPPER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER FOREIGN TABLE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER FUNCTION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER LANGUAGE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER MATERIALIZED VIEW</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER OPERATOR</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER OPERATOR CLASS</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER OPERATOR FAMILY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER POLICY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER PROCEDURE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER ROUTINE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER SCHEMA</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER SEQUENCE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER SERVER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER STATISTICS</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER TABLE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER TEXT SEARCH CONFIGURATION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER TEXT SEARCH DICTIONARY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER TEXT SEARCH PARSER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER TEXT SEARCH TEMPLATE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER TRIGGER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER TYPE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">X</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER USER MAPPING</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">ALTER VIEW</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">COMMENT</code><sup>1</sup></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE ACCESS METHOD</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE AGGREGATE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE CAST</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE COLLATION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE CONVERSION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE DOMAIN</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE EXTENSION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE FOREIGN DATA WRAPPER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE FOREIGN TABLE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE FUNCTION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE INDEX</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE LANGUAGE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE MATERIALIZED VIEW</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE OPERATOR</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE OPERATOR CLASS</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE OPERATOR FAMILY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE POLICY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE PROCEDURE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE RULE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE SCHEMA</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE SEQUENCE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE SERVER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE STATISTICS</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TABLE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TABLE AS</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TEXT SEARCH CONFIGURATION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TEXT SEARCH DICTIONARY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TEXT SEARCH PARSER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TEXT SEARCH TEMPLATE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TRIGGER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE TYPE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE USER MAPPING</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">CREATE VIEW</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP ACCESS METHOD</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP AGGREGATE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP CAST</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP COLLATION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP CONVERSION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP DOMAIN</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP EXTENSION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP FOREIGN DATA WRAPPER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP FOREIGN TABLE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP FUNCTION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP INDEX</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP LANGUAGE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP MATERIALIZED VIEW</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP OPERATOR</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP OPERATOR CLASS</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP OPERATOR FAMILY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP OWNED</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP POLICY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP PROCEDURE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP ROUTINE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP RULE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP SCHEMA</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP SEQUENCE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP SERVER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP STATISTICS</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP TABLE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP TEXT SEARCH CONFIGURATION</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP TEXT SEARCH DICTIONARY</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP TEXT SEARCH PARSER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP TEXT SEARCH TEMPLATE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP TRIGGER</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP TYPE</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP USER MAPPING</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">DROP VIEW</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">GRANT</code><sup>1</sup></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">IMPORT FOREIGN SCHEMA</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">REFRESH MATERIALIZED VIEW</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">REVOKE</code><sup>1</sup></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
          <tr>
            <td align="left"><code class="literal">SELECT INTO</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">X</code></td>
            <td align="center"><code class="literal">-</code></td>
            <td align="center"><code class="literal">-</code></td>
          </tr>
        </tbody>
      </table>

<sup>1</sup> Only for local objects

## <a id="functions"></a>Event Trigger Functions

Greenplum Database provides helper functions to retrieve information from event triggers.

### <a id="func_capchange"></a>Capturing Changes at Command End

`pg_event_trigger_ddl_commands()` returns a list of DDL commands run by each user action, when invoked in a function attached to a `ddl_command_end` event trigger. If called in any other context, Greenplum Database raises an error. `pg_event_trigger_ddl_commands()` returns one row for each base command run; some commands that are a single SQL statement may return more than one row.

The `pg_event_trigger_ddl_commands()` function returns the following columns:

| Name | Type | Description |
|------|------|-------------|
| classid | oid | The object identifier of the catalog in which the object belongs. |
| objid | oid | The object identifier of the object itself. |
| objsubid | integer | The sub-object identifier (for example, the attribute number for a column). |
| command_tag | text | The command tag. |
| object_type | text | The type of the object. |
| schema_name | text | The name of the schema in which the object belongs, if any; otherwise NULL. No quoting is applied. |
| object_identity | text | A text rendering of the object identity, schema-qualified. Each identifier included in the identity is quoted if necessary. |
| in_extension | bool | True if the command is part of an extension script. |
| command | pg_ddl_command | A complete representation of the command, in internal format. This cannot be output directly, but you can pass it to other functions to obtain different pieces of information about the command. |

### <a id="func_procdropped"></a>Processing Objects Dropped by a DDL Command

`pg_event_trigger_dropped_objects()` returns a list of all objects dropped by the command in whose `sql_drop` event it is called. If called in any other context, `pg_event_trigger_dropped_objects()` raises an error.

The `pg_event_trigger_dropped_objects()` function returns the following columns:

| Name | Type | Description |
|------|------|-------------|
| classid | oid | The object identifier of the catalog in which the object belongs. |
| objid | oid | The object identifier of the object itself. |
| objsubid | integer | The sub-object identifier (for example, the attribute number for a column). |
| original | bool | True if this was one of the root object(s) of the deletion. |
| normal | bool | True if there was a normal dependency relationship in the dependency graph leading to this object. |
| is_temporary | bool | True if this was a temporary object. |
| object_type | text | The type of the object. |
| schema_name | text | The name of the schema in which the object belonged, if any; otherwise NULL. No quoting is applied. |
| object_name | text | The name of the object, if the combination of schema and name can be used as a unique identifier for the object; otherwise NULL. No quoting is applied, and name is never schema-qualified. |
| object_identity | text | A text rendering of the object identity, schema-qualified. Each identifier included in the identity is quoted if necessary. |
| address_names | text[] | An array that, together with `object_type` and `address_args`, can be used by the `pg_get_object_address()` function to recreate the object address in a remote server containing an identically named object of the same kind. |
| address_args | text[] | Complement for `address_names`. |

### <a id="func_tabrewrite"></a>Handling a Table Rewrite Event

The following functions provide information about a table for which a `table_rewrite` event has just been invoked. If called in any other context, Greenplum Database raises an error.

| Name | Return Type | Description |
|------|------|-------------|
| pg_event_trigger_table_rewrite_oid() | Oid | The object identifier of the table about to be rewritten. |
| pg_event_trigger_table_rewrite_reason() | int | The reason code(s) explaining the reason for rewriting. The exact meaning of the codes is release dependent. |


## <a id="plpgsql"></a>PL/pgSQL Examples

You can use the PL/pgSQL procedural language to define an event trigger function. Greenplum Database requires that a function that is to be called as an event trigger be declared as a function with no arguments and a return type of `event_trigger`.

When a function is called as an event trigger, PL/pgSQL automatically creates several special variables in the top-level block. They are:

`TG_EVENT`
:   Data type `text`; a string representing the event the trigger is fired for.

`TG_TAG`
:   Data type `text`; variable that contains the command tag for which the trigger is fired.

### <a id="notice"></a>Example: Defining a ddl_command_start Event Trigger

The following example event trigger function raises a `NOTICE` message each time a supported SQL command is run:

```
CREATE OR REPLACE FUNCTION raise_notice() RETURNS event_trigger AS $$
BEGIN
    RAISE NOTICE 'raise_notice: % %', tg_event, tg_tag;
END;
$$ LANGUAGE plpgsql;
```

This statement creates the event trigger:

```
CREATE EVENT TRIGGER ddl_command_notice ON ddl_command_start EXECUTE FUNCTION raise_notice();
```

### <a id="notice"></a>Example: Defining an sql_drop Event Trigger

The following example event trigger uses the `pg_event_trigger_dropped_objects()` function to raise a `NOTICE` message each time an object is dropped:

```
CREATE FUNCTION test_event_trigger_for_drops()
        RETURNS event_trigger LANGUAGE plpgsql AS $$
DECLARE
    obj record;
BEGIN
    FOR obj IN SELECT * FROM pg_event_trigger_dropped_objects()
    LOOP
        RAISE NOTICE '% dropped object: % %.% %',
                     tg_tag,
                     obj.object_type,
                     obj.schema_name,
                     obj.object_name,
                     obj.object_identity;
    END LOOP;
END;
$$;
```

This statement creates the event trigger:

```
CREATE EVENT TRIGGER test_event_trigger_for_drops ON sql_drop
   EXECUTE FUNCTION test_event_trigger_for_drops();
```

In this next example, an event trigger is created to prevent dropping any table:

```
CREATE OR REPLACE FUNCTION prevent_drop_table()
    RETURNS event_trigger AS $$
BEGIN
    RAISE EXCEPTION 'Dropping tables is prohibited!';
END;
$$ LANGUAGE plpgsql;
```

The trigger creation statement:

```
CREATE EVENT TRIGGER no_drop_table ON sql_drop
   WHEN tag IN ('DROP TABLE')
   EXECUTE FUNCTION prevent_drop_table();
```

With this event trigger in place, Greenplum Database returns an error when a table is dropped:

```
testdb=# CREATE TABLE t1(a int);
CREATE TABLE
testdb=# DROP TABLE t1;
ERROR:  Dropping tables is prohibited!
CONTEXT:  PL/pgSQL function prevent_drop_table() line 4 at RAISE
```

### <a id="rewrite"></a>Examples: Defining a table_rewrite Event Trigger

The following example event trigger uses the `pg_event_trigger_table_rewrite_oid()` function to raise a `NOTICE` message on table rewrite:

```
CREATE FUNCTION test_event_trigger_table_rewrite_oid()
 RETURNS event_trigger
 LANGUAGE plpgsql AS
$$
BEGIN
  RAISE NOTICE 'rewriting table % for reason %',
                pg_event_trigger_table_rewrite_oid()::regclass,
                pg_event_trigger_table_rewrite_reason();
END;
$$;
```

This statement creates the event trigger:

```
CREATE EVENT TRIGGER test_table_rewrite_oid ON table_rewrite
   EXECUTE FUNCTION test_event_trigger_table_rewrite_oid();
```

In another example, you can implement a table rewriting policy that allows the rewrite only in maintenance windows. The following example event trigger uses the `pg_event_trigger_table_rewrite_oid()` function to implement such a policy:

```
CREATE OR REPLACE FUNCTION no_rewrite()
 RETURNS event_trigger
 LANGUAGE plpgsql AS
$$
---
--- Implement local Table Rewriting policy:
---   public.foo is not allowed rewriting, ever
---   other tables are only allowed rewriting between 1am and 6am
---   unless they have more than 100 blocks
---
DECLARE
  table_oid oid := pg_event_trigger_table_rewrite_oid();
  current_hour integer := extract('hour' from current_time);
  pages integer;
  max_pages integer := 100;
BEGIN
  IF pg_event_trigger_table_rewrite_oid() = 'public.foo'::regclass
  THEN RAISE EXCEPTION 'you''re not allowed to rewrite the table %',
                        table_oid::regclass;
  END IF;

  SELECT INTO pages relpages FROM pg_class WHERE oid = table_oid;
  IF pages > max_pages
  THEN
        RAISE EXCEPTION 'rewrites only allowed for table with less than % pages',
                        max_pages;
  END IF;

  IF current_hour NOT BETWEEN 1 AND 6
  THEN
        RAISE EXCEPTION 'rewrites only allowed between 1am and 6am';
  END IF;
END;
$$;
```

This statement creates the event trigger:

```
CREATE EVENT TRIGGER no_rewrite_allowed ON table_rewrite EXECUTE FUNCTION no_rewrite();
```

