/*-------------------------------------------------------------------------
 *
 * prepare.h
 *	  PREPARE, EXECUTE and DEALLOCATE command prototypes
 *
 *
 * Copyright (c) 2002, PostgreSQL Global Development Group
 *
 * $Id: prepare.h,v 1.1.1.1 2003/04/17 23:02:14 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PREPARE_H
#define PREPARE_H

#include "nodes/parsenodes.h"
#include "tcop/dest.h"


extern void PrepareQuery(PrepareStmt *stmt);

extern void ExecuteQuery(ExecuteStmt *stmt, CommandDest outputDest);

extern void DeallocateQuery(DeallocateStmt *stmt);

extern List *FetchQueryParams(const char *plan_name);

#endif   /* PREPARE_H */
