/*
 * Copyright (c) 2004, 2005 TADA AB - Taby Sweden
 * Distributed under the terms shown in the file COPYRIGHT
 * found in the root folder of this project or at
 * http://eng.tada.se/osprojects/COPYRIGHT.html
 */
package org.postgresql.example;

import java.sql.Connection;
import java.sql.Date;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Savepoint;
import java.sql.Statement;
import java.sql.Time;
import java.util.logging.Logger;

import org.postgresql.pljava.SavepointListener;
import org.postgresql.pljava.Session;
import org.postgresql.pljava.SessionManager;

/**
 * Some methods used for testing the SPI JDBC driver.
 *
 * @author Thomas Hallgren
 */
public class SPIActions
{
	static void log(String msg)
	{
		// GCJ has a somewhat serious bug (reported)
		//
		if("GNU libgcj".equals(System.getProperty("java.vm.name")))
		{
			System.out.print("INFO: ");
			System.out.println(msg);
		}
		else
			Logger.getAnonymousLogger().config(msg);
	}

	private static final String SP_CHECKSTATE = "sp.checkState";

	private static final SavepointListener spListener = new SavepointListener()
	{
		public void onAbort(Session session, Savepoint savepoint, Savepoint parent) throws SQLException
		{
			log("Abort of savepoint " + savepoint.getSavepointId());
			nextState(session, 3, 4);
		}

		public void onCommit(Session session, Savepoint savepoint, Savepoint parent) throws SQLException
		{
			log("Commit of savepoint " + savepoint.getSavepointId());
			nextState(session, 3, 4);
		}

		public void onStart(Session session, Savepoint savepoint, Savepoint parent) throws SQLException
		{
			log("Start of savepoint " + savepoint.getSavepointId());
			nextState(session, 0, 1);
		}
	};

	private static void nextState(Session session, int expected, int next) throws SQLException
	{
		Integer state = (Integer)session.getAttribute(SP_CHECKSTATE);
		if(state == null || state.intValue() != expected)
			throw new SQLException(SP_CHECKSTATE + ": Expected " + expected + ", got " + state);
		session.setAttribute(SP_CHECKSTATE, new Integer(next));
	}

	public static int testSavepointSanity()
	throws SQLException
	{
		Connection conn = DriverManager.getConnection("jdbc:default:connection");

		// Create an anonymous savepoint.
		//
		log("Attempting to set an anonymous savepoint");
		Session currentSession = SessionManager.current();
		currentSession.setAttribute(SP_CHECKSTATE, new Integer(0));
		currentSession.addSavepointListener(spListener);

		Savepoint sp = conn.setSavepoint();
		nextState(currentSession, 1, 2);
		try
		{
			Statement stmt = conn.createStatement();
			log("Attempting to set a SAVEPOINT using SQL (should fail)");
			stmt.execute("SAVEPOINT foo");
		}
		catch(SQLException e)
		{
			log("It failed allright. Everything OK then");
			log("Rolling back to anonymous savepoint");

			nextState(currentSession, 2, 3);
			conn.rollback(sp);
			nextState(currentSession, 4, 5);
			return 1;
		}
		finally
		{
			currentSession.removeSavepointListener(spListener);
		}
		throw new SQLException("SAVEPOINT through SQL succeeded. That's bad news!");
	}

	public static int testTransactionRecovery()
	throws SQLException
	{
		Connection conn = DriverManager.getConnection("jdbc:default:connection");

		// Create an anonymous savepoint.
		//
		log("Attempting to set an anonymous savepoint");
		Session currentSession = SessionManager.current();
		currentSession.setAttribute(SP_CHECKSTATE, new Integer(0));
		currentSession.addSavepointListener(spListener);

		Statement stmt = conn.createStatement();
		Savepoint sp = conn.setSavepoint();
		nextState(currentSession, 1, 2);

        try
		{
			log("Attempting to execute a statement with a syntax error");
			stmt.execute("THIS MUST BE A SYNTAX ERROR");
		}
		catch(SQLException e)
		{
			log("It failed. Let's try to recover " +
                "by rolling back to anonymous savepoint");
			nextState(currentSession, 2, 3);
			conn.rollback(sp);
			nextState(currentSession, 4, 5);
			log("Rolled back.");
			log("Now let's try to execute a correct statement.");

			currentSession.setAttribute(SP_CHECKSTATE, new Integer(0));
			sp = conn.setSavepoint();
			nextState(currentSession, 1, 2);
            ResultSet rs = stmt.executeQuery("SELECT 'OK'");
            while (rs.next())
            {
                log("Expected: OK; Retrieved: " + rs.getString(1));
            }
            rs.close();
            stmt.close();
			nextState(currentSession, 2, 3);
			conn.releaseSavepoint(sp);
			nextState(currentSession, 4, 5);
			return 1;
		}
		finally
		{
			currentSession.removeSavepointListener(spListener);
		}

        //Should never get here
        return -1;
	}

