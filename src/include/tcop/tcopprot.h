/*-------------------------------------------------------------------------
 *
 * tcopprot.h
 *	  prototypes for postgres.c.
 *
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1996-2001, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: tcopprot.h,v 1.10 2003/07/17 05:18:50 sailesh Exp $
 *
 * OLD COMMENTS
 *	  This file was created so that other c files could get the two
 *	  function prototypes without having to include tcop.h which single
 *	  handedly includes the whole f*cking tree -- mer 5 Nov. 1991
 *
 *-------------------------------------------------------------------------
 */
#ifndef TCOPPROT_H
#define TCOPPROT_H

#include <setjmp.h>

#include "executor/execdesc.h"
#include "lib/stringinfo.h"
#include "tcop/dest.h"



extern DLLIMPORT sigjmp_buf Warn_restart;
extern bool Warn_restart_ready;
extern bool InError;
extern CommandDest whereToSendOutput;
extern bool HostnameLookup;
extern bool ShowPortNumber;
extern DLLIMPORT char *debug_query_string;

#ifndef BOOTSTRAP_INCLUDE

extern List *pg_parse_query(StringInfo query_string, Oid *typev, int nargs);
extern List *pg_analyze_and_rewrite(Node *parsetree);
extern List *pg_parse_and_rewrite(char *query_string,
					 Oid *typev, int nargs);
extern Plan *pg_plan_query(Query *querytree, MemoryContext ctx);
extern void pg_exec_query_string(StringInfo query_string,
					 CommandDest dest,
					 MemoryContext parse_context);
#endif   /* BOOTSTRAP_INCLUDE */

extern void die(SIGNAL_ARGS);
extern void quickdie(SIGNAL_ARGS);
extern void authdie(SIGNAL_ARGS);
extern int	PostgresMain(int argc, char *argv[], const char *username);
extern void ResetUsage(void);
extern void ShowUsage(const char *title);

#endif   /* TCOPPROT_H */
