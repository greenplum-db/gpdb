# SET SESSION AUTHORIZATION 

Sets the session role identifier and the current role identifier of the current session.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
SET [SESSION | LOCAL] SESSION AUTHORIZATION <rolename>

SET [SESSION | LOCAL] SESSION AUTHORIZATION DEFAULT

RESET SESSION AUTHORIZATION
```

## <a id="section3"></a>Description 

This command sets the session role identifier and the current role identifier of the current SQL-session context to be rolename. The role name may be written as either an identifier or a string literal. Using this command, it is possible, for example, to temporarily become an unprivileged user and later switch back to being a superuser.

The session role identifier is initially set to be the \(possibly authenticated\) role name provided by the client. The current role identifier is normally equal to the session user identifier, but may change temporarily in the context of `setuid` functions and similar mechanisms; it can also be changed by [SET ROLE](SET_ROLE.html). The current user identifier is relevant for permission checking.

The session user identifier may be changed only if the initial session user \(the authenticated user\) had the superuser privilege. Otherwise, the command is accepted only if it specifies the authenticated user name.

The `DEFAULT` and `RESET` forms reset the session and current user identifiers to be the originally authenticated user name. These forms may be run by any user.

## <a id="section4"></a>Parameters 

SESSION
:   Specifies that the command takes effect for the current session. This is the default.

LOCAL
:   Specifies that the command takes effect for only the current transaction. After `COMMIT` or `ROLLBACK`, the session-level setting takes effect again. Note that `SET LOCAL` will appear to have no effect if it is run outside of a transaction.

rolename
:   The name of the role to assume.

NONE
RESET
:   Reset the session and current role identifiers to be that of the role used to log in.

## <a id="section5"></a>Examples 

```
SELECT SESSION_USER, CURRENT_USER;
 session_user | current_user 
--------------+--------------
 peter        | peter

SET SESSION AUTHORIZATION 'paul';

SELECT SESSION_USER, CURRENT_USER;
 session_user | current_user 
--------------+--------------
 paul         | paul
```

## <a id="section6"></a>Compatibility 

The SQL standard allows some other expressions to appear in place of the literal rolename, but these options are not important in practice. Greenplum Database allows identifier syntax \(rolename\), which SQL does not. SQL does not allow this command during a transaction; Greenplum Database does not make this restriction. The `SESSION` and `LOCAL` modifiers are a Greenplum Database extension, as is the `RESET` syntax.

## <a id="section7"></a>See Also 

[SET ROLE](SET_ROLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