	public static int transferPeopleWithSalary(int salary)
	throws SQLException
	{
		Connection conn = DriverManager.getConnection("jdbc:default:connection");
		PreparedStatement select = null;
		PreparedStatement insert = null;
		PreparedStatement delete = null;
		ResultSet rs = null;

		String stmt;
		try
		{
			stmt = "SELECT id, name, salary FROM employees1 WHERE salary > ?";
			log(stmt);
			select = conn.prepareStatement(stmt);

			stmt = "INSERT INTO employees2(id, name, salary, transferDay, transferTime) VALUES (?, ?, ?, ?, ?)";
			log(stmt);
			insert = conn.prepareStatement(stmt);

			stmt = "DELETE FROM employees1 WHERE id = ?";
			log(stmt);
			delete = conn.prepareStatement(stmt);

			log("assigning parameter value " + salary);
			select.setInt(1, salary);
			log("Executing query");
			rs = select.executeQuery();
			int rowNo = 0;
			log("Doing next");
			while(rs.next())
			{
				log("Processing row " + ++rowNo);
				int id = rs.getInt(1);
				String name = rs.getString(2);
				int empSal = rs.getInt(3);

				insert.setInt(1, id);
				insert.setString(2, name);
				insert.setInt(3, empSal);
				long now = System.currentTimeMillis();
				insert.setDate(4, new Date(now));
				insert.setTime(5, new Time(now));
				int nRows = insert.executeUpdate();
				log("Insert processed " + nRows + " rows");

				delete.setInt(1, id);
				nRows = delete.executeUpdate();
				log("Delete processed " + nRows + " rows");
				log("Doing next");
			}
			if(rowNo == 0)
				log("No row found");
			return rowNo;
		}
		finally
		{
			if(select != null)
				select.close();
			if(insert != null)
				insert.close();
			if(delete != null)
				delete.close();
			conn.close();
		}
	}

	public static int maxFromSetReturnExample(int base, int increment)
	throws SQLException
	{
		int max = Integer.MIN_VALUE;
		Connection conn = DriverManager.getConnection("jdbc:default:connection");
		PreparedStatement stmt = null;
		ResultSet rs = null;

		try
		{
			stmt = conn.prepareStatement("SELECT base FROM setReturnExample(?, ?)");
			stmt.setInt(1, base);
			stmt.setInt(2, increment);
			rs = stmt.executeQuery();
			while(rs.next())
			{
				base = rs.getInt(1);
				if(base > max)
					max = base;
			}
			return base;
		}
		finally
		{
			if(rs != null)
				rs.close();
			if(stmt != null)
				stmt.close();
			conn.close();
		}
	}

	/**
	 * Test of bug #1556
	 *
	 */
	public static void nestedStatements(int innerCount)
	throws SQLException
	{
		Connection connection = DriverManager.getConnection("jdbc:default:connection");
		Statement statement = connection.createStatement();

		// Create a set of ID's so that we can do somthing semi-useful during
		// the long loop.
		//
		statement.execute("DELETE FROM javatest.employees1");
		statement.execute("INSERT INTO javatest.employees1 VALUES("
			+ "1, 'Calvin Forrester', 10000)");
		statement.execute("INSERT INTO javatest.employees1 VALUES("
			+ "2, 'Edwin Archer', 20000)");
		statement.execute("INSERT INTO javatest.employees1 VALUES("
			+ "3, 'Rebecka Shawn', 30000)");
		statement.execute("INSERT INTO javatest.employees1 VALUES("
			+ "4, 'Priscilla Johnson', 25000)");

		int idx = 1;
		ResultSet results = statement.executeQuery("SELECT * FROM javatest.hugeResult(" + innerCount + ")");
		while(results.next())
		{
			Statement innerStatement = connection.createStatement();
			innerStatement.executeUpdate(
				"UPDATE javatest.employees1 SET salary = salary + 1 WHERE id=" + idx);
			innerStatement.close();
			if(++idx == 5)
				idx = 0;
		}
		results.close();
		statement.close();
		connection.close();
	}

	public static String getDateAsString()
	throws SQLException
	{
		ResultSet rs = null;
		Statement stmt = null;
		Connection conn = DriverManager.getConnection("jdbc:default:connection");
		try
		{
			stmt = conn.createStatement();
			rs = stmt.executeQuery("SELECT CURRENT_DATE");
			if(rs.next())
				return rs.getDate(1).toString();
			return "Date could not be retrieved";
		}
		finally
		{
			if(rs != null)
				rs.close();
			if(stmt != null)
				stmt.close();
			conn.close();
		}
	}

	public static String getTimeAsString()
	throws SQLException
	{
		ResultSet rs = null;
		Statement stmt = null;
		Connection conn = DriverManager.getConnection("jdbc:default:connection");
		try
		{
			stmt = conn.createStatement();
			rs = stmt.executeQuery("SELECT CURRENT_TIME");
			if(rs.next())
				return rs.getTime(1).toString();
			return "Time could not be retrieved";
		}
		finally
		{
			if(rs != null)
				rs.close();
			if(stmt != null)
				stmt.close();
			conn.close();
		}
	}
}
