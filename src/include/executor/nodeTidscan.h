/*-------------------------------------------------------------------------
 *
 * nodeTidscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: nodeTidscan.h,v 1.1.1.2 2003/04/17 23:02:16 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODETIDSCAN_H
#define NODETIDSCAN_H

#include "nodes/plannodes.h"

extern TupleTableSlot *ExecTidScan(TidScan *node);
extern void ExecTidReScan(TidScan *node, ExprContext *exprCtxt, Plan *parent);
extern void ExecEndTidScan(TidScan *node);
extern void ExecTidMarkPos(TidScan *node);
extern bool ExecInitTidScan(TidScan *node, EState *estate, Plan *parent);
extern int	ExecCountSlotsTidScan(TidScan *node);
extern void ExecTidReScan(TidScan *node, ExprContext *exprCtxt, Plan *parent);

#endif   /* NODETIDSCAN_H */
