/*-------------------------------------------------------------------------
 *
 * pg_dumpall
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * $Header: /project/eecs/db/cvsroot/postgres/src/bin/pg_dump/pg_dumpall.c,v 1.3 2004/03/24 08:11:24 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <unistd.h>
#ifdef ENABLE_NLS
#include <locale.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifndef HAVE_STRDUP
#include "strdup.h"
#endif
#include <errno.h>

#include "dumputils.h"
#include "libpq-fe.h"
#include "pg_backup.h"
#include "pqexpbuffer.h"

#define _(x) gettext((x))


static char *progname;

static void help(void);

static void dumpUsers(PGconn *conn);
static void dumpGroups(PGconn *conn);
static void dumpCreateDB(PGconn *conn);
static void dumpDatabaseConfig(PGconn *conn, const char *dbname);
static void dumpUserConfig(PGconn *conn, const char *username);
static void makeAlterConfigCommand(const char *arrayitem, const char *type, const char *name);
static void dumpDatabases(PGconn *conn);

static int	runPgDump(const char *dbname);
static PGconn *connectDatabase(const char *dbname, const char *pghost, const char *pgport,
				const char *pguser, bool require_password);
static PGresult *executeQuery(PGconn *conn, const char *query);
static char *findPgDump(const char *argv0);


char	   *pgdumploc;
PQExpBuffer pgdumpopts;
bool		output_clean = false;
bool		verbose = false;
int			server_version;



int
main(int argc, char *argv[])
{
	char	   *pghost = NULL;
	char	   *pgport = NULL;
	char	   *pguser = NULL;
	bool		force_password = false;
	bool		globals_only = false;
	PGconn	   *conn;
	int			c;

#ifdef HAVE_GETOPT_LONG
	static struct option long_options[] = {
		{"clean", no_argument, NULL, 'c'},
		{"inserts", no_argument, NULL, 'd'},
		{"attribute-inserts", no_argument, NULL, 'D'},
		{"column-inserts", no_argument, NULL, 'D'},
		{"host", required_argument, NULL, 'h'},
		{"ignore-version", no_argument, NULL, 'i'},
		{"oids", no_argument, NULL, 'o'},
		{"port", required_argument, NULL, 'p'},
		{"password", no_argument, NULL, 'W'},
		{"username", required_argument, NULL, 'U'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	int			optindex;
#endif

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain("pg_dump", LOCALEDIR);
	textdomain("pg_dump");
#endif

	if (!strrchr(argv[0], '/'))
		progname = argv[0];
	else
		progname = strrchr(argv[0], '/') + 1;

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			help();
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_dumpall (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

	pgdumploc = findPgDump(argv[0]);
	pgdumpopts = createPQExpBuffer();

#ifdef HAVE_GETOPT_LONG
	while ((c = getopt_long(argc, argv, "cdDgh:iop:U:vW", long_options, &optindex)) != -1)
#else
	while ((c = getopt(argc, argv, "cdDgh:iop:U:vW")) != -1)
#endif
	{
		switch (c)
		{
			case 'c':
				output_clean = true;
				break;

			case 'd':
			case 'D':
				appendPQExpBuffer(pgdumpopts, " -%c", c);
				break;

			case 'g':
				globals_only = true;
				break;

			case 'h':
				pghost = optarg;
				appendPQExpBuffer(pgdumpopts, " -h '%s'", pghost);
				break;

			case 'i':
			case 'o':
				appendPQExpBuffer(pgdumpopts, " -%c", c);
				break;

			case 'p':
				pgport = optarg;
				appendPQExpBuffer(pgdumpopts, " -p '%s'", pgport);
				break;

			case 'U':
				pguser = optarg;
				appendPQExpBuffer(pgdumpopts, " -U '%s'", pguser);
				break;

			case 'v':
				verbose = true;
				appendPQExpBuffer(pgdumpopts, " -v");
				break;

			case 'W':
				force_password = true;
				appendPQExpBuffer(pgdumpopts, " -W");
				break;

			default:
				fprintf(stderr, _("Try '%s --help' for more information.\n"), progname);
				exit(1);
		}
	}

	if (optind < argc)
	{
		fprintf(stderr,
				_("%s: too many command line options (first is '%s')\n"
				  "Try '%s --help' for more information.\n"),
				progname, argv[optind], progname);
		exit(1);
	}


	conn = connectDatabase("template1", pghost, pgport, pguser, force_password);

	printf("--\n");
	printf("-- PostgreSQL database cluster dump\n");
	printf("--\n\n");
	printf("\\connect \"template1\"\n\n");

	dumpUsers(conn);
	dumpGroups(conn);

	if (globals_only)
		goto end;

	dumpCreateDB(conn);
	dumpDatabases(conn);

end:
	PQfinish(conn);
	exit(0);
}



static void
help(void)
{
	printf(_("%s extracts a PostgreSQL database cluster into an SQL script file.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]...\n"), progname);

	printf(_("\nOptions:\n"));
#ifdef HAVE_GETOPT_LONG
	printf(_("  -c, --clean              clean (drop) databases prior to create\n"));
	printf(_("  -d, --inserts            dump data as INSERT, rather than COPY, commands\n"));
	printf(_("  -D, --column-inserts     dump data as INSERT commands with column names\n"));
	printf(_("  -g, --globals-only       dump only global objects, no databases\n"));
	printf(_("  -i, --ignore-version     proceed even when server version mismatches\n"
			 "                           pg_dumpall version\n"));
	printf(_("  -o, --oids               include OIDs in dump\n"));
	printf(_("  -v, --verbose            verbose mode\n"));
#else							/* not HAVE_GETOPT_LONG */
	printf(_("  -c                       clean (drop) databases prior to create\n"));
	printf(_("  -d                       dump data as INSERT, rather than COPY, commands\n"));
	printf(_("  -D                       dump data as INSERT commands with column names\n"));
	printf(_("  -g                       dump only global objects, no databases\n"));
	printf(_("  -i                       proceed even when server version mismatches\n"
			 "                           pg_dumpall version\n"));
	printf(_("  -o                       include OIDs in dump\n"));
	printf(_("  -v                       verbose mode\n"));
