/*-------------------------------------------------------------------------
 *
 * ConnectorUtil
 *
 * Copyright (c) 2011 EMC Corporation All Rights Reserved
 *
 * This software is protected, without limitation, by copyright law
 * and international treaties. Use of this software and the intellectual
 * property contained therein is expressly limited to the terms and
 * conditions of the License Agreement under which it is provided by
 * or on behalf of EMC.
 *
 *-------------------------------------------------------------------------
 */

package com.emc.greenplum.gpdb.hdfsconnector;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.security.SecurityUtil;

import java.net.URI;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.net.InetAddress;
import java.util.Set;

import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;
import javax.security.auth.Subject;
import javax.security.auth.kerberos.KerberosTicket;

/**
 * This is a common utility for the GPDB side Hadoop connector. Routines common to
 * HDFSReader and HDFSWriter would appear here.
 * Therefore, most of the routine should be protected.
 * @author achoi
 *
 */
public class ConnectorUtil
{
	/**
	 * Helper routine to translate the External table URI to the actual
	 * Hadoop fs.default.name and set it in the conf.
	 * MapR's URI starts with maprfs:///
	 * Hadoop's URI starts with hdfs://
	 *
	 * @param conf the configuration
	 * @param inputURI the external table URI
	 * @param connVer the Hadoop Connector version
	 */
	protected static void setHadoopFSURI(Configuration conf, URI inputURI, String connVer)
	{
		/**
		 * Parse the URI and reconstruct the destination URI
		 * Scheme could be hdfs or maprfs
		 *
		 * NOTE: Unless the version is MR, we're going to use ordinary
		 * hdfs://. 		 *
		 * NOTE: MapR isn't really an URI because of its "///" notation.
		 * MapR also does not use port.
		 */
	    if (connVer.startsWith("gpmr")) {
	    	conf.set("fs.maprfs.impl", "com.mapr.fs.MapRFileSystem");
	    	conf.set("fs.default.name", "maprfs:///"+inputURI.getHost());
	    } else {
	    	String uri = "hdfs://"+inputURI.getHost()+":"+inputURI.getPort();
	    	conf.set("fs.default.name", uri);
	    }
	}

	protected static final String HADOOP_SECURITY_USER_KEYTAB_FILE =
		"com.emc.greenplum.gpdb.hdfsconnector.security.user.keytab.file";
	protected static final String HADOOP_SECURITY_USERNAME =
		"com.emc.greenplum.gpdb.hdfsconnector.security.user.name";
	protected static final String HADOOP_DISABLE_KINIT =
		"com.emc.greenplum.gpdb.hdfsconnector.security.disable.kinit";

	/**
	* Helper routine to login to secure Hadoop. If it's not configured to use
	* security (in the core-site.xml) then return
	*
	* Create a LoginContext using config in $GPHOME/lib/hadoop/jaas.conf and search for a valid TGT
	* which matches HADOOP_SECURITY_USERNAME.
	* Check if the TGT needs to be renewed or recreated and use installed kinit command to handle the
	* credential cache
	*
	* @param conf the configuration
	*/
	protected static void loginSecureHadoop(Configuration conf) throws IOException, InterruptedException
	{
		// if security config does not exist then assume no security
		if ( conf.get(HADOOP_SECURITY_USERNAME) == null || conf.get(HADOOP_SECURITY_USER_KEYTAB_FILE) == null ) {
			return;
		}

		String principal = SecurityUtil.getServerPrincipal( conf.get(HADOOP_SECURITY_USERNAME), InetAddress.getLocalHost().getCanonicalHostName() );
		String JaasConf = System.getenv("GPHOME") + "/lib/hadoop/jaas.conf";
		System.setProperty("java.security.auth.login.config", JaasConf);
		Boolean kinitDisabled = conf.getBoolean(HADOOP_DISABLE_KINIT, false);

		/*
			Attempt to find the TGT from the users ticket cache and check if its a valid TGT
			If the TGT needs to be renewed or recreated then we use kinit binary command so the cache can be persisted
			allowing future queries to reuse cached TGT's

			If user disables kinit then we fail back SecurityUtil.login which will always perform a AS_REQ
			followed by a TGS_REQ to the KDC and set the global login context.  the problem with this method is if you have 300
			or more GPDB segments then every gphdfs query will issue 300 AS_REQ to the KDC and may result in intermittent failures
			or longer running queries if the KDC can not keep up with the demand
		*/
		try {
			LoginContext login = new LoginContext("gphdfs");
			login.login();
			Subject subject = login.getSubject();
			Set<KerberosTicket> tickets = subject.getPrivateCredentials(KerberosTicket.class);

			// find the TGT that matches the configured principal
			for (KerberosTicket ticket : tickets) {
				if ( ticket.getClient().toString().equals(principal) ) {
					long start = ticket.getStartTime().getTime();
					long end = ticket.getEndTime().getTime();
					long current = System.currentTimeMillis();
					Long rtime = start +  (long) ((end - start) * .8); // compute start time of ticket plus 80% to find the refresh window

					if ( current <= rtime && ticket.isCurrent() ) { // Ticket is current so no further action required
						return;
					} else if ( current >= rtime && ticket.isRenewable() && !kinitDisabled ) { // Ticket needs to be renewed and is renewable
						String[] kinitRefresh = {"kinit", "-R"};
						Process kinitRenew = Runtime.getRuntime().exec(kinitRefresh);
						int rt = kinitRenew.waitFor();
						if ( rt == 0 ) {
							return;
						}

					}
					break;
				}
			}
		} catch (LoginException e) {
			if ( kinitDisabled ) {
				SecurityUtil.login(conf,HADOOP_SECURITY_USER_KEYTAB_FILE,HADOOP_SECURITY_USERNAME);
				return;
			}
			// if kinit is not disabled then do nothing because we will request a new TGT and update the ticket cache
		}

		// fail back to securityutil if kinit is disabled
		if ( kinitDisabled ) { // login from keytab
			SecurityUtil.login(conf,HADOOP_SECURITY_USER_KEYTAB_FILE,HADOOP_SECURITY_USERNAME);
			return;
		}

		// if we made it here then there is not a current TGT found in cache that matches the principal and we need to request a new TGT
		String[] kinitCmd = {"kinit", "-kt", conf.get(HADOOP_SECURITY_USER_KEYTAB_FILE), principal};
		try {
			Process kinit = Runtime.getRuntime().exec(kinitCmd);
			int rt = kinit.waitFor();
			if ( rt != 0 ) {
				BufferedReader errOut = new BufferedReader(new InputStreamReader(kinit.getErrorStream()));
				String line;
				String errOutput = "";
				while( (line = errOut.readLine()) != null ) {
					errOutput += line;
				}
				throw new IOException(String.format("Failed to Acquire TGT using command \"kinit -kt\" with configured keytab and principal settings:\n%s", errOutput));
			}
		} catch (InterruptedException e) {
			throw new InterruptedException(String.format("command \"kinit -kt\" with configured keytab and principal settings:\n%s", e.getMessage()));
		}
	}

}
