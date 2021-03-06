/*-------------------------------------------------------------------------
 *
 * lsyscache.h
 *	  Convenience routines for common queries in the system catalog cache.
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: lsyscache.h,v 1.1.1.2 2003/04/17 23:02:48 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef LSYSCACHE_H
#define LSYSCACHE_H

#include "access/htup.h"

extern bool op_in_opclass(Oid opno, Oid opclass);
extern bool op_requires_recheck(Oid opno, Oid opclass);
extern char *get_attname(Oid relid, AttrNumber attnum);
extern AttrNumber get_attnum(Oid relid, const char *attname);
extern Oid	get_atttype(Oid relid, AttrNumber attnum);
extern int32 get_atttypmod(Oid relid, AttrNumber attnum);
extern void get_atttypetypmod(Oid relid, AttrNumber attnum,
				  Oid *typid, int32 *typmod);
extern bool opclass_is_btree(Oid opclass);
extern RegProcedure get_opcode(Oid opno);
extern char *get_opname(Oid opno);
extern bool op_mergejoinable(Oid opno, Oid ltype, Oid rtype,
				 Oid *leftOp, Oid *rightOp);
extern void op_mergejoin_crossops(Oid opno, Oid *ltop, Oid *gtop,
					  RegProcedure *ltproc, RegProcedure *gtproc);
extern Oid	op_hashjoinable(Oid opno, Oid ltype, Oid rtype);
extern char op_volatile(Oid opno);
extern Oid	get_commutator(Oid opno);
extern Oid	get_negator(Oid opno);
extern RegProcedure get_oprrest(Oid opno);
extern RegProcedure get_oprjoin(Oid opno);
extern char *get_func_name(Oid funcid);
extern Oid	get_func_rettype(Oid funcid);
extern bool get_func_retset(Oid funcid);
extern char func_volatile(Oid funcid);
extern Oid	get_relname_relid(const char *relname, Oid relnamespace);
extern Oid	get_system_catalog_relid(const char *catname);
extern char *get_rel_name(Oid relid);
extern Oid	get_rel_namespace(Oid relid);
extern Oid	get_rel_type_id(Oid relid);
extern char get_rel_relkind(Oid relid);
extern bool get_typisdefined(Oid typid);
extern int16 get_typlen(Oid typid);
extern bool get_typbyval(Oid typid);
extern void get_typlenbyval(Oid typid, int16 *typlen, bool *typbyval);
extern void get_typlenbyvalalign(Oid typid, int16 *typlen, bool *typbyval,
					 char *typalign);
extern char get_typstorage(Oid typid);
extern Node *get_typdefault(Oid typid);
extern char get_typtype(Oid typid);
extern Oid	get_typ_typrelid(Oid typid);
extern void getTypeInputInfo(Oid type, Oid *typInput, Oid *typElem);
extern bool getTypeOutputInfo(Oid type, Oid *typOutput, Oid *typElem,
				  bool *typIsVarlena);
extern Oid	getBaseType(Oid typid);
extern int32 get_typavgwidth(Oid typid, int32 typmod);
extern int32 get_attavgwidth(Oid relid, AttrNumber attnum);
extern bool get_attstatsslot(HeapTuple statstuple,
				 Oid atttype, int32 atttypmod,
				 int reqkind, Oid reqop,
				 Datum **values, int *nvalues,
				 float4 **numbers, int *nnumbers);
extern void free_attstatsslot(Oid atttype,
				  Datum *values, int nvalues,
				  float4 *numbers, int nnumbers);
extern char *get_namespace_name(Oid nspid);
extern int32 get_usesysid(const char *username);

#define TypeIsToastable(typid)	(get_typstorage(typid) != 'p')

#endif   /* LSYSCACHE_H */
