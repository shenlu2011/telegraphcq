/*-------------------------------------------------------------------------
 *
 * parse_clause.c
 *	  handle clauses in parser
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/parser/parse_clause.c,v 1.26 2005/07/27 20:04:08 phred Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "catalog/heap.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "optimizer/clauses.h"
#include "parser/analyze.h"
#include "parser/parse.h"
#include "parser/parsetree.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"	/* @dwndwSK */
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "telegraphcq/telegraphcqinit.h"		/* @dtstampSK */
#include "telegraphcq/telegraphcq.h"		/* @dtstampSK */
#include "catalog/namespace.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/relcache.h"		/* @dwndwSK */

#include "telegraphcq/window.h" /* For round_timestamp() */

#define ORDER_CLAUSE 0
#define GROUP_CLAUSE 1
#define DISTINCT_ON_CLAUSE 2

static char *clauseText[] = {"ORDER BY", "GROUP BY", "DISTINCT ON"};

static void extractRemainingColumns(List *common_colnames,
						List *src_colnames, List *src_colvars,
						List **res_colnames, List **res_colvars);
static Node *transformJoinUsingClause(ParseState *pstate,
						 List *leftVars, List *rightVars);
static Node *transformJoinOnClause(ParseState *pstate, JoinExpr *j,
					  List *containedRels);
static RangeTblRef *transformTableEntry(ParseState *pstate, RangeVar *r);
static RangeTblRef *transformRangeSubselect(ParseState *pstate,
						RangeSubselect *r);
static RangeTblRef *transformRangeFunction(ParseState *pstate,
					   RangeFunction *r);
static Node *transformFromClauseItem(ParseState *pstate, Node *n,
						List **containedRels);
static Node *buildMergedJoinVar(JoinType jointype,
				   Var *l_colvar, Var *r_colvar);
static TargetEntry *findTargetlistEntry(ParseState *pstate, Node *node,
					List *tlist, int clause);
static List *addTargetToSortList(TargetEntry *tle, List *sortlist,
					List *targetlist, List *opname);
static bool targetIsInSortList(TargetEntry *tle, List *sortList);

/* TCQOC
 * transforms the RECURSIVE_RESULT IS clause of a recursive query.  It ensures that the 
 * target relation for the query is compatible with the output of this query.  If so, the 
 * OID of the target relation is returned, otherwise an error is trown
 */

Oid transformRecurseRelation(ParseState *pstate, 
							 Node * recurse,
							 List * targetlist)
{
	Oid resultOid = InvalidOid;
	Relation rel = NULL;
	List * iter=NIL;
	List * attrnos =NIL;
	List * cols=NIL;
	TargetEntry *tle = NULL;
	ResTarget *col=NULL;

	if(recurse == NULL)
		return InvalidOid;
	if(!IsA(recurse,RangeVar))
		{
			elog(ERROR, "recurse relation node should have been a RangeVar but was not");
		}
	
	resultOid = RangeVarGetRelid((RangeVar*)recurse,false);
	rel = relation_open(resultOid, AccessShareLock);
	pstate->p_target_relation = rel;
	if(rel->rd_att->natts != length(targetlist))
		elog(ERROR, "number of attributes in select list of recursive query must match number in the target");
	cols = checkInsertTargets(pstate, NULL, &attrnos);

	foreach(iter, targetlist)
		{
			if(cols == NIL || attrnos == NULL)
				elog(ERROR, "RQP: unexpected error condition");

			col = (ResTarget *) lfirst(cols);
			tle = lfirst(iter);
			updateTargetListEntry(pstate, tle, col->name, lfirsti(attrnos), NULL);
			cols=lnext(cols);
			attrnos=lnext(attrnos);
		}
								  
	relation_close(rel, AccessShareLock);
	pstate->p_target_relation=NULL;
	return resultOid;

}

/*
 * transformFromClause -
 *	  Process the FROM clause and add items to the query's range table,
 *	  joinlist, and namespace.
 *
 * Note: we assume that pstate's p_rtable, p_joinlist, and p_namespace lists
 * were initialized to NIL when the pstate was created.  We will add onto
 * any entries already present --- this is needed for rule processing, as
 * well as for UPDATE and DELETE.
 *
 * The range table may grow still further when we transform the expressions
 * in the query's quals and target list. (This is possible because in
 * POSTQUEL, we allowed references to relations not specified in the
 * from-clause.  PostgreSQL keeps this extension to standard SQL.)
 */
void
transformFromClause(ParseState *pstate, List *frmList)
{
	List	   *fl;

	/*
	 * The grammar will have produced a list of RangeVars,
	 * RangeSubselects, RangeFunctions, and/or JoinExprs. Transform each
	 * one (possibly adding entries to the rtable), check for duplicate
	 * refnames, and then add it to the joinlist and namespace.
	 */
	foreach(fl, frmList)
	{
		Node	   *n = lfirst(fl);
		Node       *noderef=n;
		List	   *containedRels;


		n = transformFromClauseItem(pstate, n, &containedRels);
		checkNameSpaceConflicts(pstate, (Node *) pstate->p_namespace, n);


		pstate->p_joinlist = lappend(pstate->p_joinlist, n);
		pstate->p_namespace = lappend(pstate->p_namespace, n);
		transformWindowExpr(pstate, noderef,n);
	}
}

static Const *
StringToInterval(Value *string) 
{
	Const *tmp = make_const(string);
	Const *interval = (Const*) coerce_type((Node *) tmp, 
										   tmp->consttype,
										   (Oid) INTERVALOID,
										   COERCION_IMPLICIT,
										   COERCE_IMPLICIT_CAST);
	return interval;
}

static Const *
StringToTimestamp(Value *string) 
{
	Const *tmp = make_const(string);
	Const *tstamp = (Const*) coerce_type((Node *) tmp, 
										 tmp->consttype,
										 (Oid) TIMESTAMPOID,
										 COERCION_IMPLICIT,
										 COERCE_IMPLICIT_CAST);
	return tstamp;
}

/* transformWindowExpr
 *
 * transform a new CQL style window expr
 *  [RANGE BY 'sconst' SLIDE BY 'sconst' [SLIDE TIMEOUT int ] START AT 'sconst']
 *
 * or transform
 *
 *  [SLICES 'sconst','sconst',...,'sconst' [SLIDE TIMEOUT int] START AT 'sconst']
 */
