package org.postgresql.core;

import org.postgresql.PG_Stream;
import java.io.IOException;

/**
 * Sent to the backend to initialize a newly created connection.
 *
 * $Id: StartupPacket.java,v 1.2 2003/07/17 05:18:51 sailesh Exp $
 */

public class StartupPacket
{
	private static final int SM_DATABASE = 64;
	private static final int SM_USER = 32;
	private static final int SM_OPTIONS = 64;
	private static final int SM_UNUSED = 64;
	private static final int SM_TTY = 64;

	private int protocolMajor;
	private int protocolMinor;
	private String user;
	private String database;

	public StartupPacket(int protocolMajor, int protocolMinor, String user, String database)
	{
		this.protocolMajor = protocolMajor;
		this.protocolMinor = protocolMinor;
		this.user = user;
		this.database = database;
	}

	public void writeTo(PG_Stream stream) throws IOException
	{
		stream.SendInteger(4 + 4 + SM_DATABASE + SM_USER + SM_OPTIONS + SM_UNUSED + SM_TTY, 4);
		stream.SendInteger(protocolMajor, 2);
		stream.SendInteger(protocolMinor, 2);
		stream.Send(database.getBytes(), SM_DATABASE);

		// This last send includes the unused fields
		stream.Send(user.getBytes(), SM_USER + SM_OPTIONS + SM_UNUSED + SM_TTY);
	}
}

