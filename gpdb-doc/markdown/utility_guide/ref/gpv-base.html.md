# gpv base

Manage the base virtual machine for a deployment of Greenplum Database on vSphere. 

## <a id="section2"></a>Synopsis

```
gpv base deploy
gpv base import [ -f <file_name> | --ova=<file_name> ] [ -t <template_name> | --template=<template_name> ]
```

## <a id="section3"></a>Description

The `gpv base` command allows you to import and deploy the base virtual machine for your Greenplum Database cluster on VMware vSphere.

## <a id="opts"></a>Sub-commands

The available sub-commands for `gpv base` are `deploy` and `import`.

### <a id="deploy"></a>deploy

Configure the imported base virtual machine for Greenplum deployment. The `gpv base deploy` command performs the following operations in the base virtual machine:

- Copies and installs Greenplum and Greenplum Virtual Service packages.
- Configures the OS using the Greenplum Virtual Service.
- Changes secrets for the `root` and `gpadmin` roles.
- Validates that the base virtual machine is configured correctly.

### <a id="import"></a>import

Import an existing virtual machine template or an OVA file as a new base virtual machine. The `gpv base import` command creates a new base virtual machine from your own virtual machine template (certified to follow all the existing policies), or from an OVA file, such as the Greenplum Virtual Machine base template available on [VMware Tanzu Network](https://network.tanzu.vmware.com/products/vmware-greenplum).

Options:

-f, --ova=file_name
:   Specify a local OVA file name.

-t, --template=template_name
:   Specify an existing virtual machine template `template_name`.


#### <a id="examples"></a>Examples

Import an existing virtual machine template:

```
gpv base import -t my-company-template
```

Import a local OVA:

```
gpv base import -f /path/to/pre-built.ova
```