#endif   /* not HAVE_GETOPT_LONG */
	printf(_("  --help                   show this help, then exit\n"));
	printf(_("  --version                output version information, then exit\n"));

	printf(_("\nConnection options:\n"));
#ifdef HAVE_GETOPT_LONG
	printf(_("  -h, --host=HOSTNAME      database server host name\n"));
	printf(_("  -p, --port=PORT          database server port number\n"));
	printf(_("  -U, --username=NAME      connect as specified database user\n"));
	printf(_("  -W, --password           force password prompt (should happen automatically)\n"));
#else							/* not HAVE_GETOPT_LONG */
	printf(_("  -h HOSTNAME              database server host name\n"));
	printf(_("  -p PORT                  database server port number\n"));
	printf(_("  -U NAME                  connect as specified database user\n"));
	printf(_("  -W                       force password prompt (should happen automatically)\n"));
#endif   /* not HAVE_GETOPT_LONG */

	printf(_("\nThe SQL script will be written to the standard output.\n\n"));
	printf(_("Report bugs to <pgsql-bugs@postgresql.org>.\n"));
}



/*
 * Dump users (but not the user created by initdb).
 */
static void
dumpUsers(PGconn *conn)
{
	PGresult   *res;
	int			i;

	printf("--\n-- Users\n--\n\n");
	printf("DELETE FROM pg_shadow WHERE usesysid <> (SELECT datdba FROM pg_database WHERE datname = 'template0');\n\n");

	res = executeQuery(conn,
					   "SELECT usename, usesysid, passwd, usecreatedb, usesuper, CAST(valuntil AS timestamp) "
					   "FROM pg_shadow "
					   "WHERE usesysid <> (SELECT datdba FROM pg_database WHERE datname = 'template0');");

	for (i = 0; i < PQntuples(res); i++)
	{
		PQExpBuffer buf = createPQExpBuffer();
		const char *username;

		username = PQgetvalue(res, i, 0);
		appendPQExpBuffer(buf, "CREATE USER %s WITH SYSID %s",
						  fmtId(username),
						  PQgetvalue(res, i, 1));

		if (!PQgetisnull(res, i, 2))
		{
			appendPQExpBuffer(buf, " PASSWORD ");
			appendStringLiteral(buf, PQgetvalue(res, i, 2), true);
		}

		if (strcmp(PQgetvalue(res, i, 3), "t") == 0)
			appendPQExpBuffer(buf, " CREATEDB");
		else
			appendPQExpBuffer(buf, " NOCREATEDB");

		if (strcmp(PQgetvalue(res, i, 4), "t") == 0)
			appendPQExpBuffer(buf, " CREATEUSER");
		else
			appendPQExpBuffer(buf, " NOCREATEUSER");

		if (!PQgetisnull(res, i, 5))
			appendPQExpBuffer(buf, " VALID UNTIL '%s'", PQgetvalue(res, i, 5));

		appendPQExpBuffer(buf, ";\n");

		printf("%s", buf->data);
		destroyPQExpBuffer(buf);

		if (server_version >= 70300)
			dumpUserConfig(conn, username);
	}

	PQclear(res);
	printf("\n\n");
}



