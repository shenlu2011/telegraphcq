/*-------------------------------------------------------------------------
 *
 * funcapi.c
 *	  Utility and convenience functions for fmgr functions that return
 *	  sets and/or composite types.
 *
 * Copyright (c) 2002, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/utils/fmgr/funcapi.c,v 1.2 2004/03/24 08:11:14 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "catalog/pg_type.h"
#include "utils/syscache.h"


/*
 * init_MultiFuncCall
 * Create an empty FuncCallContext data structure
 * and do some other basic Multi-function call setup
 * and error checking
 */
FuncCallContext *
init_MultiFuncCall(PG_FUNCTION_ARGS)
{
	FuncCallContext *retval;

	/*
	 * Bail if we're called in the wrong context
	 */
	if (fcinfo->resultinfo == NULL || !IsA(fcinfo->resultinfo, ReturnSetInfo))
		elog(ERROR, "function called in context that does not accept a set result");

	if (fcinfo->flinfo->fn_extra == NULL)
	{
		/*
		 * First call
		 *
		 * Allocate suitably long-lived space and zero it
		 */
		retval = (FuncCallContext *)
			MemoryContextAlloc(fcinfo->flinfo->fn_mcxt,
							   sizeof(FuncCallContext));
		MemSet(retval, 0, sizeof(FuncCallContext));

		/*
		 * initialize the elements
		 */
		retval->call_cntr = 0;
		retval->max_calls = 0;
		retval->slot = NULL;
		retval->user_fctx = NULL;
		retval->attinmeta = NULL;
		retval->multi_call_memory_ctx = fcinfo->flinfo->fn_mcxt;

		/*
		 * save the pointer for cross-call use
		 */
		fcinfo->flinfo->fn_extra = retval;
	}
	else
/* second and subsequent calls */
	{
		elog(ERROR, "init_MultiFuncCall may not be called more than once");

		/* never reached, but keep compiler happy */
		retval = NULL;
	}

	return retval;
}

/*
 * per_MultiFuncCall
 *
 * Do Multi-function per-call setup
 */
FuncCallContext *
per_MultiFuncCall(PG_FUNCTION_ARGS)
{
	FuncCallContext *retval = (FuncCallContext *) fcinfo->flinfo->fn_extra;

	/*
	 * Clear the TupleTableSlot, if present.  This is for safety's sake:
	 * the Slot will be in a long-lived context (it better be, if the
	 * FuncCallContext is pointing to it), but in most usage patterns the
	 * tuples stored in it will be in the function's per-tuple context. So
	 * at the beginning of each call, the Slot will hold a dangling
	 * pointer to an already-recycled tuple.  We clear it out here.  (See
	 * also the definition of TupleGetDatum() in funcapi.h!)
	 */
	if (retval->slot != NULL)
		ExecClearTuple(retval->slot);

	return retval;
}

/*
 * end_MultiFuncCall
 * Clean up after init_MultiFuncCall
 */
void
end_MultiFuncCall(PG_FUNCTION_ARGS, FuncCallContext *funcctx)
{
	/* unbind from fcinfo */
	fcinfo->flinfo->fn_extra = NULL;

	/*
	 * Caller is responsible to free up memory for individual struct
	 * elements other than att_in_funcinfo and elements.
	 */
	if (funcctx->attinmeta != NULL)
		pfree(funcctx->attinmeta);

	pfree(funcctx);
}