void transformWindowExpr(ParseState *pstate, 
			 Node * fromEntry, 
			 Node * transformedEntry)
{
  RangeTblEntry *rte = NULL;
  RangeTblRef *rtr = NULL;
  Relation reldesc;
  TupleDesc tdesc;
  RangeVar *rv;
  WindowExpr *we;
  int natt=0;
  int tsattr=0;

  if(!fromEntry) return;
  if(!IsA(fromEntry, RangeVar)) return ;
  rv = (RangeVar*) fromEntry;
  we = (WindowExpr*)rv->window;
  
  if(we == NULL) return;
  if(IsA(transformedEntry, RangeTblRef))
    {
      rtr=(RangeTblRef*)transformedEntry;
      rte = rt_fetch(rtr->rtindex, pstate->p_rtable); 
      we->rtref=rtr;
    }
  else
    {
      elog(ERROR, "relation '%s' cannot be used in the WINDOW clause",
	   we->relation->relname);
    }
  if (we->slices == NIL) 
  {
	  if(we->rangeby == NULL)
		  we->rangeby=makeString(RANGEBYDEFAULT);
	  if(we->slideby == NULL)
		  we->slideby=makeString(SLIDEBYDEFAULT);
	  if(we->rangeint == NULL) 
	  {
		  we->rangeint = StringToInterval(we->rangeby);
	  }
	  if(we->slideint == NULL)
	  {
		  we->slideint = StringToInterval(we->slideby);
	  }
	  Assert(IsA(we->rangeint, Const));
	  Assert(IsA(we->slideint, Const));
  }
  else 
  {
	  Assert(we->rangeby == NULL);
	  Assert(we->slideby == NULL);

	  we->rangeint = we->slideint = NULL;

	  we->slideby = (Value *) lfirst(we->slices);
  }
  if(we->startat == NULL) 
  {
	  Interval *slideby_p;
	  Timestamp startat_ts;
	  
	  
	  /*elog(NOTICE, "Using current timestamp for START AT");*/
	  /* Use the parse time so that every stream will get the same START AT */
	  /* Round the timestamp up to the next SLIDE BY interval. */
	  slideby_p = DatumGetIntervalP(
		  DirectFunctionCall3(interval_in,
							  CStringGetDatum(we->slideby->val.str),
							  ObjectIdGetDatum(InvalidOid),
							  Int32GetDatum(-1)));
	  startat_ts = round_timestamp(pstate->p_time, *slideby_p);
	  
	  pfree(slideby_p);
	  
	  we->startat = makeString(
		  DatumGetCString(
			  DirectFunctionCall1(timestamp_out, TimestampGetDatum(startat_ts))));
  }

  if(we->startts == NULL)
  {
	  we->startts = StringToTimestamp(we->startat);
  }
 

  Assert(IsA(we->startts, Const));

  
  reldesc = RelationIdGetRelation(rte->relid);
  if (!IsAStream(reldesc->rd_rel->relkind))
    {
      RelationClose(reldesc);		/* Decrement the refcount in the
					 * relcache ! */
      elog(ERROR, "window expressions are only allowed on streams");
    }
  tdesc = reldesc->rd_att;
  natt = tdesc->natts;
  

  Assert(tdesc->constr != NULL);
  tsattr = tdesc->constr->ts_attrnum;
  
  if (tsattr == TCQTSATTRNOP)
    {
      RelationClose(reldesc);		/* Decrement the refcount in the
					 * relcache ! */
      elog(ERROR, "No attribute with a TIMESTAMPCOLUMN constraint");
      
    }
  
  we->tsvar = makeVar(rtr->rtindex,
		      tsattr + 1,
		      tdesc->attrs[tsattr]->atttypid,
		      tdesc->attrs[tsattr]->atttypmod,
		      0);
  RelationClose(reldesc);

  pstate->windowClause->windowExprs = lcons(we, pstate->windowClause->windowExprs);
 

}



/*
 * setTargetTable
 *	  Add the target relation of INSERT/UPDATE/DELETE to the range table,
 *	  and make the special links to it in the ParseState.
 *
 *	  We also open the target relation and acquire a write lock on it.
 *	  This must be done before processing the FROM list, in case the target
 *	  is also mentioned as a source relation --- we want to be sure to grab
 *	  the write lock before any read lock.
 *
 *	  If alsoSource is true, add the target to the query's joinlist and
 *	  namespace.  For INSERT, we don't want the target to be joined to;
 *	  it's a destination of tuples, not a source.	For UPDATE/DELETE,
 *	  we do need to scan or join the target.  (NOTE: we do not bother
 *	  to check for namespace conflict; we assume that the namespace was
 *	  initially empty in these cases.)
 *
 *	  Returns the rangetable index of the target relation.
 */
int
setTargetTable(ParseState *pstate, RangeVar *relation,
			   bool inh, bool alsoSource)
{
	RangeTblEntry *rte;
	int			rtindex;

	/* Close old target; this could only happen for multi-action rules */
	if (pstate->p_target_relation != NULL)
		heap_close(pstate->p_target_relation, NoLock);

	/*
	 * Open target rel and grab suitable lock (which we will hold till end
	 * of transaction).
	 *
	 * analyze.c will eventually do the corresponding heap_close(), but *not*
	 * release the lock.
	 */
	pstate->p_target_relation = heap_openrv(relation, RowExclusiveLock);

	/*
	 * Now build an RTE.
	 */
	rte = addRangeTableEntry(pstate, relation, NULL, inh, false);
	pstate->p_target_rangetblentry = rte;

	/* assume new rte is at end */
	rtindex = length(pstate->p_rtable);
	Assert(rte == rt_fetch(rtindex, pstate->p_rtable));

	/*
	 * Override addRangeTableEntry's default checkForRead, and instead
	 * mark target table as requiring write access.
	 *
	 * If we find an explicit reference to the rel later during parse
	 * analysis, scanRTEForColumn will change checkForRead to 'true'
	 * again.  That can't happen for INSERT but it is possible for UPDATE
	 * and DELETE.
	 */
	rte->checkForRead = false;
	rte->checkForWrite = true;

	/*
	 * If UPDATE/DELETE, add table to joinlist and namespace.
	 */
	if (alsoSource)
		addRTEtoQuery(pstate, rte, true, true);

	return rtindex;
}

/*
 * Simplify InhOption (yes/no/default) into boolean yes/no.
 *
 * The reason we do things this way is that we don't want to examine the
 * SQL_inheritance option flag until parse_analyze is run.	Otherwise,
 * we'd do the wrong thing with query strings that intermix SET commands
 * with queries.
 */
bool
interpretInhOption(InhOption inhOpt)
{
	switch (inhOpt)
	{
		case INH_NO:
			return false;
		case INH_YES:
			return true;
		case INH_DEFAULT:
			return SQL_inheritance;
	}
	elog(ERROR, "Bogus InhOption value");
	return false;				/* keep compiler quiet */
}

/*
 * Extract all not-in-common columns from column lists of a source table
 */
static void
extractRemainingColumns(List *common_colnames,
						List *src_colnames, List *src_colvars,
						List **res_colnames, List **res_colvars)
{
	List	   *new_colnames = NIL;
	List	   *new_colvars = NIL;
	List	   *lnames,
			   *lvars = src_colvars;

	foreach(lnames, src_colnames)
	{
		char	   *colname = strVal(lfirst(lnames));
		bool		match = false;
		List	   *cnames;

		foreach(cnames, common_colnames)
		{
			char	   *ccolname = strVal(lfirst(cnames));

			if (strcmp(colname, ccolname) == 0)
			{
				match = true;
				break;
			}
		}

		if (!match)
		{
			new_colnames = lappend(new_colnames, lfirst(lnames));
			new_colvars = lappend(new_colvars, lfirst(lvars));
		}

		lvars = lnext(lvars);
	}

	*res_colnames = new_colnames;
	*res_colvars = new_colvars;
}

