/*-------------------------------------------------------------------------
 *
 * spin.c
 *	   Hardware-independent implementation of spinlocks.
 *
 *
 * For machines that have test-and-set (TAS) instructions, s_lock.h/.c
 * define the spinlock implementation.	This file contains only a stub
 * implementation for spinlocks using PGSemaphores.  Unless semaphores
 * are implemented in a way that doesn't involve a kernel call, this
 * is too slow to be very useful :-(
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/storage/lmgr/spin.c,v 1.2 2004/03/24 08:11:09 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "storage/lwlock.h"
#include "storage/pg_sema.h"
#include "storage/spin.h"


#ifdef HAS_TEST_AND_SET

/*
 * Report number of semaphores needed to support spinlocks.
 */
int
SpinlockSemas(void)
{
	return 0;
}

#else							/* !HAS_TEST_AND_SET */

/*
 * No TAS, so spinlocks are implemented as PGSemaphores.
 */


/*
 * Report number of semaphores needed to support spinlocks.
 */
int
SpinlockSemas(void)
{
	/*
	 * It would be cleaner to distribute this logic into the affected
	 * modules, similar to the way shmem space estimation is handled.
	 *
	 * For now, though, we just need a few spinlocks (10 should be plenty)
	 * plus one for each LWLock.
	 */
	return NumLWLocks() + 10;
}

/*
 * s_lock.h hardware-spinlock emulation
 */

void
s_init_lock_sema(volatile slock_t *lock)
{
	PGSemaphoreCreate((PGSemaphore) lock);
}

void
s_unlock_sema(volatile slock_t *lock)
{
	PGSemaphoreUnlock((PGSemaphore) lock);
}

bool
s_lock_free_sema(volatile slock_t *lock)
{
	/* We don't currently use S_LOCK_FREE anyway */
	elog(ERROR, "spin.c does not support S_LOCK_FREE()");
	return false;
}

int
tas_sema(volatile slock_t *lock)
{
	/* Note that TAS macros return 0 if *success* */
	return !PGSemaphoreTryLock((PGSemaphore) lock);
}

#endif   /* !HAS_TEST_AND_SET */
