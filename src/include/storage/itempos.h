/*-------------------------------------------------------------------------
 *
 * itempos.h
 *	  Standard POSTGRES buffer page long item subposition definitions.
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: itempos.h,v 1.1.1.2 2003/04/17 23:02:44 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef ITEMPOS_H
#define ITEMPOS_H

#include "storage/buf.h"
#include "storage/itemid.h"

typedef struct ItemSubpositionData
{
	Buffer		op_db;
	ItemId		op_lpp;
	char	   *op_cp;			/* XXX */
	uint32		op_len;
}	ItemSubpositionData;

typedef ItemSubpositionData *ItemSubposition;

/*
 *		PNOBREAK(OBJP, LEN)
 *		struct	objpos	*OBJP;
 *		unsigned		LEN;
 */
#define PNOBREAK(OBJP, LEN)		((OBJP)->op_len >= LEN)

/*
 *		PSKIP(OBJP, LEN)
 *		struct	objpos	*OBJP;
 *		unsigned		LEN;
 */
#define PSKIP(OBJP, LEN)\
		do { (OBJP)->op_cp += (LEN); (OBJP)->op_len -= (LEN); } while (0)

#endif   /* ITEMPOS_H */
