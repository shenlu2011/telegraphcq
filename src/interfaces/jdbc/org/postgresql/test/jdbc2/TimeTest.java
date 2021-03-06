package org.postgresql.test.jdbc2;

import org.postgresql.test.TestUtil;
import junit.framework.TestCase;
import java.sql.*;

/*
 * $Id: TimeTest.java,v 1.1.1.2 2003/04/17 23:03:26 sailesh Exp $
 *
 * Some simple tests based on problems reported by users. Hopefully these will
 * help prevent previous problems from re-occuring ;-)
 *
 */
public class TimeTest extends TestCase
{

	private Connection con;

	public TimeTest(String name)
	{
		super(name);
	}

	protected void setUp() throws Exception
	{
		con = TestUtil.openDB();
		TestUtil.createTable(con, "testtime", "tm time");
	}

	protected void tearDown() throws Exception
	{
		TestUtil.dropTable(con, "testtime");
		TestUtil.closeDB(con);
	}

	/*
	 * Tests the time methods in ResultSet
	 */
	public void testGetTime()
	{
		try
		{
			Statement stmt = con.createStatement();

			assertEquals(1, stmt.executeUpdate(TestUtil.insertSQL("testtime", "'01:02:03'")));
			assertEquals(1, stmt.executeUpdate(TestUtil.insertSQL("testtime", "'23:59:59'")));

			// Fall through helper
			timeTest();

			assertEquals(2, stmt.executeUpdate("DELETE FROM testtime"));
			stmt.close();
		}
		catch (Exception ex)
		{
			fail(ex.getMessage());
		}
	}

	/*
	 * Tests the time methods in PreparedStatement
	 */
	public void testSetTime()
	{
		try
		{
			PreparedStatement ps = con.prepareStatement(TestUtil.insertSQL("testtime", "?"));
			Statement stmt = con.createStatement();

			ps.setTime(1, makeTime(1, 2, 3));
			assertEquals(1, ps.executeUpdate());

			ps.setTime(1, makeTime(23, 59, 59));
			assertEquals(1, ps.executeUpdate());

			// Fall through helper
			timeTest();

			assertEquals(2, stmt.executeUpdate("DELETE FROM testtime"));
			stmt.close();
			ps.close();
		}
		catch (Exception ex)
		{
			fail(ex.getMessage());
		}
	}

	/*
	 * Helper for the TimeTests. It tests what should be in the db
	 */
	private void timeTest() throws SQLException
	{
		Statement st = con.createStatement();
		ResultSet rs;
		java.sql.Time t;

		rs = st.executeQuery(TestUtil.selectSQL("testtime", "tm"));
		assertNotNull(rs);

		assertTrue(rs.next());
		t = rs.getTime(1);
		assertNotNull(t);
		assertEquals(makeTime(1, 2, 3), t);

		assertTrue(rs.next());
		t = rs.getTime(1);
		assertNotNull(t);
		assertEquals(makeTime(23, 59, 59), t);

		assertTrue(! rs.next());

		rs.close();
	}

	private java.sql.Time makeTime(int h, int m, int s)
	{
		return java.sql.Time.valueOf(TestUtil.fix(h, 2) + ":" +
									 TestUtil.fix(m, 2) + ":" +
									 TestUtil.fix(s, 2));
	}
}
