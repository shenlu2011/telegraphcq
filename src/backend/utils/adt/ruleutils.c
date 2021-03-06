/**********************************************************************
 * ruleutils.c	- Functions to convert stored expressions/querytrees
 *				back to source text
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/utils/adt/ruleutils.c,v 1.3 2004/03/24 08:11:12 chungwu Exp $
 *
 *	  This software is copyrighted by Jan Wieck - Hamburg.
 *
 *	  The author hereby grants permission  to  use,  copy,	modify,
 *	  distribute,  and	license this software and its documentation
 *	  for any purpose, provided that existing copyright notices are
 *	  retained	in	all  copies  and  that	this notice is included
 *	  verbatim in any distributions. No written agreement, license,
 *	  or  royalty  fee	is required for any of the authorized uses.
 *	  Modifications to this software may be  copyrighted  by  their
 *	  author  and  need  not  follow  the licensing terms described
 *	  here, provided that the new terms are  clearly  indicated  on
 *	  the first page of each file where they apply.
 *
 *	  IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTORS BE LIABLE TO ANY
 *	  PARTY  FOR  DIRECT,	INDIRECT,	SPECIAL,   INCIDENTAL,	 OR
 *	  CONSEQUENTIAL   DAMAGES  ARISING	OUT  OF  THE  USE  OF  THIS
 *	  SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN
 *	  IF  THE  AUTHOR  HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
 *	  DAMAGE.
 *
 *	  THE  AUTHOR  AND	DISTRIBUTORS  SPECIFICALLY	 DISCLAIM	ANY
 *	  WARRANTIES,  INCLUDING,  BUT	NOT  LIMITED  TO,  THE	IMPLIED
 *	  WARRANTIES  OF  MERCHANTABILITY,	FITNESS  FOR  A  PARTICULAR
 *	  PURPOSE,	AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON
 *	  AN "AS IS" BASIS, AND THE AUTHOR	AND  DISTRIBUTORS  HAVE  NO
 *	  OBLIGATION   TO	PROVIDE   MAINTENANCE,	 SUPPORT,  UPDATES,
 *	  ENHANCEMENTS, OR MODIFICATIONS.
 *
 **********************************************************************/

#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>

#include "access/genam.h"
#include "catalog/catname.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_index.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_shadow.h"
#include "executor/spi.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/tlist.h"
#include "parser/keywords.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_type.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rewriteSupport.h"
#include "utils/array.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"


/* ----------
 * Local data types
 * ----------
 */

/* Context info needed for invoking a recursive querytree display routine */
typedef struct
{
	StringInfo	buf;			/* output buffer to append to */
	List	   *namespaces;		/* List of deparse_namespace nodes */
	bool		varprefix;		/* TRUE to print prefixes on Vars */
} deparse_context;

/*
 * Each level of query context around a subtree needs a level of Var namespace.
 * A Var having varlevelsup=N refers to the N'th item (counting from 0) in
 * the current context's namespaces list.
 *
 * The rangetable is the list of actual RTEs from the query tree.
 *
 * For deparsing plan trees, we allow two special RTE entries that are not
 * part of the rtable list (mainly because they don't have consecutively
 * allocated varnos).
 */
typedef struct
{
	List	   *rtable;			/* List of RangeTblEntry nodes */
	int			outer_varno;	/* varno for outer_rte */
	RangeTblEntry *outer_rte;	/* special RangeTblEntry, or NULL */
	int			inner_varno;	/* varno for inner_rte */
	RangeTblEntry *inner_rte;	/* special RangeTblEntry, or NULL */
} deparse_namespace;


/* ----------
 * Global data
 * ----------
 */
static void *plan_getrulebyoid = NULL;
static char *query_getrulebyoid = "SELECT * FROM pg_catalog.pg_rewrite WHERE oid = $1";
static void *plan_getviewrule = NULL;
static char *query_getviewrule = "SELECT * FROM pg_catalog.pg_rewrite WHERE ev_class = $1 AND rulename = $2";


/* ----------
 * Local functions
 *
 * Most of these functions used to use fixed-size buffers to build their
 * results.  Now, they take an (already initialized) StringInfo object
 * as a parameter, and append their text output to its contents.
 * ----------
 */
static text *pg_do_getviewdef(Oid viewoid);
static void decompile_column_index_array(Datum column_index_array, Oid relId,
							 StringInfo buf);
static void make_ruledef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc);
static void make_viewdef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc);
static void get_query_def(Query *query, StringInfo buf, List *parentnamespace,
			  TupleDesc resultDesc);
static void get_select_query_def(Query *query, deparse_context *context,
					 TupleDesc resultDesc);
static void get_insert_query_def(Query *query, deparse_context *context);
static void get_update_query_def(Query *query, deparse_context *context);
static void get_delete_query_def(Query *query, deparse_context *context);
static void get_utility_query_def(Query *query, deparse_context *context);
static void get_basic_select_query(Query *query, deparse_context *context,
					   TupleDesc resultDesc);
static void get_setop_query(Node *setOp, Query *query,
				deparse_context *context,
				TupleDesc resultDesc);
static Node *get_rule_sortgroupclause(SortClause *srt, List *tlist,
						 bool force_colno,
						 deparse_context *context);
static void get_names_for_var(Var *var, deparse_context *context,
				  char **schemaname, char **refname, char **attname);
static RangeTblEntry *find_rte_by_refname(const char *refname,
					deparse_context *context);
static void get_rule_expr(Node *node, deparse_context *context,
			  bool showimplicit);
static void get_oper_expr(Expr *expr, deparse_context *context);
static void get_func_expr(Expr *expr, deparse_context *context,
			  bool showimplicit);
static void get_agg_expr(Aggref *aggref, deparse_context *context);
static Node *strip_type_coercion(Node *expr, Oid resultType);
static void get_const_expr(Const *constval, deparse_context *context);
static void get_sublink_expr(Node *node, deparse_context *context);
static void get_from_clause(Query *query, deparse_context *context);
static void get_from_clause_item(Node *jtnode, Query *query,
					 deparse_context *context);
static void get_from_clause_coldeflist(List *coldeflist,
						   deparse_context *context);
static void get_opclass_name(Oid opclass, Oid actual_datatype,
				 StringInfo buf);
static bool tleIsArrayAssign(TargetEntry *tle);
static char *generate_relation_name(Oid relid);
static char *generate_function_name(Oid funcid, int nargs, Oid *argtypes);
static char *generate_operator_name(Oid operid, Oid arg1, Oid arg2);
static char *get_relid_attribute_name(Oid relid, AttrNumber attnum);

#define only_marker(rte)  ((rte)->inh ? "" : "ONLY ")


/* ----------
 * get_ruledef			- Do it all and return a text
 *				  that could be used as a statement
 *				  to recreate the rule
 * ----------
 */
Datum
pg_get_ruledef(PG_FUNCTION_ARGS)
{
	Oid			ruleoid = PG_GETARG_OID(0);
	text	   *ruledef;
	Datum		args[1];
	char		nulls[1];
	int			spirc;
	HeapTuple	ruletup;
	TupleDesc	rulettc;
	StringInfoData buf;
	int			len;

	/*
	 * Connect to SPI manager
	 */
	if (SPI_connect() != SPI_OK_CONNECT)
		elog(ERROR, "get_ruledef: cannot connect to SPI manager");

	/*
	 * On the first call prepare the plan to lookup pg_rewrite. We read
	 * pg_rewrite over the SPI manager instead of using the syscache to be
	 * checked for read access on pg_rewrite.
	 */
	if (plan_getrulebyoid == NULL)
	{
		Oid			argtypes[1];
		void	   *plan;

		argtypes[0] = OIDOID;
		plan = SPI_prepare(query_getrulebyoid, 1, argtypes);
		if (plan == NULL)
			elog(ERROR, "SPI_prepare() failed for \"%s\"", query_getrulebyoid);
		plan_getrulebyoid = SPI_saveplan(plan);
	}

	/*
	 * Get the pg_rewrite tuple for this rule
	 */
	args[0] = ObjectIdGetDatum(ruleoid);
	nulls[0] = ' ';
	spirc = SPI_execp(plan_getrulebyoid, args, nulls, 1);
	if (spirc != SPI_OK_SELECT)
		elog(ERROR, "failed to get pg_rewrite tuple for %u", ruleoid);
	if (SPI_processed != 1)
	{
		if (SPI_finish() != SPI_OK_FINISH)
			elog(ERROR, "get_ruledef: SPI_finish() failed");
		ruledef = palloc(VARHDRSZ + 1);
		VARATT_SIZEP(ruledef) = VARHDRSZ + 1;
		VARDATA(ruledef)[0] = '-';
		PG_RETURN_TEXT_P(ruledef);
	}

	ruletup = SPI_tuptable->vals[0];
	rulettc = SPI_tuptable->tupdesc;

	/*
	 * Get the rules definition and put it into executors memory
	 */
	initStringInfo(&buf);
	make_ruledef(&buf, ruletup, rulettc);
	len = buf.len + VARHDRSZ;
	ruledef = SPI_palloc(len);
	VARATT_SIZEP(ruledef) = len;
	memcpy(VARDATA(ruledef), buf.data, buf.len);
	pfree(buf.data);

	/*
	 * Disconnect from SPI manager
	 */
	if (SPI_finish() != SPI_OK_FINISH)
		elog(ERROR, "get_ruledef: SPI_finish() failed");

	/*
	 * Easy - isn't it?
	 */
	PG_RETURN_TEXT_P(ruledef);
}


/* ----------
 * get_viewdef			- Mainly the same thing, but we
 *				  only return the SELECT part of a view
 * ----------
 */
Datum
pg_get_viewdef(PG_FUNCTION_ARGS)
{
	/* By OID */
	Oid			viewoid = PG_GETARG_OID(0);
	text	   *ruledef;

	ruledef = pg_do_getviewdef(viewoid);
	PG_RETURN_TEXT_P(ruledef);
}

Datum
pg_get_viewdef_name(PG_FUNCTION_ARGS)
{
	/* By qualified name */
	text	   *viewname = PG_GETARG_TEXT_P(0);
	RangeVar   *viewrel;
	Oid			viewoid;
	text	   *ruledef;

	viewrel = makeRangeVarFromNameList(textToQualifiedNameList(viewname,
														 "get_viewdef"));
	viewoid = RangeVarGetRelid(viewrel, false);

	ruledef = pg_do_getviewdef(viewoid);
	PG_RETURN_TEXT_P(ruledef);
}

/*
 * Common code for by-OID and by-name variants of pg_get_viewdef
 */
static text *
pg_do_getviewdef(Oid viewoid)
{
	text	   *ruledef;
	Datum		args[2];
	char		nulls[2];
	int			spirc;
	HeapTuple	ruletup;
	TupleDesc	rulettc;
	StringInfoData buf;
	int			len;

	/*
	 * Connect to SPI manager
	 */
	if (SPI_connect() != SPI_OK_CONNECT)
		elog(ERROR, "get_viewdef: cannot connect to SPI manager");

	/*
	 * On the first call prepare the plan to lookup pg_rewrite. We read
	 * pg_rewrite over the SPI manager instead of using the syscache to be
	 * checked for read access on pg_rewrite.
	 */
	if (plan_getviewrule == NULL)
	{
		Oid			argtypes[2];
		void	   *plan;

		argtypes[0] = OIDOID;
		argtypes[1] = NAMEOID;
		plan = SPI_prepare(query_getviewrule, 2, argtypes);
		if (plan == NULL)
			elog(ERROR, "SPI_prepare() failed for \"%s\"", query_getviewrule);
		plan_getviewrule = SPI_saveplan(plan);
	}

	/*
	 * Get the pg_rewrite tuple for the view's SELECT rule
	 */
	args[0] = ObjectIdGetDatum(viewoid);
	args[1] = PointerGetDatum(ViewSelectRuleName);
	nulls[0] = ' ';
	nulls[1] = ' ';
	spirc = SPI_execp(plan_getviewrule, args, nulls, 2);
	if (spirc != SPI_OK_SELECT)
		elog(ERROR, "failed to get pg_rewrite tuple for view %u", viewoid);
	initStringInfo(&buf);
	if (SPI_processed != 1)
		appendStringInfo(&buf, "Not a view");
	else
	{
		/*
		 * Get the rules definition and put it into executors memory
		 */
		ruletup = SPI_tuptable->vals[0];
		rulettc = SPI_tuptable->tupdesc;
		make_viewdef(&buf, ruletup, rulettc);
	}
	len = buf.len + VARHDRSZ;
	ruledef = SPI_palloc(len);
	VARATT_SIZEP(ruledef) = len;
	memcpy(VARDATA(ruledef), buf.data, buf.len);
	pfree(buf.data);

	/*
	 * Disconnect from SPI manager
	 */
	if (SPI_finish() != SPI_OK_FINISH)
		elog(ERROR, "get_viewdef: SPI_finish() failed");

	return ruledef;
}


