---
title: Installing Additional Supplied Modules 
---

The Greenplum Database distribution includes several PostgreSQL- and Greenplum-sourced `contrib` modules that you have the option to install.

Each module is typically packaged as a Greenplum Database extension. You must register these modules in each database in which you want to use it. For example, to register the `dblink` module in the database named `testdb`, use the command:

```
$ psql -d testdb -c 'CREATE EXTENSION dblink;'
```

To remove a module from a database, drop the associated extension. For example, to remove the `dblink` module from the `testdb` database:

```
$ psql -d testdb -c 'DROP EXTENSION dblink;'
```

**Note:** When you drop a module extension from a database, any user-defined function that you created in the database that references functions defined in the module will no longer work. If you created any database objects that use data types defined in the module, Greenplum Database will notify you of these dependencies when you attempt to drop the module extension.

You can register the following modules in this manner:

<table cellpadding="4" cellspacing="0" summary="" border="1" class="simpletable"><col style="width:33.33333333333333%" /><col style="width:33.33333333333333%" /><thead></thead><tbody><tr>
<td style="vertical-align:top;">
<ul class="ul" id="topic_d45_wcw_pgb__ul_tc3_nlx_wp">
<li class="li"><a class="xref" href="../ref_guide/modules/btree_gin.html">btree_gin</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/citext.html">citext</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/dblink.html">dblink</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/diskquota.html">diskquota</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/fuzzystrmatch.html">fuzzystrmatch</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/gp_array_agg.html">gp_array_agg</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/gp_parallel_retrieve_cursor.html">gp_parallel_retrieve_cursor</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/gp_percentile_agg.html">gp_percentile_agg</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/gp_sparse_vector.html">gp_sparse_vector</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/greenplum_fdw.html">greenplum_fdw</a></li>
</ul>
</td>
<td style="vertical-align:top;">
<ul class="ul">
<li class="li"><a class="xref" href="../ref_guide/modules/hstore.html">hstore</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/ip4r.html">ip4r</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/ltree.html">ltree</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/orafce_ref.html">orafce</a> (Tanzu Greenplum only)</li>
<li class="li"><a class="xref" href="../ref_guide/modules/pageinspect.html">pageinspect</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/pg_trgm.html">pg_trgm</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/pgcrypto.html">pgcrypto</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/postgres_fdw.html">postgres_fdw</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/postgresql-hll.html">postgresql-hll</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/sslinfo.html">sslinfo</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/timestamp9.html">timestamp9</a></li>
<li class="li"><a class="xref" href="../ref_guide/modules/uuid-ossp.html">uuid-ossp</a></li>
</ul>
</td>
</tr>
</tbody></table>

For additional information about the modules supplied with Greenplum Database, refer to [Additional Supplied Modules](../ref_guide/modules/intro.html) in the *Greenplum Database Reference Guide*.

**Parent topic:** [Installing and Upgrading Greenplum](install_guide.html)

