/*-------------------------------------------------------------------------
 *
 * nodeIndexscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: nodeIndexscan.h,v 1.1.1.2 2003/04/17 23:02:15 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEINDEXSCAN_H
#define NODEINDEXSCAN_H

#include "nodes/plannodes.h"

extern TupleTableSlot *ExecIndexScan(IndexScan *node);
extern void ExecIndexReScan(IndexScan *node, ExprContext *exprCtxt, Plan *parent);
extern void ExecEndIndexScan(IndexScan *node);
extern void ExecIndexMarkPos(IndexScan *node);
extern void ExecIndexRestrPos(IndexScan *node);
extern void ExecUpdateIndexScanKeys(IndexScan *node, ExprContext *econtext);
extern bool ExecInitIndexScan(IndexScan *node, EState *estate, Plan *parent);
extern int	ExecCountSlotsIndexScan(IndexScan *node);

#endif   /* NODEINDEXSCAN_H */