/*
 * Dump groups.
 */
static void
dumpGroups(PGconn *conn)
{
	PGresult   *res;
	int			i;

	printf("--\n-- Groups\n--\n\n");
	printf("DELETE FROM pg_group;\n\n");

	res = executeQuery(conn, "SELECT groname, grosysid, grolist FROM pg_group;");

	for (i = 0; i < PQntuples(res); i++)
	{
		PQExpBuffer buf = createPQExpBuffer();
		char	   *val;
		char	   *tok;

		appendPQExpBuffer(buf, "CREATE GROUP %s WITH SYSID %s;\n",
						  fmtId(PQgetvalue(res, i, 0)),
						  PQgetvalue(res, i, 1));

		val = strdup(PQgetvalue(res, i, 2));
		tok = strtok(val, ",{}");
		do
		{
			PGresult   *res2;
			PQExpBuffer buf2 = createPQExpBuffer();
			int			j;

			appendPQExpBuffer(buf2, "SELECT usename FROM pg_shadow WHERE usesysid = %s;", tok);
			res2 = executeQuery(conn, buf2->data);
			destroyPQExpBuffer(buf2);

			for (j = 0; j < PQntuples(res2); j++)
			{
				appendPQExpBuffer(buf, "ALTER GROUP %s ", fmtId(PQgetvalue(res, i, 0)));
				appendPQExpBuffer(buf, "ADD USER %s;\n", fmtId(PQgetvalue(res2, j, 0)));
			}

			PQclear(res2);

			tok = strtok(NULL, "{},");
		}
		while (tok);

		printf("%s", buf->data);
		destroyPQExpBuffer(buf);
	}

	PQclear(res);
	printf("\n\n");
}



/*
 * Dump commands to create each database.
 *
 * To minimize the number of reconnections (and possibly ensuing
 * password prompts) required by the output script, we emit all CREATE
 * DATABASE commands during the initial phase of the script, and then
 * run pg_dump for each database to dump the contents of that
 * database.  We skip databases marked not datallowconn, since we'd be
 * unable to connect to them anyway (and besides, we don't want to
 * dump template0).
 */
static void
dumpCreateDB(PGconn *conn)
{
	PGresult   *res;
	int			i;

	printf("--\n-- Database creation\n--\n\n");

	/*
	 * Basically this query returns: dbname, dbowner, encoding,
	 * istemplate, dbpath
	 */
	res = executeQuery(conn, "SELECT datname, coalesce(usename, (select usename from pg_shadow where usesysid=(select datdba from pg_database where datname='template0'))), pg_encoding_to_char(d.encoding), datistemplate, datpath FROM pg_database d LEFT JOIN pg_shadow u ON (datdba = usesysid) WHERE datallowconn ORDER BY 1;");

	for (i = 0; i < PQntuples(res); i++)
	{
		PQExpBuffer buf = createPQExpBuffer();
		char	   *dbname = PQgetvalue(res, i, 0);
		char	   *dbowner = PQgetvalue(res, i, 1);
		char	   *dbencoding = PQgetvalue(res, i, 2);
		char	   *dbistemplate = PQgetvalue(res, i, 3);
		char	   *dbpath = PQgetvalue(res, i, 4);

		if (strcmp(dbname, "template1") == 0)
			continue;

		if (output_clean)
			appendPQExpBuffer(buf, "DROP DATABASE %s;\n", fmtId(dbname));

		appendPQExpBuffer(buf, "CREATE DATABASE %s", fmtId(dbname));
		appendPQExpBuffer(buf, " WITH OWNER = %s TEMPLATE = template0", fmtId(dbowner));

		if (strcmp(dbpath, "") != 0)
		{
			appendPQExpBuffer(buf, " LOCATION = ");
			appendStringLiteral(buf, dbpath, true);
		}

		appendPQExpBuffer(buf, " ENCODING = ");
		appendStringLiteral(buf, dbencoding, true);

		appendPQExpBuffer(buf, ";\n");

		if (strcmp(dbistemplate, "t") == 0)
		{
			appendPQExpBuffer(buf, "UPDATE pg_database SET datistemplate = 't' WHERE datname = ");
			appendStringLiteral(buf, dbname, true);
			appendPQExpBuffer(buf, ";\n");
		}
		printf("%s", buf->data);
		destroyPQExpBuffer(buf);

		if (server_version >= 70300)
			dumpDatabaseConfig(conn, dbname);
	}

	PQclear(res);
	printf("\n\n");
}