/* ----------
 * get_indexdef			- Get the definition of an index
 * ----------
 */
Datum
pg_get_indexdef(PG_FUNCTION_ARGS)
{
	Oid			indexrelid = PG_GETARG_OID(0);
	text	   *indexdef;
	HeapTuple	ht_idx;
	HeapTuple	ht_idxrel;
	HeapTuple	ht_am;
	Form_pg_index idxrec;
	Form_pg_class idxrelrec;
	Form_pg_am	amrec;
	Oid			indrelid;
	int			len;
	int			keyno;
	Oid			keycoltypes[INDEX_MAX_KEYS];
	StringInfoData buf;
	StringInfoData keybuf;
	char	   *sep;

	/*
	 * Fetch the pg_index tuple by the Oid of the index
	 */
	ht_idx = SearchSysCache(INDEXRELID,
							ObjectIdGetDatum(indexrelid),
							0, 0, 0);
	if (!HeapTupleIsValid(ht_idx))
		elog(ERROR, "syscache lookup for index %u failed", indexrelid);
	idxrec = (Form_pg_index) GETSTRUCT(ht_idx);

	indrelid = idxrec->indrelid;
	Assert(indexrelid == idxrec->indexrelid);

	/*
	 * Fetch the pg_class tuple of the index relation
	 */
	ht_idxrel = SearchSysCache(RELOID,
							   ObjectIdGetDatum(indexrelid),
							   0, 0, 0);
	if (!HeapTupleIsValid(ht_idxrel))
		elog(ERROR, "syscache lookup for relid %u failed", indexrelid);
	idxrelrec = (Form_pg_class) GETSTRUCT(ht_idxrel);

	/*
	 * Fetch the pg_am tuple of the index' access method
	 */
	ht_am = SearchSysCache(AMOID,
						   ObjectIdGetDatum(idxrelrec->relam),
						   0, 0, 0);
	if (!HeapTupleIsValid(ht_am))
		elog(ERROR, "syscache lookup for AM %u failed", idxrelrec->relam);
	amrec = (Form_pg_am) GETSTRUCT(ht_am);

	/*
	 * Start the index definition.	Note that the index's name should
	 * never be schema-qualified, but the indexed rel's name may be.
	 */
	initStringInfo(&buf);
	appendStringInfo(&buf, "CREATE %sINDEX %s ON %s USING %s (",
					 idxrec->indisunique ? "UNIQUE " : "",
					 quote_identifier(NameStr(idxrelrec->relname)),
					 generate_relation_name(indrelid),
					 quote_identifier(NameStr(amrec->amname)));

	/*
	 * Collect the indexed attributes in keybuf
	 */
	initStringInfo(&keybuf);
	sep = "";
	for (keyno = 0; keyno < INDEX_MAX_KEYS; keyno++)
	{
		AttrNumber	attnum = idxrec->indkey[keyno];
		char	   *attname;

		if (attnum == InvalidAttrNumber)
			break;

		attname = get_relid_attribute_name(indrelid, attnum);
		keycoltypes[keyno] = get_atttype(indrelid, attnum);

		appendStringInfo(&keybuf, sep);
		sep = ", ";

		/*
		 * Add the indexed field name
		 */
		appendStringInfo(&keybuf, "%s", quote_identifier(attname));

		/*
		 * If not a functional index, add the operator class name
		 */
		if (idxrec->indproc == InvalidOid)
			get_opclass_name(idxrec->indclass[keyno],
							 keycoltypes[keyno],
							 &keybuf);
	}

	if (idxrec->indproc != InvalidOid)
	{
		/*
		 * For functional index say 'func (attrs) opclass'
		 */
		appendStringInfo(&buf, "%s(%s)",
						 generate_function_name(idxrec->indproc,
												keyno, keycoltypes),
						 keybuf.data);
		get_opclass_name(idxrec->indclass[0],
						 get_func_rettype(idxrec->indproc),
						 &buf);
	}
	else
	{
		/*
		 * Otherwise say 'attr opclass [, ...]'
		 */
		appendStringInfo(&buf, "%s", keybuf.data);
	}

	appendStringInfoChar(&buf, ')');

	/*
	 * If it's a partial index, decompile and append the predicate
	 */
	if (VARSIZE(&idxrec->indpred) > VARHDRSZ)
	{
		Node	   *node;
		List	   *context;
		char	   *exprstr;
		char	   *str;

		/* Convert TEXT object to C string */
		exprstr = DatumGetCString(DirectFunctionCall1(textout,
									 PointerGetDatum(&idxrec->indpred)));
		/* Convert expression to node tree */
		node = (Node *) stringToNode(exprstr);

		/*
		 * If top level is a List, assume it is an implicit-AND structure,
		 * and convert to explicit AND.  This is needed for partial index
		 * predicates.
		 */
		if (node && IsA(node, List))
			node = (Node *) make_ands_explicit((List *) node);
		/* Deparse */
		context = deparse_context_for(get_rel_name(indrelid), indrelid);
		str = deparse_expression(node, context, false, false);
		appendStringInfo(&buf, " WHERE %s", str);
	}

	/*
	 * Create the result as a TEXT datum, and free working data
	 */
	len = buf.len + VARHDRSZ;
	indexdef = (text *) palloc(len);
	VARATT_SIZEP(indexdef) = len;
	memcpy(VARDATA(indexdef), buf.data, buf.len);

	pfree(buf.data);
	pfree(keybuf.data);

	ReleaseSysCache(ht_idx);
	ReleaseSysCache(ht_idxrel);
	ReleaseSysCache(ht_am);

	PG_RETURN_TEXT_P(indexdef);
}


/*
 * pg_get_constraintdef
 *
 * Returns the definition for the constraint, ie, everything that needs to
 * appear after "ALTER TABLE ... ADD CONSTRAINT <constraintname>".
 *
 * XXX The present implementation only works for foreign-key constraints, but
 * it could and should handle anything pg_constraint stores.
 */
Datum
pg_get_constraintdef(PG_FUNCTION_ARGS)
{
	Oid			constraintId = PG_GETARG_OID(0);
	text	   *result;
	StringInfoData buf;
	int			len;
	Relation	conDesc;
	SysScanDesc conscan;
	ScanKeyData skey[1];
	HeapTuple	tup;
	Form_pg_constraint conForm;

	/*
	 * Fetch the pg_constraint row.  There's no syscache for pg_constraint
	 * so we must do it the hard way.
	 */
	conDesc = heap_openr(ConstraintRelationName, AccessShareLock);

	ScanKeyEntryInitialize(&skey[0], 0x0,
						   ObjectIdAttributeNumber, F_OIDEQ,
						   ObjectIdGetDatum(constraintId));

	conscan = systable_beginscan(conDesc, ConstraintOidIndex, true,
								 SnapshotNow, 1, skey);

	tup = systable_getnext(conscan);
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "Failed to find constraint with OID %u", constraintId);
	conForm = (Form_pg_constraint) GETSTRUCT(tup);

	initStringInfo(&buf);

	switch (conForm->contype)
	{
		case CONSTRAINT_FOREIGN:
			{
				Datum		val;
				bool		isnull;
				const char *string;

				/* Start off the constraint definition */
				appendStringInfo(&buf, "FOREIGN KEY (");

				/* Fetch and build referencing-column list */
				val = heap_getattr(tup, Anum_pg_constraint_conkey,
								   RelationGetDescr(conDesc), &isnull);
				if (isnull)
					elog(ERROR, "pg_get_constraintdef: Null conkey for constraint %u",
						 constraintId);

				decompile_column_index_array(val, conForm->conrelid, &buf);

				/* add foreign relation name */
				appendStringInfo(&buf, ") REFERENCES %s(",
							 generate_relation_name(conForm->confrelid));

				/* Fetch and build referenced-column list */
				val = heap_getattr(tup, Anum_pg_constraint_confkey,
								   RelationGetDescr(conDesc), &isnull);
				if (isnull)
					elog(ERROR, "pg_get_constraintdef: Null confkey for constraint %u",
						 constraintId);

				decompile_column_index_array(val, conForm->confrelid, &buf);

				appendStringInfo(&buf, ")");

				/* Add match type */
				switch (conForm->confmatchtype)
				{
					case FKCONSTR_MATCH_FULL:
						string = " MATCH FULL";
						break;
					case FKCONSTR_MATCH_PARTIAL:
						string = " MATCH PARTIAL";
						break;
					case FKCONSTR_MATCH_UNSPECIFIED:
						string = "";
						break;
					default:
						elog(ERROR, "pg_get_constraintdef: Unknown confmatchtype '%c' for constraint %u",
							 conForm->confmatchtype, constraintId);
						string = "";	/* keep compiler quiet */
						break;
				}
				appendStringInfo(&buf, "%s", string);

				/* Add ON UPDATE and ON DELETE clauses */
				switch (conForm->confupdtype)
				{
					case FKCONSTR_ACTION_NOACTION:
						string = "NO ACTION";
						break;
					case FKCONSTR_ACTION_RESTRICT:
						string = "RESTRICT";
						break;
					case FKCONSTR_ACTION_CASCADE:
						string = "CASCADE";
						break;
					case FKCONSTR_ACTION_SETNULL:
						string = "SET NULL";
						break;
					case FKCONSTR_ACTION_SETDEFAULT:
						string = "SET DEFAULT";
						break;
					default:
						elog(ERROR, "pg_get_constraintdef: Unknown confupdtype '%c' for constraint %u",
							 conForm->confupdtype, constraintId);
						string = "";	/* keep compiler quiet */
						break;
				}
				appendStringInfo(&buf, " ON UPDATE %s", string);

				switch (conForm->confdeltype)
				{
					case FKCONSTR_ACTION_NOACTION:
						string = "NO ACTION";
						break;
					case FKCONSTR_ACTION_RESTRICT:
						string = "RESTRICT";
						break;
					case FKCONSTR_ACTION_CASCADE:
						string = "CASCADE";
						break;
					case FKCONSTR_ACTION_SETNULL:
						string = "SET NULL";
						break;
					case FKCONSTR_ACTION_SETDEFAULT:
						string = "SET DEFAULT";
						break;
					default:
						elog(ERROR, "pg_get_constraintdef: Unknown confdeltype '%c' for constraint %u",
							 conForm->confdeltype, constraintId);
						string = "";	/* keep compiler quiet */
						break;
				}
				appendStringInfo(&buf, " ON DELETE %s", string);

				if (conForm->condeferrable)
					appendStringInfo(&buf, " DEFERRABLE");
				if (conForm->condeferred)
					appendStringInfo(&buf, " INITIALLY DEFERRED");

				break;
			}

			/*
			 * XXX Add more code here for other contypes
			 */
		default:
			elog(ERROR, "pg_get_constraintdef: unsupported constraint type '%c'",
				 conForm->contype);
			break;
	}

	/* Record the results */
	len = buf.len + VARHDRSZ;
	result = (text *) palloc(len);
	VARATT_SIZEP(result) = len;
	memcpy(VARDATA(result), buf.data, buf.len);

	/* Cleanup */
	pfree(buf.data);
	systable_endscan(conscan);
	heap_close(conDesc, AccessShareLock);

	PG_RETURN_TEXT_P(result);
}


/*
 * Convert an int16[] Datum into a comma-separated list of column names
 * for the indicated relation; append the list to buf.
 */
