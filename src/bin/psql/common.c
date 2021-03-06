
/*
 * psql - the PostgreSQL interactive terminal
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Copyright 2000 by PostgreSQL Global Development Group
 *
 * $Header: /project/eecs/db/cvsroot/postgres/src/bin/psql/common.c,v 1.32 2006/02/14 22:34:41 phred Exp $
 */
#include "postgres_fe.h"
#include "common.h"

#include <errno.h>
#include <stdarg.h>
#ifndef HAVE_STRDUP
#include <strdup.h>
#endif
#include <signal.h>
#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>				/* for write() */
#include <setjmp.h>
#else
#include <io.h>					/* for _write() */
#include <win32.h>
#include <sys/timeb.h>			/* for _ftime() */
#endif

#ifndef WIN32
#include <sys/ioctl.h>			/* for ioctl() */
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "libpq-fe.h"
#include "pqsignal.h"
#include <ctype.h>
#include "settings.h"
#include "variables.h"
#include "copy.h"
#include "prompt.h"
#include "print.h"
#include "mainloop.h"

extern bool prompt_state;
static bool isCQQuery(char * tmp);
/*
 * "Safe" wrapper around strdup()
 */
char *
xstrdup(const char *string)
{
	char	   *tmp;

	if (!string)
	{
		fprintf(stderr,
				gettext
		("%s: xstrdup: cannot duplicate null pointer (internal error)\n"),
				pset.progname);
		exit(EXIT_FAILURE);
	}
	tmp = strdup(string);
	if (!tmp)
	{
		psql_error("out of memory\n");
		exit(EXIT_FAILURE);
	}
	return tmp;
}



/*
 * setQFout
 * -- handler for -o command line option and \o command
 *
 * Tries to open file fname (or pipe if fname starts with '|')
 * and stores the file handle in pset)
 * Upon failure, sets stdout and returns false.
 */
bool
setQFout(const char *fname)
{
	bool		status = true;

	/* Close old file/pipe */
	if (pset.queryFout && pset.queryFout != stdout
		&& pset.queryFout != stderr)
	{
		if (pset.queryFoutPipe)
			pclose(pset.queryFout);
		else
			fclose(pset.queryFout);
	}

	/* If no filename, set stdout */
	if (!fname || fname[0] == '\0')
	{
		pset.queryFout = stdout;
		pset.queryFoutPipe = false;
	}
	else if (*fname == '|')
	{
		pset.queryFout = popen(fname + 1, "w");
		pset.queryFoutPipe = true;
	}
	else
	{
		pset.queryFout = fopen(fname, "w");
		pset.queryFoutPipe = false;
	}

	if (!(pset.queryFout))
	{
		psql_error("%s: %s\n", fname, strerror(errno));
		pset.queryFout = stdout;
		pset.queryFoutPipe = false;
		status = false;
	}

	/* Direct signals */
#ifndef WIN32
	if (pset.queryFoutPipe)
		pqsignal(SIGPIPE, SIG_IGN);
	else
		pqsignal(SIGPIPE, SIG_DFL);
#endif

	return status;
}



/*
 * Error reporting for scripts. Errors should look like
 *	 psql:filename:lineno: message
 *
 */
void
psql_error(const char *fmt,...)
{
	va_list		ap;

	fflush(stdout);
	if (pset.queryFout != stdout)
		fflush(pset.queryFout);

	if (pset.inputfile)
		fprintf(stderr, "%s:%s:%u: ", pset.progname, pset.inputfile,
				pset.lineno);
	va_start(ap, fmt);
	vfprintf(stderr, gettext(fmt), ap);
	va_end(ap);
}



/*
 * for backend INFO, WARNING, ERROR
 */
void
NoticeProcessor(void *arg, const char *message)
{
	(void) arg;					/* not used */
	psql_error("%s", message);
}



/*
 * Code to support query cancellation
 *
 * Before we start a query, we enable a SIGINT signal catcher that sends a
 * cancel request to the backend. Note that sending the cancel directly from
 * the signal handler is safe because PQrequestCancel() is written to make it
 * so. We use write() to print to stdout because it's better to use simple
 * facilities in a signal handler.
 */