/* transformJoinUsingClause()
 *	  Build a complete ON clause from a partially-transformed USING list.
 *	  We are given lists of nodes representing left and right match columns.
 *	  Result is a transformed qualification expression.
 */
static Node *
transformJoinUsingClause(ParseState *pstate, List *leftVars, List *rightVars)
{
	Node	   *result = NULL;
	List	   *lvars,
			   *rvars = rightVars;

	/*
	 * We cheat a little bit here by building an untransformed operator
	 * tree whose leaves are the already-transformed Vars.	This is OK
	 * because transformExpr() won't complain about already-transformed
	 * subnodes.
	 */
	foreach(lvars, leftVars)
	{
		Node	   *lvar = (Node *) lfirst(lvars);
		Node	   *rvar = (Node *) lfirst(rvars);
		A_Expr	   *e;

		e = makeSimpleA_Expr(OP, "=", copyObject(lvar), copyObject(rvar));

		if (result == NULL)
			result = (Node *) e;
		else
		{
			A_Expr	   *a;

			a = makeA_Expr(AND, NIL, result, (Node *) e);
			result = (Node *) a;
		}

		rvars = lnext(rvars);
	}

	/*
	 * Since the references are already Vars, and are certainly from the
	 * input relations, we don't have to go through the same pushups that
	 * transformJoinOnClause() does.  Just invoke transformExpr() to fix
	 * up the operators, and we're done.
	 */
	result = transformExpr(pstate, result);

	result = coerce_to_boolean(result, "JOIN/USING");

	return result;
}	/* transformJoinUsingClause() */

/* transformJoinOnClause()
 *	  Transform the qual conditions for JOIN/ON.
 *	  Result is a transformed qualification expression.
 */
static Node *
transformJoinOnClause(ParseState *pstate, JoinExpr *j,
					  List *containedRels)
{
	Node	   *result;
	List	   *save_namespace;
	List	   *clause_varnos,
			   *l;

	/*
	 * This is a tad tricky, for two reasons.  First, the namespace that
	 * the join expression should see is just the two subtrees of the JOIN
	 * plus any outer references from upper pstate levels.	So,
	 * temporarily set this pstate's namespace accordingly.  (We need not
	 * check for refname conflicts, because transformFromClauseItem()
	 * already did.) NOTE: this code is OK only because the ON clause
	 * can't legally alter the namespace by causing implicit relation refs
	 * to be added.
	 */
	save_namespace = pstate->p_namespace;
	pstate->p_namespace = makeList2(j->larg, j->rarg);

	/* This part is just like transformWhereClause() */
	result = transformExpr(pstate, j->quals);

	result = coerce_to_boolean(result, "JOIN/ON");

	pstate->p_namespace = save_namespace;

	/*
	 * Second, we need to check that the ON condition doesn't refer to any
	 * rels outside the input subtrees of the JOIN.  It could do that
	 * despite our hack on the namespace if it uses fully-qualified names.
	 * So, grovel through the transformed clause and make sure there are
	 * no bogus references.  (Outer references are OK, and are ignored
	 * here.)
	 */
	clause_varnos = pull_varnos(result);
	foreach(l, clause_varnos)
	{
		int			varno = lfirsti(l);

		if (!intMember(varno, containedRels))
		{
			elog(ERROR, "JOIN/ON clause refers to \"%s\", which is not part of JOIN",
				 rt_fetch(varno, pstate->p_rtable)->eref->aliasname);
		}
	}
	freeList(clause_varnos);

	return result;
}

/*
 * transformTableEntry --- transform a RangeVar (simple relation reference)
 */
static RangeTblRef *
transformTableEntry(ParseState *pstate, RangeVar *r)
{

	RangeTblEntry *rte;
	RangeTblRef *rtr;

	/*
	 * mark this entry to indicate it comes from the FROM clause. In SQL,
	 * the target list can only refer to range variables specified in the
	 * from clause but we follow the more powerful POSTQUEL semantics and
	 * automatically generate the range variable if not specified. However
	 * there are times we need to know whether the entries are legitimate.
	 */
	rte = addRangeTableEntry(pstate, r, r->alias,
							 interpretInhOption(r->inhOpt), true);

	/*
	 * We create a RangeTblRef, but we do not add it to the joinlist or
	 * namespace; our caller must do that if appropriate.
	 */
	rtr = makeNode(RangeTblRef);
	/* assume new rte is at end */
	rtr->rtindex = length(pstate->p_rtable);
	Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));

	return rtr;
}


/*
 * transformRangeSubselect --- transform a sub-SELECT appearing in FROM
 */
static RangeTblRef *
transformRangeSubselect(ParseState *pstate, RangeSubselect *r)
{
	List	   *save_namespace;
	List	   *parsetrees;
	Query	   *query;
	RangeTblEntry *rte;
	RangeTblRef *rtr;

	/*
	 * We require user to supply an alias for a subselect, per SQL92. To
	 * relax this, we'd have to be prepared to gin up a unique alias for
	 * an unlabeled subselect.
	 */
	if (r->alias == NULL)
		elog(ERROR, "sub-select in FROM must have an alias");

	/*
	 * Analyze and transform the subquery.	This is a bit tricky because
	 * we don't want the subquery to be able to see any FROM items already
	 * created in the current query (per SQL92, the scope of a FROM item
	 * does not include other FROM items).	But it does need to be able to
	 * see any further-up parent states, so we can't just pass a null
	 * parent pstate link.	So, temporarily make the current query level
	 * have an empty namespace.
	 */
	save_namespace = pstate->p_namespace;
	pstate->p_namespace = NIL;

	parsetrees = parse_analyze(r->subquery, pstate);

	pstate->p_namespace = save_namespace;

	/*
	 * Check that we got something reasonable.	Some of these conditions
	 * are probably impossible given restrictions of the grammar, but
	 * check 'em anyway.
	 */
	if (length(parsetrees) != 1)
		elog(ERROR, "Unexpected parse analysis result for subselect in FROM");
	query = (Query *) lfirst(parsetrees);
	if (query == NULL || !IsA(query, Query))
		elog(ERROR, "Unexpected parse analysis result for subselect in FROM");

	if (query->commandType != CMD_SELECT)
		elog(ERROR, "Expected SELECT query from subselect in FROM");
	if (query->resultRelation != 0 || query->into != NULL || query->isPortal)
		elog(ERROR, "Subselect in FROM may not have SELECT INTO");

	/*
	 * OK, build an RTE for the subquery.
	 */
	rte = addRangeTableEntryForSubquery(pstate, query, r->alias, true);

	/*
	 * We create a RangeTblRef, but we do not add it to the joinlist or
	 * namespace; our caller must do that if appropriate.
	 */
	rtr = makeNode(RangeTblRef);
	/* assume new rte is at end */
	rtr->rtindex = length(pstate->p_rtable);
	Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));

	return rtr;
}


