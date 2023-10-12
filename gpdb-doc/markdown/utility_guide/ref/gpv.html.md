# gpv 

Configures, deploys, and manages Greenplum Database clusters on top of a virtual platform such as VMware vSphere.

## <a id="section2"></a>Synopsis 

```
gpv base <base_options>
gpv config <config_options>
gpv greenplum <greenplum_options>
gpv version
gpv -h | --help
```

## <a id="section3"></a>Description 

The `gpv` utility automates the deployment of a Greenplum Database cluster on top of virtual platforms such as vSphere. You use `gpv` to specify a Greenplum cluster's configuration; automate the deployment of a base virtual machine, clone the base virtual machine to create a cluster, and initialize a Greenplum Database on the set of virtual machines. See [VMware Greenplum on vSphere](/gpvirtual/vsphere/index.html) for more details.

## <a id="section4"></a>Options

[gpv base](gpv-base.html)
:   The base virtual machine commands.

[gpv config](gpv-config.html)
:   The commands to manage the configuration of the Greenplum deployment.

[gpv greenplum](gpv-greenplum.html)
:   The Greenplum initialization and validation commands.

version
:   Print the version information for the `gpv` utility.

-h, --help
:   Display the online help. 
