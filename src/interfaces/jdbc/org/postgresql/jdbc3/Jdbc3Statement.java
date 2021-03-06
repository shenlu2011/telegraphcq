package org.postgresql.jdbc3;


import java.sql.*;

/* $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/jdbc/org/postgresql/jdbc3/Jdbc3Statement.java,v 1.3 2004/03/24 08:11:32 chungwu Exp $
 * This class implements the java.sql.Statement interface for JDBC3.
 * However most of the implementation is really done in
 * org.postgresql.jdbc3.AbstractJdbc3Statement or one of it's parents
 */
public class Jdbc3Statement extends org.postgresql.jdbc3.AbstractJdbc3Statement implements java.sql.Statement
{

	public Jdbc3Statement (Jdbc3Connection c)
	{
		super(c);
	}

   public void initDefaults()
   {
      super.initDefaults();
      m_useServerPrepare=((AbstractJdbc3Connection) connection).useServerPrepareAlways;
   }

}
