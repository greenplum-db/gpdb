---
title: Installing and Upgrading Greenplum 
---

Information about installing, configuring, and upgrading Greenplum Database software and configuring Greenplum Database host machines.

-   **[Platform Requirements](platform-requirements-overview.html)**  
This topic describes the Greenplum Database 6 platform and operating system software requirements.
-   **[Estimating Storage Capacity](capacity_planning.html)**  
To estimate how much data your Greenplum Database system can accommodate, use these measurements as guidelines. Also keep in mind that you may want to have extra space for landing backup files and data load files on each segment host.
-   **[Configuring Your Systems](prep_os.html)**  
Describes how to prepare your operating system environment for Greenplum Database software installation.
-   **[Installing the Greenplum Database Software](install_gpdb.html)**  
Describes how to install the Greenplum Database software binaries on all of the hosts that will comprise your Greenplum Database system, how to enable passwordless SSH for the `gpadmin` user, and how to verify the installation.
-   **[Creating the Data Storage Areas](create_data_dirs.html)**  
Describes how to create the directory locations where Greenplum Database data is stored for each master, standby, and segment instance.
-   **[Validating Your Systems](validate.html)**  
Validate your hardware and network performance.
-   **[Initializing a Greenplum Database System](init_gpdb.html)**  
Describes how to initialize a Greenplum Database database system.
-   **[Installing Optional Extensions \(VMware Greenplum\)](data_sci_pkgs.html)**  
Information about installing optional VMware Greenplum Database extensions and packages, such as the Procedural Language extensions and the Python and R Data Science Packages.
-   **[Installing Additional Supplied Modules](install_modules.html)**  
The Greenplum Database distribution includes several PostgreSQL- and Greenplum-sourced `contrib` modules that you have the option to install.
-   **[Configuring Timezone and Localization Settings](localization.html)**  
Describes the available timezone and localization features of Greenplum Database.
-   **[Upgrading to Greenplum 6](upgrade_intro.html)**  
This topic identifies the upgrade paths for upgrading a Greenplum Database 6.x release to a newer 6.x release. The topic also describes the migration paths for migrating VMware Greenplum Database 4.x or 5.x data to Greenplum Database 6.x.
-   **[Migrating Open Source Greenplum Database to VMware Greenplum Database](migrate-oss-to-vmware.html)**
This topic describes how to migrate an Open Source Greenplum Database installation to commercial VMware Greenplum.
-   **[Migrating VMware Greenplum from Enterprise Linux 7 to 8 or 9](migrate-linux.html)**
This topic describes how to migrate a VMware Greenplum Database installation from Enterprise Linux (EL) version 7 to Enterprise Linux 8 or Enterprise Linux 9, while maintaining your existing version of Greenplum Database.
-   **[Enabling iptables \(Optional\)](enable_iptables.html)**  
On Linux systems, you can configure and enable the `iptables` firewall to work with Greenplum Database.
-   **[Installation Management Utilities](apx_mgmt_utils.html)**  
References for the command-line management utilities used to install and initialize a Greenplum Database system.
-   **[Greenplum Environment Variables](env_var_ref.html)**  
Reference of the environment variables to set for Greenplum Database.
-   **[Example Ansible Playbook](ansible-example.html)**  
A sample Ansible playbook to install a Greenplum Database software release onto the hosts that will comprise a Greenplum Database system.