static void
decompile_column_index_array(Datum column_index_array, Oid relId,
							 StringInfo buf)
{
	Datum	   *keys;
	int			nKeys;
	int			j;

	/* Extract data from array of int16 */
	deconstruct_array(DatumGetArrayTypeP(column_index_array),
					  INT2OID, 2, true, 's',
					  &keys, &nKeys);

	for (j = 0; j < nKeys; j++)
	{
		char	   *colName;

		colName = get_attname(relId, DatumGetInt16(keys[j]));

		if (j == 0)
			appendStringInfo(buf, "%s",
							 quote_identifier(colName));
		else
			appendStringInfo(buf, ", %s",
							 quote_identifier(colName));
	}
}


/* ----------
 * get_expr			- Decompile an expression tree
 *
 * Input: an expression tree in nodeToString form, and a relation OID
 *
 * Output: reverse-listed expression
 *
 * Currently, the expression can only refer to a single relation, namely
 * the one specified by the second parameter.  This is sufficient for
 * partial indexes, column default expressions, etc.
 * ----------
 */
Datum
pg_get_expr(PG_FUNCTION_ARGS)
{
	text	   *expr = PG_GETARG_TEXT_P(0);
	Oid			relid = PG_GETARG_OID(1);
	text	   *result;
	Node	   *node;
	List	   *context;
	char	   *exprstr;
	char	   *relname;
	char	   *str;

	/* Get the name for the relation */
	relname = get_rel_name(relid);
	if (relname == NULL)
		PG_RETURN_NULL();		/* should we raise an error? */

	/* Convert input TEXT object to C string */
	exprstr = DatumGetCString(DirectFunctionCall1(textout,
												  PointerGetDatum(expr)));

	/* Convert expression to node tree */
	node = (Node *) stringToNode(exprstr);

	/*
	 * If top level is a List, assume it is an implicit-AND structure, and
	 * convert to explicit AND.  This is needed for partial index
	 * predicates.
	 */
	if (node && IsA(node, List))
		node = (Node *) make_ands_explicit((List *) node);

	/* Deparse */
	context = deparse_context_for(relname, relid);
	str = deparse_expression(node, context, false, false);

	/* Pass the result back as TEXT */
	result = DatumGetTextP(DirectFunctionCall1(textin,
											   CStringGetDatum(str)));

	PG_RETURN_TEXT_P(result);
}


/* ----------
 * get_userbyid			- Get a user name by usesysid and
 *				  fallback to 'unknown (UID=n)'
 * ----------
 */
Datum
pg_get_userbyid(PG_FUNCTION_ARGS)
{
	int32		uid = PG_GETARG_INT32(0);
	Name		result;
	HeapTuple	usertup;
	Form_pg_shadow user_rec;

	/*
	 * Allocate space for the result
	 */
	result = (Name) palloc(NAMEDATALEN);
	memset(NameStr(*result), 0, NAMEDATALEN);

	/*
	 * Get the pg_shadow entry and print the result
	 */
	usertup = SearchSysCache(SHADOWSYSID,
							 ObjectIdGetDatum(uid),
							 0, 0, 0);
	if (HeapTupleIsValid(usertup))
	{
		user_rec = (Form_pg_shadow) GETSTRUCT(usertup);
		StrNCpy(NameStr(*result), NameStr(user_rec->usename), NAMEDATALEN);
		ReleaseSysCache(usertup);
	}
	else
		sprintf(NameStr(*result), "unknown (UID=%d)", uid);

	PG_RETURN_NAME(result);
}

/* ----------
 * deparse_expression			- General utility for deparsing expressions
 *
 * expr is the node tree to be deparsed.  It must be a transformed expression
 * tree (ie, not the raw output of gram.y).
 *
 * dpcontext is a list of deparse_namespace nodes representing the context
 * for interpreting Vars in the node tree.
 *
 * forceprefix is TRUE to force all Vars to be prefixed with their table names.
 *
 * showimplicit is TRUE to force all implicit casts to be shown explicitly.
 *
 * The result is a palloc'd string.
 * ----------
 */
char *
deparse_expression(Node *expr, List *dpcontext,
				   bool forceprefix, bool showimplicit)
{
	StringInfoData buf;
	deparse_context context;

	initStringInfo(&buf);
	context.buf = &buf;
	context.namespaces = dpcontext;
	context.varprefix = forceprefix;

	get_rule_expr(expr, &context, showimplicit);

	return buf.data;
}

/* ----------
 * deparse_context_for			- Build deparse context for a single relation
 *
 * Given the reference name (alias) and OID of a relation, build deparsing
 * context for an expression referencing only that relation (as varno 1,
 * varlevelsup 0).	This is sufficient for many uses of deparse_expression.
 * ----------
 */
List *
deparse_context_for(const char *aliasname, Oid relid)
{
	deparse_namespace *dpns;
	RangeTblEntry *rte;

	dpns = (deparse_namespace *) palloc(sizeof(deparse_namespace));

	/* Build a minimal RTE for the rel */
	rte = makeNode(RangeTblEntry);
	rte->rtekind = RTE_RELATION;
	rte->relid = relid;
	rte->eref = makeAlias(aliasname, NIL);
	rte->inh = false;
	rte->inFromCl = true;

	/* Build one-element rtable */
	dpns->rtable = makeList1(rte);
	dpns->outer_varno = dpns->inner_varno = 0;
	dpns->outer_rte = dpns->inner_rte = NULL;

	/* Return a one-deep namespace stack */
	return makeList1(dpns);
}

/*
 * deparse_context_for_plan		- Build deparse context for a plan node
 *
 * We assume we are dealing with an upper-level plan node having either
 * one or two referenceable children (pass innercontext = NULL if only one).
 * The passed-in Nodes should be made using deparse_context_for_subplan
 * and/or deparse_context_for_relation.  The resulting context will work
 * for deparsing quals, tlists, etc of the plan node.
 *
 * An rtable list can also be passed in case plain Vars might be seen.
 * This is not needed for true upper-level expressions, but is helpful for
 * Sort nodes and similar cases with slightly bogus targetlists.
 */
List *
deparse_context_for_plan(int outer_varno, Node *outercontext,
						 int inner_varno, Node *innercontext,
						 List *rtable)
{
	deparse_namespace *dpns;

	dpns = (deparse_namespace *) palloc(sizeof(deparse_namespace));

	dpns->rtable = rtable;
	dpns->outer_varno = outer_varno;
	dpns->outer_rte = (RangeTblEntry *) outercontext;
	dpns->inner_varno = inner_varno;
	dpns->inner_rte = (RangeTblEntry *) innercontext;

	/* Return a one-deep namespace stack */
	return makeList1(dpns);
}

/*
 * deparse_context_for_rte		- Build deparse context for 1 relation
 *
 * Helper routine to build one of the inputs for deparse_context_for_plan.
 *
 * The returned node is actually the given RangeTblEntry, but we declare it
 * as just Node to discourage callers from assuming anything.
 */
Node *
deparse_context_for_rte(RangeTblEntry *rte)
{
	return (Node *) rte;
}

/*
 * deparse_context_for_subplan	- Build deparse context for a plan node
 *
 * Helper routine to build one of the inputs for deparse_context_for_plan.
 * Pass the tlist of the subplan node, plus the query rangetable.
 *
 * The returned node is actually a RangeTblEntry, but we declare it as just
 * Node to discourage callers from assuming anything.
 */
Node *
deparse_context_for_subplan(const char *name, List *tlist,
							List *rtable)
{
	RangeTblEntry *rte = makeNode(RangeTblEntry);
	List	   *attrs = NIL;
	int			nattrs = 0;
	int			rtablelength = length(rtable);
	List	   *tl;
	char		buf[32];

	foreach(tl, tlist)
	{
		TargetEntry *tle = lfirst(tl);
		Resdom	   *resdom = tle->resdom;

		nattrs++;
		Assert(resdom->resno == nattrs);
		if (resdom->resname)
		{
			attrs = lappend(attrs, makeString(resdom->resname));
			continue;
		}
		if (tle->expr && IsA(tle->expr, Var))
		{
			Var		   *var = (Var *) tle->expr;

			/* varno/varattno won't be any good, but varnoold might be */
			if (var->varnoold > 0 && var->varnoold <= rtablelength)
			{
				RangeTblEntry *varrte = rt_fetch(var->varnoold, rtable);
				char	   *varname;

				varname = get_rte_attribute_name(varrte, var->varoattno);
				attrs = lappend(attrs, makeString(varname));
				continue;
			}
		}
		/* Fallback if can't get name */
		snprintf(buf, sizeof(buf), "?column%d?", resdom->resno);
		attrs = lappend(attrs, makeString(pstrdup(buf)));
	}

	rte->rtekind = RTE_SPECIAL; /* XXX */
	rte->relid = InvalidOid;
	rte->eref = makeAlias(name, attrs);
	rte->inh = false;
	rte->inFromCl = true;

	return (Node *) rte;
}

/* ----------
 * make_ruledef			- reconstruct the CREATE RULE command
 *				  for a given pg_rewrite tuple
 * ----------
 */
static void
make_ruledef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc)
{
	char	   *rulename;
	char		ev_type;
	Oid			ev_class;
	int2		ev_attr;
	bool		is_instead;
	char	   *ev_qual;
	char	   *ev_action;
	List	   *actions = NIL;
	int			fno;
	Datum		dat;
	bool		isnull;

	/*
	 * Get the attribute values from the rules tuple
	 */
	fno = SPI_fnumber(rulettc, "rulename");
	dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
	Assert(!isnull);
	rulename = NameStr(*(DatumGetName(dat)));

	fno = SPI_fnumber(rulettc, "ev_type");
	dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
	Assert(!isnull);
	ev_type = DatumGetChar(dat);

	fno = SPI_fnumber(rulettc, "ev_class");
	dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
	Assert(!isnull);
	ev_class = DatumGetObjectId(dat);

	fno = SPI_fnumber(rulettc, "ev_attr");
	dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
	Assert(!isnull);
	ev_attr = DatumGetInt16(dat);

	fno = SPI_fnumber(rulettc, "is_instead");
	dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
	Assert(!isnull);
	is_instead = DatumGetBool(dat);

	/* these could be nulls */
	fno = SPI_fnumber(rulettc, "ev_qual");
	ev_qual = SPI_getvalue(ruletup, rulettc, fno);

	fno = SPI_fnumber(rulettc, "ev_action");
	ev_action = SPI_getvalue(ruletup, rulettc, fno);
	if (ev_action != NULL)
		actions = (List *) stringToNode(ev_action);

	/*
	 * Build the rules definition text
	 */
	appendStringInfo(buf, "CREATE RULE %s AS ON ",
					 quote_identifier(rulename));

	/* The event the rule is fired for */
	switch (ev_type)
	{
		case '1':
			appendStringInfo(buf, "SELECT");
			break;

		case '2':
			appendStringInfo(buf, "UPDATE");
			break;

		case '3':
			appendStringInfo(buf, "INSERT");
			break;

		case '4':
			appendStringInfo(buf, "DELETE");
			break;

		default:
			elog(ERROR, "get_ruledef: rule %s has unsupported event type %d",
				 rulename, ev_type);
			break;
	}

	/* The relation the rule is fired on */
	appendStringInfo(buf, " TO %s", generate_relation_name(ev_class));
	if (ev_attr > 0)
		appendStringInfo(buf, ".%s",
					  quote_identifier(get_relid_attribute_name(ev_class,
															  ev_attr)));

	/* If the rule has an event qualification, add it */
	if (ev_qual == NULL)
		ev_qual = "";
	if (strlen(ev_qual) > 0 && strcmp(ev_qual, "<>") != 0)
	{
		Node	   *qual;
		Query	   *query;
		deparse_context context;
		deparse_namespace dpns;

		appendStringInfo(buf, " WHERE ");

		qual = stringToNode(ev_qual);

		/*
		 * We need to make a context for recognizing any Vars in the qual
		 * (which can only be references to OLD and NEW).  Use the rtable
		 * of the first query in the action list for this purpose.
		 */
		query = (Query *) lfirst(actions);

		/*
		 * If the action is INSERT...SELECT, OLD/NEW have been pushed down
		 * into the SELECT, and that's what we need to look at. (Ugly
		 * kluge ... try to fix this when we redesign querytrees.)
		 */
		query = getInsertSelectQuery(query, NULL);

		context.buf = buf;
		context.namespaces = makeList1(&dpns);
		context.varprefix = (length(query->rtable) != 1);
		dpns.rtable = query->rtable;
		dpns.outer_varno = dpns.inner_varno = 0;
		dpns.outer_rte = dpns.inner_rte = NULL;

		get_rule_expr(qual, &context, false);
	}

	appendStringInfo(buf, " DO ");

	/* The INSTEAD keyword (if so) */
	if (is_instead)
		appendStringInfo(buf, "INSTEAD ");

	/* Finally the rules actions */
	if (length(actions) > 1)
	{
		List	   *action;
		Query	   *query;

		appendStringInfo(buf, "(");
		foreach(action, actions)
		{
			query = (Query *) lfirst(action);
			get_query_def(query, buf, NIL, NULL);
			appendStringInfo(buf, "; ");
		}
		appendStringInfo(buf, ");");
	}
	else if (length(actions) == 0)
	{
		appendStringInfo(buf, "NOTHING;");
	}
	else
	{
		Query	   *query;

		query = (Query *) lfirst(actions);
		get_query_def(query, buf, NIL, NULL);
		appendStringInfo(buf, ";");
	}
}