PGconn	   *cancelConn;
volatile bool cancel_pressed;

#ifndef WIN32

#define write_stderr(String) write(fileno(stderr), String, strlen(String))

void
handle_sigint(SIGNAL_ARGS)
{
	int			save_errno = errno;

	if (GetVariableBool(pset.vars, "CQ"))
		cqcancel = true;

	/* Don't muck around if copying in or prompting for a password. */
	if ((copy_in_state && pset.cur_cmd_interactive) || prompt_state)
		return;


	if (cancelConn == NULL)
	{
		write_stderr("cancel called with NULL cancelcon... doing nothing");
		/* regular postgres queries can just jump back to  */
		/* the main psql loop.	continuous queries spend some  */
		/* time in the client displaying partial results, and we  */
		/* must return to the cq processing loop to make sure the  */
		/* cursor gets closed down. */

		if (!cqcancel)
			siglongjmp(main_loop_jmp, 1);
		else
			return;
	}




	cancel_pressed = true;

	if (!PQrequestCancel(cancelConn))
	{
		write_stderr("Could not send cancel request: ");
		write_stderr(PQerrorMessage(cancelConn));
	}
	errno = save_errno;			/* just in case the write changed it */
}
#endif   /* not WIN32 */


/*
 * PSQLexec
 *
 * This is the way to send "backdoor" queries (those not directly entered
 * by the user). It is subject to -E but not -e.
 *
 * If the given querystring generates multiple PGresults, normally the last
 * one is returned to the caller.  However, if ignore_command_ok is TRUE,
 * then PGresults with status PGRES_COMMAND_OK are ignored.  This is intended
 * mainly to allow locutions such as "begin; select ...; commit".
 */
PGresult *
PSQLexec(const char *query, bool ignore_command_ok)
{
	PGresult   *res = NULL;
	PGresult   *newres;
	const char *var;
	ExecStatusType rstatus;

	if (!pset.db)
	{
		psql_error("You are currently not connected to a database.\n");
		return NULL;
	}

	var = GetVariable(pset.vars, "ECHO_HIDDEN");
	if (var)
	{
		printf("********* QUERY **********\n"
			   "%s\n" "**************************\n\n", query);
		fflush(stdout);
	}

	if (var && strcmp(var, "noexec") == 0)
		return NULL;

	/* discard any uneaten results of past queries */
	while ((newres = PQgetResult(pset.db)) != NULL)
		PQclear(newres);

	cancelConn = pset.db;
	if (PQsendQuery(pset.db, query))
	{
		while ((newres = PQgetResult(pset.db)) != NULL)
		{
			rstatus = PQresultStatus(newres);
			if (ignore_command_ok && rstatus == PGRES_COMMAND_OK)
			{
				PQclear(newres);
				continue;
			}
			PQclear(res);
			res = newres;
			if (rstatus == PGRES_COPY_IN ||
				rstatus == PGRES_COPY_OUT)
				break;
		}
	}
	rstatus = PQresultStatus(res);
	/* keep cancel connection for copy out state */
	if (rstatus != PGRES_COPY_OUT)
		cancelConn = NULL;
	if (rstatus == PGRES_COPY_IN)
		copy_in_state = true;

	if (res && (rstatus == PGRES_COMMAND_OK ||
				rstatus == PGRES_TUPLES_OK ||
				rstatus == PGRES_COPY_IN ||
				rstatus == PGRES_COPY_OUT))
		return res;
	else
	{
		psql_error("%s", PQerrorMessage(pset.db));
		PQclear(res);

		if (PQstatus(pset.db) == CONNECTION_BAD)
		{
			if (!pset.cur_cmd_interactive)
			{
				psql_error("connection to server was lost\n");
				exit(EXIT_BADCONN);
			}
			fputs(gettext
			("The connection to the server was lost. Attempting reset: "),
				  stderr);
			PQreset(pset.db);
			if (PQstatus(pset.db) == CONNECTION_BAD)
			{
				fputs(gettext("Failed.\n"), stderr);
				PQfinish(pset.db);
				pset.db = NULL;
				SetVariable(pset.vars, "DBNAME", NULL);
				SetVariable(pset.vars, "HOST", NULL);
				SetVariable(pset.vars, "PORT", NULL);
				SetVariable(pset.vars, "USER", NULL);
				SetVariable(pset.vars, "ENCODING", NULL);
			}
			else
				fputs(gettext("Succeeded.\n"), stderr);
		}

		return NULL;
	}
}

