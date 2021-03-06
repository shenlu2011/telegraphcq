package org.postgresql.test.jdbc3;

import java.sql.Connection;
import java.sql.SQLException;
import javax.sql.PooledConnection;
import org.postgresql.test.jdbc2.optional.ConnectionPoolTest;
import org.postgresql.test.TestUtil;
import org.postgresql.jdbc3.*;

/**
 * Tests JDBC3 implementation of ConnectionPoolDataSource.
 *
 * @author Aaron Mulder (ammulder@chariotsolutions.com)
 * @version $Revision: 1.1.1.1 $
 */
public class Jdbc3ConnectionPoolTest extends ConnectionPoolTest
{
    public Jdbc3ConnectionPoolTest(String name)
    {
        super(name);
    }

    /**
     * Creates and configures a Jdbc3ConnectionPool
     */
    protected void initializeDataSource()
    {
        if (bds == null)
        {
            bds = new Jdbc3ConnectionPool();
            String db = TestUtil.getURL();
            if (db.indexOf('/') > -1)
            {
                db = db.substring(db.lastIndexOf('/') + 1);
            }
            else if (db.indexOf(':') > -1)
            {
                db = db.substring(db.lastIndexOf(':') + 1);
            }
            bds.setDatabaseName(db);
            bds.setUser(TestUtil.getUser());
            bds.setPassword(TestUtil.getPassword());
        }
    }

    /**
     * Makes sure this is a JDBC 3 implementation producing JDBC3
     * connections.  Depends on toString implementation of
     * connection wrappers.
     */
    public void testConfirmJdbc3Impl()
    {
        try
        {
            initializeDataSource();
            assertTrue("Wrong ConnectionPool impl used by test: " + bds.getClass().getName(), bds instanceof Jdbc3ConnectionPool);
            PooledConnection pc = ((Jdbc3ConnectionPool) bds).getPooledConnection();
            assertTrue("Wrong PooledConnection impl generated by JDBC3 ConnectionPoolDataSource: " + pc.getClass().getName(), pc instanceof Jdbc3PooledConnection);
            assertTrue("Wrong Connnection class used in JDBC3 ConnectionPoolDataSource's PooledConnection impl: " + pc.getConnection().toString(), pc.getConnection().toString().indexOf("Jdbc3") > -1);
            pc.close();
        }
        catch (SQLException e)
        {
            fail(e.getMessage());
        }
    }
}