/* ----------
 * make_viewdef			- reconstruct the SELECT part of a
 *				  view rewrite rule
 * ----------
 */
static void
make_viewdef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc)
{
	Query	   *query;
	char		ev_type;
	Oid			ev_class;
	int2		ev_attr;
	bool		is_instead;
	char	   *ev_qual;
	char	   *ev_action;
	List	   *actions = NIL;
	Relation	ev_relation;
	int			fno;
	bool		isnull;

	/*
	 * Get the attribute values from the rules tuple
	 */
	fno = SPI_fnumber(rulettc, "ev_type");
	ev_type = (char) SPI_getbinval(ruletup, rulettc, fno, &isnull);

	fno = SPI_fnumber(rulettc, "ev_class");
	ev_class = (Oid) SPI_getbinval(ruletup, rulettc, fno, &isnull);

	fno = SPI_fnumber(rulettc, "ev_attr");
	ev_attr = (int2) SPI_getbinval(ruletup, rulettc, fno, &isnull);

	fno = SPI_fnumber(rulettc, "is_instead");
	is_instead = (bool) SPI_getbinval(ruletup, rulettc, fno, &isnull);

	fno = SPI_fnumber(rulettc, "ev_qual");
	ev_qual = SPI_getvalue(ruletup, rulettc, fno);

	fno = SPI_fnumber(rulettc, "ev_action");
	ev_action = SPI_getvalue(ruletup, rulettc, fno);
	if (ev_action != NULL)
		actions = (List *) stringToNode(ev_action);

	if (length(actions) != 1)
	{
		appendStringInfo(buf, "Not a view");
		return;
	}

	query = (Query *) lfirst(actions);

	if (ev_type != '1' || ev_attr >= 0 || !is_instead ||
		strcmp(ev_qual, "<>") != 0 || query->commandType != CMD_SELECT)
	{
		appendStringInfo(buf, "Not a view");
		return;
	}

	ev_relation = heap_open(ev_class, AccessShareLock);

	get_query_def(query, buf, NIL, RelationGetDescr(ev_relation));
	appendStringInfo(buf, ";");

	heap_close(ev_relation, AccessShareLock);
}


/* ----------
 * get_query_def			- Parse back one query parsetree
 *
 * If resultDesc is not NULL, then it is the output tuple descriptor for
 * the view represented by a SELECT query.
 * ----------
 */
static void
get_query_def(Query *query, StringInfo buf, List *parentnamespace,
			  TupleDesc resultDesc)
{
	deparse_context context;
	deparse_namespace dpns;

	context.buf = buf;
	context.namespaces = lcons(&dpns, parentnamespace);
	context.varprefix = (parentnamespace != NIL ||
						 length(query->rtable) != 1);
	dpns.rtable = query->rtable;
	dpns.outer_varno = dpns.inner_varno = 0;
	dpns.outer_rte = dpns.inner_rte = NULL;

	switch (query->commandType)
	{
		case CMD_SELECT:
			get_select_query_def(query, &context, resultDesc);
			break;

		case CMD_UPDATE:
			get_update_query_def(query, &context);
			break;

		case CMD_INSERT:
			get_insert_query_def(query, &context);
			break;

		case CMD_DELETE:
			get_delete_query_def(query, &context);
			break;

		case CMD_NOTHING:
			appendStringInfo(buf, "NOTHING");
			break;

		case CMD_UTILITY:
			get_utility_query_def(query, &context);
			break;

		default:
			elog(ERROR, "get_query_def: unknown query command type %d",
				 query->commandType);
			break;
	}
}


/* ----------
 * get_select_query_def			- Parse back a SELECT parsetree
 * ----------
 */
static void
get_select_query_def(Query *query, deparse_context *context,
					 TupleDesc resultDesc)
{
	StringInfo	buf = context->buf;
	bool		force_colno;
	char	   *sep;
	List	   *l;

	/*
	 * If the Query node has a setOperations tree, then it's the top level
	 * of a UNION/INTERSECT/EXCEPT query; only the ORDER BY and LIMIT
	 * fields are interesting in the top query itself.
	 */
	if (query->setOperations)
	{
		get_setop_query(query->setOperations, query, context, resultDesc);
		/* ORDER BY clauses must be simple in this case */
		force_colno = true;
	}
	else
	{
		get_basic_select_query(query, context, resultDesc);
		force_colno = false;
	}

	/* Add the ORDER BY clause if given */
	if (query->sortClause != NIL)
	{
		appendStringInfo(buf, " ORDER BY ");
		sep = "";
		foreach(l, query->sortClause)
		{
			SortClause *srt = (SortClause *) lfirst(l);
			Node	   *sortexpr;
			Oid			sortcoltype;
			char	   *opname;

			appendStringInfo(buf, sep);
			sortexpr = get_rule_sortgroupclause(srt, query->targetList,
												force_colno, context);
			sortcoltype = exprType(sortexpr);
			opname = generate_operator_name(srt->sortop,
											sortcoltype, sortcoltype);
			if (strcmp(opname, "<") != 0)
			{
				if (strcmp(opname, ">") == 0)
					appendStringInfo(buf, " DESC");
				else
					appendStringInfo(buf, " USING %s", opname);
			}
			sep = ", ";
		}
	}

	/* Add the LIMIT clause if given */
	if (query->limitOffset != NULL)
	{
		appendStringInfo(buf, " OFFSET ");
		get_rule_expr(query->limitOffset, context, false);
	}
	if (query->limitCount != NULL)
	{
		appendStringInfo(buf, " LIMIT ");
		if (IsA(query->limitCount, Const) &&
			((Const *) query->limitCount)->constisnull)
			appendStringInfo(buf, "ALL");
		else
			get_rule_expr(query->limitCount, context, false);
	}
}

static void
get_basic_select_query(Query *query, deparse_context *context,
					   TupleDesc resultDesc)
{
	StringInfo	buf = context->buf;
	char	   *sep;
	List	   *l;
	int			colno;

	/*
	 * Build up the query string - first we say SELECT
	 */
	appendStringInfo(buf, "SELECT");

	/* Add the DISTINCT clause if given */
	if (query->distinctClause != NIL)
	{
		if (has_distinct_on_clause(query))
		{
			appendStringInfo(buf, " DISTINCT ON (");
			sep = "";
			foreach(l, query->distinctClause)
			{
				SortClause *srt = (SortClause *) lfirst(l);

				appendStringInfo(buf, sep);
				get_rule_sortgroupclause(srt, query->targetList,
										 false, context);
				sep = ", ";
			}
			appendStringInfo(buf, ")");
		}
		else
			appendStringInfo(buf, " DISTINCT");
	}

	/* Then we tell what to select (the targetlist) */
	sep = " ";
	colno = 0;
	foreach(l, query->targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);
		bool		tell_as = false;
		char	   *colname;

		if (tle->resdom->resjunk)
			continue;			/* ignore junk entries */

		appendStringInfo(buf, sep);
		sep = ", ";
		colno++;

		get_rule_expr(tle->expr, context, true);

		/*
		 * Figure out what the result column should be called.	In the
		 * context of a view, use the view's tuple descriptor (so as to
		 * pick up the effects of any column RENAME that's been done on
		 * the view).  Otherwise, just use what we can find in the TLE.
		 */
		if (resultDesc && colno <= resultDesc->natts)
			colname = NameStr(resultDesc->attrs[colno - 1]->attname);
		else
			colname = tle->resdom->resname;

		/* Check if we must say AS ... */
		if (!IsA(tle->expr, Var))
			tell_as = (strcmp(colname, "?column?") != 0);
		else
		{
			Var		   *var = (Var *) (tle->expr);
			char	   *schemaname;
			char	   *refname;
			char	   *attname;

			get_names_for_var(var, context, &schemaname, &refname, &attname);
			tell_as = (attname == NULL ||
					   strcmp(attname, colname) != 0);
		}

		/* and do if so */
		if (tell_as)
			appendStringInfo(buf, " AS %s", quote_identifier(colname));
	}

	/* Add the FROM clause if needed */
	get_from_clause(query, context);

	/* Add the WHERE clause if given */
	if (query->jointree->quals != NULL)
	{
		appendStringInfo(buf, " WHERE ");
		get_rule_expr(query->jointree->quals, context, false);
	}

	/* Add the GROUP BY clause if given */
	if (query->groupClause != NULL)
	{
		appendStringInfo(buf, " GROUP BY ");
		sep = "";
		foreach(l, query->groupClause)
		{
			GroupClause *grp = (GroupClause *) lfirst(l);

			appendStringInfo(buf, sep);
			get_rule_sortgroupclause(grp, query->targetList,
									 false, context);
			sep = ", ";
		}
	}

	/* Add the HAVING clause if given */
	if (query->havingQual != NULL)
	{
		appendStringInfo(buf, " HAVING ");
		get_rule_expr(query->havingQual, context, false);
	}
}

static void
get_setop_query(Node *setOp, Query *query, deparse_context *context,
				TupleDesc resultDesc)
{
	StringInfo	buf = context->buf;

	if (IsA(setOp, RangeTblRef))
	{
		RangeTblRef *rtr = (RangeTblRef *) setOp;
		RangeTblEntry *rte = rt_fetch(rtr->rtindex, query->rtable);
		Query	   *subquery = rte->subquery;

		Assert(subquery != NULL);
		get_query_def(subquery, buf, context->namespaces, resultDesc);
	}
	else if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *op = (SetOperationStmt *) setOp;

		appendStringInfo(buf, "((");
		get_setop_query(op->larg, query, context, resultDesc);
		switch (op->op)
		{
			case SETOP_UNION:
				appendStringInfo(buf, ") UNION ");
				break;
			case SETOP_INTERSECT:
				appendStringInfo(buf, ") INTERSECT ");
				break;
			case SETOP_EXCEPT:
				appendStringInfo(buf, ") EXCEPT ");
				break;
			default:
				elog(ERROR, "get_setop_query: unexpected set op %d",
					 (int) op->op);
		}
		if (op->all)
			appendStringInfo(buf, "ALL (");
		else
			appendStringInfo(buf, "(");
		get_setop_query(op->rarg, query, context, resultDesc);
		appendStringInfo(buf, "))");
	}
	else
	{
		elog(ERROR, "get_setop_query: unexpected node %d",
			 (int) nodeTag(setOp));
	}
}

/*
 * Display a sort/group clause.
 *
 * Also returns the expression tree, so caller need not find it again.
 */
static Node *
get_rule_sortgroupclause(SortClause *srt, List *tlist, bool force_colno,
						 deparse_context *context)
{
	StringInfo	buf = context->buf;
	TargetEntry *tle;
	Node	   *expr;

	tle = get_sortgroupclause_tle(srt, tlist);
	expr = tle->expr;

	/*
	 * Use column-number form if requested by caller or if expression is a
	 * constant --- a constant is ambiguous (and will be misinterpreted by
	 * findTargetlistEntry()) if we dump it explicitly.
	 */
	if (force_colno || (expr && IsA(expr, Const)))
	{
		Assert(!tle->resdom->resjunk);
		appendStringInfo(buf, "%d", tle->resdom->resno);
	}
	else
		get_rule_expr(expr, context, true);

	return expr;
}