/*
 * SendQueryCQ: send a continuous query to the system, and print out the
 * results as they are sent to the client.`
 * TCQOC
 */

bool
SendQueryCQ(char *query)
{
	char	   *declareqstr = NULL;
	char	   *beginq = "BEGIN";
	char	   *commitq = "COMMIT";
	char	   *declareq = "declare cqcursor cursor for ";
	char	   *closeq = "close cqcursor";
	char		fetchq[256];	/* "fetch [# rows] from cqcursor" */

	PGresult   *res = NULL;
	PGnotify   *notify = NULL;
	bool		success = true;
	bool		isfirst = true;
	bool		oldtuples_only = pset.popt.topt.tuples_only;

    unsigned int *widths = NULL;
        /* Max. width seen for each field. */

	/*
	 * The user may specify the number of rows to fetch at a time and how
	 * many rows to fetch in total.
	 */
	unsigned	rows_per_iteration =
	(0 == pset.cq_rows_iter ? 1 : pset.cq_rows_iter);
	unsigned	rows_per_query = pset.cq_rows_quer;		/* 0 --> infinity */
	unsigned	num_rows_read = 0;

    /* Indicate to our pretty-printers that we're in CQ mode. */
    pset.popt.cq_mode = true; 
    pset.popt.topt.cq_mode = true; 

       
	snprintf(fetchq, sizeof(fetchq), "fetch %d from cqcursor",
			 rows_per_iteration);

	declareqstr = malloc(strlen(declareq) + strlen(query) + 1);
	strcpy(declareqstr, declareq);
	strcat(declareqstr, query);
	cqcancel = FALSE;
	cancelConn = pset.db;

	/* Start the query. */
	res = PQexec(pset.db, beginq);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		char	   *err = PQresultErrorMessage(res);

		fprintf(pset.queryFout, err);
		return false;
	}


	/* Declare a cursor */
	res = PQexec(pset.db, declareqstr);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		char	   *err = PQresultErrorMessage(res);

		fprintf(pset.queryFout, err);
		success = false;
	}
	free(declareqstr);
	declareqstr = NULL;


	/* Fetch tuples from the cursor. */
	while (cqcancel == FALSE && success
		   && (res = PQexec(pset.db, fetchq)) != NULL)
	{

		switch (PQresultStatus(res))
		{
			case PGRES_TUPLES_OK:
			        if (PQntuples(res) == 0)
				  success = false;
				else
				{
					/* Have we read the requested number of rows? */
					num_rows_read += PQntuples(res);
					if (rows_per_query > 0)
					{

						if (num_rows_read >= rows_per_query)
						{
							/*
							 * For now, just pretend the query was
							 * cancelled. TODO: Come with a special state
							 * for "we read the requested number of rows
							 * and then stopped."
							 */
							cqcancel = TRUE;
						}
						else if (rows_per_query - num_rows_read
								 <= rows_per_iteration)
						{
							/* Last iteration. */
							snprintf(fetchq, sizeof(fetchq),
									 "fetch %d from cqcursor",
									 rows_per_query - num_rows_read);
						}
					}
					/* write output to \g argument, if any */
					if (pset.gfname)
					{
						FILE	   *queryFout_copy = pset.queryFout;
						bool		queryFoutPipe_copy = pset.queryFoutPipe;

						pset.queryFout = stdout;		/* so it doesn't get
														 * closed */

						/* open file/pipe */
						if (!setQFout(pset.gfname))
						{
							pset.queryFout = queryFout_copy;
							pset.queryFoutPipe = queryFoutPipe_copy;
							break;
						}

						printQuery(res, &pset.popt, pset.queryFout, NULL);

						/* close file/pipe, restore old setting */
						setQFout(NULL);

						pset.queryFout = queryFout_copy;
						pset.queryFoutPipe = queryFoutPipe_copy;

						free(pset.gfname);
						pset.gfname = NULL;

						success = true;
					}
					else
					{
                        int nfields = PQnfields(res);

                        if (nfields > 0 && NULL == widths) {
                            int i;
                            widths = calloc(nfields, sizeof(*widths));
                            for (i = 0; i < nfields; i++) {
                                widths[i] = 0UL;
                            }
                        }

						printQuery(res, &pset.popt, pset.queryFout, widths);
						success = true;
					}
				}

				break;
			case PGRES_EMPTY_QUERY:
				success = true;
				break;
			case PGRES_COMMAND_OK:
				{
					fprintf(pset.queryFout,
							"PGRES_COMMAND_OK not supported for CQ");
					success = true;
					cqcancel = TRUE;

					break;
				}
			case PGRES_COPY_OUT:
			case PGRES_COPY_IN:
				fprintf(pset.queryFout,
						"PGRES_COMMAND_COPY_* not supported for CQ");

				break;

			case PGRES_NONFATAL_ERROR:
			case PGRES_FATAL_ERROR:
			case PGRES_BAD_RESPONSE:
				success = false;
				psql_error("%s", PQerrorMessage(pset.db));
				break;

            default:
                /* This should be unreachable. */
                psql_error("Something has gone horribly wrong.");
                break;
		}

		fflush(pset.queryFout);

		if (PQstatus(pset.db) == CONNECTION_BAD)
		{
			if (!pset.cur_cmd_interactive)
			{
				psql_error("connection to server was lost\n");
				exit(EXIT_BADCONN);
			}
			fputs(gettext
			("The connection to the server was lost. Attempting reset: "),
				  stderr);
			PQreset(pset.db);
			if (PQstatus(pset.db) == CONNECTION_BAD)
			{
				fputs(gettext("Failed.\n"), stderr);
				PQfinish(pset.db);
				PQclear(res);
				pset.db = NULL;
				SetVariable(pset.vars, "DBNAME", NULL);
				SetVariable(pset.vars, "HOST", NULL);
				SetVariable(pset.vars, "PORT", NULL);
				SetVariable(pset.vars, "USER", NULL);
				SetVariable(pset.vars, "ENCODING", NULL);
				return false;
			}
			else
				fputs(gettext("Succeeded.\n"), stderr);
		}

		/* check for asynchronous notification returns */
		while ((notify = PQnotifies(pset.db)) != NULL)
		{
			fprintf(pset.queryFout,
					gettext
					("Asynchronous NOTIFY '%s' from backend with pid %d received.\n"),
					notify->relname, notify->be_pid);
			free(notify);
			fflush(pset.queryFout);
		}

		if (res)
			PQclear(res);
		if (isfirst)
		{
			isfirst = false;
			pset.popt.topt.tuples_only = true;
		}
	}

	pset.popt.topt.tuples_only = oldtuples_only;

	/*fprintf(pset.queryFout, "success=%d, cqcancel=%d\n", success,
     * cqcancel);*/

	if (success)
	{							/* else txn aborted */
		res = PQexec(pset.db, closeq);
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
			char	   *err = PQresultErrorMessage(res);

			fprintf(pset.queryFout, err);
			return false;
		}
	}
	res = PQexec(pset.db, commitq);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		char	   *err = PQresultErrorMessage(res);

		fprintf(pset.queryFout, err);
		return false;
	}


	return true;
}

