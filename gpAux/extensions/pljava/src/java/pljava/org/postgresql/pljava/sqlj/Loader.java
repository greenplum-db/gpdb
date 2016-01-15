/*
 * Copyright (c) 2004, 2005, 2006 TADA AB - Taby Sweden
 * Portions Copyright (c) 2010 - Greenplum Inc
 *
 * Distributed under the terms shown in the file COPYRIGHT
 * found in the root folder of this project or at
 * http://eng.tada.se/osprojects/COPYRIGHT.html
 */
package org.postgresql.pljava.sqlj;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.sql.SQLException;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.ListIterator;
import java.util.Map;
import java.util.logging.Logger;

import org.postgresql.pljava.internal.Backend;
import org.postgresql.pljava.internal.Oid;
import org.postgresql.pljava.jdbc.SQLUtils;
import org.postgresql.pljava.sqlj.JarLoader;

/**
 *
 * The Loader class is used by PL/Java to load java bytecode into the VM.
 *
 * In the postgres distribution this bytecode is always loaded from the 
 * sqlj.jar_repository table.
 *
 * In Greenplum that is difficult because it would require uniform access 
 * to the sqlj.jar_repository table from every segment, which is currently
 * not well supported by our architecture. 
 *
 * Instead we mostly use our own loader JarLoader() which reads a jarfile 
 * from the filesystem.  Note that the security manager allows READING from
 * "$GPHOME/lib/postgresql/java/*.class" files, even in trusted mode, since
 * this is shared between all databases any user can theoretically see every
 * installed jar.  
 * 
 * Note: This creates a couple differences from the postgres implementation that
 * currently pull us further from the sql standard embedded java implementation.
 * Specifically:
 *   - We do not support per-database/per-schema CLASSPATH
 *   - CLASSPATH is instead set by a user GUC.
 */
public class Loader extends ClassLoader
{
	private static final String PUBLIC_SCHEMA	= "public";

	private static final Map	s_schemaLoaders = new HashMap();
	private static final Map	s_typeMap		= new HashMap();

	private final static Logger s_logger		= Logger.getLogger(Loader.class.getName());

	/* Greeplum Additions */
	private final LinkedList	    m_jarloaders;
	private final String[]          m_classpath;
	// private final Map			m_filespace;  // Not yet supported
	private static String           m_current_classpath;

	/**
	 * Create a new Loader.
	 * @param entries
	 * @param parent
	 */
	Loader(String classpath)
	throws SQLException
	{
		super(Loader.class.getClassLoader());

		m_jarloaders = new LinkedList();

		if (classpath == null || classpath.length() == 0)
			m_classpath = new String[0];
		else
			m_classpath = classpath.split(":");

		/* Find the directory that contains our jar files */
		String jarpath = Backend.getLibraryPath() + "/java/";

		/* Create a new JarLoader for every element in the classpath */
		for (int i = 0; i < m_classpath.length; i++)
		{
			try 
			{
				String searchPath;

                		if (!m_classpath[i].startsWith("/"))
                		{
                		    searchPath = jarpath + m_classpath[i];
                		}
                		else
                		{
                    			searchPath = m_classpath[i];
                		}

                		URL url = null;
	
        		        File tmp = new File(searchPath);

                		// if directory then lets get all the jar files
                		if (tmp.isDirectory()) {
                	    		File jarFiles[] = tmp.listFiles(new FileFilter() {
                        			public boolean accept(File pathname) {
                        			    // not interested in directories
                        			    if (pathname.isDirectory()) return false;
                        			    // only interested in jar files
                        			    return pathname.getPath().endsWith("jar");
                       				 }
                    			});
                   		 	for (int j=0; j<jarFiles.length;j++)
                    			{
                        			url = new URL("file:///"+jarFiles[j].getPath());
                        			JarLoader loader = new JarLoader(this, url);
                        			m_jarloaders.add(loader);
                   			 }
                		}
                		else
                		{
                		    url = new URL("file:///" + searchPath);
                		    JarLoader loader = new JarLoader(this, url);
                		    m_jarloaders.add(loader);
               			 }

			}
			catch (MalformedURLException e)
		    	{	
				// XXX - Ignore malformed URLs?
				throw new SQLException("Malformed URL Exception: " +
									   e.getMessage());
			}
			catch (IOException e)
			{
				// XXX - Ignore jar doesn't exist?
				throw new SQLException("IOException reading jar: " +
									   e.getMessage());
			}
		}
	}


	/**
	 * Removes all cached schema loaders, functions, and type maps. This
	 * method is called by the utility functions that manipulate the
	 * data that has been cached. It is not intended to be called
	 * from user code.
	 */
	public static void clearSchemaLoaders()
	{
		s_schemaLoaders.clear();
		s_typeMap.clear();
		Backend.clearFunctionCache();
	}

