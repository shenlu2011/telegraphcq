/*-------------------------------------------------------------------------
 *
 * ibit.h
 *	  POSTGRES index valid attribute bit map definitions.
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: ibit.h,v 1.1.1.2 2003/04/17 23:01:21 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef IBIT_H
#define IBIT_H

typedef struct IndexAttributeBitMapData
{
	bits8		bits[(INDEX_MAX_KEYS + 8 - 1) / 8];
} IndexAttributeBitMapData;

typedef IndexAttributeBitMapData *IndexAttributeBitMap;

#define IndexAttributeBitMapSize		sizeof(IndexAttributeBitMapData)

/*
 * IndexAttributeBitMapIsValid
 *		True iff attribute bit map is valid.
 */
#define IndexAttributeBitMapIsValid(bits) PointerIsValid(bits)

#endif   /* IBIT_H */