/*
 * SendQuery: send the query string to the backend
 * (and print out results)
 *
 * Note: This is the "front door" way to send a query. That is, use it to
 * send queries actually entered by the user. These queries will be subject to
 * single step mode.
 * To send "back door" queries (generated by slash commands, etc.) in a
 * controlled way, use PSQLexec().
 *
 * Returns true if the query executed successfully, false otherwise.
 */
bool
SendQuery(const char *query)
{
	bool		success = false;
	PGresult   *results;
	PGnotify   *notify;
	char	   *tmp = (char *) query;

#ifndef WIN32
	struct timeval before,
				after;

#else
	struct _timeb before,
				after;
#endif

	if (!pset.db)
	{
		psql_error("You are currently not connected to a database.\n");
		return false;
	}

	/*
	 * If the client is running in "cursor mode" and this SQL statement is
	 * a query (as opposed to a DDL statement), fetch results in a
	 * streaming fashion.
	 */

	while (tmp && *tmp != '\0' && isspace(*tmp))
		tmp++;

	if (GetVariableBool(pset.vars, "CQ") &&
	    (isCQQuery(tmp))) 
	  {
	    return SendQueryCQ((char *) query);
	  }
	else if(GetVariableBool(pset.vars, "CQ")) /* - TCQ SR*/
	  { 
	  printf("ATTENTION: Running in CQ mode but not sending this query as a CQ query\n");
	  }
	

    /* If we get here, we're NOT in CQ mode. */
    pset.popt.cq_mode = false;
    pset.popt.topt.cq_mode = false; 

	if (GetVariableBool(pset.vars, "SINGLESTEP"))
	{
		char		buf[3];

		printf(gettext
			   ("***"
				"(Single step mode: Verify query)"
				"*********************************************\n"
				"%s\n"
				"***"
			  "(press return to proceed or enter x and return to cancel)"
				"********************\n"),
			   query);
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) != NULL)
			if (buf[0] == 'x')
				return false;
	}
	else
	{
		const char *var = GetVariable(pset.vars, "ECHO");

		if (var && strncmp(var, "queries", strlen(var)) == 0)
			puts(query);
	}

	cancelConn = pset.db;

