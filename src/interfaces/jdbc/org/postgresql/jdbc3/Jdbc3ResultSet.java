package org.postgresql.jdbc3;


import java.sql.*;
import java.util.Vector;
import org.postgresql.Field;

/* $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/jdbc/org/postgresql/jdbc3/Jdbc3ResultSet.java,v 1.3 2004/03/24 08:11:32 chungwu Exp $
 * This class implements the java.sql.ResultSet interface for JDBC3.
 * However most of the implementation is really done in
 * org.postgresql.jdbc3.AbstractJdbc3ResultSet or one of it's parents
 */
public class Jdbc3ResultSet extends org.postgresql.jdbc3.AbstractJdbc3ResultSet implements java.sql.ResultSet
{

	public Jdbc3ResultSet(org.postgresql.PGConnection conn, Statement statement)
	{
		super (conn, statement);
	}

	public Jdbc3ResultSet(Jdbc3Connection conn, Statement statement, Field[] fields, Vector tuples, String status, int updateCount, long insertOID, boolean binaryCursor)
	{
		super(conn, statement, fields, tuples, status, updateCount, insertOID, binaryCursor);
	}

	public java.sql.ResultSetMetaData getMetaData() throws SQLException
	{
		return new Jdbc3ResultSetMetaData(rows, fields);
	}

	public java.sql.Clob getClob(int i) throws SQLException
	{
		wasNullFlag = (this_row[i - 1] == null);
		if (wasNullFlag)
			return null;

		return new Jdbc3Clob(connection, getInt(i));
	}

	public java.sql.Blob getBlob(int i) throws SQLException
	{
		wasNullFlag = (this_row[i - 1] == null);
		if (wasNullFlag)
			return null;

		return new Jdbc3Blob(connection, getInt(i));
	}

}

