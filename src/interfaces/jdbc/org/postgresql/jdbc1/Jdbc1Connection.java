package org.postgresql.jdbc1;


import java.util.Vector;
import java.sql.*;
import org.postgresql.Field;
import org.postgresql.util.PSQLException;

/* $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/jdbc/org/postgresql/jdbc1/Jdbc1Connection.java,v 1.3 2004/03/24 08:11:32 chungwu Exp $
 * This class implements the java.sql.Connection interface for JDBC1.
 * However most of the implementation is really done in
 * org.postgresql.jdbc1.AbstractJdbc1Connection
 */
public class Jdbc1Connection extends org.postgresql.jdbc1.AbstractJdbc1Connection implements java.sql.Connection
{

	public java.sql.Statement createStatement() throws SQLException
	{
		return new org.postgresql.jdbc1.Jdbc1Statement(this);
	}

	public java.sql.PreparedStatement prepareStatement(String sql) throws SQLException
	{
		return new org.postgresql.jdbc1.Jdbc1PreparedStatement(this, sql);
	}

	public java.sql.CallableStatement prepareCall(String sql) throws SQLException
	{
		return new org.postgresql.jdbc1.Jdbc1CallableStatement(this, sql);
	}

	public java.sql.DatabaseMetaData getMetaData() throws SQLException
	{
		if (metadata == null)
			metadata = new org.postgresql.jdbc1.Jdbc1DatabaseMetaData(this);
		return metadata;
	}

	public java.sql.ResultSet getResultSet(java.sql.Statement stat, Field[] fields, Vector tuples, String status, int updateCount, long insertOID, boolean binaryCursor) throws SQLException
	{
		return new Jdbc1ResultSet(this, stat, fields, tuples, status, updateCount, insertOID, binaryCursor);
	}

	public java.sql.ResultSet getResultSet(java.sql.Statement stat, Field[] fields, Vector tuples, String status, int updateCount) throws SQLException
	{
		return new Jdbc1ResultSet(this, stat, fields, tuples, status, updateCount, 0, false);
	}


	public java.sql.ResultSet createResultSet (java.sql.Statement stat) throws SQLException
	{
		// This needs doing.
		return null;
	}
}


