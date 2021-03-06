/*-------------------------------------------------------------------------
 *
 * defrem.h
 *	  POSTGRES define and remove utility definitions.
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: defrem.h,v 1.1.1.2 2003/04/17 23:02:14 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef DEFREM_H
#define DEFREM_H

#include "nodes/parsenodes.h"

#define DEFAULT_TYPDELIM		','

/* commands/indexcmds.c */
extern void DefineIndex(RangeVar *heapRelation,
			char *indexRelationName,
			char *accessMethodName,
			List *attributeList,
			bool unique,
			bool primary,
			bool isconstraint,
			Expr *predicate,
			List *rangetable);
extern void RemoveIndex(RangeVar *relation, DropBehavior behavior);
extern void ReindexIndex(RangeVar *indexRelation, bool force);
extern void ReindexTable(RangeVar *relation, bool force);
extern void ReindexDatabase(const char *databaseName, bool force, bool all);

/* commands/functioncmds.c */
extern void CreateFunction(CreateFunctionStmt *stmt);
extern void RemoveFunction(RemoveFuncStmt *stmt);
extern void RemoveFunctionById(Oid funcOid);
extern void SetFunctionReturnType(Oid funcOid, Oid newRetType);
extern void SetFunctionArgType(Oid funcOid, int argIndex, Oid newArgType);
extern void CreateCast(CreateCastStmt *stmt);
extern void DropCast(DropCastStmt *stmt);
extern void DropCastById(Oid castOid);

/* commands/operatorcmds.c */
extern void DefineOperator(List *names, List *parameters);
extern void RemoveOperator(RemoveOperStmt *stmt);
extern void RemoveOperatorById(Oid operOid);

/* commands/aggregatecmds.c */
extern void DefineAggregate(List *names, List *parameters);
extern void RemoveAggregate(RemoveAggrStmt *stmt);

/* commands/typecmds.c */
extern void DefineType(List *names, List *parameters);
extern void RemoveType(List *names, DropBehavior behavior);
extern void RemoveTypeById(Oid typeOid);
extern void DefineDomain(CreateDomainStmt *stmt);
extern void RemoveDomain(List *names, DropBehavior behavior);
extern Oid	DefineCompositeType(const RangeVar *typevar, List *coldeflist);

/* commands/opclasscmds.c */
extern void DefineOpClass(CreateOpClassStmt *stmt);
extern void RemoveOpClass(RemoveOpClassStmt *stmt);
extern void RemoveOpClassById(Oid opclassOid);


/* support routines in commands/define.c */

extern void case_translate_language_name(const char *input, char *output);

extern char *defGetString(DefElem *def);
extern double defGetNumeric(DefElem *def);
extern int64 defGetInt64(DefElem *def);
extern List *defGetQualifiedName(DefElem *def);
extern TypeName *defGetTypeName(DefElem *def);
extern int	defGetTypeLength(DefElem *def);

#endif   /* DEFREM_H */