/*
 * transformRangeFunction --- transform a function call appearing in FROM
 */
static RangeTblRef *
transformRangeFunction(ParseState *pstate, RangeFunction *r)
{
	Node	   *funcexpr;
	char	   *funcname;
	List	   *save_namespace;
	RangeTblEntry *rte;
	RangeTblRef *rtr;

	/* Get function name for possible use as alias */
	Assert(IsA(r->funccallnode, FuncCall));
	funcname = strVal(llast(((FuncCall *) r->funccallnode)->funcname));

	/*
	 * Transform the raw FuncCall node.  This is a bit tricky because we
	 * don't want the function expression to be able to see any FROM items
	 * already created in the current query (compare to
	 * transformRangeSubselect). But it does need to be able to see any
	 * further-up parent states. So, temporarily make the current query
	 * level have an empty namespace. NOTE: this code is OK only because
	 * the expression can't legally alter the namespace by causing
	 * implicit relation refs to be added.
	 */
	save_namespace = pstate->p_namespace;
	pstate->p_namespace = NIL;

	funcexpr = transformExpr(pstate, r->funccallnode);

	pstate->p_namespace = save_namespace;

	/*
	 * We still need to check that the function parameters don't refer to
	 * any other rels.	That could happen despite our hack on the
	 * namespace if fully-qualified names are used.  So, check there are
	 * no local Var references in the transformed expression.  (Outer
	 * references are OK, and are ignored here.)
	 */
	if (pull_varnos(funcexpr) != NIL)
		elog(ERROR, "FROM function expression may not refer to other relations of same query level");

	/*
	 * Disallow aggregate functions in the expression.	(No reason to
	 * postpone this check until parseCheckAggregates.)
	 */
	if (pstate->p_hasAggs)
	{
		if (contain_agg_clause(funcexpr))
			elog(ERROR, "cannot use aggregate function in FROM function expression");
	}

	/*
	 * If a coldeflist is supplied, ensure it defines a legal set of names
	 * (no duplicates) and datatypes (no pseudo-types, for instance).
	 */
	if (r->coldeflist)
	{
		TupleDesc	tupdesc;

		tupdesc = BuildDescForRelation(r->coldeflist);
		CheckAttributeNamesTypes(tupdesc, RELKIND_COMPOSITE_TYPE);
	}

	/*
	 * Insist we have a bare function call (explain.c is the only place
	 * that depends on this, I think).	If this fails, it's probably
	 * because transformExpr interpreted the function notation as a type
	 * coercion.
	 */
	if (!funcexpr ||
		!IsA(funcexpr, Expr) ||
		((Expr *) funcexpr)->opType != FUNC_EXPR)
		elog(ERROR, "Coercion function not allowed in FROM clause");

	/*
	 * OK, build an RTE for the function.
	 */
	rte = addRangeTableEntryForFunction(pstate, funcname, funcexpr,
										r, true);

	/*
	 * We create a RangeTblRef, but we do not add it to the joinlist or
	 * namespace; our caller must do that if appropriate.
	 */
	rtr = makeNode(RangeTblRef);
	/* assume new rte is at end */
	rtr->rtindex = length(pstate->p_rtable);
	Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));

	return rtr;
}


/*
 * transformFromClauseItem -
 *	  Transform a FROM-clause item, adding any required entries to the
 *	  range table list being built in the ParseState, and return the
 *	  transformed item ready to include in the joinlist and namespace.
 *	  This routine can recurse to handle SQL92 JOIN expressions.
 *
 *	  Aside from the primary return value (the transformed joinlist item)
 *	  this routine also returns an integer list of the rangetable indexes
 *	  of all the base and join relations represented in the joinlist item.
 *	  This list is needed for checking JOIN/ON conditions in higher levels.
 */
