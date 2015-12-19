/*
 * Copyright (c) 2004, 2005, 2006 TADA AB - Taby Sweden
 * Distributed under the terms shown in the file COPYRIGHT
 * found in the root folder of this project or at
 * http://eng.tada.se/osprojects/COPYRIGHT.html
 */
package org.postgresql.pljava.internal;

import java.io.File;
import java.io.FilePermission;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.security.Permission;
import java.sql.SQLException;
import java.util.PropertyPermission;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.postgresql.pljava.management.Commands;


/**
 * Provides access to some useful routines in the PostgreSQL server.
 * @author Thomas Hallgren
 */
public class Backend
{
	/**
	 * All native calls synchronize on this object.
	 */
	public static final Object THREADLOCK = new Object();

	private static Session s_session;

	public static synchronized Session getSession()
	{
		if(s_session == null)
			s_session = new Session();
		return s_session;
	}

	/**
	 * Returns the configuration option as read from the Global
	 * Unified Config package (GUC).
	 * @param key The name of the option.
	 * @return The value of the option.
	 */
	public static String getConfigOption(String key)
	{
		synchronized(THREADLOCK)
		{
			return _getConfigOption(key);
		}
	}

	/**
	 * returns the library path '$libdir'
	 */
	public static String getLibraryPath()
	{
		synchronized(THREADLOCK)
		{
			return _getLibraryPath();
		}
	}

	/**
	 * Returns the size of the statement cache.
	 * @return the size of the statement cache.
	 */
	public static int getStatementCacheSize()
	{
		synchronized(THREADLOCK)
		{
			return _getStatementCacheSize();
		}
	}

	/**
	 * Log a message using the internal elog command.
	 * @param logLevel The log level as defined in
	 * {@link ELogHandler}.
	 * @param str The message
	 */
	static void log(int logLevel, String str)
	{
		synchronized(THREADLOCK)
		{
			_log(logLevel, str);
		}
	}

	private static class PLJavaSecurityManager extends SecurityManager
	{
		private boolean m_recursion = false;

		public void checkPermission(Permission perm)
		{
			this.nonRecursiveCheck(perm);
		}

		public void checkPermission(Permission perm, Object context)
		{
			this.nonRecursiveCheck(perm);
		}

		private synchronized void nonRecursiveCheck(Permission perm)
		{
			if(m_recursion)
				//
				// Something, probably a ClassLoader loading one of
				// the referenced classes, caused a recursion. Well
				// everything done within this method is permitted
				// so we just return here.
				//
				return;

			m_recursion = true;
			try
			{
				this.assertPermission(perm);
			}
			finally
			{
				m_recursion = false;
			}
		}

		void assertPermission(Permission perm)
		{
			if(perm instanceof RuntimePermission)
			{
				String name = perm.getName();
				if("*".equals(name) || "exitVM".equals(name))
					throw new SecurityException();
				else if("setSecurityManager".equals(name) && !s_inSetTrusted)
					//
					// Attempt to set another security manager while not
					// in the setTrusted method
					//
					throw new SecurityException();
			}
			else if(perm instanceof PropertyPermission)
			{
				if(perm.getActions().indexOf("write") >= 0)
				{
					// We never allow this to be changed.
					//
					String propName = perm.getName();
					if(propName.equals("java.home"))
						throw new SecurityException();
				}
			}
		}
	}

	private static boolean s_inSetTrusted = false;

	private static final SecurityManager s_untrustedSecurityManager = new PLJavaSecurityManager();

	/**
	 * This security manager will block all attempts to access the file system
	 */
	private static final SecurityManager s_trustedSecurityManager = new PLJavaSecurityManager()
	{
		void assertPermission(Permission perm)
		{
			if(perm instanceof FilePermission)
			{
				String actions = perm.getActions();
				if("read".equals(actions))
				{
					// Must be able to read timezone info etc. in the java
					// installation directory.
					//
					String classpath = Backend._getConfigOption("pljava_classpath");
                    			String[] classpathArray = classpath.split(":");

					String jarpath =  Backend.getLibraryPath() + "/java";
					File javaHome = new File(System.getProperty("java.home"));

                    			File GPJavaLib = new File(jarpath);
					File accessedFile = new File(perm.getName());

                    			File fileDir = accessedFile.getParentFile();

                    			while(fileDir != null)
					{
						if(fileDir.equals(javaHome))
							return;
						if(fileDir.equals(GPJavaLib))
							return;
			                        // now search through the classpath's
			                        for (int i = 0; i < classpathArray.length; i++)
                        			{
							File classPathEntry = new File(classpathArray[i]);
                            				// need to check to see if we can read the directory as well as read the actual file.
                            				if ( fileDir.equals(classPathEntry.getPath()) || fileDir.equals(classPathEntry.getParentFile()))return;
                			        }
						fileDir = fileDir.getParentFile();
					}
				}
				throw new SecurityException(perm.getActions() + " on " + perm.getName());
			}
			super.assertPermission(perm);
		}
	};

	public static void addClassImages(int jarId, String urlString)
	throws SQLException
	{
		InputStream urlStream = null;
		boolean wasTrusted = (System.getSecurityManager() == s_trustedSecurityManager);

		if(wasTrusted)
			setTrusted(false);

		try
		{
			URL url = new URL(urlString);
			urlStream = url.openStream();
			Commands.addClassImages(jarId, urlStream);
		}
		catch(IOException e)
		{
			throw new SQLException("I/O exception reading jar file: " + e.getMessage());
		}
		finally
		{
			if(urlStream != null)
				try { urlStream.close(); } catch(IOException e) {}
			if(wasTrusted)
				setTrusted(true);
		}
	}

	public static void clearFunctionCache()
	{
		synchronized(THREADLOCK)
		{
			_clearFunctionCache();
		}
	}

	/**
	 * Called when the JVM is first booted and then everytime a switch
	 * is made between calling a trusted function versus an untrusted
	 * function.
	 */
	private static void setTrusted(boolean trusted)
	{
		s_inSetTrusted = true;
		try
		{
			Logger log = Logger.getAnonymousLogger();
			if(log.isLoggable(Level.FINE))
				log.fine("Using SecurityManager for " + (trusted ? "trusted" : "untrusted") + " language");
			System.setSecurityManager(trusted ? s_trustedSecurityManager : s_untrustedSecurityManager);
		}
		finally
		{
			s_inSetTrusted = false;
		}
	}

	/**
	 * Returns <code>true</code> if the backend is awaiting a return from a
	 * call into the JVM. This method will only return <code>false</code>
	 * when called from a thread other then the main thread and the main
	 * thread has returned from the call into the JVM.
	 */
	public native static boolean isCallingJava();

	/**
	 * Returns the value of the GUC custom variable <code>
	 * pljava.release_lingering_savepoints</code>.
	 */
	public native static boolean isReleaseLingeringSavepoints();

	private native static String _getConfigOption(String key);
	private native static String _getLibraryPath();

	private native static int  _getStatementCacheSize();
	private native static void _log(int logLevel, String str);
	private native static void _clearFunctionCache();
}
