/*-------------------------------------------------------------------------
 *
 * nodeHashjoin.h
 *
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: nodeHashjoin.h,v 1.1.1.2 2003/04/17 23:02:15 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEHASHJOIN_H
#define NODEHASHJOIN_H

#include "nodes/plannodes.h"

extern TupleTableSlot *ExecHashJoin(HashJoin *node);
extern bool ExecInitHashJoin(HashJoin *node, EState *estate, Plan *parent);
extern int	ExecCountSlotsHashJoin(HashJoin *node);
extern void ExecEndHashJoin(HashJoin *node);
extern void ExecHashJoinSaveTuple(HeapTuple heapTuple, BufFile *file);
extern void ExecReScanHashJoin(HashJoin *node, ExprContext *exprCtxt, Plan *parent);

#endif   /* NODEHASHJOIN_H */