#ifndef WIN32
	if (pset.timing)
		gettimeofday(&before, NULL);
	results = PQexec(pset.db, query);
	if (pset.timing)
		gettimeofday(&after, NULL);
#else
	if (pset.timing)
		_ftime(&before);
	results = PQexec(pset.db, query);
	if (pset.timing)
		_ftime(&after);
#endif

	if (PQresultStatus(results) == PGRES_COPY_IN)
		copy_in_state = true;
	/* keep cancel connection for copy out state */
	if (PQresultStatus(results) != PGRES_COPY_OUT)
		cancelConn = NULL;

	if (results == NULL)
	{
		fputs(PQerrorMessage(pset.db), pset.queryFout);
		success = false;
	}
	else
	{
		switch (PQresultStatus(results))
		{
			case PGRES_TUPLES_OK:
				/* write output to \g argument, if any */
				if (pset.gfname)
				{
					FILE	   *queryFout_copy = pset.queryFout;
					bool		queryFoutPipe_copy = pset.queryFoutPipe;

					pset.queryFout = stdout;	/* so it doesn't get
												 * closed */

					/* open file/pipe */
					if (!setQFout(pset.gfname))
					{
						pset.queryFout = queryFout_copy;
						pset.queryFoutPipe = queryFoutPipe_copy;
						success = false;
						break;
					}

					printQuery(results, &pset.popt, pset.queryFout, NULL);

					/* close file/pipe, restore old setting */
					setQFout(NULL);

					pset.queryFout = queryFout_copy;
					pset.queryFoutPipe = queryFoutPipe_copy;

					free(pset.gfname);
					pset.gfname = NULL;

					success = true;
				}
				else
				{
					printQuery(results, &pset.popt, pset.queryFout, NULL);
					success = true;
				}
				break;
			case PGRES_EMPTY_QUERY:
				success = true;
				break;
			case PGRES_COMMAND_OK:
				{
					char		buf[10];

					success = true;
					sprintf(buf, "%u", (unsigned int) PQoidValue(results));
					if (!QUIET())
						fprintf(pset.queryFout, "%s\n", PQcmdStatus(results));
					SetVariable(pset.vars, "LASTOID", buf);
					break;
				}
			case PGRES_COPY_OUT:
				success = handleCopyOut(pset.db, pset.queryFout);
				break;

			case PGRES_COPY_IN:
				if (pset.cur_cmd_interactive && !QUIET())
					puts(gettext
					  ("Enter data to be copied followed by a newline.\n"
					   "End with a backslash and a period on a line by itself."));

				success = handleCopyIn(pset.db, pset.cur_cmd_source,
									   pset.
									   cur_cmd_interactive ?
									   get_prompt(PROMPT_COPY) : NULL);
				break;

			case PGRES_NONFATAL_ERROR:
			case PGRES_FATAL_ERROR:
			case PGRES_BAD_RESPONSE:
				success = false;
				psql_error("%s", PQerrorMessage(pset.db));
				break;

            default:
                /* This should be unreachable. */
                psql_error("Something has gone horribly wrong.");
                break;
		}

		fflush(pset.queryFout);

		if (PQstatus(pset.db) == CONNECTION_BAD)
		{
			if (!pset.cur_cmd_interactive)
			{
				psql_error("connection to server was lost\n");
				exit(EXIT_BADCONN);
			}
			fputs(gettext
			("The connection to the server was lost. Attempting reset: "),
				  stderr);
			PQreset(pset.db);
			if (PQstatus(pset.db) == CONNECTION_BAD)
			{
				fputs(gettext("Failed.\n"), stderr);
				PQfinish(pset.db);
				PQclear(results);
				pset.db = NULL;
				SetVariable(pset.vars, "DBNAME", NULL);
				SetVariable(pset.vars, "HOST", NULL);
				SetVariable(pset.vars, "PORT", NULL);
				SetVariable(pset.vars, "USER", NULL);
				SetVariable(pset.vars, "ENCODING", NULL);
				return false;
			}
			else
				fputs(gettext("Succeeded.\n"), stderr);
		}

		/* check for asynchronous notification returns */
		while ((notify = PQnotifies(pset.db)) != NULL)
		{
			fprintf(pset.queryFout,
					gettext
					("Asynchronous NOTIFY '%s' from backend with pid %d received.\n"),
					notify->relname, notify->be_pid);
			free(notify);
			fflush(pset.queryFout);
		}

		if (results)
			PQclear(results);
	}

	/* Possible microtiming output */
	if (pset.timing && success)