/*
 * Dump database-specific configuration
 */
static void
dumpDatabaseConfig(PGconn *conn, const char *dbname)
{
	PQExpBuffer buf = createPQExpBuffer();
	int			count = 1;

	for (;;)
	{
		PGresult   *res;

		printfPQExpBuffer(buf, "SELECT datconfig[%d] FROM pg_database WHERE datname = ", count);
		appendStringLiteral(buf, dbname, true);
		appendPQExpBuffer(buf, ";");

		res = executeQuery(conn, buf->data);
		if (!PQgetisnull(res, 0, 0))
		{
			makeAlterConfigCommand(PQgetvalue(res, 0, 0), "DATABASE", dbname);
			PQclear(res);
			count++;
		}
		else
		{
			PQclear(res);
			break;
		}
	}

	destroyPQExpBuffer(buf);
}



/*
 * Dump user-specific configuration
 */
static void
dumpUserConfig(PGconn *conn, const char *username)
{
	PQExpBuffer buf = createPQExpBuffer();
	int			count = 1;

	for (;;)
	{
		PGresult   *res;

		printfPQExpBuffer(buf, "SELECT useconfig[%d] FROM pg_shadow WHERE usename = ", count);
		appendStringLiteral(buf, username, true);
		appendPQExpBuffer(buf, ";");

		res = executeQuery(conn, buf->data);
		if (!PQgetisnull(res, 0, 0))
		{
			makeAlterConfigCommand(PQgetvalue(res, 0, 0), "USER", username);
			PQclear(res);
			count++;
		}
		else
		{
			PQclear(res);
			break;
		}
	}

	destroyPQExpBuffer(buf);
}



/*
 * Helper function for dumpXXXConfig().
 */
static void
makeAlterConfigCommand(const char *arrayitem, const char *type, const char *name)
{
	char	   *pos;
	char	   *mine;
	PQExpBuffer buf = createPQExpBuffer();

	mine = strdup(arrayitem);
	pos = strchr(mine, '=');
	if (pos == NULL)
		return;

	*pos = 0;
	appendPQExpBuffer(buf, "ALTER %s %s ", type, fmtId(name));
	appendPQExpBuffer(buf, "SET %s TO ", fmtId(mine));
	appendStringLiteral(buf, pos + 1, false);
	appendPQExpBuffer(buf, ";\n");

	printf("%s", buf->data);
	destroyPQExpBuffer(buf);
	free(mine);
}



/*
 * Dump contents of databases.
 */
static void
dumpDatabases(PGconn *conn)
{
	PGresult   *res;
	int			i;

	res = executeQuery(conn, "SELECT datname FROM pg_database WHERE datallowconn ORDER BY 1;");
	for (i = 0; i < PQntuples(res); i++)
	{
		int			ret;

		char	   *dbname = PQgetvalue(res, i, 0);

		if (verbose)
			fprintf(stderr, _("%s: dumping database \"%s\"...\n"), progname, dbname);

		printf("\\connect %s\n", fmtId(dbname));
		ret = runPgDump(dbname);
		if (ret != 0)
		{
			fprintf(stderr, _("%s: pg_dump failed on %s, exiting\n"), progname, dbname);
			exit(1);
		}
	}

	PQclear(res);
}



/*
 * Run pg_dump on dbname.
 */
