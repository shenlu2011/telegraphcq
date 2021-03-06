/*-------------------------------------------------------------------------
 *
 * pg_namespace.c
 *	  routines to support manipulation of the pg_namespace relation
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/catalog/pg_namespace.c,v 1.3 2004/05/15 18:57:23 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/catname.h"
#include "catalog/indexing.h"
#include "catalog/pg_namespace.h"
#include "utils/builtins.h"
#include "utils/syscache.h"


/* ----------------
 * NamespaceCreate
 * ---------------
 */
Oid
NamespaceCreate(const char *nspName, int32 ownerSysId)
{
	Relation	nspdesc;
	HeapTuple	tup;
	Oid			nspoid;
	char		nulls[Natts_pg_namespace];
	Datum		values[Natts_pg_namespace];
	NameData	nname;
	TupleDesc	tupDesc;
	int			i;

	/* sanity checks */
	if (!nspName)
		elog(ERROR, "no namespace name supplied");

	/* make sure there is no existing namespace of same name */
	if (SearchSysCacheExists(NAMESPACENAME,
							 PointerGetDatum(nspName),
							 0, 0, 0)) 
	  {
	    elog(ERROR, "namespace \"%s\" already exists", nspName);
	  }

	/* initialize nulls and values */
	for (i = 0; i < Natts_pg_namespace; i++)
	{
		nulls[i] = ' ';
		values[i] = (Datum) NULL;
	}
	namestrcpy(&nname, nspName);
	values[Anum_pg_namespace_nspname - 1] = NameGetDatum(&nname);
	values[Anum_pg_namespace_nspowner - 1] = Int32GetDatum(ownerSysId);
	nulls[Anum_pg_namespace_nspacl - 1] = 'n';

	nspdesc = heap_openr(NamespaceRelationName, RowExclusiveLock);
	tupDesc = nspdesc->rd_att;

	tup = heap_formtuple(tupDesc, values, nulls);

	nspoid = simple_heap_insert(nspdesc, tup);
	Assert(OidIsValid(nspoid));

	CatalogUpdateIndexes(nspdesc, tup);

	heap_close(nspdesc, RowExclusiveLock);

	return nspoid;
}
