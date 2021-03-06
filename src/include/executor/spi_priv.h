/*-------------------------------------------------------------------------
 *
 * spi_priv.h
 *				Server Programming Interface private declarations
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: spi_priv.h,v 1.2 2003/07/16 09:42:51 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef SPI_PRIV_H
#define SPI_PRIV_H

#include "executor/spi.h"


typedef struct
{
	uint32		processed;		/* by Executor */
	SPITupleTable *tuptable;
	MemoryContext procCxt;		/* procedure context */
	MemoryContext execCxt;		/* executor context */
	MemoryContext savedcxt;
} _SPI_connection;

typedef struct
{
	/*
	 * context containing _SPI_plan itself as well as subsidiary
	 * structures
	 */
	MemoryContext plancxt;
	/* List of List of querytrees; one sublist per original parsetree */
	List	   *qtlist;
	/* List of plan trees --- length == # of querytrees, but flat list */
	List	   *ptlist;
	/* Argument types, if a prepared plan */
	int			nargs;
	Oid		   *argtypes;
	/* Command type of last original parsetree */
	CmdType		origCmdType;
} _SPI_plan;


#define _SPI_CPLAN_CURCXT	0
#define _SPI_CPLAN_PROCXT	1
#define _SPI_CPLAN_TOPCXT	2

#endif   /* SPI_PRIV_H */