/* ----------
 * get_insert_query_def			- Parse back an INSERT parsetree
 * ----------
 */
static void
get_insert_query_def(Query *query, deparse_context *context)
{
	StringInfo	buf = context->buf;
	RangeTblEntry *select_rte = NULL;
	RangeTblEntry *rte;
	char	   *sep;
	List	   *l;

	/*
	 * If it's an INSERT ... SELECT there will be a single subquery RTE
	 * for the SELECT.
	 */
	foreach(l, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(l);
		if (rte->rtekind != RTE_SUBQUERY)
			continue;
		if (select_rte)
			elog(ERROR, "get_insert_query_def: too many RTEs in INSERT!");
		select_rte = rte;
	}

	/*
	 * Start the query with INSERT INTO relname
	 */
	rte = rt_fetch(query->resultRelation, query->rtable);
	Assert(rte->rtekind == RTE_RELATION);
	appendStringInfo(buf, "INSERT INTO %s",
					 generate_relation_name(rte->relid));

	/* Add the insert-column-names list */
	sep = " (";
	foreach(l, query->targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);

		if (tle->resdom->resjunk)
			continue;			/* ignore junk entries */

		appendStringInfo(buf, sep);
		sep = ", ";
		appendStringInfo(buf, "%s", quote_identifier(tle->resdom->resname));
	}
	appendStringInfo(buf, ") ");

	/* Add the VALUES or the SELECT */
	if (select_rte == NULL)
	{
		appendStringInfo(buf, "VALUES (");
		sep = "";
		foreach(l, query->targetList)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(l);

			if (tle->resdom->resjunk)
				continue;		/* ignore junk entries */

			appendStringInfo(buf, sep);
			sep = ", ";
			get_rule_expr(tle->expr, context, false);
		}
		appendStringInfoChar(buf, ')');
	}
	else
		get_query_def(select_rte->subquery, buf, NIL, NULL);
}


/* ----------
 * get_update_query_def			- Parse back an UPDATE parsetree
 * ----------
 */
static void
get_update_query_def(Query *query, deparse_context *context)
{
	StringInfo	buf = context->buf;
	char	   *sep;
	RangeTblEntry *rte;
	List	   *l;

	/*
	 * Start the query with UPDATE relname SET
	 */
	rte = rt_fetch(query->resultRelation, query->rtable);
	Assert(rte->rtekind == RTE_RELATION);
	appendStringInfo(buf, "UPDATE %s%s SET ",
					 only_marker(rte),
					 generate_relation_name(rte->relid));

	/* Add the comma separated list of 'attname = value' */
	sep = "";
	foreach(l, query->targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);

		if (tle->resdom->resjunk)
			continue;			/* ignore junk entries */

		appendStringInfo(buf, sep);
		sep = ", ";

		/*
		 * If the update expression is an array assignment, we mustn't put
		 * out "attname =" here; it will come out of the display of the
		 * ArrayRef node instead.
		 */
		if (!tleIsArrayAssign(tle))
			appendStringInfo(buf, "%s = ",
							 quote_identifier(tle->resdom->resname));
		get_rule_expr(tle->expr, context, false);
	}

	/* Add the FROM clause if needed */
	get_from_clause(query, context);

	/* Finally add a WHERE clause if given */
	if (query->jointree->quals != NULL)
	{
		appendStringInfo(buf, " WHERE ");
		get_rule_expr(query->jointree->quals, context, false);
	}
}


/* ----------
 * get_delete_query_def			- Parse back a DELETE parsetree
 * ----------
 */
static void
get_delete_query_def(Query *query, deparse_context *context)
{
	StringInfo	buf = context->buf;
	RangeTblEntry *rte;

	/*
	 * Start the query with DELETE FROM relname
	 */
	rte = rt_fetch(query->resultRelation, query->rtable);
	Assert(rte->rtekind == RTE_RELATION);
	appendStringInfo(buf, "DELETE FROM %s%s",
					 only_marker(rte),
					 generate_relation_name(rte->relid));

	/* Add a WHERE clause if given */
	if (query->jointree->quals != NULL)
	{
		appendStringInfo(buf, " WHERE ");
		get_rule_expr(query->jointree->quals, context, false);
	}
}


/* ----------
 * get_utility_query_def			- Parse back a UTILITY parsetree
 * ----------
 */
static void
get_utility_query_def(Query *query, deparse_context *context)
{
	StringInfo	buf = context->buf;

	if (query->utilityStmt && IsA(query->utilityStmt, NotifyStmt))
	{
		NotifyStmt *stmt = (NotifyStmt *) query->utilityStmt;

		appendStringInfo(buf, "NOTIFY %s",
				   quote_qualified_identifier(stmt->relation->schemaname,
											  stmt->relation->relname));
	}
	else
		elog(ERROR, "get_utility_query_def: unexpected statement type");
}


/*
 * Get the schemaname, refname and attname for a (possibly nonlocal) Var.
 *
 * schemaname is usually returned as NULL.	It will be non-null only if
 * use of the unqualified refname would find the wrong RTE.
 *
 * refname will be returned as NULL if the Var references an unnamed join.
 * In this case the Var *must* be displayed without any qualification.
 *
 * attname will be returned as NULL if the Var represents a whole tuple
 * of the relation.  (Typically we'd want to display the Var as "foo.*",
 * but it's convenient to return NULL to make it easier for callers to
 * distinguish this case.)
 */
static void
get_names_for_var(Var *var, deparse_context *context,
				  char **schemaname, char **refname, char **attname)
{
	List	   *nslist = context->namespaces;
	int			sup = var->varlevelsup;
	deparse_namespace *dpns;
	RangeTblEntry *rte;

	/* Find appropriate nesting depth */
	while (sup-- > 0 && nslist != NIL)
		nslist = lnext(nslist);
	if (nslist == NIL)
		elog(ERROR, "get_names_for_var: bogus varlevelsup %d",
			 var->varlevelsup);
	dpns = (deparse_namespace *) lfirst(nslist);

	/* Find the relevant RTE */
	if (var->varno >= 1 && var->varno <= length(dpns->rtable))
		rte = rt_fetch(var->varno, dpns->rtable);
	else if (var->varno == dpns->outer_varno)
		rte = dpns->outer_rte;
	else if (var->varno == dpns->inner_varno)
		rte = dpns->inner_rte;
	else
		rte = NULL;
	if (rte == NULL)
		elog(ERROR, "get_names_for_var: bogus varno %d",
			 var->varno);

	/* Emit results */
	*schemaname = NULL;			/* default assumptions */
	*refname = rte->eref->aliasname;

	/* Exceptions occur only if the RTE is alias-less */
	if (rte->alias == NULL)
	{
		if (rte->rtekind == RTE_RELATION)
		{
			/*
			 * It's possible that use of the bare refname would find
			 * another more-closely-nested RTE, or be ambiguous, in which
			 * case we need to specify the schemaname to avoid these
			 * errors.
			 */
			if (find_rte_by_refname(rte->eref->aliasname, context) != rte)
				*schemaname =
					get_namespace_name(get_rel_namespace(rte->relid));
		}
		else if (rte->rtekind == RTE_JOIN)
		{
			/* Unnamed join has neither schemaname nor refname */
			*refname = NULL;
		}
	}

	if (var->varattno == InvalidAttrNumber)
		*attname = NULL;
	else
		*attname = get_rte_attribute_name(rte, var->varattno);
}

/*
 * find_rte_by_refname		- look up an RTE by refname in a deparse context
 *
 * Returns NULL if there is no matching RTE or the refname is ambiguous.
 *
 * NOTE: this code is not really correct since it does not take account of
 * the fact that not all the RTEs in a rangetable may be visible from the
 * point where a Var reference appears.  For the purposes we need, however,
 * the only consequence of a false match is that we might stick a schema
 * qualifier on a Var that doesn't really need it.  So it seems close
 * enough.
 */
static RangeTblEntry *
find_rte_by_refname(const char *refname, deparse_context *context)
{
	RangeTblEntry *result = NULL;
	List	   *nslist;

	foreach(nslist, context->namespaces)
	{
		deparse_namespace *dpns = (deparse_namespace *) lfirst(nslist);
		List	   *rtlist;

		foreach(rtlist, dpns->rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry *) lfirst(rtlist);

			if (strcmp(rte->eref->aliasname, refname) == 0)
			{
				if (result)
					return NULL;	/* it's ambiguous */
				result = rte;
			}
		}
		if (dpns->outer_rte &&
			strcmp(dpns->outer_rte->eref->aliasname, refname) == 0)
		{
			if (result)
				return NULL;	/* it's ambiguous */
			result = dpns->outer_rte;
		}
		if (dpns->inner_rte &&
			strcmp(dpns->inner_rte->eref->aliasname, refname) == 0)
		{
			if (result)
				return NULL;	/* it's ambiguous */
			result = dpns->inner_rte;
		}
		if (result)
			break;
	}
	return result;
}


/* ----------
 * get_rule_expr			- Parse back an expression
 *
 * Note: showimplicit determines whether we display any implicit cast that
 * is present at the top of the expression tree.  It is a passed argument,
 * not a field of the context struct, because we change the value as we
 * recurse down into the expression.  In general we suppress implicit casts
 * when the result type is known with certainty (eg, the arguments of an
 * OR must be boolean).  We display implicit casts for arguments of functions
 * and operators, since this is needed to be certain that the same function
 * or operator will be chosen when the expression is re-parsed.
 * ----------
 */