#ifndef WIN32
		printf(gettext("Time: %.2f ms\n"),
			   ((after.tv_sec - before.tv_sec) * 1000000.0 + after.tv_usec - before.tv_usec) / 1000.0);
#else
		printf(gettext("Time: %.2f ms\n"),
			   ((after.time - before.time) * 1000.0 + after.millitm - before.millitm));
#endif

	return success;
}


/*
 * PageOutput
 *
 * Tests if pager is needed and returns appropriate FILE pointer.
 */
FILE *
PageOutput(int lines, bool pager)
{
	/* check whether we need / can / are supposed to use pager */
	if (pager
#ifndef WIN32
		&&
		isatty(fileno(stdin)) &&
		isatty(fileno(stdout))
#endif
		)
	{
		const char *pagerprog;

#ifdef TIOCGWINSZ
		int			result;
		struct winsize screen_size;

		result = ioctl(fileno(stdout), TIOCGWINSZ, &screen_size);
		if (result == -1 || lines > screen_size.ws_row)
		{
#endif
			pagerprog = getenv("PAGER");
			if (!pagerprog)
				pagerprog = DEFAULT_PAGER;
#ifndef WIN32
			pqsignal(SIGPIPE, SIG_IGN);
#endif
			return popen(pagerprog, "w");
#ifdef TIOCGWINSZ
		}
#endif
	}

	return stdout;
}

static bool isCQQuery(char * tmp)
{
  bool res = false;

  if(strncasecmp(tmp, "select", 6) == 0 || strncasecmp(tmp, "SELECT", 6) == 0)
    res=true;
  if(strncasecmp(tmp, "with",4) == 0 || strncasecmp(tmp, "WITH", 4)==0)
    res=true;
  return res;
}
