/*-------------------------------------------------------------------------
 *
 * pquery.h
 *	  prototypes for pquery.c.
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: pquery.h,v 1.1.1.2 2003/04/17 23:02:46 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQUERY_H
#define PQUERY_H

#include "executor/execdesc.h"
#include "utils/portal.h"


extern void ProcessQuery(Query *parsetree, Plan *plan, CommandDest dest,
			 char *completionTag);

extern EState *CreateExecutorState(void);

extern Portal PreparePortal(char *portalName);

#endif   /* PQUERY_H */