static void
get_rule_expr(Node *node, deparse_context *context,
			  bool showimplicit)
{
	StringInfo	buf = context->buf;

	if (node == NULL)
		return;

	/*
	 * Each level of get_rule_expr must emit an indivisible term
	 * (parenthesized if necessary) to ensure result is reparsed into the
	 * same expression tree.
	 *
	 * There might be some work left here to support additional node types.
	 * Can we ever see Param nodes here?
	 */
	switch (nodeTag(node))
	{
		case T_Const:
			get_const_expr((Const *) node, context);
			break;

		case T_Var:
			{
				Var		   *var = (Var *) node;
				char	   *schemaname;
				char	   *refname;
				char	   *attname;

				get_names_for_var(var, context,
								  &schemaname, &refname, &attname);
				if (refname && (context->varprefix || attname == NULL))
				{
					if (schemaname)
						appendStringInfo(buf, "%s.",
										 quote_identifier(schemaname));

					if (strcmp(refname, "*NEW*") == 0)
						appendStringInfo(buf, "new.");
					else if (strcmp(refname, "*OLD*") == 0)
						appendStringInfo(buf, "old.");
					else
						appendStringInfo(buf, "%s.",
										 quote_identifier(refname));
				}
				if (attname)
					appendStringInfo(buf, "%s", quote_identifier(attname));
				else
					appendStringInfo(buf, "*");
			}
			break;

		case T_Expr:
			{
				Expr	   *expr = (Expr *) node;
				List	   *args = expr->args;

				/*
				 * Expr nodes have to be handled a bit detailed
				 */
				switch (expr->opType)
				{
					case OP_EXPR:
						get_oper_expr(expr, context);
						break;

					case DISTINCT_EXPR:
						appendStringInfoChar(buf, '(');
						Assert(length(args) == 2);
						{
							/* binary operator */
							Node	   *arg1 = (Node *) lfirst(args);
							Node	   *arg2 = (Node *) lsecond(args);

							get_rule_expr(arg1, context, true);
							appendStringInfo(buf, " IS DISTINCT FROM ");
							get_rule_expr(arg2, context, true);
						}
						appendStringInfoChar(buf, ')');
						break;

					case FUNC_EXPR:
						get_func_expr(expr, context, showimplicit);
						break;

					case OR_EXPR:
						appendStringInfoChar(buf, '(');
						get_rule_expr((Node *) lfirst(args), context, false);
						while ((args = lnext(args)) != NIL)
						{
							appendStringInfo(buf, " OR ");
							get_rule_expr((Node *) lfirst(args), context,
										  false);
						}
						appendStringInfoChar(buf, ')');
						break;

					case AND_EXPR:
						appendStringInfoChar(buf, '(');
						get_rule_expr((Node *) lfirst(args), context, false);
						while ((args = lnext(args)) != NIL)
						{
							appendStringInfo(buf, " AND ");
							get_rule_expr((Node *) lfirst(args), context,
										  false);
						}
						appendStringInfoChar(buf, ')');
						break;

					case NOT_EXPR:
						appendStringInfo(buf, "(NOT ");
						get_rule_expr((Node *) lfirst(args), context, false);
						appendStringInfoChar(buf, ')');
						break;

					case SUBPLAN_EXPR:

						/*
						 * We cannot see an already-planned subplan in
						 * rule deparsing, only while EXPLAINing a query
						 * plan. For now, just punt.
						 */
						appendStringInfo(buf, "(subplan)");
						break;

					default:
						elog(ERROR, "get_rule_expr: expr opType %d not supported",
							 expr->opType);
				}
			}
			break;

		case T_Aggref:
			get_agg_expr((Aggref *) node, context);
			break;

		case T_ArrayRef:
			{
				ArrayRef   *aref = (ArrayRef *) node;
				bool		savevarprefix = context->varprefix;
				List	   *lowlist;
				List	   *uplist;

				/*
				 * If we are doing UPDATE array[n] = expr, we need to
				 * suppress any prefix on the array name.  Currently, that
				 * is the only context in which we will see a non-null
				 * refassgnexpr --- but someday a smarter test may be
				 * needed.
				 */
				if (aref->refassgnexpr)
					context->varprefix = false;
				get_rule_expr(aref->refexpr, context, showimplicit);
				context->varprefix = savevarprefix;
				lowlist = aref->reflowerindexpr;
				foreach(uplist, aref->refupperindexpr)
				{
					appendStringInfo(buf, "[");
					if (lowlist)
					{
						get_rule_expr((Node *) lfirst(lowlist), context,
									  false);
						appendStringInfo(buf, ":");
						lowlist = lnext(lowlist);
					}
					get_rule_expr((Node *) lfirst(uplist), context, false);
					appendStringInfo(buf, "]");
				}
				if (aref->refassgnexpr)
				{
					appendStringInfo(buf, " = ");
					get_rule_expr(aref->refassgnexpr, context, showimplicit);
				}
			}
			break;

		case T_FieldSelect:
			{
				FieldSelect *fselect = (FieldSelect *) node;
				Oid			argType = exprType(fselect->arg);
				Oid			typrelid;
				char	   *fieldname;

				/* lookup arg type and get the field name */
				typrelid = get_typ_typrelid(argType);
				if (!OidIsValid(typrelid))
					elog(ERROR, "Argument type %s of FieldSelect is not a tuple type",
						 format_type_be(argType));
				fieldname = get_relid_attribute_name(typrelid,
													 fselect->fieldnum);

				/*
				 * If the argument is simple enough, we could emit
				 * arg.fieldname, but most cases where FieldSelect is used
				 * are *not* simple.  So, always use parenthesized syntax.
				 */
				appendStringInfoChar(buf, '(');
				get_rule_expr(fselect->arg, context, true);
				appendStringInfo(buf, ").%s", quote_identifier(fieldname));
			}
			break;

		case T_RelabelType:
			{
				RelabelType *relabel = (RelabelType *) node;
				Node	   *arg = relabel->arg;

				if (relabel->relabelformat == COERCE_IMPLICIT_CAST &&
					!showimplicit)
				{
					/* don't show the implicit cast */
					get_rule_expr(arg, context, showimplicit);
				}
				else
				{
					/*
					 * Strip off any type coercions on the input, so we
					 * don't print redundancies like
					 * x::bpchar::character(8).
					 *
					 * XXX Are there any cases where this is a bad idea?
					 */
					arg = strip_type_coercion(arg, relabel->resulttype);

					appendStringInfoChar(buf, '(');
					get_rule_expr(arg, context, showimplicit);
					appendStringInfo(buf, ")::%s",
							format_type_with_typemod(relabel->resulttype,
												 relabel->resulttypmod));
				}
			}
			break;

		case T_CaseExpr:
			{
				CaseExpr   *caseexpr = (CaseExpr *) node;
				List	   *temp;

				appendStringInfo(buf, "CASE");
				foreach(temp, caseexpr->args)
				{
					CaseWhen   *when = (CaseWhen *) lfirst(temp);

					appendStringInfo(buf, " WHEN ");
					get_rule_expr(when->expr, context, false);
					appendStringInfo(buf, " THEN ");
					get_rule_expr(when->result, context, true);
				}
				appendStringInfo(buf, " ELSE ");
				get_rule_expr(caseexpr->defresult, context, true);
				appendStringInfo(buf, " END");
			}
			break;

		case T_NullTest:
			{
				NullTest   *ntest = (NullTest *) node;

				appendStringInfo(buf, "(");
				get_rule_expr(ntest->arg, context, true);
				switch (ntest->nulltesttype)
				{
					case IS_NULL:
						appendStringInfo(buf, " IS NULL)");
						break;
					case IS_NOT_NULL:
						appendStringInfo(buf, " IS NOT NULL)");
						break;
					default:
						elog(ERROR, "get_rule_expr: unexpected nulltesttype %d",
							 (int) ntest->nulltesttype);
				}
			}
			break;

		case T_BooleanTest:
			{
				BooleanTest *btest = (BooleanTest *) node;

				appendStringInfo(buf, "(");
				get_rule_expr(btest->arg, context, false);
				switch (btest->booltesttype)
				{
					case IS_TRUE:
						appendStringInfo(buf, " IS TRUE)");
						break;
					case IS_NOT_TRUE:
						appendStringInfo(buf, " IS NOT TRUE)");
						break;
					case IS_FALSE:
						appendStringInfo(buf, " IS FALSE)");
						break;
					case IS_NOT_FALSE:
						appendStringInfo(buf, " IS NOT FALSE)");
						break;
					case IS_UNKNOWN:
						appendStringInfo(buf, " IS UNKNOWN)");
						break;
					case IS_NOT_UNKNOWN:
						appendStringInfo(buf, " IS NOT UNKNOWN)");
						break;
					default:
						elog(ERROR, "get_rule_expr: unexpected booltesttype %d",
							 (int) btest->booltesttype);
				}
			}
			break;

		case T_ConstraintTest:
			{
				ConstraintTest *ctest = (ConstraintTest *) node;

				/*
				 * We assume that the operations of the constraint node
				 * need not be explicitly represented in the output.
				 */
				get_rule_expr(ctest->arg, context, showimplicit);
			}
			break;

		case T_SubLink:
			get_sublink_expr(node, context);
			break;

		case T_Param:
			{
				Param	   *param = (Param *) node;

				switch (param->paramkind)
				{
					case PARAM_NAMED:
					case PARAM_NEW:
					case PARAM_OLD:
						appendStringInfo(buf, "$%s", param->paramname);
						break;
					case PARAM_NUM:
					case PARAM_EXEC:
						appendStringInfo(buf, "$%d", param->paramid);
						break;
					default:
						appendStringInfo(buf, "(param)");
						break;
				}
			}
			break;

		default:
			elog(ERROR, "get_rule_expr: unknown node type %d", nodeTag(node));
			break;
	}
}


/*
 * get_oper_expr			- Parse back an Oper node
 */
static void
get_oper_expr(Expr *expr, deparse_context *context)
{
	StringInfo	buf = context->buf;
	Oid			opno = ((Oper *) expr->oper)->opno;
	List	   *args = expr->args;

	appendStringInfoChar(buf, '(');
	if (length(args) == 2)
	{
		/* binary operator */
		Node	   *arg1 = (Node *) lfirst(args);
		Node	   *arg2 = (Node *) lsecond(args);

		get_rule_expr(arg1, context, true);
		appendStringInfo(buf, " %s ",
						 generate_operator_name(opno,
												exprType(arg1),
												exprType(arg2)));
		get_rule_expr(arg2, context, true);
	}
	else
	{
		/* unary operator --- but which side? */
		Node	   *arg = (Node *) lfirst(args);
		HeapTuple	tp;
		Form_pg_operator optup;

		tp = SearchSysCache(OPEROID,
							ObjectIdGetDatum(opno),
							0, 0, 0);
		if (!HeapTupleIsValid(tp))
			elog(ERROR, "cache lookup for operator %u failed", opno);
		optup = (Form_pg_operator) GETSTRUCT(tp);
		switch (optup->oprkind)
		{
			case 'l':
				appendStringInfo(buf, "%s ",
								 generate_operator_name(opno,
														InvalidOid,
														exprType(arg)));
				get_rule_expr(arg, context, true);
				break;
			case 'r':
				get_rule_expr(arg, context, true);
				appendStringInfo(buf, " %s",
								 generate_operator_name(opno,
														exprType(arg),
														InvalidOid));
				break;
			default:
				elog(ERROR, "get_rule_expr: bogus oprkind");
		}
		ReleaseSysCache(tp);
	}
	appendStringInfoChar(buf, ')');
}

/*
 * get_func_expr			- Parse back a Func node
 */
static void
get_func_expr(Expr *expr, deparse_context *context,
			  bool showimplicit)
{
	StringInfo	buf = context->buf;
	Func	   *func = (Func *) (expr->oper);
	Oid			funcoid = func->funcid;
	Oid			argtypes[FUNC_MAX_ARGS];
	int			nargs;
	List	   *l;
	char	   *sep;

	/*
	 * If the function call came from an implicit coercion, then just show
	 * the first argument --- unless caller wants to see implicit
	 * coercions.
	 */
	if (func->funcformat == COERCE_IMPLICIT_CAST && !showimplicit)
	{
		get_rule_expr((Node *) lfirst(expr->args), context, showimplicit);
		return;
	}

	/*
	 * If the function call came from a cast, then show the first argument
	 * plus an explicit cast operation.
	 */
	if (func->funcformat == COERCE_EXPLICIT_CAST ||
		func->funcformat == COERCE_IMPLICIT_CAST)
	{
		Node	   *arg = lfirst(expr->args);
		Oid			rettype = expr->typeOid;
		int32		coercedTypmod;

		/* Get the typmod if this is a length-coercion function */
		(void) exprIsLengthCoercion((Node *) expr, &coercedTypmod);

		/*
		 * Strip off any type coercions on the input, so we don't print
		 * redundancies like x::bpchar::character(8).
		 *
		 * XXX Are there any cases where this is a bad idea?
		 */
		arg = strip_type_coercion(arg, rettype);

		appendStringInfoChar(buf, '(');
		get_rule_expr(arg, context, showimplicit);
		appendStringInfo(buf, ")::%s",
					   format_type_with_typemod(rettype, coercedTypmod));

		return;
	}

	/*
	 * Normal function: display as proname(args).  First we need to
	 * extract the argument datatypes.
	 */
	nargs = 0;
	foreach(l, expr->args)
	{
		Assert(nargs < FUNC_MAX_ARGS);
		argtypes[nargs] = exprType((Node *) lfirst(l));
		nargs++;
	}

	appendStringInfo(buf, "%s(",
					 generate_function_name(funcoid, nargs, argtypes));

	sep = "";
	foreach(l, expr->args)
	{
		appendStringInfo(buf, sep);
		sep = ", ";
		get_rule_expr((Node *) lfirst(l), context, true);
	}
	appendStringInfoChar(buf, ')');
}

/*
 * get_agg_expr			- Parse back an Aggref node
 */
static void
get_agg_expr(Aggref *aggref, deparse_context *context)
{
	StringInfo	buf = context->buf;
	Oid			argtype = exprType(aggref->target);

	appendStringInfo(buf, "%s(%s",
				   generate_function_name(aggref->aggfnoid, 1, &argtype),
					 aggref->aggdistinct ? "DISTINCT " : "");
	if (aggref->aggstar)
		appendStringInfo(buf, "*");
	else
		get_rule_expr(aggref->target, context, true);
	appendStringInfoChar(buf, ')');
}


/*
 * strip_type_coercion
 *		Strip any type coercion at the top of the given expression tree,
 *		if it is a coercion to the given datatype.
 *
 * We use this to avoid printing two levels of coercion in situations where
 * the expression tree has a length-coercion node atop a type-coercion node.
 *
 * Note: avoid stripping a length-coercion node, since two successive
 * coercions to different lengths aren't a no-op.
 */
