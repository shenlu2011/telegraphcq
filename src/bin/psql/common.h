/*
 * psql - the PostgreSQL interactive terminal
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Copyright 2000 by PostgreSQL Global Development Group
 *
 * $Header: /project/eecs/db/cvsroot/postgres/src/bin/psql/common.h,v 1.6 2004/03/24 08:11:26 chungwu Exp $
 */
#ifndef COMMON_H
#define COMMON_H

#include "postgres_fe.h"
#include <signal.h>
#include "pqsignal.h"
#include "libpq-fe.h"

extern char *xstrdup(const char *string);

extern bool setQFout(const char *fname);

extern void
psql_error(const char *fmt,...)
/* This lets gcc check the format string for consistency. */
__attribute__((format(printf, 1, 2)));

extern void NoticeProcessor(void *arg, const char *message);

extern char *simple_prompt(const char *prompt, int maxlen, bool echo);

extern volatile bool cancel_pressed;
extern PGconn *cancelConn;

#ifndef WIN32
extern void handle_sigint(SIGNAL_ARGS);
#endif   /* not WIN32 */

extern PGresult *PSQLexec(const char *query, bool ignore_command_ok);

extern bool SendQuery(const char *query);
extern bool SendQueryCQ(char *query);
extern bool cqcancel;

extern FILE *PageOutput(int lines, bool pager);

/* sprompt.h */
extern char *simple_prompt(const char *prompt, int maxlen, bool echo);

/* Used for all Win32 popen/pclose calls */
#ifdef WIN32
#define popen(x,y) _popen(x,y)
#define pclose(x) _pclose(x)
#endif

#endif   /* COMMON_H */