static Node *
transformFromClauseItem(ParseState *pstate, Node *n, List **containedRels)
{
	if (IsA(n, RangeVar))
	{
		/* Plain relation reference */
		RangeTblRef *rtr;

		rtr = transformTableEntry(pstate, (RangeVar *) n);
		*containedRels = makeListi1(rtr->rtindex);
		return (Node *) rtr;
	}
	else if (IsA(n, RangeSubselect))
	{
		/* sub-SELECT is like a plain relation */
		RangeTblRef *rtr;

		rtr = transformRangeSubselect(pstate, (RangeSubselect *) n);
		*containedRels = makeListi1(rtr->rtindex);
		return (Node *) rtr;
	}
	else if (IsA(n, RangeFunction))
	{
		/* function is like a plain relation */
		RangeTblRef *rtr;

		rtr = transformRangeFunction(pstate, (RangeFunction *) n);
		*containedRels = makeListi1(rtr->rtindex);
		return (Node *) rtr;
	}
	else if (IsA(n, JoinExpr))
	{
		/* A newfangled join expression */
		JoinExpr   *j = (JoinExpr *) n;
		List	   *my_containedRels,
				   *l_containedRels,
				   *r_containedRels,
				   *l_colnames,
				   *r_colnames,
				   *res_colnames,
				   *l_colvars,
				   *r_colvars,
				   *res_colvars;
		Index		leftrti,
					rightrti;
		RangeTblEntry *rte;

		/*
		 * Recursively process the left and right subtrees
		 */
		j->larg = transformFromClauseItem(pstate, j->larg, &l_containedRels);
		j->rarg = transformFromClauseItem(pstate, j->rarg, &r_containedRels);

		/*
		 * Generate combined list of relation indexes for possible use by
		 * transformJoinOnClause below.
		 */
		my_containedRels = nconc(l_containedRels, r_containedRels);

		/*
		 * Check for conflicting refnames in left and right subtrees. Must
		 * do this because higher levels will assume I hand back a self-
		 * consistent namespace subtree.
		 */
		checkNameSpaceConflicts(pstate, j->larg, j->rarg);

		/*
		 * Extract column name and var lists from both subtrees
		 *
		 * Note: expandRTE returns new lists, safe for me to modify
		 */
		if (IsA(j->larg, RangeTblRef))
			leftrti = ((RangeTblRef *) j->larg)->rtindex;
		else if (IsA(j->larg, JoinExpr))
			leftrti = ((JoinExpr *) j->larg)->rtindex;
		else
		{
			elog(ERROR, "transformFromClauseItem: unexpected subtree type");
			leftrti = 0;		/* keep compiler quiet */
		}
		rte = rt_fetch(leftrti, pstate->p_rtable);
		expandRTE(pstate, rte, &l_colnames, &l_colvars);

		if (IsA(j->rarg, RangeTblRef))
			rightrti = ((RangeTblRef *) j->rarg)->rtindex;
		else if (IsA(j->rarg, JoinExpr))
			rightrti = ((JoinExpr *) j->rarg)->rtindex;
		else
		{
			elog(ERROR, "transformFromClauseItem: unexpected subtree type");
			rightrti = 0;		/* keep compiler quiet */
		}
		rte = rt_fetch(rightrti, pstate->p_rtable);
		expandRTE(pstate, rte, &r_colnames, &r_colvars);

		/*
		 * Natural join does not explicitly specify columns; must generate
		 * columns to join. Need to run through the list of columns from
		 * each table or join result and match up the column names. Use
		 * the first table, and check every column in the second table for
		 * a match.  (We'll check that the matches were unique later on.)
		 * The result of this step is a list of column names just like an
		 * explicitly-written USING list.
		 */
		if (j->isNatural)
		{
			List	   *rlist = NIL;
			List	   *lx,
					   *rx;

			Assert(j->using == NIL);	/* shouldn't have USING() too */

			foreach(lx, l_colnames)
			{
				char	   *l_colname = strVal(lfirst(lx));
				Value	   *m_name = NULL;

				foreach(rx, r_colnames)
				{
					char	   *r_colname = strVal(lfirst(rx));

					if (strcmp(l_colname, r_colname) == 0)
					{
						m_name = makeString(l_colname);
						break;
					}
				}

				/* matched a right column? then keep as join column... */
				if (m_name != NULL)
					rlist = lappend(rlist, m_name);
			}

			j->using = rlist;
		}

		/*
		 * Now transform the join qualifications, if any.
		 */
		res_colnames = NIL;
		res_colvars = NIL;

		if (j->using)
		{
			/*
			 * JOIN/USING (or NATURAL JOIN, as transformed above).
			 * Transform the list into an explicit ON-condition, and
			 * generate a list of merged result columns.
			 */
			List	   *ucols = j->using;
			List	   *l_usingvars = NIL;
			List	   *r_usingvars = NIL;
			List	   *ucol;

			Assert(j->quals == NULL);	/* shouldn't have ON() too */

			foreach(ucol, ucols)
			{
				char	   *u_colname = strVal(lfirst(ucol));
				List	   *col;
				int			ndx;
				int			l_index = -1;
				int			r_index = -1;
				Var		   *l_colvar,
						   *r_colvar;

				/* Check for USING(foo,foo) */
				foreach(col, res_colnames)
				{
					char	   *res_colname = strVal(lfirst(col));

					if (strcmp(res_colname, u_colname) == 0)
						elog(ERROR, "USING column name \"%s\" appears more than once", u_colname);
				}

				/* Find it in left input */
				ndx = 0;
				foreach(col, l_colnames)
				{
					char	   *l_colname = strVal(lfirst(col));

					if (strcmp(l_colname, u_colname) == 0)
					{
						if (l_index >= 0)
							elog(ERROR, "Common column name \"%s\" appears more than once in left table", u_colname);
						l_index = ndx;
					}
					ndx++;
				}
				if (l_index < 0)
					elog(ERROR, "JOIN/USING column \"%s\" not found in left table",
						 u_colname);

				/* Find it in right input */
				ndx = 0;
				foreach(col, r_colnames)
				{
					char	   *r_colname = strVal(lfirst(col));

					if (strcmp(r_colname, u_colname) == 0)
					{
						if (r_index >= 0)
							elog(ERROR, "Common column name \"%s\" appears more than once in right table", u_colname);
						r_index = ndx;
					}
					ndx++;
				}
				if (r_index < 0)
					elog(ERROR, "JOIN/USING column \"%s\" not found in right table",
						 u_colname);

				l_colvar = nth(l_index, l_colvars);
				l_usingvars = lappend(l_usingvars, l_colvar);
				r_colvar = nth(r_index, r_colvars);
				r_usingvars = lappend(r_usingvars, r_colvar);

				res_colnames = lappend(res_colnames, lfirst(ucol));
				res_colvars = lappend(res_colvars,
									  buildMergedJoinVar(j->jointype,
														 l_colvar,
														 r_colvar));
			}

			j->quals = transformJoinUsingClause(pstate,
												l_usingvars,
												r_usingvars);
		}
		else if (j->quals)
		{
			/* User-written ON-condition; transform it */
			j->quals = transformJoinOnClause(pstate, j, my_containedRels);
		}
		else
		{
			/* CROSS JOIN: no quals */
		}

		/* Add remaining columns from each side to the output columns */
		extractRemainingColumns(res_colnames,
								l_colnames, l_colvars,
								&l_colnames, &l_colvars);
		extractRemainingColumns(res_colnames,
								r_colnames, r_colvars,
								&r_colnames, &r_colvars);
		res_colnames = nconc(res_colnames, l_colnames);
		res_colvars = nconc(res_colvars, l_colvars);
		res_colnames = nconc(res_colnames, r_colnames);
		res_colvars = nconc(res_colvars, r_colvars);

		/*
		 * Check alias (AS clause), if any.
		 */
		if (j->alias)
		{
			if (j->alias->colnames != NIL)
			{
				if (length(j->alias->colnames) > length(res_colnames))
					elog(ERROR, "Column alias list for \"%s\" has too many entries",
						 j->alias->aliasname);
			}
		}

		/*
		 * Now build an RTE for the result of the join
		 */
		rte = addRangeTableEntryForJoin(pstate,
										res_colnames,
										j->jointype,
										res_colvars,
										j->alias,
										true);

		/* assume new rte is at end */
		j->rtindex = length(pstate->p_rtable);
		Assert(rte == rt_fetch(j->rtindex, pstate->p_rtable));

		/*
		 * Include join RTE in returned containedRels list
		 */
		*containedRels = lconsi(j->rtindex, my_containedRels);

		return (Node *) j;
	}
	else
		elog(ERROR, "transformFromClauseItem: unexpected node (internal error)"
			 "\n\t%s", nodeToString(n));
	return NULL;				/* can't get here, just keep compiler
								 * quiet */
}

/*
 * buildMergedJoinVar -
 *	  generate a suitable replacement expression for a merged join column
 */
