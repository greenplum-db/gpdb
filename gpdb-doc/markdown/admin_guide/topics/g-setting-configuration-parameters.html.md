---
title: Setting Configuration Parameters 
---

Many configuration parameters limit who can change them and where or when they can be set. For example, to change certain parameters, you must be a Greenplum Database superuser. Other parameters can be set only at the system level in the postgresql.conf file or require a system restart to take effect.

Many configuration parameters are *session* parameters. You can set session parameters at the system level, the database level, the role level or the session level. Database users can change most session parameters within their session, but some require superuser permissions.

See the *Greenplum Database Reference Guide* for information about setting server configuration parameters.

-   **[Setting a Local Configuration Parameter](../topics/g-setting-a-local-configuration-parameter.html)**  

-   **[Setting a Master Configuration Parameter](../topics/g-setting-a-master-configuration-parameter.html)**  


**Parent topic:**[Configuring the Greenplum Database System](../topics/g-configuring-the-greenplum-system.html)

