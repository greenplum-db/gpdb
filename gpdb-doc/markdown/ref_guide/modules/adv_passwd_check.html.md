# advanced\_password\_check 

The `advanced_password_check` module provides password quality checking for Greenplum Database.

The Greenplum Database `advanced_password_check` module is based on the `passwordcheck_extra` module, which enhances the PostgreSQL `passwordcheck` module to support user-defined policies to strengthen `passwordcheck`'s minimum password requirements.

## <a id="topic_reg"></a>Loading the Module 

The `advanced_password_check` module provides no SQL-accessible functions. To use it, simply load it into the server. You can load it into an individual session by entering this command as a superuser:

```
# LOAD 'advanced_password_check';
```

More typical usage is to preload it into all sessions by including `advanced_password_check` in `shared_preload_libraries` in `postgresql.conf`:

```
shared_preload_libraries = '*other\_libraries*,advanced_password_check'

```

and then restarting the Greenplum Database server.

## <a id="topic_using"></a>Using the advanced\_password\_check Module 

`advanced_password_check` is a Greenplum Database module that you can enable and configure to check password strings against one or more user-defined policies. You can configure policies that:

-   Set a minimum password string length.
-   Set a maximum password string length.
-   Define a custom list of special characters.
-   Define rules for special character, upper/lower case character, and number inclusion in the password string.

The `advanced_password_check` module defines server configuration parameters that you set to configure password setting policies. These parameters include:

|Parameter Name|Type|Default Value|Description|
|--------------|----|-------------|-----------|
|minimum\_length|int|8|The minimum allowable length of a Greenplum Database password.|
|maxmum\_length|int|15|The maximum allowable length of Greenplum Database password.|
|special\_chars|string|!@\#$%^&\*\(\)\_+\{\}\|<\>?=|The set of characters that Greenplum Database considers to be special characters in a password.|
|restrict\_upper|bool|true|Specifies whether or not the password string must contain at least one upper case character.|
|restrict\_lower|bool|true|Specifies whether or not the password string must contain at least one lower case character.|
|restrict\_numbers|bool|true|Specifies whether or not the password string must contain at least one number.|
|restrict\_special|bool|true|Specifies whether or not the password string must contain at least one special character.|

After you define your password policies, you run the `gpconfig` command for each configuration parameter that you must set. When you run the command, you must qualify the parameter with the module name. For example, to configure Greenplum Database to remove any requirements for a lower case letter in the password string, you run the following command:

```
gpadmin@gpcoordinator$ gpconfig -c advanced_password_check.restrict_lower -v false
```

After you set or change module configuration in this manner, you must reload the Greenplum Database configuration:

```
gpadmin@gpcoordinator$ gpstop -u
```

## <a id="topic_example"></a>Example 

Suppose that you have defined the following password policies:

-   The password must contain a minimum of 10 characters and a maximum of 18.
-   The password must contain a mixture of upper case and lower case characters.
-   The password must contain at least one of the default special characters.
-   The are no requirements that the password contain a number.

You would run the following commands to configure Greenplum Database to enforce these policies:

```

gpadmin@gpcoordinator$ gpconfig -c advanced_password_check.minimum_length -v 10
gpadmin@gpcoordinator$ gpconfig -c advanced_password_check.maximum_length -v 18
gpadmin@gpcoordinator$ gpconfig -c advanced_password_check.restrict_numbers -v false
gpadmin@gpcoordinator$ gpstop -u
```

After loading the new configuration, passwords that the Greenplum superuser sets must now follow the policies, and Greenplum returns an error for every policy that is not met. Note that Greenplum checks the password string against all of the policies, and concatenates together the messages for any errors that it encounters. For example \(line breaks added for better viewability\):

```
# testdb=# CREATE role r1 PASSWORD '12345678901112';
ERROR:  Incorrect password format: lower-case character missing, upper-case character
missing, special character missing (needs to be one listed in "<list-of-special-chars>")
```

## <a id="topic_info"></a>Additional Module Documentation 

Refer to the [passwordcheck](https://www.postgresql.org/docs/12/passwordcheck.html) PostgreSQL documentation for more information about this module.