static Node *
buildMergedJoinVar(JoinType jointype, Var *l_colvar, Var *r_colvar)
{
	Oid			outcoltype;
	int32		outcoltypmod;
	Node	   *l_node,
			   *r_node,
			   *res_node;

	/*
	 * Choose output type if input types are dissimilar.
	 */
	outcoltype = l_colvar->vartype;
	outcoltypmod = l_colvar->vartypmod;
	if (outcoltype != r_colvar->vartype)
	{
		outcoltype = select_common_type(makeListi2(l_colvar->vartype,
												   r_colvar->vartype),
										"JOIN/USING");
		outcoltypmod = -1;		/* ie, unknown */
	}
	else if (outcoltypmod != r_colvar->vartypmod)
	{
		/* same type, but not same typmod */
		outcoltypmod = -1;		/* ie, unknown */
	}

	/*
	 * Insert coercion functions if needed.  Note that a difference in
	 * typmod can only happen if input has typmod but outcoltypmod is -1.
	 * In that case we insert a RelabelType to clearly mark that result's
	 * typmod is not same as input.
	 */
	if (l_colvar->vartype != outcoltype)
		l_node = coerce_type((Node *) l_colvar, l_colvar->vartype,
							 outcoltype,
							 COERCION_IMPLICIT, COERCE_IMPLICIT_CAST);
	else if (l_colvar->vartypmod != outcoltypmod)
		l_node = (Node *) makeRelabelType((Node *) l_colvar,
										  outcoltype, outcoltypmod,
										  COERCE_IMPLICIT_CAST);
	else
		l_node = (Node *) l_colvar;

	if (r_colvar->vartype != outcoltype)
		r_node = coerce_type((Node *) r_colvar, r_colvar->vartype,
							 outcoltype,
							 COERCION_IMPLICIT, COERCE_IMPLICIT_CAST);
	else if (r_colvar->vartypmod != outcoltypmod)
		r_node = (Node *) makeRelabelType((Node *) r_colvar,
										  outcoltype, outcoltypmod,
										  COERCE_IMPLICIT_CAST);
	else
		r_node = (Node *) r_colvar;

	/*
	 * Choose what to emit
	 */
	switch (jointype)
	{
		case JOIN_INNER:

			/*
			 * We can use either var; prefer non-coerced one if available.
			 */
			if (IsA(l_node, Var))
				res_node = l_node;
			else if (IsA(r_node, Var))
				res_node = r_node;
			else
				res_node = l_node;
			break;
		case JOIN_LEFT:
			/* Always use left var */
			res_node = l_node;
			break;
		case JOIN_RIGHT:
			/* Always use right var */
			res_node = r_node;
			break;
		case JOIN_FULL:
			{
				/*
				 * Here we must build a COALESCE expression to ensure that
				 * the join output is non-null if either input is.
				 */
				CaseExpr   *c = makeNode(CaseExpr);
				CaseWhen   *w = makeNode(CaseWhen);
				NullTest   *n = makeNode(NullTest);

				n->arg = l_node;
				n->nulltesttype = IS_NOT_NULL;
				w->expr = (Node *) n;
				w->result = l_node;
				c->casetype = outcoltype;
				c->args = makeList1(w);
				c->defresult = r_node;
				res_node = (Node *) c;
				break;
			}
		default:
			elog(ERROR, "buildMergedJoinVar: unexpected jointype %d",
				 (int) jointype);
			res_node = NULL;	/* keep compiler quiet */
			break;
	}

	return res_node;
}


/*
 * transformWhereClause -
 *	  transforms the qualification and make sure it is of type Boolean
 */
Node *
transformWhereClause(ParseState *pstate, Node *clause)
{
	Node	   *qual;

	if (clause == NULL)
		return NULL;

	qual = transformExpr(pstate, clause);

	qual = coerce_to_boolean(qual, "WHERE");

	return qual;
}




/*
 * transformEventClause -
 *	  transforms the EVENT clause 
 *
 * All we want to do here is to transform an EventExpr node
 * into another EventExpr node. The transformation will resolve
 * alias references (like in Both(S, T, '2 sec')) to pointers to the RangeTableEntries
 * and also to indexes in the range table
 *
 *            -TCQ SR
 */
Node *
transformEventClause(ParseState *pstate, Node *clause)
{
	RangeVar *stream_name;
	char *relname;
	EventOperator event_op;
	RangeTblEntry *rte;
	int rt_index = -1;
	int sublevels_up;
	/* Const *tmp; */
	List *partition; 
	List *elt, *tmplist, *var_lists;
	int rangeTableSize = length(pstate->p_rtable);
	Var *var;

	if (clause == NULL) /* In case we are not an event query */
		return NULL;

	partition = ((EventExpr *)clause)->partition;
 

	/* Now resolve the "partition by" clause 
	* Currently, it is just a list of attributes, like:
	*
	* Partition by person_id, book_id
	*
	* We need to take this list of attributes, and convert it to a list of lists of 
	* Var. This is trickier than resolving normal aliases,
	* as T.book_id gets resolved to only one Var, which points to the rte of T, but
	* here we are looking for the book_id attribute of ALL the streams in the FROM clause
	*
	* For this, I have extended colnameToVar() to colnameToVarList()
	*
	*/
	
	var_lists = NIL;
	
	if(partition)
	  foreach(elt, partition) {
	  tmplist = colnameToVarList(pstate, (char *)(((Value *)lfirst(elt))->val.str));
	  
	  Assert(length(tmplist) <= rangeTableSize);
	  
	  if(length(tmplist) < rangeTableSize)
	    elog(ERROR, "The column \"%s\" is not present in one of the streams", (char *)(((Value *)lfirst(elt))->val.str));

	  var_lists = lappend(var_lists, (void *)tmplist);
	}
	
	((EventExpr *)clause)->var_lists = var_lists;


	event_op = ((EventExpr *)clause)->event_op;
	

	if(event_op != EVENT_SIMPLE) { /* Found the recursive structure of an event expression */
	  
	  transformEventClause(pstate, (Node *)((EventExpr *)clause)->lchild);
	  transformEventClause(pstate, (Node *)((EventExpr *)clause)->rchild);

	  /* Now we convert the time_delay_val Value to a Const of type Interval */
	  if(((EventExpr *)clause)->time_delay_val) {
	    
	   /*  tmp  = make_const(((EventExpr *)clause)->time_delay_val); */
/* 	    ((EventExpr *)clause)->time_delay = (Const *) coerce_type((Node *) tmp, tmp->consttype, */
/* 								     (Oid) INTERVALOID, */
/* 								     COERCION_IMPLICIT, COERCE_IMPLICIT_CAST); */

	    ((EventExpr *)clause)->time_delay = *DatumGetIntervalP(DirectFunctionCall3(interval_in,
										      CStringGetDatum((((EventExpr *)clause)->time_delay_val)->val.str),
										      ObjectIdGetDatum(InvalidOid),
										      Int32GetDatum(-1)));
	    
	  }
	  

	  return clause; /* Return this transformed node itself */
	  
	}

       
	/* We are here => this node is a leaf node in the event expression */

	stream_name = ((EventExpr *)clause)->stream_name;
	relname = stream_name->relname;

	rte = refnameRangeTblEntry(pstate, NULL, relname, &sublevels_up);

	if(!rte)
	  elog(ERROR, "Invalid Tuple Variable %s", relname);

	rt_index = RTERangeTablePosn(pstate, rte, &sublevels_up);

	if(sublevels_up != 0)
	  elog(ERROR, "Event queries can not be nested!");
	
	((EventExpr *)clause)->rte = rte;
	
	/* ((EventExpr *)clause)->rt_index = rt_index; */

	/* Now store the RTIndex of this guy as a Var with attno = 0 */

	((EventExpr *)clause)->var = makeNode(Var);
	var = ((EventExpr *)clause)->var;
	var->varno = rt_index;
	var->varnoold = rt_index;
	var->varattno = 0;
	var->varoattno = 0;
	var->varlevelsup = 0;

	return clause;  /* Return this node itself */
}



