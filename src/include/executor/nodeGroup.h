/*-------------------------------------------------------------------------
 *
 * nodeGroup.h
 *	  prototypes for nodeGroup.c
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: nodeGroup.h,v 1.1.1.2 2003/04/17 23:02:15 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEGROUP_H
#define NODEGROUP_H

#include "nodes/plannodes.h"

extern TupleTableSlot *ExecGroup(Group *node);
extern bool ExecInitGroup(Group *node, EState *estate, Plan *parent);
extern int	ExecCountSlotsGroup(Group *node);
extern void ExecEndGroup(Group *node);
extern void ExecReScanGroup(Group *node, ExprContext *exprCtxt, Plan *parent);

extern bool execTuplesMatch(HeapTuple tuple1,
				HeapTuple tuple2,
				TupleDesc tupdesc,
				int numCols,
				AttrNumber *matchColIdx,
				FmgrInfo *eqfunctions,
				MemoryContext evalContext);
extern FmgrInfo *execTuplesMatchPrepare(TupleDesc tupdesc,
					   int numCols,
					   AttrNumber *matchColIdx);

#endif   /* NODEGROUP_H */