static int
runPgDump(const char *dbname)
{
	PQExpBuffer cmd = createPQExpBuffer();
	const char *p;
	int			ret;

	appendPQExpBuffer(cmd, "%s %s -X use-set-session-authorization -Fp '",
					  pgdumploc, pgdumpopts->data);

	/* Shell quoting is not quite like SQL quoting, so can't use fmtId */
	for (p = dbname; *p; p++)
	{
		if (*p == '\'')
			appendPQExpBuffer(cmd, "'\"'\"'");
		else
			appendPQExpBufferChar(cmd, *p);
	}

	appendPQExpBufferChar(cmd, '\'');

	if (verbose)
		fprintf(stderr, _("%s: running %s\n"), progname, cmd->data);

	fflush(stdout);
	fflush(stderr);

	ret = system(cmd->data);

	destroyPQExpBuffer(cmd);

	return ret;
}



/*
 * Make a database connection with the given parameters.  An
 * interactive password prompt is automatically issued if required.
 */
static PGconn *
connectDatabase(const char *dbname, const char *pghost, const char *pgport,
				const char *pguser, bool require_password)
{
	PGconn	   *conn;
	char	   *password = NULL;
	bool		need_pass = false;
	PGresult   *res;

	if (require_password)
		password = simple_prompt("Password: ", 100, false);

	/*
	 * Start the connection.  Loop until we have a password if requested
	 * by backend.
	 */
	do
	{
		need_pass = false;
		conn = PQsetdbLogin(pghost, pgport, NULL, NULL, dbname, pguser, password);

		if (!conn)
		{
			fprintf(stderr, _("%s: could not connect to database %s\n"),
					progname, dbname);
			exit(1);
		}

		if (PQstatus(conn) == CONNECTION_BAD &&
			strcmp(PQerrorMessage(conn), "fe_sendauth: no password supplied\n") == 0 &&
			!feof(stdin))
		{
			PQfinish(conn);
			need_pass = true;
			free(password);
			password = NULL;
			password = simple_prompt("Password: ", 100, false);
		}
	} while (need_pass);

	if (password)
		free(password);

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		fprintf(stderr, _("%s: could not connect to database %s: %s\n"),
				progname, dbname, PQerrorMessage(conn));
		exit(1);
	}

	res = executeQuery(conn, "SELECT version();");
	if (PQntuples(res) != 1)
	{
		fprintf(stderr, _("%s: could not get server version\n"), progname);
		exit(1);
	}
	else
	{
		char	   *val = PQgetvalue(res, 0, 0);

		server_version = parse_version(val + strcspn(val, "0123456789"));
		if (server_version < 0)
		{
			fprintf(stderr, _("%s: could not parse server version \"%s\"\n"), progname, val);
			exit(1);
		}
	}

	return conn;
}



/*
 * Run a query, return the results, exit program on failure.
 */
static PGresult *
executeQuery(PGconn *conn, const char *query)
{
	PGresult   *res;

	res = PQexec(conn, query);
	if (!res ||
		PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, _("%s: query failed: %s"), progname, PQerrorMessage(conn));
		fprintf(stderr, _("%s: query was: %s\n"), progname, query);
		PQfinish(conn);
		exit(1);
	}

	return res;
}



/*
 * Find location of pg_dump executable.
 */
static char *
findPgDump(const char *argv0)
{
	char	   *last;
	PQExpBuffer cmd;
	static char *result = NULL;

	if (result)
		return result;

	cmd = createPQExpBuffer();
	last = strrchr(argv0, '/');

	if (!last)
		appendPQExpBuffer(cmd, "pg_dump");
	else
	{
		char	   *dir = strdup(argv0);

		*(dir + (last - argv0)) = '\0';
		appendPQExpBuffer(cmd, "%s/pg_dump", dir);
	}

	result = strdup(cmd->data);

	appendPQExpBuffer(cmd, " -V >/dev/null 2>&1");
	if (system(cmd->data) == 0)
		goto end;

	result = BINDIR "/pg_dump";
	if (system(BINDIR "/pg_dump -V >/dev/null 2>&1") == 0)
		goto end;

	fprintf(stderr, _("%s: could not find pg_dump\n"
		"Make sure it is in the path or in the same directory as %s.\n"),
			progname, progname);
	exit(1);

end:
	destroyPQExpBuffer(cmd);
	return result;
}