/*
 *	findTargetlistEntry -
 *	  Returns the targetlist entry matching the given (untransformed) node.
 *	  If no matching entry exists, one is created and appended to the target
 *	  list as a "resjunk" node.
 *
 * node		the ORDER BY, GROUP BY, or DISTINCT ON expression to be matched
 * tlist	the existing target list (NB: this will never be NIL, which is a
 *			good thing since we'd be unable to append to it if it were...)
 * clause	identifies clause type being processed.
 */
static TargetEntry *
findTargetlistEntry(ParseState *pstate, Node *node, List *tlist, int clause)
{
	TargetEntry *target_result = NULL;
	List	   *tl;
	Node	   *expr;

	/*----------
	 * Handle two special cases as mandated by the SQL92 spec:
	 *
	 * 1. Bare ColumnName (no qualifier or subscripts)
	 *	  For a bare identifier, we search for a matching column name
	 *	  in the existing target list.	Multiple matches are an error
	 *	  unless they refer to identical values; for example,
	 *	  we allow	SELECT a, a FROM table ORDER BY a
	 *	  but not	SELECT a AS b, b FROM table ORDER BY b
	 *	  If no match is found, we fall through and treat the identifier
	 *	  as an expression.
	 *	  For GROUP BY, it is incorrect to match the grouping item against
	 *	  targetlist entries: according to SQL92, an identifier in GROUP BY
	 *	  is a reference to a column name exposed by FROM, not to a target
	 *	  list column.	However, many implementations (including pre-7.0
	 *	  PostgreSQL) accept this anyway.  So for GROUP BY, we look first
	 *	  to see if the identifier matches any FROM column name, and only
	 *	  try for a targetlist name if it doesn't.  This ensures that we
	 *	  adhere to the spec in the case where the name could be both.
	 *	  DISTINCT ON isn't in the standard, so we can do what we like there;
	 *	  we choose to make it work like ORDER BY, on the rather flimsy
	 *	  grounds that ordinary DISTINCT works on targetlist entries.
	 *
	 * 2. IntegerConstant
	 *	  This means to use the n'th item in the existing target list.
	 *	  Note that it would make no sense to order/group/distinct by an
	 *	  actual constant, so this does not create a conflict with our
	 *	  extension to order/group by an expression.
	 *	  GROUP BY column-number is not allowed by SQL92, but since
	 *	  the standard has no other behavior defined for this syntax,
	 *	  we may as well accept this common extension.
	 *
	 * Note that pre-existing resjunk targets must not be used in either case,
	 * since the user didn't write them in his SELECT list.
	 *
	 * If neither special case applies, fall through to treat the item as
	 * an expression.
	 *----------
	 */
	if (IsA(node, ColumnRef) &&
		length(((ColumnRef *) node)->fields) == 1 &&
		((ColumnRef *) node)->indirection == NIL)
	{
		char	   *name = strVal(lfirst(((ColumnRef *) node)->fields));

		if (clause == GROUP_CLAUSE)
		{
			/*
			 * In GROUP BY, we must prefer a match against a FROM-clause
			 * column to one against the targetlist.  Look to see if there
			 * is a matching column.  If so, fall through to let
			 * transformExpr() do the rest.  NOTE: if name could refer
			 * ambiguously to more than one column name exposed by FROM,
			 * colnameToVar will elog(ERROR).  That's just what we want
			 * here.
			 */
			if (colnameToVar(pstate, name) != NULL)
				name = NULL;
		}

		if (name != NULL)
		{
			foreach(tl, tlist)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(tl);
				Resdom	   *resnode = tle->resdom;

				if (!resnode->resjunk &&
					strcmp(resnode->resname, name) == 0)
				{
					if (target_result != NULL)
					{
						if (!equal(target_result->expr, tle->expr))
							elog(ERROR, "%s '%s' is ambiguous",
								 clauseText[clause], name);
					}
					else
						target_result = tle;
					/* Stay in loop to check for ambiguity */
				}
			}
			if (target_result != NULL)
				return target_result;	/* return the first match */
		}
	}
	if (IsA(node, A_Const))
	{
		Value	   *val = &((A_Const *) node)->val;
		int			targetlist_pos = 0;
		int			target_pos;

		if (!IsA(val, Integer))
			elog(ERROR, "Non-integer constant in %s", clauseText[clause]);
		target_pos = intVal(val);
		foreach(tl, tlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(tl);
			Resdom	   *resnode = tle->resdom;

			if (!resnode->resjunk)
			{
				if (++targetlist_pos == target_pos)
					return tle; /* return the unique match */
			}
		}
		elog(ERROR, "%s position %d is not in target list",
			 clauseText[clause], target_pos);
	}

	/*
	 * Otherwise, we have an expression (this is a Postgres extension not
	 * found in SQL92).  Convert the untransformed node to a transformed
	 * expression, and search for a match in the tlist. NOTE: it doesn't
	 * really matter whether there is more than one match.	Also, we are
	 * willing to match a resjunk target here, though the above cases must
	 * ignore resjunk targets.
	 */
	expr = transformExpr(pstate, node);

	foreach(tl, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(tl);

		if (equal(expr, tle->expr))
			return tle;
	}

	/*
	 * If no matches, construct a new target entry which is appended to
	 * the end of the target list.	This target is given resjunk = TRUE so
	 * that it will not be projected into the final tuple.
	 */
	target_result = transformTargetEntry(pstate, node, expr, NULL, true);
	lappend(tlist, target_result);

	return target_result;
}


/*
 * transformGroupClause -
 *	  transform a Group By clause
 *
 */
List *
transformGroupClause(ParseState *pstate, List *grouplist, List *targetlist)
{
	List	   *glist = NIL,
			   *gl;

	foreach(gl, grouplist)
	{
		TargetEntry *tle;

		tle = findTargetlistEntry(pstate, lfirst(gl),
								  targetlist, GROUP_CLAUSE);

		/* avoid making duplicate grouplist entries */
		if (!targetIsInSortList(tle, glist))
		{
			GroupClause *grpcl = makeNode(GroupClause);

			grpcl->tleSortGroupRef = assignSortGroupRef(tle, targetlist);

			grpcl->sortop = any_ordering_op(tle->resdom->restype);

			glist = lappend(glist, grpcl);
		}
	}

	return glist;
}

/*
 * transformSortClause -
 *	  transform an ORDER BY clause
 */
List *
transformSortClause(ParseState *pstate,
					List *orderlist,
					List *targetlist)
{
	List	   *sortlist = NIL;
	List	   *olitem;

	foreach(olitem, orderlist)
	{
		SortGroupBy *sortby = lfirst(olitem);
		TargetEntry *tle;

		tle = findTargetlistEntry(pstate, sortby->node,
								  targetlist, ORDER_CLAUSE);

		sortlist = addTargetToSortList(tle, sortlist, targetlist,
									   sortby->useOp);
	}

	return sortlist;
}

