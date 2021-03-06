/*-------------------------------------------------------------------------
 *
 * ipc.c
 *	  POSTGRES inter-process communication definitions.
 *
 * This file is misnamed, as it no longer has much of anything directly
 * to do with IPC.	The functionality here is concerned with managing
 * exit-time cleanup for either a postmaster or a backend.
 *
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/storage/ipc/ipc.c,v 1.18 2004/10/04 22:49:05 phred Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"


#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "miscadmin.h"
#include "storage/ipc.h"





/*
 * This flag is set during proc_exit() to change elog()'s behavior,
 * so that an elog() from an on_proc_exit routine cannot get us out
 * of the exit procedure.  We do NOT want to go back to the idle loop...
 */
bool		proc_exit_inprogress = false;


/* ----------------------------------------------------------------
 *						exit() handling stuff
 *
 * These functions are in generally the same spirit as atexit(2),
 * but provide some additional features we need --- in particular,
 * we want to register callbacks to invoke when we are disconnecting
 * from a broken shared-memory context but not exiting the postmaster.
 *
 * Callback functions can take zero, one, or two args: the first passed
 * arg is the integer exitcode, the second is the Datum supplied when
 * the callback was registered.
 * ----------------------------------------------------------------
 */

#define MAX_ON_EXITS 20

static struct ONEXIT
{
	void		(*function) ();
	Datum		arg;
}	on_proc_exit_list[MAX_ON_EXITS], on_shmem_exit_list[MAX_ON_EXITS];

static int	on_proc_exit_index,
			on_shmem_exit_index;


/* ----------------------------------------------------------------
 *		proc_exit
 *
 *		this function calls all the callbacks registered
 *		for it (to free resources) and then calls exit.
 *		This should be the only function to call exit().
 *		-cim 2/6/90
 * ----------------------------------------------------------------
 */
void
proc_exit(int code)
{
	/*
	 * Once we set this flag, we are committed to exit.  Any elog() will
	 * NOT send control back to the main loop, but right back here.
	 */
	proc_exit_inprogress = true;

	if (MyProcessType == PROC_TCQ_FRONTEND &&
		cqcancelid > -1)
	{
		elog(NOTICE, "TCQ frontend exiting while query %d is still active.  canceling it now", cqcancelid);
		AbortCurrentTransaction();
	}


	/*
	 * Forget any pending cancel or die requests; we're doing our best to
	 * close up shop already.  Note that the signal handlers will not set
	 * these flags again, now that proc_exit_inprogress is set.
	 */
	InterruptPending = false;
	ProcDiePending = false;
	QueryCancelPending = false;
	/* And let's just make *sure* we're not interrupted ... */
	ImmediateInterruptOK = false;
	InterruptHoldoffCount = 1;
	CritSectionCount = 0;

	/* do our shared memory exits first */
	shmem_exit(code);

	/*
	 * call all the callbacks registered before calling exit().
	 *
	 * Note that since we decrement on_proc_exit_index each time, if a
	 * callback calls elog(ERROR) or elog(FATAL) then it won't be invoked
	 * again when control comes back here (nor will the
	 * previously-completed callbacks).  So, an infinite loop should not
	 * be possible.
	 */
	while (--on_proc_exit_index >= 0)
		(*on_proc_exit_list[on_proc_exit_index].function) (code,
							  on_proc_exit_list[on_proc_exit_index].arg);

	if (code > 0)				/* @BdtcqSK */
	{
		/* Where did the error occur? */
		elog(LOG,"Calling proc_exit with error code %d; dumping stack.", code);
		TraceDump();
	}							/* @EdtcqSK */
	else 
	{
		/*TraceDump();*/
	}
	
	elog(DEBUG2, "exit(%d) (proctype=%d)", code, MyProcessType);
	exit(code);
}

/* ------------------
 * Run all of the on_shmem_exit routines --- but don't actually exit.
 * This is used by the postmaster to re-initialize shared memory and
 * semaphores after a backend dies horribly.
 * ------------------
 */
void
shmem_exit(int code)
{
	elog(DEBUG2, "shmem_exit(%d)  proctype=%d", code, MyProcessType);

	/*
	 * call all the registered callbacks.
	 *
	 * As with proc_exit(), we remove each callback from the list before
	 * calling it, to avoid infinite loop in case of error.
	 */
	while (--on_shmem_exit_index >= 0)
		(*on_shmem_exit_list[on_shmem_exit_index].function) (code,
							on_shmem_exit_list[on_shmem_exit_index].arg);

	on_shmem_exit_index = 0;
}

/* ----------------------------------------------------------------
 *		on_proc_exit
 *
 *		this function adds a callback function to the list of
 *		functions invoked by proc_exit().	-cim 2/6/90
 * ----------------------------------------------------------------
 */
void
			on_proc_exit(void (*function) (), Datum arg)
{
	if (on_proc_exit_index >= MAX_ON_EXITS)
		elog(FATAL, "Out of on_proc_exit slots");

	on_proc_exit_list[on_proc_exit_index].function = function;
	on_proc_exit_list[on_proc_exit_index].arg = arg;

	++on_proc_exit_index;
}

/* ----------------------------------------------------------------
 *		on_shmem_exit
 *
 *		this function adds a callback function to the list of
 *		functions invoked by shmem_exit().	-cim 2/6/90
 * ----------------------------------------------------------------
 */
void
			on_shmem_exit(void (*function) (), Datum arg)
{
	if (on_shmem_exit_index >= MAX_ON_EXITS)
		elog(FATAL, "Out of on_shmem_exit slots");

	on_shmem_exit_list[on_shmem_exit_index].function = function;
	on_shmem_exit_list[on_shmem_exit_index].arg = arg;

	++on_shmem_exit_index;
}

/* ----------------------------------------------------------------
 *		on_exit_reset
 *
 *		this function clears all on_proc_exit() and on_shmem_exit()
 *		registered functions.  This is used just after forking a backend,
 *		so that the backend doesn't believe it should call the postmaster's
 *		on-exit routines when it exits...
 * ----------------------------------------------------------------
 */
void
on_exit_reset(void)
{
	on_shmem_exit_index = 0;
	on_proc_exit_index = 0;
}
