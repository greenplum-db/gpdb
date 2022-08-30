---
title: Overview of Greenplum Public Cloud Platform 
---

Greenplum Public Cloud Platform was developed and maintained by the field engineering team, but in February of 2021, the field engineer who was responsible for this offering left VMware.  The Greenplum R&D team took over the Greenplum cloud offering, and it was rebranded internally as Greenplum Cloud Platform. 

This is the only cloud offering from Greenplum at the moment. Please refer to this [blog](https://greenplum.org/deploying-in-the-cloud/) to learn more about VMware Greenplum Public Cloud Platform.

## <a id="gpcp_deployment"></a>Deploying Greenplum on the Cloud

Customers using this offering on the Amazon Web Services, Microsoft Azure, or Google cloud platforms are accruing considerable revenue. Customers can deploy Greenplum on the cloud in two ways.

-   Standard marketplace offering: 
        Deploy a Greenplum cluster via the cloud marketplace that is developed and maintained by the R&D team.

-   Custom deployment: 
        Deploy the Greenplum cluster with assistance from the field engineering team

VMware Greenplum on Cloud supports three types of listing as a part of the standard marketplace offering.

-   Bring Your Own License (BYOL)
        -   AWS BYOL listing
        -   Azure BYOL listing
        -   GCP BYOL listing

-   Pay-as-you-go (PAYG / Hourly / metered)
        -   AWS Hourly listing
        -   Azure Hourly listing
        -   GCP Hourly listing

-   Subscription (Only available on AWS)

Existing Documentation: 

On the landing page of both BYOL and PAYG listings on different clouds, there is some documentation provided. This is applicable also to custom deployments / non-marketplace deployments.

PDF Format:

AWS
[Overview](https://vmware-tanzu-greenplum-docs.s3.amazonaws.com/VMware+Tanzu+Greenplum+on+AWS+Marketplace+Overview+v6.7.pdf)
[Release Notes](https://vmware-tanzu-greenplum-docs.s3.amazonaws.com/VMware+Tanzu+Greenplum+on+AWS+Marketplace+Release+Notes+v6.7.pdf)
[Troubleshooting Guide](https://vmware-tanzu-greenplum-docs.s3.amazonaws.com/VMware+Tanzu+Greenplum+on+AWS+Marketplace+Troubleshooting+Guide+v6.7.pdf)
Azure
[Overview](https://greenplum.blob.core.windows.net/vmware-tanzu-greenplum-docs/VMware%20Tanzu%20Greenplum%20on%20Azure%20Marketplace%20Overview%20v6.6.pdf)
[Release Notes](https://greenplum.blob.core.windows.net/vmware-tanzu-greenplum-docs/)VMware%20Tanzu%20Greenplum%20on%20Azure%20Marketplace%20Release%20Notes%20v6.6.pdf
GCP
[Overview](https://storage.cloud.google.com/vmware-tanzu-docs/VMware%20Tanzu%20Greenplum%20on%20GCP%20Marketplace%20Overview%20v6.5.1.pdf?_ga=2.103586560.-1634467729.1653031118&_gac=1.220981994.1656485649.CjwKCAjwzeqVBhAoEiwAOrEmzbZvtpG98ZAd1OLxRSYxxLXAhUAF8B1gv8PIYDQtGEza88xaC_N8gRoCl3sQAvD_BwE)
[Release Notes](https://storage.cloud.google.com/vmware-tanzu-docs/VMware%20Tanzu%20Greenplum%20on%20GCP%20Marketplace%20Release%20Notes%20v6.5.1.pdf?_ga=2.103586560.-1634467729.1653031118&_gac=1.220981994.1656485649.CjwKCAjwzeqVBhAoEiwAOrEmzbZvtpG98ZAd1OLxRSYxxLXAhUAF8B1gv8PIYDQtGEza88xaC_N8gRoCl3sQAvD_BwE)

Original Source Format (MS word document):

AWS
[Overview](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=wda432acb0b3b4cc5805d81b1e43796c4&csf=1&web=1&e=LcTEsD)
[Release Notes](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w4a3e34edc59f47aca4b71d05c5dd962c&csf=1&web=1&e=oOWZxR)
[Troubleshooting Guide](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w6f3e8ccabea947d6a9694cb775e4dd28&csf=1&web=1&e=mNkOwt)
Azure
[Overview](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=wdfef2bed99224816ad4f1fee5dfe29e2&csf=1&web=1&e=KVOduW)
[Release Notes](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w5587740ab17f4a21a1da1ccc250bc135&csf=1&web=1&e=UGHVrn)
GCP
[Overview](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w553e0504f6064b4fba8f2c4e128afaf4&csf=1&web=1&e=tcVaiT)
[Release Notes](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w363b8511d4b74106b37bc5faf238e886&csf=1&web=1&e=TGFPZf)

Greenplum on Cloud Versioning:

Currently, Greenplum on Cloud follows a different version on each cloud (AWS, Azure and GCP) from the actual Greenplum Datawarehouse version.

Current versions:

Greenplum on AWS is 6.7.0
Greenplum on Azure is 6.6.0
Greenplum on GCP is 6.5.0

Releases:

There are two types of release that we currently follow in Greenplum on Cloud.

Soft Release

This includes only the latest Greenplum Server version + the latest compatible Greenplum optional components +  cloud utility upgrades/patches without undergoing an approval process by the public cloud provider. This is made available to the customers by using the gprelease command and will not be available as a separate image in the cloud marketplace. Externally, we do not increment the version of Greenplum on Cloud in the release notes if there is a soft release. We have a tag associated with soft releases, for internal version tracking purposes.

Full Release

This includes the latest Greenplum Server version+ the latest compatible Greenplum optional components + cloud utility upgrades/patches. This release undergoes a rigorous approval process by the public cloud provider before being made available to the user on the marketplace. A full release is targeted only if there is a change required on the deployment template (or) on the virtual image. Externally, we increment the version of Greenplum on Cloud in the release notes if there is only a full release.
Greenplum on Cloud does not currently have any release cadence. But, it is safe to assume that we do a full release every quarter at the moment.

Current Process:

Currently, the documentation for each cloud is maintained by the development team. As mentioned above, whenever we make a soft release or a full release, we edit the previous word document, convert it into a PDF and then upload it into the respective cloud's object storage - S3, Blob Storage and Google Cloud Storage.

Why are we doing this?

We want a consistent user experience for documentation across all VMware products.

However, now that we are a part of VMware, it will be better to bring all the documentation related to Greenplum under one umbrella i.e. under VMware docs and then link the VMware docs pages to the landing page of each listing on different clouds so that all customers use VMware docs as a single point for all VMware products.

Alignment:

VMware Docs is the recommended documentation site for all VMware products and almost all Greenplum components have their documentation in VMware Docs

Estimations

Estimations of:

-   Time	
-   Customer Impact	
-   Complexity	
-   Resources	
-   Release Management	
-   Testing	
-   Documentation	