static Node *
strip_type_coercion(Node *expr, Oid resultType)
{
	if (expr == NULL || exprType(expr) != resultType)
		return expr;

	if (IsA(expr, RelabelType) &&
		((RelabelType *) expr)->resulttypmod == -1)
		return ((RelabelType *) expr)->arg;

	if (IsA(expr, Expr) &&
		((Expr *) expr)->opType == FUNC_EXPR)
	{
		Func	   *func = (Func *) (((Expr *) expr)->oper);

		Assert(IsA(func, Func));
		if (func->funcformat != COERCE_EXPLICIT_CAST &&
			func->funcformat != COERCE_IMPLICIT_CAST)
			return expr;		/* don't absorb into upper coercion */

		if (exprIsLengthCoercion(expr, NULL))
			return expr;

		return (Node *) lfirst(((Expr *) expr)->args);
	}

	return expr;
}


/* ----------
 * get_const_expr
 *
 *	Make a string representation of a Const
 * ----------
 */
static void
get_const_expr(Const *constval, deparse_context *context)
{
	StringInfo	buf = context->buf;
	HeapTuple	typetup;
	Form_pg_type typeStruct;
	char	   *extval;
	char	   *valptr;
	bool		isfloat = false;
	bool		needlabel;

	if (constval->constisnull)
	{
		/*
		 * Always label the type of a NULL constant to prevent
		 * misdecisions about type when reparsing.
		 */
		appendStringInfo(buf, "NULL::%s",
					  format_type_with_typemod(constval->consttype, -1));
		return;
	}

	typetup = SearchSysCache(TYPEOID,
							 ObjectIdGetDatum(constval->consttype),
							 0, 0, 0);
	if (!HeapTupleIsValid(typetup))
		elog(ERROR, "cache lookup of type %u failed", constval->consttype);

	typeStruct = (Form_pg_type) GETSTRUCT(typetup);

	extval = DatumGetCString(OidFunctionCall3(typeStruct->typoutput,
											  constval->constvalue,
								   ObjectIdGetDatum(typeStruct->typelem),
											  Int32GetDatum(-1)));

	switch (constval->consttype)
	{
		case INT2OID:
		case INT4OID:
		case INT8OID:
		case OIDOID:
		case FLOAT4OID:
		case FLOAT8OID:
		case NUMERICOID:
			{
				/*
				 * These types are printed without quotes unless they
				 * contain values that aren't accepted by the scanner
				 * unquoted (e.g., 'NaN').	Note that strtod() and friends
				 * might accept NaN, so we can't use that to test.
				 *
				 * In reality we only need to defend against infinity and
				 * NaN, so we need not get too crazy about pattern
				 * matching here.
				 */
				if (strspn(extval, "0123456789+-eE.") == strlen(extval))
				{
					appendStringInfo(buf, extval);
					if (strcspn(extval, "eE.") != strlen(extval))
						isfloat = true; /* it looks like a float */
				}
				else
					appendStringInfo(buf, "'%s'", extval);
			}
			break;

		case BITOID:
		case VARBITOID:
			appendStringInfo(buf, "B'%s'", extval);
			break;

		case BOOLOID:
			if (strcmp(extval, "t") == 0)
				appendStringInfo(buf, "true");
			else
				appendStringInfo(buf, "false");
			break;

		default:

			/*
			 * We must quote any funny characters in the constant's
			 * representation. XXX Any MULTIBYTE considerations here?
			 */
			appendStringInfoChar(buf, '\'');
			for (valptr = extval; *valptr; valptr++)
			{
				char		ch = *valptr;

				if (ch == '\'' || ch == '\\')
				{
					appendStringInfoChar(buf, '\\');
					appendStringInfoChar(buf, ch);
				}
				else if (((unsigned char) ch) < ((unsigned char) ' '))
					appendStringInfo(buf, "\\%03o", (int) ch);
				else
					appendStringInfoChar(buf, ch);
			}
			appendStringInfoChar(buf, '\'');
			break;
	}

	pfree(extval);

	/*
	 * Append ::typename unless the constant will be implicitly typed as
	 * the right type when it is read in.  XXX this code has to be kept in
	 * sync with the behavior of the parser, especially make_const.
	 */
	switch (constval->consttype)
	{
		case BOOLOID:
		case INT4OID:
		case UNKNOWNOID:
			/* These types can be left unlabeled */
			needlabel = false;
			break;
		case NUMERICOID:
			/* Float-looking constants will be typed as numeric */
			needlabel = !isfloat;
			break;
		default:
			needlabel = true;
			break;
	}
	if (needlabel)
		appendStringInfo(buf, "::%s",
					  format_type_with_typemod(constval->consttype, -1));

	ReleaseSysCache(typetup);
}


/* ----------
 * get_sublink_expr			- Parse back a sublink
 * ----------
 */
static void
get_sublink_expr(Node *node, deparse_context *context)
{
	StringInfo	buf = context->buf;
	SubLink    *sublink = (SubLink *) node;
	Query	   *query = (Query *) (sublink->subselect);
	List	   *l;
	char	   *sep;
	Oper	   *oper;
	bool		need_paren;

	appendStringInfoChar(buf, '(');

	if (sublink->lefthand != NIL)
	{
		need_paren = (length(sublink->lefthand) > 1);
		if (need_paren)
			appendStringInfoChar(buf, '(');

		sep = "";
		foreach(l, sublink->lefthand)
		{
			appendStringInfo(buf, sep);
			sep = ", ";
			get_rule_expr((Node *) lfirst(l), context, true);
		}

		if (need_paren)
			appendStringInfo(buf, ") ");
		else
			appendStringInfoChar(buf, ' ');
	}

	need_paren = true;

	/*
	 * XXX we assume here that we can get away without qualifying the
	 * operator name.  Since the name may imply multiple physical
	 * operators it's rather difficult to do otherwise --- in fact, if the
	 * operators are in different namespaces any attempt to qualify would
	 * surely fail.
	 */
	switch (sublink->subLinkType)
	{
		case EXISTS_SUBLINK:
			appendStringInfo(buf, "EXISTS ");
			break;

		case ANY_SUBLINK:
			oper = (Oper *) lfirst(sublink->oper);
			appendStringInfo(buf, "%s ANY ", get_opname(oper->opno));
			break;

		case ALL_SUBLINK:
			oper = (Oper *) lfirst(sublink->oper);
			appendStringInfo(buf, "%s ALL ", get_opname(oper->opno));
			break;

		case MULTIEXPR_SUBLINK:
			oper = (Oper *) lfirst(sublink->oper);
			appendStringInfo(buf, "%s ", get_opname(oper->opno));
			break;

		case EXPR_SUBLINK:
			need_paren = false;
			break;

		default:
			elog(ERROR, "get_sublink_expr: unsupported sublink type %d",
				 sublink->subLinkType);
			break;
	}

	if (need_paren)
		appendStringInfoChar(buf, '(');

	get_query_def(query, buf, context->namespaces, NULL);

	if (need_paren)
		appendStringInfo(buf, "))");
	else
		appendStringInfoChar(buf, ')');
}


/* ----------
 * get_from_clause			- Parse back a FROM clause
 * ----------
 */
static void
get_from_clause(Query *query, deparse_context *context)
{
	StringInfo	buf = context->buf;
	char	   *sep;
	List	   *l;

	/*
	 * We use the query's jointree as a guide to what to print.  However,
	 * we must ignore auto-added RTEs that are marked not inFromCl. (These
	 * can only appear at the top level of the jointree, so it's
	 * sufficient to check here.) Also ignore the rule pseudo-RTEs for NEW
	 * and OLD.
	 */
	sep = " FROM ";

	foreach(l, query->jointree->fromlist)
	{
		Node	   *jtnode = (Node *) lfirst(l);

		if (IsA(jtnode, RangeTblRef))
		{
			int			varno = ((RangeTblRef *) jtnode)->rtindex;
			RangeTblEntry *rte = rt_fetch(varno, query->rtable);

			if (!rte->inFromCl)
				continue;
			if (strcmp(rte->eref->aliasname, "*NEW*") == 0)
				continue;
			if (strcmp(rte->eref->aliasname, "*OLD*") == 0)
				continue;
		}

		appendStringInfo(buf, sep);
		get_from_clause_item(jtnode, query, context);
		sep = ", ";
	}
}

static void
get_from_clause_item(Node *jtnode, Query *query, deparse_context *context)
{
	StringInfo	buf = context->buf;

	if (IsA(jtnode, RangeTblRef))
	{
		int			varno = ((RangeTblRef *) jtnode)->rtindex;
		RangeTblEntry *rte = rt_fetch(varno, query->rtable);
		List	   *coldeflist = NIL;
		bool		gavealias = false;

		switch (rte->rtekind)
		{
			case RTE_RELATION:
				/* Normal relation RTE */
				appendStringInfo(buf, "%s%s",
								 only_marker(rte),
								 generate_relation_name(rte->relid));
				break;
			case RTE_SUBQUERY:
				/* Subquery RTE */
				appendStringInfoChar(buf, '(');
				get_query_def(rte->subquery, buf, context->namespaces, NULL);
				appendStringInfoChar(buf, ')');
				break;
			case RTE_FUNCTION:
				/* Function RTE */
				get_rule_expr(rte->funcexpr, context, true);
				/* might need to emit column list for RECORD function */
				coldeflist = rte->coldeflist;
				break;
			default:
				elog(ERROR, "unexpected rte kind %d", (int) rte->rtekind);
				break;
		}
		if (rte->alias != NULL)
		{
			appendStringInfo(buf, " %s",
							 quote_identifier(rte->alias->aliasname));
			gavealias = true;
			if (rte->alias->colnames != NIL && coldeflist == NIL)
			{
				List	   *col;

				appendStringInfo(buf, "(");
				foreach(col, rte->alias->colnames)
				{
					if (col != rte->alias->colnames)
						appendStringInfo(buf, ", ");
					appendStringInfo(buf, "%s",
								  quote_identifier(strVal(lfirst(col))));
				}
				appendStringInfoChar(buf, ')');
			}
		}
		else if (rte->rtekind == RTE_RELATION &&
			 strcmp(rte->eref->aliasname, get_rel_name(rte->relid)) != 0)
		{
			/*
			 * Apparently the rel has been renamed since the rule was
			 * made. Emit a fake alias clause so that variable references
			 * will still work.  This is not a 100% solution but should
			 * work in most reasonable situations.
			 */
			appendStringInfo(buf, " %s",
							 quote_identifier(rte->eref->aliasname));
			gavealias = true;
		}
		if (coldeflist != NIL)
		{
			if (!gavealias)
				appendStringInfo(buf, " AS ");
			get_from_clause_coldeflist(coldeflist, context);
		}
	}
	else if (IsA(jtnode, JoinExpr))
	{
		JoinExpr   *j = (JoinExpr *) jtnode;

		appendStringInfoChar(buf, '(');
		get_from_clause_item(j->larg, query, context);
		if (j->isNatural)
			appendStringInfo(buf, " NATURAL");
		switch (j->jointype)
		{
			case JOIN_INNER:
				if (j->quals)
					appendStringInfo(buf, " JOIN ");
				else
					appendStringInfo(buf, " CROSS JOIN ");
				break;
			case JOIN_LEFT:
				appendStringInfo(buf, " LEFT JOIN ");
				break;
			case JOIN_FULL:
				appendStringInfo(buf, " FULL JOIN ");
				break;
			case JOIN_RIGHT:
				appendStringInfo(buf, " RIGHT JOIN ");
				break;
			case JOIN_UNION:
				appendStringInfo(buf, " UNION JOIN ");
				break;
			default:
				elog(ERROR, "get_from_clause_item: unknown join type %d",
					 (int) j->jointype);
		}
		get_from_clause_item(j->rarg, query, context);
		if (!j->isNatural)
		{
			if (j->using)
			{
				List	   *col;

				appendStringInfo(buf, " USING (");
				foreach(col, j->using)
				{
					if (col != j->using)
						appendStringInfo(buf, ", ");
					appendStringInfo(buf, "%s",
								  quote_identifier(strVal(lfirst(col))));
				}
				appendStringInfoChar(buf, ')');
			}
			else if (j->quals)
			{
				appendStringInfo(buf, " ON (");
				get_rule_expr(j->quals, context, false);
				appendStringInfoChar(buf, ')');
			}
		}
		appendStringInfoChar(buf, ')');
		/* Yes, it's correct to put alias after the right paren ... */
		if (j->alias != NULL)
		{
			appendStringInfo(buf, " %s",
							 quote_identifier(j->alias->aliasname));
			if (j->alias->colnames != NIL)
			{
				List	   *col;

				appendStringInfo(buf, "(");
				foreach(col, j->alias->colnames)
				{
					if (col != j->alias->colnames)
						appendStringInfo(buf, ", ");
					appendStringInfo(buf, "%s",
								  quote_identifier(strVal(lfirst(col))));
				}
				appendStringInfoChar(buf, ')');
			}
		}
	}
	else
		elog(ERROR, "get_from_clause_item: unexpected node type %d",
			 nodeTag(jtnode));
}

