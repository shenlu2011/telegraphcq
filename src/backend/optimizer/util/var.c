/*-------------------------------------------------------------------------
 *
 * var.c
 *	  Var node manipulation routines
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/optimizer/util/var.c,v 1.2 2004/03/24 08:10:59 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/plannodes.h"
#include "optimizer/clauses.h"
#include "optimizer/var.h"
#include "parser/parsetree.h"


typedef struct
{
	List	   *varlist;
	int			sublevels_up;
} pull_varnos_context;

typedef struct
{
	int			varno;
	int			varattno;
	int			sublevels_up;
} contain_var_reference_context;

typedef struct
{
	List	   *varlist;
	bool		includeUpperVars;
} pull_var_clause_context;

typedef struct
{
	List	   *rtable;
	bool		force;
} flatten_join_alias_vars_context;

static bool pull_varnos_walker(Node *node,
				   pull_varnos_context *context);
static bool contain_var_reference_walker(Node *node,
							 contain_var_reference_context *context);
static bool contain_var_clause_walker(Node *node, void *context);
static bool pull_var_clause_walker(Node *node,
					   pull_var_clause_context *context);
static Node *flatten_join_alias_vars_mutator(Node *node,
								flatten_join_alias_vars_context *context);


/*
 *		pull_varnos
 *
 *		Create a list of all the distinct varnos present in a parsetree.
 *		Only varnos that reference level-zero rtable entries are considered.
 *
 * NOTE: this is used on not-yet-planned expressions.  It may therefore find
 * bare SubLinks, and if so it needs to recurse into them to look for uplevel
 * references to the desired rtable level!	But when we find a completed
 * SubPlan, we only need to look at the parameters passed to the subplan.
 */
List *
pull_varnos(Node *node)
{
	pull_varnos_context context;

	context.varlist = NIL;
	context.sublevels_up = 0;

	/*
	 * Must be prepared to start with a Query or a bare expression tree;
	 * if it's a Query, go straight to query_tree_walker to make sure that
	 * sublevels_up doesn't get incremented prematurely.
	 */
	if (node && IsA(node, Query))
		query_tree_walker((Query *) node, pull_varnos_walker,
						  (void *) &context, 0);
	else
		pull_varnos_walker(node, &context);

	return context.varlist;
}

static bool
pull_varnos_walker(Node *node, pull_varnos_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varlevelsup == context->sublevels_up &&
			!intMember(var->varno, context->varlist))
			context->varlist = lconsi(var->varno, context->varlist);
		return false;
	}
	if (is_subplan(node))
	{
		/*
		 * Already-planned subquery.  Examine the args list (parameters to
		 * be passed to subquery), as well as the "oper" list which is
		 * executed by the outer query.  But short-circuit recursion into
		 * the subquery itself, which would be a waste of effort.
		 */
		Expr	   *expr = (Expr *) node;

		if (pull_varnos_walker((Node *) ((SubPlan *) expr->oper)->sublink->oper,
							   context))
			return true;
		if (pull_varnos_walker((Node *) expr->args,
							   context))
			return true;
		return false;
	}
	if (IsA(node, Query))
	{
		/* Recurse into RTE subquery or not-yet-planned sublink subquery */
		bool		result;

		context->sublevels_up++;
		result = query_tree_walker((Query *) node, pull_varnos_walker,
								   (void *) context, 0);
		context->sublevels_up--;
		return result;
	}
	return expression_tree_walker(node, pull_varnos_walker,
								  (void *) context);
}


/*
 *		contain_var_reference
 *
 *		Detect whether a parsetree contains any references to a specified
 *		attribute of a specified rtable entry.
 *
 * NOTE: this is used on not-yet-planned expressions.  It may therefore find
 * bare SubLinks, and if so it needs to recurse into them to look for uplevel
 * references to the desired rtable entry!	But when we find a completed
 * SubPlan, we only need to look at the parameters passed to the subplan.
 */
bool
contain_var_reference(Node *node, int varno, int varattno, int levelsup)
{
	contain_var_reference_context context;

	context.varno = varno;
	context.varattno = varattno;
	context.sublevels_up = levelsup;

	/*
	 * Must be prepared to start with a Query or a bare expression tree;
	 * if it's a Query, go straight to query_tree_walker to make sure that
	 * sublevels_up doesn't get incremented prematurely.
	 */
	if (node && IsA(node, Query))
		return query_tree_walker((Query *) node,
								 contain_var_reference_walker,
								 (void *) &context, 0);
	else
		return contain_var_reference_walker(node, &context);
}

