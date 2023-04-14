# gpssh-exkeys 

Exchanges SSH public keys between hosts.

## <a id="section2"></a>Synopsis 

```
gpssh-exkeys -f <hostfile_exkeys> | -h <hostname> [-h <hostname> ...]

gpssh-exkeys -e <hostfile_exkeys> -x <hostfile_gpexpand>

gpssh-exkeys -? 

gpssh-exkeys --version
```

## <a id="section3"></a>Description 

The `gpssh-exkeys` utility exchanges SSH keys between the specified host names \(or host addresses\). This allows SSH connections between Greenplum hosts and network interfaces without a password prompt. The utility is used to initially prepare a Greenplum Database system for passwordless SSH access, and also to prepare additional hosts for passwordless SSH access when expanding a Greenplum Database system.

Keys are exchanged as the currently logged in user. You run the utility on the coordinator host as the `gpadmin` user \(the user designated to own your Greenplum Database installation\). Greenplum Database management utilities require that the `gpadmin` user be created on all hosts in the Greenplum Database system, and the utilities must be able to connect as that user to all hosts without a password prompt.

You can also use `gpssh-exkeys` to enable passwordless SSH for additional users, `root`, for example.

The `gpssh-exkeys` utility has the following prerequisites:

-   The user must have an account on the coordinator, standby, and every segment host in the Greenplum Database cluster.
-   The user must have an `id_rsa` SSH key pair installed on the coordinator host.
-   The user must be able to connect with SSH from the coordinator host to every other host machine without entering a password. \(This is called "1-*n* passwordless SSH."\)

You can enable 1-*n* passwordless SSH using the `ssh-copy-id` command to add the user's public key to each host's `authorized_keys` file. The `gpssh-exkeys` utility enables "*n*-*n* passwordless SSH," which allows the user to connect with SSH from any host to any other host in the cluster without a password.

To specify the hosts involved in an SSH key exchange, use the `-f` option to specify a file containing a list of host names \(recommended\), or use the `-h` option to name single host names on the command-line. At least one host name \(`-h`\) or a host file \(`-f`\) is required. Note that the local host is included in the key exchange by default.

To specify new expansion hosts to be added to an existing Greenplum Database system, use the `-e` and `-x` options. The `-e` option specifies a file containing a list of existing hosts in the system that have already exchanged SSH keys. The `-x` option specifies a file containing a list of new hosts that need to participate in the SSH key exchange.

The `gpssh-exkeys` utility performs key exchange using the following steps:

-   Adds the user's public key to the user's own `authorized_keys` file on the current host.
-   Updates the `known_hosts` file of the current user with the host key of each host specified using the `-h`, `-f`, `-e`, and `-x` options.
-   Connects to each host using `ssh` and obtains the user's `authorized_keys`, `known_hosts`, and `id_rsa.pub` files.
-   Adds keys from the `id_rsa.pub` files obtained from each host to the `authorized_keys` file of the current user.
-   Updates the `authorized_keys`, `known_hosts`, and `id_rsa.pub` files on all hosts with new host information \(if any\).

## <a id="section4"></a>Options 

-e hostfile\_exkeys
:   When doing a system expansion, this is the name and location of a file containing all configured host names and host addresses \(interface names\) for each host in your current Greenplum system \(coordinator, standby coordinator, and segments\), one name per line without blank lines or extra spaces. Hosts specified in this file cannot be specified in the host file used with `-x`.

-f hostfile\_exkeys
:   Specifies the name and location of a file containing all configured host names and host addresses \(interface names\) for each host in your Greenplum system \(coordinator, standby coordinator and segments\), one name per line without blank lines or extra spaces.

-h hostname
:   Specifies a single host name \(or host address\) that will participate in the SSH key exchange. You can use the `-h` option multiple times to specify multiple host names and host addresses.

--version
:   Displays the version of this utility.

-x hostfile\_gpexpand
:   When doing a system expansion, this is the name and location of a file containing all configured host names and host addresses \(interface names\) for each new segment host you are adding to your Greenplum system, one name per line without blank lines or extra spaces. Hosts specified in this file cannot be specified in the host file used with `-e`.

-? \(help\)
:   Displays the online help.

## <a id="section5"></a>Examples 

Exchange SSH keys between all host names and addresses listed in the file `hostfile_exkeys`:

```
$ gpssh-exkeys -f hostfile_exkeys
```

Exchange SSH keys between the hosts `sdw1`, `sdw2`, and `sdw3`:

```
$ gpssh-exkeys -h sdw1 -h sdw2 -h sdw3
```

Exchange SSH keys between existing hosts `sdw1`, `sdw2`, and `sdw3`, and new hosts `sdw4` and `sdw5` as part of a system expansion operation:

```
$ cat hostfile_exkeys
cdw
cdw-1
cdw-2
scdw
scdw-1
scdw-2
sdw1
sdw1-1
sdw1-2
sdw2
sdw2-1
sdw2-2
sdw3
sdw3-1
sdw3-2
$ cat hostfile_gpexpand
sdw4
sdw4-1
sdw4-2
sdw5
sdw5-1
sdw5-2
$ gpssh-exkeys -e hostfile_exkeys -x hostfile_gpexpand
```

## <a id="section6"></a>See Also 

[gpssh](gpssh.html), [gpsync](gpsync.html)

