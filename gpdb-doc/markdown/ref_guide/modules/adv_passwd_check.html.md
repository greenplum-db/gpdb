---
title: advanced_password_check 
---

The `advanced_password_check` module allows you to strengthen password policies for Greenplum Database. It is based on the `passwordcheck_extra` module, which enhances the PostgreSQL `passwordcheck` module to support user-defined policies to strengthen `passwordcheck`'s minimum password requirements.

## <a id="topic_reg"></a>Loading the Module 

The `advanced_password_check` module provides no SQL-accessible functions. To use it, enable the extension as a preloaded library and restart Greenplum Database:

First, check if there are any preloaded shared libraries by running the following command:

```
gpconfig -s shared_preload_libraries
```

Use the output of the above command to enable the `advanced_password_check` module, along any other shared libraries, and restart Greenplum Database:

```
gpconfig -c shared_preload_libraries -v '<other_libraries>,advanced_password_check'
gpstop -ar 
```

Finally, install the extension in your database:

```
CREATE EXTENSION advanced_password_check;
```

## <a id="topic_using"></a>Using the advanced\_password\_check Module 

`advanced_password_check` is a Greenplum Database module that you can enable and configure to check password strings against one or more user-defined policies. You can configure policies that:

-   Set a minimum password string length.
-   Set a maximum password string length.
-   Set a maximum password age before the password expires.
-   Set rules for reusing an old password: the number of days or the number of previous passwords before a user can reuse a password.
-   Set the maximum number of failed login attempts before a user is locked, and the number of minutes the user remains locked.
-   Define a custom list of special characters.
-   Define rules for special character, upper/lower case character, and number inclusion in the password string.
-   Use UDFs to manage an exception list for each feature so certain users can bypass the current restrictions.

The `advanced_password_check` module defines server configuration parameters that you set to configure password setting policies. These parameters include:

|Parameter Name|Type|Default Value|Description|
|--------------|----|-------------|-----------|
|minimum\_length|int|8|The minimum allowable length of a Greenplum Database password.|
|maximum\_length|int|15|The maximum allowable length of a Greenplum Database password.|
|special\_chars|string|!@\#$%^&\*\(\)\_+\{\}\|<\>?=|The set of characters that Greenplum Database considers to be special characters in a password.|
|restrict\_upper|bool|true|Specifies whether or not the password string must contain at least one upper case character.|
|restrict\_lower|bool|true|Specifies whether or not the password string must contain at least one lower case character.|
|restrict\_numbers|bool|true|Specifies whether or not the password string must contain at least one number.|
|restrict\_special|bool|true|Specifies whether or not the password string must contain at least one special character.|
|password_login_attempts|int|0|The number of consecutive failed login attempts before a user is locked. If set to 0, this feature is disabled.|
|password_max_age|int|0|The maximum number of days before the password expires. If set to 0, the password does not expire.|
|password_reuse_days|int|0|The number of days before a user can reuse a password. If set to 0, the user can reuse any password.|
|password_reuse_history|int|0|The number of previous passwords a user cannot reuse. If set to 0, the user can reuse any password.|
|lockout_duration|int|0|The number of minutes a user is locked after reaching password_login_attempts. If set to 0, the user is locked indefinitely.|


After you define your password policies, you run the `gpconfig` command for each configuration parameter that you must set. When you run the command, you must qualify the parameter with the module name. For example, to configure Greenplum Database to remove any requirements for a lower case letter in the password string, you run the following command:

```
gpconfig -c advanced_password_check.restrict_lower -v false
```

After you set or change module configuration in this manner, you must reload the Greenplum Database configuration:

```
gpstop -u
```

The `advanced_password_check` module provides the following user-defined functions (UDF). Specify all function arguments as single-quoted strings.

|Function Signature|Arguments|Description|
|--------|----------|-----------|
|`manage_exception_list(action, role_name, exception_type)`|- `action` can be `add`, `remove`, or `show`. <br>- `role_name`<br>- `exception_type` can be `password_max_age`, `password_reuse_days`, `password_reuse_history`, or `password_login_attempts`. |Adds, removes, and shows roles in the exception list for a policy.|
|`unblock_account(role_name)`|`role_name`|Unblocks a role|
|`status()`||Lists active password policies.|

