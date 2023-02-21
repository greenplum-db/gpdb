---
title: Verifying the VMware Greenplum Software Download 
---

Describes how to verify Greenplum Database software that you download from VMware Tanzu Network.

VMware generates a SHA256 fingerprint for each Greenplum Database software download available from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb). This fingerprint enables you to verify that your downloaded file is unaltered from the original.

After you download a Greenplum Database server or component software package, you can verify the integrity of the software as follows:

1. On VMware Tanzu Network, navigate to the Greenplum Database version and package that you downloaded.

2. On the VMware Greenplum _Release Download Files_ page, click the i icon to the right of the Greenplum Database software package that you downloaded.

    This action displays a dialog that contains information about the package file, including the **File** name and the **SHA256** fingerprint.

3. Copy/paste the **SHA256** to a local file, or keep the Tanzu Network browser tab open.

4. On your local host, open a terminal window, navigate to the download directory, and locate the Greenplum package file that you downloaded from Tanzu Network.

5. Compare the downloaded file name with the **File** name specified in the Tanzu Network package information, and verify that they are the same.

6. Identify an OS utility that you can use to locally calculate a file checksum, such as `sha256sum`.

7. Run the utility to display the checksum of the package file that you downloaded from Tanzu Network. For example:

    ```
    $ sha256sum greenplum-db-7.0.0-rhel8-x86_64.rpm
    ```

8. If the command checksum output matches the **SHA256** fingerprint specified in the Tanzu Network package information, the file was downloaded intact. You can safely proceed to install the software.