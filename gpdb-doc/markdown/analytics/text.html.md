---
title: Text Analytics and Search 
---

Greenplum Database offers two different methods for text search and analytics, **Greenplum Database full text search** and **Tanzu Greenplum Text**.

## <a id="section_yby_nv1_rqb"></a>Greenplum Database Full Text Search 

Greenplum Database text search is PostgreSQL text search ported to the Greenplum Database MPP platform. Greenplum Database text search is immediately available to you, with no need to install and maintain additional software. For full details on this topic, see [Greenplum Database text search](../admin_guide/textsearch/intro.html).

## <a id="section_ywf_4v1_rqb"></a>Tanzu Greenplum Text 

For advanced text analysis applications, VMWare also offers [Tanzu Greenplum Text](http://gptext.docs.pivotal.io/330/topics/intro.html), which integrates Greenplum Database with the Apache Solr text search platform. Tanzu Greenplum Text installs an Apache Solr cluster alongside your Greenplum Database cluster and provides Greenplum Database functions you can use to create Solr indexes, query them, and receive results in the database session.

Both of these systems provide powerful, enterprise-quality document indexing and searching services. Tanzu Greenplum Text, with Solr, has many capabilities that are not available with Greenplum Database text search. In particular, Tanzu Greenplum Text is better for advanced text analysis applications. For a comparative between these methods, see [Comparing Greenplum Database Text Search with Tanzu Greenplum Text](../admin_guide/textsearch/intro.html#gptext).