	/**
	 * Obtains the loader that is in effect for the current schema (i.e. the
	 * schema that is first in the search path).
	 * @return A loader
	 * @throws SQLException
	 */
	public static ClassLoader getCurrentLoader()
	throws SQLException
	{
		/* 
		 * Because Greenplum doesn't support per-schema classpaths we just map
		 * everything to the public schema.
		 */
		return getSchemaLoader(PUBLIC_SCHEMA);
	}

	/**
	 * Obtain a loader that has been configured for the class path of the
	 * schema named <code>schemaName</code>. Class paths are defined using the
	 * SQL procedure <code>sqlj.set_classpath</code>.
	 * @param schemaName The name of the schema.
	 * @return A loader.
	 */
	public static ClassLoader getSchemaLoader(String schemaName)
	throws SQLException
	{
		/* 
		 * Rather than having a different loader per schemaName, instead
		 * we simply create a different loader per CLASSPATH.
		 */
		String classpath = Backend.getConfigOption("pljava_classpath");
		if (classpath == null)
			classpath = "";
		if (!classpath.equals(m_current_classpath))
		{
			clearSchemaLoaders();
			m_current_classpath = classpath;
		}

		ClassLoader loader = (ClassLoader) s_schemaLoaders.get(classpath);
		if (loader == null)
		{
			loader = (ClassLoader) new Loader(classpath);
			s_schemaLoaders.put(classpath, loader);
		}
		return loader;
	}

	/**
	 * Returns the SQL type {@link Oid} to Java {@link Class} map that contains the
	 * Java UDT mappings for the given <code>schema</code>.
	 * This method is called by the function mapping mechanisms. Application code
	 * should never call this method.
	 *
	 * @param schema The schema
	 * @return The Map, possibly empty but never <code>null</code>.
	 */
	public static Map getTypeMap(final String schema) 
	throws SQLException
	{
		/* XXX - needs implementation to support TypeMaps */
		return Collections.EMPTY_MAP;

		/*
		Map typesForSchema = (Map)s_typeMap.get(schema);
		if(typesForSchema != null)
			return typesForSchema;

		s_logger.fine("Creating typeMappings for schema " + schema);
		typesForSchema = new HashMap()
		{
			public Object get(Object key)
			{
				s_logger.fine("Obtaining type mapping for OID " + key + " for schema " + schema);
				return super.get(key);
			}
		};
		ClassLoader loader = Loader.getSchemaLoader(schema);
		Statement stmt = SQLUtils.getDefaultConnection().createStatement();
		ResultSet rs = null;
		try
		{
			rs = stmt.executeQuery("SELECT javaName, sqlName FROM sqlj.typemap_entry");
			while(rs.next())
			{
				try
				{
					String javaClassName = rs.getString(1);
					String sqlName = rs.getString(2);
					Class cls = loader.loadClass(javaClassName);
					if(!SQLData.class.isAssignableFrom(cls))
						throw new SQLException("Class " + javaClassName + " does not implement java.sql.SQLData");
					
					Oid typeOid = Oid.forTypeName(sqlName);
					typesForSchema.put(typeOid, cls);
					s_logger.fine("Adding type mapping for OID " + typeOid + " -> class " + cls.getName() + " for schema " + schema);
				}
				catch(ClassNotFoundException e)
				{
					// Ignore, type is not know to this schema and that is ok
				}
			}
			if(typesForSchema.isEmpty())
				typesForSchema = Collections.EMPTY_MAP;
			s_typeMap.put(schema, typesForSchema);
			return typesForSchema;
		}
		finally
		{
			SQLUtils.close(rs);
			SQLUtils.close(stmt);
		}
		*/
	}

	protected Class findClass(final String name)
	throws ClassNotFoundException
	{
		// Scan through all jar files in the classpath
		for (ListIterator iter = m_jarloaders.listIterator(); iter.hasNext(); )
		{
			JarLoader loader = (JarLoader) iter.next();
			try 
			{
				Class r = loader.findClass(name);
				if (r != null)
					return r;
			}
   			catch (ClassNotFoundException e)
			{
				// Ignore exception, look in other loaders (JAR files)
			}
		}
		throw new ClassNotFoundException(name);
	}

	protected URL findResource(String name)
	{
		// Scan through all jar files in the classpath
		for (ListIterator iter = m_jarloaders.listIterator(); iter.hasNext();)
		{
			JarLoader loader = (JarLoader) iter.next();
			URL url = loader.findResource(name);
			if (url != null)
				return url;
		}
		return null;
	}
}