/*
 * get_from_clause_coldeflist - reproduce FROM clause coldeflist
 *
 * The coldeflist is appended immediately (no space) to buf.  Caller is
 * responsible for ensuring that an alias or AS is present before it.
 */
static void
get_from_clause_coldeflist(List *coldeflist, deparse_context *context)
{
	StringInfo	buf = context->buf;
	List	   *col;
	int			i = 0;

	appendStringInfoChar(buf, '(');

	foreach(col, coldeflist)
	{
		ColumnDef  *n = lfirst(col);
		char	   *attname;
		Oid			atttypeid;
		int32		atttypmod;

		attname = n->colname;
		atttypeid = typenameTypeId(n->typename);
		atttypmod = n->typename->typmod;

		if (i > 0)
			appendStringInfo(buf, ", ");
		appendStringInfo(buf, "%s %s",
						 quote_identifier(attname),
						 format_type_with_typemod(atttypeid, atttypmod));
		i++;
	}

	appendStringInfoChar(buf, ')');
}

/*
 * get_opclass_name			- fetch name of an index operator class
 *
 * The opclass name is appended (after a space) to buf.
 *
 * Output is suppressed if the opclass is the default for the given
 * actual_datatype.  (If you don't want this behavior, just pass
 * InvalidOid for actual_datatype.)
 */
static void
get_opclass_name(Oid opclass, Oid actual_datatype,
				 StringInfo buf)
{
	HeapTuple	ht_opc;
	Form_pg_opclass opcrec;
	char	   *opcname;
	char	   *nspname;

	/* Domains use their base type's default opclass */
	if (OidIsValid(actual_datatype))
		actual_datatype = getBaseType(actual_datatype);

	ht_opc = SearchSysCache(CLAOID,
							ObjectIdGetDatum(opclass),
							0, 0, 0);
	if (!HeapTupleIsValid(ht_opc))
		elog(ERROR, "cache lookup failed for opclass %u", opclass);
	opcrec = (Form_pg_opclass) GETSTRUCT(ht_opc);
	if (actual_datatype != opcrec->opcintype || !opcrec->opcdefault)
	{
		/* Okay, we need the opclass name.	Do we need to qualify it? */
		opcname = NameStr(opcrec->opcname);
		if (OpclassIsVisible(opclass))
			appendStringInfo(buf, " %s", quote_identifier(opcname));
		else
		{
			nspname = get_namespace_name(opcrec->opcnamespace);
			appendStringInfo(buf, " %s.%s",
							 quote_identifier(nspname),
							 quote_identifier(opcname));
		}
	}
	ReleaseSysCache(ht_opc);
}

/*
 * tleIsArrayAssign			- check for array assignment
 */
static bool
tleIsArrayAssign(TargetEntry *tle)
{
	ArrayRef   *aref;

	if (tle->expr == NULL || !IsA(tle->expr, ArrayRef))
		return false;
	aref = (ArrayRef *) tle->expr;
	if (aref->refassgnexpr == NULL)
		return false;

	/*
	 * Currently, it should only be possible to see non-null refassgnexpr
	 * if we are indeed looking at an "UPDATE array[n] = expr" situation.
	 * So aref->refexpr ought to match the tle's target.
	 */
	if (aref->refexpr == NULL || !IsA(aref->refexpr, Var) ||
		((Var *) aref->refexpr)->varattno != tle->resdom->resno)
		elog(WARNING, "tleIsArrayAssign: I'm confused ...");
	return true;
}

/*
 * quote_identifier			- Quote an identifier only if needed
 *
 * When quotes are needed, we palloc the required space; slightly
 * space-wasteful but well worth it for notational simplicity.
 */
const char *
quote_identifier(const char *ident)
{
	/*
	 * Can avoid quoting if ident starts with a lowercase letter or
	 * underscore and contains only lowercase letters, digits, and
	 * underscores, *and* is not any SQL keyword.  Otherwise, supply
	 * quotes.
	 */
	int			nquotes = 0;
	bool		safe;
	const char *ptr;
	char	   *result;
	char	   *optr;

	/*
	 * would like to use <ctype.h> macros here, but they might yield
	 * unwanted locale-specific results...
	 */
	safe = ((ident[0] >= 'a' && ident[0] <= 'z') || ident[0] == '_');

	for (ptr = ident; *ptr; ptr++)
	{
		char		ch = *ptr;

		if ((ch >= 'a' && ch <= 'z') ||
			(ch >= '0' && ch <= '9') ||
			(ch == '_'))
		{
			/* okay */
		}
		else
		{
			safe = false;
			if (ch == '"')
				nquotes++;
		}
	}

	if (safe)
	{
		/*
		 * Check for keyword.  This test is overly strong, since many of
		 * the "keywords" known to the parser are usable as column names,
		 * but the parser doesn't provide any easy way to test for whether
		 * an identifier is safe or not... so be safe not sorry.
		 *
		 * Note: ScanKeywordLookup() does case-insensitive comparison, but
		 * that's fine, since we already know we have all-lower-case.
		 */
		if (ScanKeywordLookup(ident) != NULL)
			safe = false;
	}

	if (safe)
		return ident;			/* no change needed */

	result = (char *) palloc(strlen(ident) + nquotes + 2 + 1);

	optr = result;
	*optr++ = '"';
	for (ptr = ident; *ptr; ptr++)
	{
		char		ch = *ptr;

		if (ch == '"')
			*optr++ = '"';
		*optr++ = ch;
	}
	*optr++ = '"';
	*optr = '\0';

	return result;
}

/*
 * quote_qualified_identifier	- Quote a possibly-qualified identifier
 *
 * Return a name of the form namespace.ident, or just ident if namespace
 * is NULL, quoting each component if necessary.  The result is palloc'd.
 */
char *
quote_qualified_identifier(const char *namespace,
						   const char *ident)
{
	StringInfoData buf;

	initStringInfo(&buf);
	if (namespace)
		appendStringInfo(&buf, "%s.", quote_identifier(namespace));
	appendStringInfo(&buf, "%s", quote_identifier(ident));
	return buf.data;
}

/*
 * generate_relation_name
 *		Compute the name to display for a relation specified by OID
 *
 * The result includes all necessary quoting and schema-prefixing.
 */
static char *
generate_relation_name(Oid relid)
{
	HeapTuple	tp;
	Form_pg_class reltup;
	char	   *nspname;
	char	   *result;

	tp = SearchSysCache(RELOID,
						ObjectIdGetDatum(relid),
						0, 0, 0);
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup of relation %u failed", relid);
	reltup = (Form_pg_class) GETSTRUCT(tp);

	/* Qualify the name if not visible in search path */
	if (RelationIsVisible(relid))
		nspname = NULL;
	else
		nspname = get_namespace_name(reltup->relnamespace);

	result = quote_qualified_identifier(nspname, NameStr(reltup->relname));

	ReleaseSysCache(tp);

	return result;
}

/*
 * generate_function_name
 *		Compute the name to display for a function specified by OID,
 *		given that it is being called with the specified actual arg types.
 *		(Arg types matter because of ambiguous-function resolution rules.)
 *
 * The result includes all necessary quoting and schema-prefixing.
 */
static char *
generate_function_name(Oid funcid, int nargs, Oid *argtypes)
{
	HeapTuple	proctup;
	Form_pg_proc procform;
	char	   *proname;
	char	   *nspname;
	char	   *result;
	FuncDetailCode p_result;
	Oid			p_funcid;
	Oid			p_rettype;
	bool		p_retset;
	Oid		   *p_true_typeids;

	proctup = SearchSysCache(PROCOID,
							 ObjectIdGetDatum(funcid),
							 0, 0, 0);
	if (!HeapTupleIsValid(proctup))
		elog(ERROR, "cache lookup of function %u failed", funcid);
	procform = (Form_pg_proc) GETSTRUCT(proctup);
	proname = NameStr(procform->proname);
	Assert(nargs == procform->pronargs);

	/*
	 * The idea here is to schema-qualify only if the parser would fail to
	 * resolve the correct function given the unqualified func name with
	 * the specified argtypes.
	 */
	p_result = func_get_detail(makeList1(makeString(proname)),
							   NIL, nargs, argtypes,
							   &p_funcid, &p_rettype,
							   &p_retset, &p_true_typeids);
	if (p_result != FUNCDETAIL_NOTFOUND && p_funcid == funcid)
		nspname = NULL;
	else
		nspname = get_namespace_name(procform->pronamespace);

	result = quote_qualified_identifier(nspname, proname);

	ReleaseSysCache(proctup);

	return result;
}

/*
 * generate_operator_name
 *		Compute the name to display for an operator specified by OID,
 *		given that it is being called with the specified actual arg types.
 *		(Arg types matter because of ambiguous-operator resolution rules.
 *		Pass InvalidOid for unused arg of a unary operator.)
 *
 * The result includes all necessary quoting and schema-prefixing,
 * plus the OPERATOR() decoration needed to use a qualified operator name
 * in an expression.
 */
static char *
generate_operator_name(Oid operid, Oid arg1, Oid arg2)
{
	StringInfoData buf;
	HeapTuple	opertup;
	Form_pg_operator operform;
	char	   *oprname;
	char	   *nspname;
	Operator	p_result;

	initStringInfo(&buf);

	opertup = SearchSysCache(OPEROID,
							 ObjectIdGetDatum(operid),
							 0, 0, 0);
	if (!HeapTupleIsValid(opertup))
		elog(ERROR, "cache lookup of operator %u failed", operid);
	operform = (Form_pg_operator) GETSTRUCT(opertup);
	oprname = NameStr(operform->oprname);

	/*
	 * The idea here is to schema-qualify only if the parser would fail to
	 * resolve the correct operator given the unqualified op name with the
	 * specified argtypes.
	 */
	switch (operform->oprkind)
	{
		case 'b':
			p_result = oper(makeList1(makeString(oprname)), arg1, arg2, true);
			break;
		case 'l':
			p_result = left_oper(makeList1(makeString(oprname)), arg2, true);
			break;
		case 'r':
			p_result = right_oper(makeList1(makeString(oprname)), arg1, true);
			break;
		default:
			elog(ERROR, "unexpected oprkind %c for operator %u",
				 operform->oprkind, operid);
			p_result = NULL;	/* keep compiler quiet */
			break;
	}

	if (p_result != NULL && oprid(p_result) == operid)
		nspname = NULL;
	else
	{
		nspname = get_namespace_name(operform->oprnamespace);
		appendStringInfo(&buf, "OPERATOR(%s.", quote_identifier(nspname));
	}

	appendStringInfo(&buf, "%s", oprname);

	if (nspname)
		appendStringInfoChar(&buf, ')');

	if (p_result != NULL)
		ReleaseSysCache(p_result);

	ReleaseSysCache(opertup);

	return buf.data;
}

/*
 * get_relid_attribute_name
 *		Get an attribute name by its relations Oid and its attnum
 *
 * Same as underlying syscache routine get_attname(), except that error
 * is handled by elog() instead of returning NULL.
 */
static char *
get_relid_attribute_name(Oid relid, AttrNumber attnum)
{
	char	   *attname;

	attname = get_attname(relid, attnum);
	if (attname == NULL)
		elog(ERROR, "cache lookup of attribute %d in relation %u failed",
			 attnum, relid);
	return attname;
}
