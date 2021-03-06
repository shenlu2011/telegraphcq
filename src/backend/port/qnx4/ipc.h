/*-------------------------------------------------------------------------
 *
 * ipc.h
 *	  System V IPC Emulation
 *
 * Copyright (c) 1999, repas AEG Automation GmbH
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/port/qnx4/ipc.h,v 1.2 2004/03/24 08:11:05 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef _SYS_IPC_H
#define _SYS_IPC_H

/* Common IPC definitions. */
/* Mode bits. */
#define IPC_CREAT	0001000		/* create entry if key doesn't exist */
#define IPC_EXCL	0002000		/* fail if key exists */
#define IPC_NOWAIT	0004000		/* error if request must wait */

/* Keys. */
#define IPC_PRIVATE (key_t)0	/* private key */

/* Control Commands. */
#define IPC_RMID	0			/* remove identifier */
#define IPC_STAT	1			/* get shm status */

#endif   /* _SYS_IPC_H */