/*
 * transformDistinctClause -
 *	  transform a DISTINCT or DISTINCT ON clause
 *
 * Since we may need to add items to the query's sortClause list, that list
 * is passed by reference.	We might also need to add items to the query's
 * targetlist, but we assume that cannot be empty initially, so we can
 * lappend to it even though the pointer is passed by value.
 */
List *
transformDistinctClause(ParseState *pstate, List *distinctlist,
						List *targetlist, List **sortClause)
{
	List	   *result = NIL;
	List	   *slitem;
	List	   *dlitem;

	/* No work if there was no DISTINCT clause */
	if (distinctlist == NIL)
		return NIL;

	if (lfirst(distinctlist) == NIL)
	{
		/* We had SELECT DISTINCT */

		/*
		 * All non-resjunk elements from target list that are not already
		 * in the sort list should be added to it.	(We don't really care
		 * what order the DISTINCT fields are checked in, so we can leave
		 * the user's ORDER BY spec alone, and just add additional sort
		 * keys to it to ensure that all targetlist items get sorted.)
		 */
		*sortClause = addAllTargetsToSortList(*sortClause, targetlist);

		/*
		 * Now, DISTINCT list consists of all non-resjunk sortlist items.
		 * Actually, all the sortlist items had better be non-resjunk!
		 * Otherwise, user wrote SELECT DISTINCT with an ORDER BY item
		 * that does not appear anywhere in the SELECT targetlist, and we
		 * can't implement that with only one sorting pass...
		 */
		foreach(slitem, *sortClause)
		{
			SortClause *scl = (SortClause *) lfirst(slitem);
			TargetEntry *tle = get_sortgroupclause_tle(scl, targetlist);

			if (tle->resdom->resjunk)
				elog(ERROR, "For SELECT DISTINCT, ORDER BY expressions must appear in target list");
			else
				result = lappend(result, copyObject(scl));
		}
	}
	else
	{
		/* We had SELECT DISTINCT ON (expr, ...) */

		/*
		 * If the user writes both DISTINCT ON and ORDER BY, then the two
		 * expression lists must match (until one or the other runs out).
		 * Otherwise the ORDER BY requires a different sort order than the
		 * DISTINCT does, and we can't implement that with only one sort
		 * pass (and if we do two passes, the results will be rather
		 * unpredictable). However, it's OK to have more DISTINCT ON
		 * expressions than ORDER BY expressions; we can just add the
		 * extra DISTINCT values to the sort list, much as we did above
		 * for ordinary DISTINCT fields.
		 *
		 * Actually, it'd be OK for the common prefixes of the two lists to
		 * match in any order, but implementing that check seems like more
		 * trouble than it's worth.
		 */
		List	   *nextsortlist = *sortClause;

		foreach(dlitem, distinctlist)
		{
			TargetEntry *tle;

			tle = findTargetlistEntry(pstate, lfirst(dlitem),
									  targetlist, DISTINCT_ON_CLAUSE);

			if (nextsortlist != NIL)
			{
				SortClause *scl = (SortClause *) lfirst(nextsortlist);

				if (tle->resdom->ressortgroupref != scl->tleSortGroupRef)
					elog(ERROR, "SELECT DISTINCT ON expressions must match initial ORDER BY expressions");
				result = lappend(result, copyObject(scl));
				nextsortlist = lnext(nextsortlist);
			}
			else
			{
				*sortClause = addTargetToSortList(tle, *sortClause,
												  targetlist, NIL);

				/*
				 * Probably, the tle should always have been added at the
				 * end of the sort list ... but search to be safe.
				 */
				foreach(slitem, *sortClause)
				{
					SortClause *scl = (SortClause *) lfirst(slitem);

					if (tle->resdom->ressortgroupref == scl->tleSortGroupRef)
					{
						result = lappend(result, copyObject(scl));
						break;
					}
				}
				if (slitem == NIL)
					elog(ERROR, "transformDistinctClause: failed to add DISTINCT ON clause to target list");
			}
		}
	}

	return result;
}

/*
 * addAllTargetsToSortList
 *		Make sure all non-resjunk targets in the targetlist are in the
 *		ORDER BY list, adding the not-yet-sorted ones to the end of the list.
 *		This is typically used to help implement SELECT DISTINCT.
 *
 * Returns the updated ORDER BY list.
 */
List *
addAllTargetsToSortList(List *sortlist, List *targetlist)
{
	List	   *i;

	foreach(i, targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(i);

		if (!tle->resdom->resjunk)
			sortlist = addTargetToSortList(tle, sortlist, targetlist, NIL);
	}
	return sortlist;
}

/*
 * addTargetToSortList
 *		If the given targetlist entry isn't already in the ORDER BY list,
 *		add it to the end of the list, using the sortop with given name
 *		or any available sort operator if opname == NIL.
 *
 * Returns the updated ORDER BY list.
 */
static List *
addTargetToSortList(TargetEntry *tle, List *sortlist, List *targetlist,
					List *opname)
{
	/* avoid making duplicate sortlist entries */
	if (!targetIsInSortList(tle, sortlist))
	{
		SortClause *sortcl = makeNode(SortClause);

		sortcl->tleSortGroupRef = assignSortGroupRef(tle, targetlist);

		if (opname)
			sortcl->sortop = compatible_oper_opid(opname,
												  tle->resdom->restype,
												  tle->resdom->restype,
												  false);
		else
			sortcl->sortop = any_ordering_op(tle->resdom->restype);

		sortlist = lappend(sortlist, sortcl);
	}
	return sortlist;
}

/*
 * assignSortGroupRef
 *	  Assign the targetentry an unused ressortgroupref, if it doesn't
 *	  already have one.  Return the assigned or pre-existing refnumber.
 *
 * 'tlist' is the targetlist containing (or to contain) the given targetentry.
 */
Index
assignSortGroupRef(TargetEntry *tle, List *tlist)
{
	Index		maxRef;
	List	   *l;

	if (tle->resdom->ressortgroupref)	/* already has one? */
		return tle->resdom->ressortgroupref;

	/* easiest way to pick an unused refnumber: max used + 1 */
	maxRef = 0;
	foreach(l, tlist)
	{
		Index		ref = ((TargetEntry *) lfirst(l))->resdom->ressortgroupref;

		if (ref > maxRef)
			maxRef = ref;
	}
	tle->resdom->ressortgroupref = maxRef + 1;
	return tle->resdom->ressortgroupref;
}

/*
 * targetIsInSortList
 *		Is the given target item already in the sortlist?
 *
 * Works for both SortClause and GroupClause lists.  Note that the main
 * reason we need this routine (and not just a quick test for nonzeroness
 * of ressortgroupref) is that a TLE might be in only one of the lists.
 */
static bool
targetIsInSortList(TargetEntry *tle, List *sortList)
{
	Index		ref = tle->resdom->ressortgroupref;
	List	   *i;

	/* no need to scan list if tle has no marker */
	if (ref == 0)
		return false;

	foreach(i, sortList)
	{
		SortClause *scl = (SortClause *) lfirst(i);

		if (scl->tleSortGroupRef == ref)
			return true;
	}
	return false;
}

  
