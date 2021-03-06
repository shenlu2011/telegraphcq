/*-------------------------------------------------------------------------
 *
 * nodeSeqscan.c
 *	  Support routines for sequential scans of relations.
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/executor/nodeSeqscan.c,v 1.16 2004/10/15 02:07:37 phred Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecSeqScan				sequentially scans a relation.
 *		ExecSeqNext				retrieve next tuple in sequential order.
 *		ExecInitSeqScan			creates and initializes a seqscan node.
 *		ExecEndSeqScan			releases any storage allocated.
 *		ExecSeqReScan			rescans the relation
 *		ExecMarkPos				marks scan position
 *		ExecRestrPos			restores scan position
 *
 */
#include "postgres.h"
#include "miscadmin.h"

#include "access/heapam.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"
#include "parser/parsetree.h"

static Oid InitScanRelation(SeqScan *node, EState *estate,
				 CommonScanState *scanstate);
static TupleTableSlot *SeqNext(SeqScan *node);

/* ----------------------------------------------------------------
 *						Scan Support
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *		SeqNext
 *
 *		This is a workhorse for ExecSeqScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
SeqNext(SeqScan *node)
{
	HeapTuple	tuple;
	HeapScanDesc scandesc;
	CommonScanState *scanstate;
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;

	/*
	 * get information from the estate and scan state
	 */
	estate = node->plan.state;
	scanstate = node->scanstate;
	scandesc = scanstate->css_currentScanDesc;
	direction = estate->es_direction;
	slot = scanstate->css_ScanTupleSlot;

	/*
	 * Check if we are evaluating PlanQual for tuple of this relation.
	 * Additional checking is not good, but no other way for now. We could
	 * introduce new nodes for this case and handle SeqScan --> NewNode
	 * switching in Init/ReScan plan...
	 */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[node->scanrelid - 1] != NULL)
	{
		ExecClearTuple(slot);
		if (estate->es_evTupleNull[node->scanrelid - 1])
			return slot;		/* return empty slot */

		ExecStoreTuple(estate->es_evTuple[node->scanrelid - 1],
					   slot, InvalidBuffer, false);

		/*
		 * Note that unlike IndexScan, SeqScan never use keys in
		 * heap_beginscan (and this is very bad) - so, here we do not
		 * check are keys ok or not.
		 */

		/* Flag for the next call that no more tuples */
		estate->es_evTupleNull[node->scanrelid - 1] = true;
		return (slot);
	}

	/*
	 * get the next tuple from the access methods
	 */
	tuple = heap_getnext(scandesc, direction);

	if (! HeapTupleIsValid(tuple)) 
	{
		if (! IsAStream(scandesc->rs_rd->rd_rel->relkind)) 
		{
			elog(LOG, "Oops, fetched a NULL from table '%s'",
                    scandesc->rs_rd->rd_rel->relname.data);
		}
	}
	

	/*
	 * save the tuple and the buffer returned to us by the access methods
	 * in our scan tuple slot and return the slot.	Note: we pass 'false'
	 * because tuples returned by heap_getnext() are pointers onto disk
	 * pages and were not created with palloc() and so should not be
	 * pfree()'d.  Note also that ExecStoreTuple will increment the
	 * refcount of the buffer; the refcount will not be dropped until the
	 * tuple table slot is cleared.
	 */

	slot = ExecStoreTuple(tuple,	/* tuple to store */
						  slot, /* slot to store in */
						  scandesc->rs_cbuf,	/* buffer associated with
												 * this tuple */
						  false);		/* don't pfree this pointer */

	return slot;
}

/* ----------------------------------------------------------------
 *		ExecSeqScan(node)
 *
 *		Scans the relation sequentially and returns the next qualifying
 *		tuple.
 *		It calls the ExecScan() routine and passes it the access method
 *		which retrieve tuples sequentially.
 *
 */

TupleTableSlot *
ExecSeqScan(SeqScan *node)
{
	/*
	 * use SeqNext as access method
	 */
	return ExecScan(node, (ExecScanAccessMtd) SeqNext);
}

/* ----------------------------------------------------------------
 *		InitScanRelation
 *
 *		This does the initialization for scan relations and
 *		subplans of scans.
 * ----------------------------------------------------------------
 */
static Oid
InitScanRelation(SeqScan *node, EState *estate,
				 CommonScanState *scanstate)
{
	Index		relid;
	List	   *rangeTable;
	RangeTblEntry *rtentry;
	Oid			reloid;
	Relation	currentRelation;
	HeapScanDesc currentScanDesc;

	/*
	 * get the relation object id from the relid'th entry in the range
	 * table, open that relation and initialize the scan state.
	 *
	 * We acquire AccessShareLock for the duration of the scan.
	 */
	relid = node->scanrelid;
	rangeTable = estate->es_range_table;
	rtentry = rt_fetch(relid, rangeTable);
	reloid = rtentry->relid;

	currentRelation = heap_open(reloid, AccessShareLock);

	currentScanDesc = heap_beginscan(currentRelation,
									 estate->es_snapshot,
									 0,
									 NULL);

	scanstate->css_currentRelation = currentRelation;
	scanstate->css_currentScanDesc = currentScanDesc;

	ExecAssignScanType(scanstate, RelationGetDescr(currentRelation), false);

	return reloid;
}


/* ----------------------------------------------------------------
 *		ExecInitSeqScan
 * ----------------------------------------------------------------
 */
