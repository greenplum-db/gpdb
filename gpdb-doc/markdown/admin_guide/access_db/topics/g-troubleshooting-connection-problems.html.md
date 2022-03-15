---
title: Troubleshooting Connection Problems 
---

A number of things can prevent a client application from successfully connecting to Greenplum Database. This topic explains some of the common causes of connection problems and how to correct them.

<table class="table" id="topic14__io140982"><caption><span class="table--title-label">Table 1. </span><span class="title">Common connection problems</span></caption><colgroup><col style="width:20%"><col style="width:80%"></colgroup><thead class="thead">
          <tr class="row">
            <th class="entry" id="topic14__io140982__entry__1">Problem</th>
            <th class="entry" id="topic14__io140982__entry__2">Solution</th>
          </tr>
        </thead><tbody class="tbody">
          <tr class="row">
            <td class="entry" headers="topic14__io140982__entry__1">No <span class="ph filepath">pg_hba.conf</span> entry for host or user</td>
            <td class="entry" headers="topic14__io140982__entry__2">To enable Greenplum Database to accept remote client
              connections, you must configure your Greenplum Database master instance
              so that connections are allowed from the client hosts and database users that will be
              connecting to Greenplum Database. This is done by adding the appropriate
              entries to the <span class="ph filepath">pg_hba.conf</span> configuration file (located in the
              master instance's data directory). For more detailed information, see <a class="xref" href="../../client_auth.html#topic2">Allowing Connections to Greenplum Database</a>.</td>
          </tr>
          <tr class="row">
            <td class="entry" headers="topic14__io140982__entry__1">Greenplum Database is not running</td>
            <td class="entry" headers="topic14__io140982__entry__2">If the Greenplum Database master instance is down,
              users will not be able to connect. You can verify that the Greenplum Database system is up by running the <code class="ph codeph">gpstate</code> utility
              on the Greenplum master host.</td>
          </tr>
          <tr class="row">
            <td class="entry" headers="topic14__io140982__entry__1">Network problems<p class="p">Interconnect timeouts</p>
            </td>
            <td class="entry" headers="topic14__io140982__entry__2">If users connect to the Greenplum
              master host from a remote client, network problems can prevent a connection (for
              example, DNS host name resolution problems, the host system is down, and so on.). To
              ensure that network problems are not the cause, connect to the Greenplum master host from the remote client host. For example:
                <code class="ph codeph">ping hostname
              </code>. <p class="p" id="topic14__io141723">If the system cannot resolve the host names and IP
                addresses of the hosts involved in Greenplum Database, queries and
                connections will fail. For some operations, connections to the Greenplum Database master use <code class="ph codeph">localhost</code> and others use the
                actual host name, so you must be able to resolve both. If you encounter this error,
                first make sure you can connect to each host in your Greenplum Database array from the master host over the network. In the <code class="ph codeph">/etc/hosts</code>
                file of the master and all segments, make sure you have the correct host names and
                IP addresses for all hosts involved in the Greenplum Database array.
                The <code class="ph codeph">127.0.0.1</code> IP must resolve to <code class="ph codeph">localhost</code>.</p>
            </td>
          </tr>
          <tr class="row">
            <td class="entry" headers="topic14__io140982__entry__1">Too many clients already</td>
            <td class="entry" headers="topic14__io140982__entry__2">By default, Greenplum Database is configured to
              allow a maximum of 250 concurrent user connections on the master and 750 on a segment.
              A connection attempt that causes that limit to be exceeded will be refused. This limit
              is controlled by the <code class="ph codeph">max_connections</code> parameter in the
                <code class="ph codeph">postgresql.conf</code> configuration file of the Greenplum Database master. If you change this setting for the master, you must
              also make appropriate changes at the segments.</td>
          </tr>
        </tbody></table>
        
**Parent topic:**[Accessing the Database](../../access_db/topics/g-accessing-the-database.html)

