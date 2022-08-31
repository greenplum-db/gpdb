---
title: Overview of Greenplum Cloud Platform 
---

VMware Greenplum Public Cloud Platform was developed and maintained by the field engineering team. But in February of 2021, the field engineer who was responsible for this offering left VMware.  

The Greenplum R&D team took over the Greenplum cloud offering, and it was rebranded internally as Greenplum Cloud Platform. It is currently the only cloud offering from Greenplum. 

To learn more about Greenplum Cloud Platform, see the [blog](https://greenplum.org/deploying-in-the-cloud/).

## <a id="gpcp_deployment"></a>Deploying Greenplum Cloud Platform

Customers using this offering on the Amazon Web Services, Microsoft Azure, or Google cloud platforms are accruing considerable revenue. They deploy VMware Greenplum Cloud Platform in one of two ways.
   
### <a id="standard_deployment"></a>Standard Marketplace Deployment
Customers can deploy a Greenplum cluster by using the cloud marketplace, which was developed by and is maintained by the R&D team.

### <a id="custom_deployment"></a>Custom Deployment
Customers can deploy the Greenplum cluster with assistance from the field engineering team.

## <a id="gpcp_offerings"></a>Greenplum Cloud Platform Offerings

Greenplum Cloud Platform supports the three public cloud platforms through licensing, hourly usage, or subscription.

### <a id="BYOL"></a>Bring Your Own License (BYOL)

-  [AWS BYOL listing](https://aws.amazon.com/marketplace/pp/prodview-piiukzn26stas)

-  [Azure BYOL listing](https://azuremarketplace.microsoft.com/en-us/marketplace/apps/pivotal.pivotal-greenplum-azure-byol?tab=Overview)

-  [GCP BYOL listing](https://console.cloud.google.com/marketplace/product/pivotal-public/pivotal-greenplum-byol?project=pivotal-public)

### <a id="Hourly usage"></a>Pay-as-you-go (PAYG / Hourly / metered)

-   [AWS Hourly listing](https://aws.amazon.com/marketplace/pp/prodview-sbg6yvoyllr46?sr=0-1&ref_=beagle&applicationId=AWSMPContessa)
-   [Azure Hourly listing](https://azuremarketplace.microsoft.com/en-us/marketplace/apps/pivotal.pivotal-greenplum-azure-hourly?tab=Overview)
-   [GCP Hourly listing](https://console.cloud.google.com/marketplace/product/pivotal-public/pivotal-greenplum-metered?project=pivotal-public)

### <a id="Subscription"></a>Subscription

-   [Subscription](https://aws.amazon.com/marketplace/pp/prodview-k4snnsc2cznxk?sr=0-3&ref_=beagle&applicationId=AWSMPContessa) (Available only on AWS)

## <a id="gpcp_documentation"></a>Greenplum Cloud Platform Documentation 

The development team created the documentation for each public cloud platform. Before each full or soft release, they edited the original Microsoft Word documents, saved PDFs, and uploaded them into each public cloud platform's object storage (S3 for AWS, Blob Storage for Azure, and Google Cloud Storage for GCP). 

They planned a consistent user experience for documentation by locating it under VMware docs and linking to the landing pages of each listing on the public clouds, which contain information on the  the BYOL and PAYG listings, and the custom and non-marketplace deployments. 

### <a id="PDF_docs"></a> Greenplum Cloud Platform PDF Documentation

The original source documents are is in Microsoft Word. (The documntation team needs a link)

#### <a id="AWS PDFs"></a>Amazon Web Services PDFs

[Overview](https://vmware-tanzu-greenplum-docs.s3.amazonaws.com/VMware+Tanzu+Greenplum+on+AWS+Marketplace+Overview+v6.7.pdf)

[Release Notes](https://vmware-tanzu-greenplum-docs.s3.amazonaws.com/VMware+Tanzu+Greenplum+on+AWS+Marketplace+Release+Notes+v6.7.pdf)

[Troubleshooting Guide](https://vmware-tanzu-greenplum-docs.s3.amazonaws.com/VMware+Tanzu+Greenplum+on+AWS+Marketplace+Troubleshooting+Guide+v6.7.pdf)

#### <a id="Azure PDFs"></a>Microsoft Azure PDFs

[Overview](https://greenplum.blob.core.windows.net/vmware-tanzu-greenplum-docs/VMware%20Tanzu%20Greenplum%20on%20Azure%20Marketplace%20Overview%20v6.6.pdf)

[Release Notes](https://greenplum.blob.core.windows.net/vmware-tanzu-greenplum-docs/VMware%20Tanzu%20Greenplum%20on%20Azure%20Marketplace%20Release%20Notes%20v6.6.pdf)

#### <a id="GCP PDFs"></a>Google Cloud Platform PDFs

[Overview](https://storage.cloud.google.com/vmware-tanzu-docs/VMware%20Tanzu%20Greenplum%20on%20GCP%20Marketplace%20Overview%20v6.5.1.pdf?_ga=2.103586560.-1634467729.1653031118&_gac=1.220981994.1656485649.CjwKCAjwzeqVBhAoEiwAOrEmzbZvtpG98ZAd1OLxRSYxxLXAhUAF8B1gv8PIYDQtGEza88xaC_N8gRoCl3sQAvD_BwE)

[Release Notes](https://storage.cloud.google.com/vmware-tanzu-docs/VMware%20Tanzu%20Greenplum%20on%20GCP%20Marketplace%20Release%20Notes%20v6.5.1.pdf?_ga=2.103586560.-1634467729.1653031118&_gac=1.220981994.1656485649.CjwKCAjwzeqVBhAoEiwAOrEmzbZvtpG98ZAd1OLxRSYxxLXAhUAF8B1gv8PIYDQtGEza88xaC_N8gRoCl3sQAvD_BwE)

### <a id="Word_docs"></a> Greenplum Cloud Platform Microsoft Word Source Documentation

#### <a id="AWS Word Docs"></a>Amazon Web Services Word Documents

[Overview](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=wda432acb0b3b4cc5805d81b1e43796c4&csf=1&web=1&e=LcTEsD)

[Release Notes](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w4a3e34edc59f47aca4b71d05c5dd962c&csf=1&web=1&e=oOWZxR)

[Troubleshooting Guide](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w6f3e8ccabea947d6a9694cb775e4dd28&csf=1&web=1&e=mNkOwt)

#### <a id="Azure Word Docs"></a>Azure Word Documents

Azure

[Overview](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=wdfef2bed99224816ad4f1fee5dfe29e2&csf=1&web=1&e=KVOduW)

[Release Notes](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w5587740ab17f4a21a1da1ccc250bc135&csf=1&web=1&e=UGHVrn)

#### <a id="GCP Word Docs"></a>Google Cloud Platform Word Documents

[Overview](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w553e0504f6064b4fba8f2c4e128afaf4&csf=1&web=1&e=tcVaiT)

[Release Notes](https://onevmw.sharepoint.com/:w:/r/teams/tanzudataservices/Shared%20Documents/Tanzu%20[…]x?d=w363b8511d4b74106b37bc5faf238e886&csf=1&web=1&e=TGFPZf)

## <a id="gpcp_versioning"></a>Greenplum Cloud Platform Versioning

Each of the three public cloud platforms has a version that is different from the current Greenplum data warehouse version.

|Public Cloud Platform | Version |
|------|---------------|
|Greenplum Cloud Platform on AWS|6.7.0|
|Greenplum Cloud Platform on Azure|6.6.0|
|Greenplum Cloud Platform on GCP|6.5.0|

## <a id="gpcp_releases"></a>Greenplum Cloud Platform Releases

There are two types of release for Greenplum Cloud Platform.

### Soft Release

The soft release includes the latest Greenplum Server version, the latest compatible Greenplum optional components, and the cloud utility upgrades / patches without undergoing an approval process by the public cloud provider. 

The soft release is made available to customers by using the `gprelease` command, and will not be available as a separate image in the cloud marketplace. 

For external soft releases, the version of Greenplum Cloud Platform is not incremented in the release notes. For internal soft releases, there is an associated tag for version tracking purposes.

### Full Release

The full release includes the latest Greenplum Server version, the latest compatible Greenplum optional components, and cloud utility upgrades/patches. 

The full release undergoes a rigorous approval process by the public cloud provider before being made available to customers on the marketplace. It is targeted only if there is a change required on the deployment template (or) on the virtual image. 

For external full releases, the version of Greenplum Cloud Platform is incremented in the release notes. 

NOTE: Greenplum on Cloud does not currently have any release cadence, but a full release is planned for every quarter.

### Estimations

-   Time	
-   Customer Impact	
-   Complexity	
-   Resources	
-   Release Management	
-   Testing	
-   Documentation	