Use the `manage_exception_list()` function to manage the exception list for a password policy. Greenplum does not enforce the rules set for the policy for any users specified in the exception list.
The function takes three arguments: the action to take, the role name and the name of the exception. The value of `action` can be `add`, `remove`, or `show`. The value of `exception_type` can be `password_max_age`, `password_reuse_days`, `password_reuse_history`, `password_login_attempts`, or an empty string to represent all of the available exception types. When the action is `show`, you may specify `role_name` as an empty string to include all roles.

For example, to add the role `user1` to the exception list for `password_max_age`, so the role does not have a password expiration date: 

```
SELECT advanced_password_check.manage_exception_list('add', 'user1', 'password_max_age');

role_id | role_name | exception_type 
---------+-----------+---------------- 
(0 rows) 
```

The following example checks all exception lists for all roles:

```
SELECT * FROM advanced_password_check.manage_exception_list('show','', ''); 

role_id | role_name |     exception_type 
---------+-----------+------------------------- 
1826266 | user1     | password_max_age 
1826267 | user2     | password_reuse_history 
1826266 | user1     | password_reuse_days 
1826266 | user1     | password_login_attempts 
(4 rows) 
```

Use the `unblock_account_role()` function to manually unblock a user that reached the limit set by the configuration parameter `password_login_attempts`. For example:

```
SELECT * FROM advanced_password_check.unblock_account('user1'); 

unblock_account 
----------------- 
(1 row) 
```

Use the `status()` function to view the values of all active password policies. For example:

```
SELECT * FROM advanced_password_check.status(); 

           guc           |        value 
-------------------------+--------------------- 
special_chars           | !@#$%^&*()_+{}|<>?= 
restrict_lower          | t 
restrict_upper          | t 
restrict_numbers        | t 
restrict_special        | t 
minimum_length          | 8 
maximum_length          | 15 
password_max_age        | 192 
password_reuse_days     | 0 
password_reuse_history  | 0 
password_login_attempts | 0 
lockout_duration        | 0 
(12 rows) 
```

> **Note** The password history information is stored in the Greenplum coordinator host only. In the event of a primary coordinator failure, if you activate the standby coordinator host, the `advanced_password_check` module  will not be able to access the password history information, and it will affect any server configuration parameters or UDFs that involve password history information. For example, a role may reuse a password even if the `password_reuse_history` is set, as there is no information about previous passwords. If you switch back to the original coordinator host, unless there is data corruption, the password history information will be accessible again.

## <a id="topic_example"></a>Example 

Suppose that you have defined the following password policies:

-   The password must contain a minimum of 10 characters and a maximum of 18.
-   The password must contain a mixture of upper case and lower case characters.
-   The password must contain at least one of the default special characters.
-   The are no requirements that the password contain a number.
-   The password must not be the same as the current password.
-   The password must expire after three months.
-   The user must be blocked during an hour after the third of three unsuccessful login attempts. 

You would run the following commands to configure Greenplum Database to enforce these policies:

```
gpconfig -c advanced_password_check.minimum_length -v 10
gpconfig -c advanced_password_check.maximum_length -v 18
gpconfig -c advanced_password_check.restrict_numbers -v false
gpconfig -c advanced_password_check.password_reuse_history -v 1
gpconfig -c advanced_password_check.password_max_age -v 90
gpconfig -c advanced_password_check.password_login_attempts -v 3
gpconfig -c advanced_password_check.lockout_duration -v 60
gpstop -u
```

After loading the new configuration, passwords that the Greenplum superuser sets must now follow the policies, and Greenplum returns an error for every policy that is not met. Note that Greenplum checks the password string against all of the policies, and concatenates together the messages for any errors that it encounters. For example \(line breaks added for better viewability\):

```
# testdb=# CREATE role r1 PASSWORD '12345678901112';
ERROR:  Incorrect password format: lower-case character missing, upper-case character
missing, special character missing (needs to be one listed in "<list-of-special-chars>")
```

## <a id="topic_upgrade"></a>Upgrading the Module

You may upgrade the module from a previous version by following the steps below.

1. Check your existing version of the module by running `\dx` from your database.

1. Verify that `advanced_password_check` is listed as one of the preloaded shared libraries by running the following command:

    ``` 
    gpconfig -s shared_preload_libraries
    ``` 

1. Update the extension from your database prompt. The following example upgrades to version 1.2:

    ```
    ALTER EXTENSION advanced_password_check UPDATE TO ‘1.2’;
    ```

## <a id="topic_info"></a>Additional Module Documentation 

Refer to the [passwordcheck](https://www.postgresql.org/docs/9.4/passwordcheck.html) PostgreSQL documentation for more information about this module.