static bool
contain_var_reference_walker(Node *node,
							 contain_var_reference_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varno == context->varno &&
			var->varattno == context->varattno &&
			var->varlevelsup == context->sublevels_up)
			return true;
		return false;
	}
	if (is_subplan(node))
	{
		/*
		 * Already-planned subquery.  Examine the args list (parameters to
		 * be passed to subquery), as well as the "oper" list which is
		 * executed by the outer query.  But short-circuit recursion into
		 * the subquery itself, which would be a waste of effort.
		 */
		Expr	   *expr = (Expr *) node;

		if (contain_var_reference_walker((Node *) ((SubPlan *) expr->oper)->sublink->oper,
										 context))
			return true;
		if (contain_var_reference_walker((Node *) expr->args,
										 context))
			return true;
		return false;
	}
	if (IsA(node, Query))
	{
		/* Recurse into RTE subquery or not-yet-planned sublink subquery */
		bool		result;

		context->sublevels_up++;
		result = query_tree_walker((Query *) node,
								   contain_var_reference_walker,
								   (void *) context, 0);
		context->sublevels_up--;
		return result;
	}
	return expression_tree_walker(node, contain_var_reference_walker,
								  (void *) context);
}


/*
 *		contain_whole_tuple_var
 *
 *		Detect whether a parsetree contains any references to the whole
 *		tuple of a given rtable entry (ie, a Var with varattno = 0).
 */
bool
contain_whole_tuple_var(Node *node, int varno, int levelsup)
{
	return contain_var_reference(node, varno, InvalidAttrNumber, levelsup);
}


/*
 * contain_var_clause
 *	  Recursively scan a clause to discover whether it contains any Var nodes
 *	  (of the current query level).
 *
 *	  Returns true if any varnode found.
 *
 * Does not examine subqueries, therefore must only be used after reduction
 * of sublinks to subplans!
 */
bool
contain_var_clause(Node *node)
{
	return contain_var_clause_walker(node, NULL);
}

static bool
contain_var_clause_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		if (((Var *) node)->varlevelsup == 0)
			return true;		/* abort the tree traversal and return
								 * true */
		return false;
	}
	return expression_tree_walker(node, contain_var_clause_walker, context);
}


/*
 * pull_var_clause
 *	  Recursively pulls all var nodes from an expression clause.
 *
 *	  Upper-level vars (with varlevelsup > 0) are included only
 *	  if includeUpperVars is true.	Most callers probably want
 *	  to ignore upper-level vars.
 *
 *	  Returns list of varnodes found.  Note the varnodes themselves are not
 *	  copied, only referenced.
 *
 * Does not examine subqueries, therefore must only be used after reduction
 * of sublinks to subplans!
 */
List *
pull_var_clause(Node *node, bool includeUpperVars)
{
	pull_var_clause_context context;

	context.varlist = NIL;
	context.includeUpperVars = includeUpperVars;

	pull_var_clause_walker(node, &context);
	return context.varlist;
}

static bool
pull_var_clause_walker(Node *node, pull_var_clause_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		if (((Var *) node)->varlevelsup == 0 || context->includeUpperVars)
			context->varlist = lappend(context->varlist, node);
		return false;
	}
	return expression_tree_walker(node, pull_var_clause_walker,
								  (void *) context);
}


/*
 * flatten_join_alias_vars
 *	  Replace Vars that reference JOIN outputs with references to the original
 *	  relation variables instead.  This allows quals involving such vars to be
 *	  pushed down.
 *
 * If force is TRUE then we will reduce all JOIN alias Vars to non-alias Vars
 * or expressions thereof (there may be COALESCE and/or type conversions
 * involved).  If force is FALSE we will not expand a Var to a non-Var
 * expression.	This is a hack to avoid confusing mergejoin planning, which
 * currently cannot cope with non-Var join items --- we leave the join vars
 * as Vars till after planning is done, then expand them during setrefs.c.
 *
 * Upper-level vars (with varlevelsup > 0) are ignored; normally there
 * should not be any by the time this routine is called.
 *
 * Does not examine subqueries, therefore must only be used after reduction
 * of sublinks to subplans!
 */
Node *
flatten_join_alias_vars(Node *node, List *rtable, bool force)
{
	flatten_join_alias_vars_context context;

	context.rtable = rtable;
	context.force = force;

	return flatten_join_alias_vars_mutator(node, &context);
}

static Node *
flatten_join_alias_vars_mutator(Node *node,
								flatten_join_alias_vars_context *context)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;
		RangeTblEntry *rte;
		Node	   *newvar;

		if (var->varlevelsup != 0)
			return node;		/* no need to copy, really */
		rte = rt_fetch(var->varno, context->rtable);
		if (rte->rtekind != RTE_JOIN)
			return node;
		Assert(var->varattno > 0);
		newvar = (Node *) nth(var->varattno - 1, rte->joinaliasvars);
		if (IsA(newvar, Var) ||context->force)
		{
			/* expand it; recurse in case join input is itself a join */
			return flatten_join_alias_vars_mutator(newvar, context);
		}
		/* we don't want to force expansion of this alias Var */
		return node;
	}
	return expression_tree_mutator(node, flatten_join_alias_vars_mutator,
								   (void *) context);
}