bool
ExecInitSeqScan(SeqScan *node, EState *estate, Plan *parent)
{
	CommonScanState *scanstate;
	Oid			reloid;

	/*
	 * Once upon a time it was possible to have an outerPlan of a SeqScan,
	 * but not any more.
	 */
	Assert(outerPlan((Plan *) node) == NULL);
	Assert(innerPlan((Plan *) node) == NULL);

	/*
	 * assign the node's execution state
	 */
	node->plan.state = estate;

	/*
	 * create new CommonScanState for node
	 */
	scanstate = makeNode(CommonScanState);
	node->scanstate = scanstate;

    /** This is used for deciding whether a NULL response implies 
     *  finish or not. */
    node->scanstate->really_done = false;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &scanstate->cstate);

#define SEQSCAN_NSLOTS 2

	/*
	 * tuple table initialization
	 */
#ifndef DISABLE_CQ_SUPPORT
	if (!ExecUseNewSlots((Plan *) node, parent))
	{
		ExecInitResultTupleSlot(estate, &scanstate->cstate);
		ExecInitScanTupleSlot(estate, scanstate);
	}
	else
	{
		ExecInitNewResultTupleSlot(estate, &scanstate->cstate);
		ExecInitNewScanTupleSlot(estate, scanstate);
		InitSlotForEddy(scanstate->cstate.cs_ResultTupleSlot);
		InitSlotForEddy(scanstate->css_ScanTupleSlot);
	}
#else
	ExecInitResultTupleSlot(estate, &scanstate->cstate);
	ExecInitScanTupleSlot(estate, scanstate);
#endif

	/*
	 * initialize scan relation
	 */
	reloid = InitScanRelation(node, estate, scanstate);

	scanstate->cstate.cs_TupFromTlist = false;

	/*
	 * initialize tuple type
	 */


	if (!node->dontProject)
	{
		ExecAssignResultTypeFromTL((Plan *) node, &scanstate->cstate);
		ExecAssignProjectionInfo((Plan *) node, &scanstate->cstate);
	}
	else
	{
		/* note... we are in trouble here for postgres plans because the  */
		/* tlist of the scan refers to the projected tuple, not the  */
		/* base tuple. */
		Assert(UseCQEddy);
		ExecAssignResultType(&scanstate->cstate, CreateTupleDescCopyConstr(scanstate->css_ScanTupleSlot->ttc_tupleDescriptor), true);
		ExecAssignProjectionInfo((Plan *) node, &scanstate->cstate);
	}


/*	printf("XXXX %d\n", scanstate->cstate.cs_ResultTupleSlot->ttc_tupleDescriptor->natts); */

	return TRUE;
}

int
ExecCountSlotsSeqScan(SeqScan *node)
{
	return ExecCountSlotsNode(outerPlan(node)) +
		ExecCountSlotsNode(innerPlan(node)) +
		SEQSCAN_NSLOTS;
}

/* ----------------------------------------------------------------
 *		ExecEndSeqScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndSeqScan(SeqScan *node)
{
	CommonScanState *scanstate;
	Relation	relation;
	HeapScanDesc scanDesc;

	/*
	 * get information from node
	 */
	scanstate = node->scanstate;
	relation = scanstate->css_currentRelation;
	scanDesc = scanstate->css_currentScanDesc;

	/*
	 * Free the projection info and the scan attribute info
	 *
	 * Note: we don't ExecFreeResultType(scanstate) because the rule manager
	 * depends on the tupType returned by ExecMain().  So for now, this is
	 * freed at end-transaction time.  -cim 6/2/91
	 */
	ExecFreeProjectionInfo(&scanstate->cstate);
	ExecFreeExprContext(&scanstate->cstate);

	/*
	 * close heap scan
	 */
	heap_endscan(scanDesc);

	/*
	 * close the heap relation.
	 *
	 * Currently, we do not release the AccessShareLock acquired by
	 * InitScanRelation.  This lock should be held till end of
	 * transaction. (There is a faction that considers this too much
	 * locking, however.)
	 */
	heap_close(relation, NoLock);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(scanstate->cstate.cs_ResultTupleSlot);
	ExecClearTuple(scanstate->css_ScanTupleSlot);
	ExecFreeSlotIfOwned(scanstate->cstate.cs_ResultTupleSlot);
	ExecFreeSlotIfOwned(scanstate->css_ScanTupleSlot);

}

/* ----------------------------------------------------------------
 *						Join Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecSeqReScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecSeqReScan(SeqScan *node, ExprContext *exprCtxt, Plan *parent)
{
	CommonScanState *scanstate;
	EState	   *estate;
	HeapScanDesc scan;

	scanstate = node->scanstate;
	estate = node->plan.state;

	/* If this is re-scanning of PlanQual ... */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[node->scanrelid - 1] != NULL)
	{
		estate->es_evTupleNull[node->scanrelid - 1] = false;
		return;
	}

	scan = scanstate->css_currentScanDesc;

	heap_rescan(scan,			/* scan desc */
				NULL);			/* new scan keys */
}

/* ----------------------------------------------------------------
 *		ExecSeqMarkPos(node)
 *
 *		Marks scan position.
 * ----------------------------------------------------------------
 */
void
ExecSeqMarkPos(SeqScan *node)
{
	CommonScanState *scanstate;
	HeapScanDesc scan;

	scanstate = node->scanstate;
	scan = scanstate->css_currentScanDesc;
	heap_markpos(scan);
}

/* ----------------------------------------------------------------
 *		ExecSeqRestrPos
 *
 *		Restores scan position.
 * ----------------------------------------------------------------
 */
void
ExecSeqRestrPos(SeqScan *node)
{
	CommonScanState *scanstate;
	HeapScanDesc scan;

	scanstate = node->scanstate;
	scan = scanstate->css_currentScanDesc;
	heap_restrpos(scan);
}
