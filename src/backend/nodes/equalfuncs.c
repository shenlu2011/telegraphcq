/*-------------------------------------------------------------------------
 *
 * equalfuncs.c
 *	  Equality functions to compare node trees.
 *
 * NOTE: a general convention when copying or comparing plan nodes is
 * that we ignore the executor state subnode.  We do not need to look
 * at it because no current uses of copyObject() or equal() need to
 * deal with already-executing plan trees.	By leaving the state subnodes
 * out, we avoid needing to write copy/compare routines for all the
 * different executor state node types.
 *
 * Currently, in fact, equal() doesn't know how to compare Plan nodes
 * at all, let alone their executor-state subnodes.  This will probably
 * need to be fixed someday, but presently there is no need to compare
 * plan trees.
 *
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/nodes/equalfuncs.c,v 1.14 2006/02/02 01:46:43 phred Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "utils/datum.h"


/* Macro for comparing string fields that might be NULL */
#define equalstr(a, b)	\
	(((a) != NULL && (b) != NULL) ? (strcmp(a, b) == 0) : (a) == (b))


/*
 *	Stuff from primnodes.h
 */

static bool
_equalResdom(Resdom *a, Resdom *b)
{
	if (a->resno != b->resno)
		return false;
	if (a->restype != b->restype)
		return false;
	if (a->restypmod != b->restypmod)
		return false;
	if (!equalstr(a->resname, b->resname))
		return false;
	if (a->ressortgroupref != b->ressortgroupref)
		return false;
	if (a->reskey != b->reskey)
		return false;
	if (a->reskeyop != b->reskeyop)
		return false;
	/* we ignore resjunk flag ... is this correct? */

	return true;
}

static bool
_equalFjoin(Fjoin *a, Fjoin *b)
{
	int			nNodes;

	if (a->fj_initialized != b->fj_initialized)
		return false;
	if (a->fj_nNodes != b->fj_nNodes)
		return false;
	if (!equal(a->fj_innerNode, b->fj_innerNode))
		return false;

	nNodes = a->fj_nNodes;
	if (memcmp(a->fj_results, b->fj_results, nNodes * sizeof(Datum)) != 0)
		return false;
	if (memcmp(a->fj_alwaysDone, b->fj_alwaysDone, nNodes * sizeof(bool)) != 0)
		return false;

	return true;
}
extern bool GrossSigmodHack;

extern bool GrossSigmodHack;

static bool
_equalExpr(Expr *a, Expr *b)
{
	/*
	 * We do not examine typeOid, since the optimizer often doesn't bother
	 * to set it in created nodes, and it is logically a derivative of the
	 * oper field anyway.
	 */
	if (a->opType != b->opType)
		return false;
	if (!equal(a->oper, b->oper))
		return false;
	if (GrossSigmodHack) 
	{
		List *lr = lreverse(b->args);
		
		if ((!equal(a->args, b->args)) &&
			(!equal(a->args, lr))) 
		{
			pfree(lr);
			return false;
		}
		pfree(lr);
	}
	else 
	{
		if (!equal(a->args, b->args))
			return false;
	}

	return true;
}

static bool
_equalVar(Var *a, Var *b)
{
	if (a->varno != b->varno)
		return false;
	if (a->varattno != b->varattno)
		return false;
	if (a->vartype != b->vartype)
		return false;
	if (a->vartypmod != b->vartypmod)
		return false;
	if (a->varlevelsup != b->varlevelsup)
		return false;
	if (a->varnoold != b->varnoold)
		return false;
	if (a->varoattno != b->varoattno)
		return false;

	return true;
}

static bool
_equalOper(Oper *a, Oper *b)
{
	if (a->opno != b->opno)
		return false;
	if (a->opresulttype != b->opresulttype)
		return false;
	if (a->opretset != b->opretset)
		return false;

	/*
	 * We do not examine opid or op_fcache, since these are logically
	 * derived from opno, and they may not be set yet depending on how far
	 * along the node is in the parse/plan pipeline.
	 *
	 * (Besides, op_fcache is executor state, which we don't check --- see
	 * notes at head of file.)
	 *
	 * It's probably not really necessary to check opresulttype or opretset,
	 * either...
	 */

	return true;
}

static bool
_equalConst(Const *a, Const *b)
{
	if (a->consttype != b->consttype)
		return false;
	if (a->constlen != b->constlen)
		return false;
	if (a->constisnull != b->constisnull)
		return false;
	if (a->constbyval != b->constbyval)
		return false;
	/* XXX What about constisset and constiscast? */

	/*
	 * We treat all NULL constants of the same type as equal. Someday this
	 * might need to change?  But datumIsEqual doesn't work on nulls,
	 * so...
	 */
	if (a->constisnull)
		return true;
	return datumIsEqual(a->constvalue, b->constvalue,
						a->constbyval, a->constlen);
}

static bool
_equalParam(Param *a, Param *b)
{
	if (a->paramkind != b->paramkind)
		return false;
	if (a->paramtype != b->paramtype)
		return false;

	switch (a->paramkind)
	{
		case PARAM_NAMED:
		case PARAM_NEW:
		case PARAM_OLD:
			if (strcmp(a->paramname, b->paramname) != 0)
				return false;
			break;
		case PARAM_NUM:
		case PARAM_EXEC:
			if (a->paramid != b->paramid)
				return false;
			break;
		case PARAM_INVALID:

			/*
			 * XXX: Hmmm... What are we supposed to return in this case ??
			 */
			return true;
			break;
		default:
			elog(ERROR, "_equalParam: Invalid paramkind value: %d",
				 a->paramkind);
	}

	return true;
}

static bool
_equalFunc(Func *a, Func *b)
{
	if (a->funcid != b->funcid)
		return false;
	if (a->funcresulttype != b->funcresulttype)
		return false;
	if (a->funcretset != b->funcretset)
		return false;

	/*
	 * Special-case COERCE_DONTCARE, so that pathkeys can build coercion
	 * nodes that are equal() to both explicit and implicit coercions.
	 */
	if (a->funcformat != b->funcformat &&
		a->funcformat != COERCE_DONTCARE &&
		b->funcformat != COERCE_DONTCARE)
		return false;

	/* Note we do not look at func_fcache; see notes for _equalOper */

	return true;
}

static bool
_equalAggref(Aggref *a, Aggref *b)
{
	if (a->aggfnoid != b->aggfnoid)
		return false;
	if (a->aggtype != b->aggtype)
		return false;
	if (!equal(a->target, b->target))
		return false;
	if (a->aggstar != b->aggstar)
		return false;
	if (a->aggdistinct != b->aggdistinct)
		return false;
	/* ignore aggno, which is only a private field for the executor */
	return true;
}

static bool
_equalSubLink(SubLink *a, SubLink *b)
{
	if (a->subLinkType != b->subLinkType)
		return false;
	if (a->useor != b->useor)
		return false;
	if (!equal(a->lefthand, b->lefthand))
		return false;
	if (!equal(a->oper, b->oper))
		return false;
	if (!equal(a->subselect, b->subselect))
		return false;
	return true;
}

static bool
_equalArrayRef(ArrayRef *a, ArrayRef *b)
{
	if (a->refrestype != b->refrestype)
		return false;
	if (a->refattrlength != b->refattrlength)
		return false;
	if (a->refelemlength != b->refelemlength)
		return false;
	if (a->refelembyval != b->refelembyval)
		return false;
	if (a->refelemalign != b->refelemalign)
		return false;
	if (!equal(a->refupperindexpr, b->refupperindexpr))
		return false;
	if (!equal(a->reflowerindexpr, b->reflowerindexpr))
		return false;
	if (!equal(a->refexpr, b->refexpr))
		return false;
	if (!equal(a->refassgnexpr, b->refassgnexpr))
		return false;
	return true;
}

static bool
_equalFieldSelect(FieldSelect *a, FieldSelect *b)
{
	if (!equal(a->arg, b->arg))
		return false;
	if (a->fieldnum != b->fieldnum)
		return false;
	if (a->resulttype != b->resulttype)
		return false;
	if (a->resulttypmod != b->resulttypmod)
		return false;
	return true;
}

static bool
_equalRelabelType(RelabelType *a, RelabelType *b)
{
	if (!equal(a->arg, b->arg))
		return false;
	if (a->resulttype != b->resulttype)
		return false;
	if (a->resulttypmod != b->resulttypmod)
		return false;

	/*
	 * Special-case COERCE_DONTCARE, so that pathkeys can build coercion
	 * nodes that are equal() to both explicit and implicit coercions.
	 */
	if (a->relabelformat != b->relabelformat &&
		a->relabelformat != COERCE_DONTCARE &&
		b->relabelformat != COERCE_DONTCARE)
		return false;
	return true;
}

static bool
_equalRangeTblRef(RangeTblRef *a, RangeTblRef *b)
{
	if (a->rtindex != b->rtindex)
		return false;

	return true;
}

static bool
_equalFromExpr(FromExpr *a, FromExpr *b)
{
	if (!equal(a->fromlist, b->fromlist))
		return false;
	if (!equal(a->quals, b->quals))
		return false;

	return true;
}

static bool
_equalJoinExpr(JoinExpr *a, JoinExpr *b)
{
	if (a->jointype != b->jointype)
		return false;
	if (a->isNatural != b->isNatural)
		return false;
	if (!equal(a->larg, b->larg))
		return false;
	if (!equal(a->rarg, b->rarg))
		return false;
	if (!equal(a->using, b->using))
		return false;
	if (!equal(a->quals, b->quals))
		return false;
	if (!equal(a->alias, b->alias))
		return false;
	if (a->rtindex != b->rtindex)
		return false;

	return true;
}

/*
 * Stuff from relation.h
 */

static bool
_equalRelOptInfo(RelOptInfo *a, RelOptInfo *b)
{
	/*
	 * We treat RelOptInfos as equal if they refer to the same base rels
	 * joined in the same order.  Is this appropriate/sufficient?
	 */
	return equali(a->relids, b->relids);
}

static bool
_equalIndexOptInfo(IndexOptInfo *a, IndexOptInfo *b)
{
	/*
	 * We treat IndexOptInfos as equal if they refer to the same index. Is
	 * this sufficient?
	 */
	if (a->indexoid != b->indexoid)
		return false;
	return true;
}

static bool
_equalPathKeyItem(PathKeyItem *a, PathKeyItem *b)
{
	if (a->sortop != b->sortop)
		return false;
	if (!equal(a->key, b->key))
		return false;
	return true;
}

static bool
_equalPath(Path *a, Path *b)
{
	if (a->pathtype != b->pathtype)
		return false;
	if (!equal(a->parent, b->parent))
		return false;

	/*
	 * do not check path costs, since they may not be set yet, and being
	 * float values there are roundoff error issues anyway...
	 */
	if (!equal(a->pathkeys, b->pathkeys))
		return false;
	return true;
}

static bool
_equalIndexPath(IndexPath *a, IndexPath *b)
{
	if (!_equalPath((Path *) a, (Path *) b))
		return false;
	if (!equal(a->indexinfo, b->indexinfo))
		return false;
	if (!equal(a->indexqual, b->indexqual))
		return false;
	if (a->indexscandir != b->indexscandir)
		return false;
	if (!equali(a->joinrelids, b->joinrelids))
		return false;
	if (a->alljoinquals != b->alljoinquals)
		return false;

	/*
	 * Skip 'rows' because of possibility of floating-point roundoff
	 * error. It should be derivable from the other fields anyway.
	 */
	return true;
}

static bool
_equalTidPath(TidPath *a, TidPath *b)
{
	if (!_equalPath((Path *) a, (Path *) b))
		return false;
	if (!equal(a->tideval, b->tideval))
		return false;
	if (!equali(a->unjoined_relids, b->unjoined_relids))
		return false;
	return true;
}

static bool
_equalAppendPath(AppendPath *a, AppendPath *b)
{
	if (!_equalPath((Path *) a, (Path *) b))
		return false;
	if (!equal(a->subpaths, b->subpaths))
		return false;
	return true;
}

static bool
_equalJoinPath(JoinPath *a, JoinPath *b)
{
	if (!_equalPath((Path *) a, (Path *) b))
		return false;
	if (a->jointype != b->jointype)
		return false;
	if (!equal(a->outerjoinpath, b->outerjoinpath))
		return false;
	if (!equal(a->innerjoinpath, b->innerjoinpath))
		return false;
	if (!equal(a->joinrestrictinfo, b->joinrestrictinfo))
		return false;
	return true;
}

static bool
_equalNestPath(NestPath *a, NestPath *b)
{
	if (!_equalJoinPath((JoinPath *) a, (JoinPath *) b))
		return false;
	return true;
}

static bool
_equalMergePath(MergePath *a, MergePath *b)
{
	if (!_equalJoinPath((JoinPath *) a, (JoinPath *) b))
		return false;
	if (!equal(a->path_mergeclauses, b->path_mergeclauses))
		return false;
	if (!equal(a->outersortkeys, b->outersortkeys))
		return false;
	if (!equal(a->innersortkeys, b->innersortkeys))
		return false;
	return true;
}

static bool
_equalHashPath(HashPath *a, HashPath *b)
{
	if (!_equalJoinPath((JoinPath *) a, (JoinPath *) b))
		return false;
	if (!equal(a->path_hashclauses, b->path_hashclauses))
		return false;
	return true;
}

static bool
_equalSubPlan(SubPlan *a, SubPlan *b)
{
	/* should compare plans, but have to settle for comparing plan IDs */
	if (a->plan_id != b->plan_id)
		return false;

	if (!equal(a->rtable, b->rtable))
		return false;

	if (!equal(a->sublink, b->sublink))
		return false;

	return true;
}

static bool
_equalRestrictInfo(RestrictInfo *a, RestrictInfo *b)
{
	if (!equal(a->clause, b->clause))
		return false;
	if (a->ispusheddown != b->ispusheddown)
		return false;

	/*
	 * We ignore eval_cost, this_selec, left/right_pathkey, and
	 * left/right_bucketsize, since they may not be set yet, and should be
	 * derivable from the clause anyway.  Probably it's not really
	 * necessary to compare any of these remaining fields ...
	 */
	if (!equal(a->subclauseindices, b->subclauseindices))
		return false;
	if (a->mergejoinoperator != b->mergejoinoperator)
		return false;
	if (a->left_sortop != b->left_sortop)
		return false;
	if (a->right_sortop != b->right_sortop)
		return false;
	if (a->hashjoinoperator != b->hashjoinoperator)
		return false;
	return true;
}

static bool
_equalJoinInfo(JoinInfo *a, JoinInfo *b)
{
	if (!equali(a->unjoined_relids, b->unjoined_relids))
		return false;
	if (!equal(a->jinfo_restrictinfo, b->jinfo_restrictinfo))
		return false;
	return true;
}

/*
 * Stuff from parsenodes.h
 */

static bool
_equalQuery(Query *a, Query *b)
{
	if (a->commandType != b->commandType)
		return false;
	if (a->querySource != b->querySource)
		return false;
	if (!equal(a->utilityStmt, b->utilityStmt))
		return false;
	if (a->resultRelation != b->resultRelation)
		return false;
	if (!equal(a->into, b->into))
		return false;
	if (a->isPortal != b->isPortal)
		return false;
	if (a->isBinary != b->isBinary)
		return false;
	if (a->hasAggs != b->hasAggs)
		return false;
	if (a->hasSubLinks != b->hasSubLinks)
		return false;
	if (!equal(a->rtable, b->rtable))
		return false;
	if (!equal(a->jointree, b->jointree))
		return false;
	if (!equali(a->rowMarks, b->rowMarks))
		return false;
	if (!equal(a->targetList, b->targetList))
		return false;
	if (!equal(a->groupClause, b->groupClause))
		return false;
	if (!equal(a->havingQual, b->havingQual))
		return false;
	if (!equal(a->distinctClause, b->distinctClause))
		return false;
	if (!equal(a->sortClause, b->sortClause))
		return false;
	if (!equal(a->limitOffset, b->limitOffset))
		return false;
	if (!equal(a->limitCount, b->limitCount))
		return false;
	if (!equal(a->setOperations, b->setOperations))
		return false;
	if (!equali(a->resultRelations, b->resultRelations))
		return false;

	/*
	 * We do not check the internal-to-the-planner fields: base_rel_list,
	 * other_rel_list, join_rel_list, equi_key_list, query_pathkeys. They
	 * might not be set yet, and in any case they should be derivable from
	 * the other fields.
	 */
	return true;
}

static bool
_equalInsertStmt(InsertStmt *a, InsertStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equal(a->cols, b->cols))
		return false;
	if (!equal(a->targetList, b->targetList))
		return false;
	if (!equal(a->selectStmt, b->selectStmt))
		return false;

	return true;
}

static bool
_equalDeleteStmt(DeleteStmt *a, DeleteStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equal(a->whereClause, b->whereClause))
		return false;

	return true;
}

static bool
_equalUpdateStmt(UpdateStmt *a, UpdateStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equal(a->targetList, b->targetList))
		return false;
	if (!equal(a->whereClause, b->whereClause))
		return false;
	if (!equal(a->fromClause, b->fromClause))
		return false;

	return true;
}

static bool
_equalSelectStmt(SelectStmt *a, SelectStmt *b)
{
	if (!equal(a->distinctClause, b->distinctClause))
		return false;
	if (!equal(a->into, b->into))
		return false;
	if (!equal(a->intoColNames, b->intoColNames))
		return false;
	if (!equal(a->targetList, b->targetList))
		return false;
	if (!equal(a->fromClause, b->fromClause))
		return false;
	if (!equal(a->whereClause, b->whereClause))
		return false;
	if (!equal(a->groupClause, b->groupClause))
		return false;
	if (!equal(a->havingClause, b->havingClause))
		return false;
	if (!equal(a->sortClause, b->sortClause))
		return false;
	if (!equalstr(a->portalname, b->portalname))
		return false;
	if (a->binary != b->binary)
		return false;
	if (!equal(a->limitOffset, b->limitOffset))
		return false;
	if (!equal(a->limitCount, b->limitCount))
		return false;
	if (!equal(a->forUpdate, b->forUpdate))
		return false;
	if (a->op != b->op)
		return false;
	if (a->all != b->all)
		return false;
	if (!equal(a->larg, b->larg))
		return false;
	if (!equal(a->rarg, b->rarg))
		return false;

	return true;
}

static bool
_equalSetOperationStmt(SetOperationStmt *a, SetOperationStmt *b)
{
	if (a->op != b->op)
		return false;
	if (a->all != b->all)
		return false;
	if (!equal(a->larg, b->larg))
		return false;
	if (!equal(a->rarg, b->rarg))
		return false;
	if (!equali(a->colTypes, b->colTypes))
		return false;

	return true;
}

static bool
_equalAlterTableStmt(AlterTableStmt *a, AlterTableStmt *b)
{
	if (a->subtype != b->subtype)
		return false;
	if (!equal(a->relation, b->relation))
		return false;
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->def, b->def))
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalAlterStreamStmt(AlterStreamStmt * a, AlterStreamStmt * b)
{
	if (a->subtype != b->subtype)
		return false;
	if (!equal(a->stream, b->stream))
		return false;
	if (!equal(a->wrapper, b->wrapper))
		return false;
	return true;
}

static bool
_equalGrantStmt(GrantStmt *a, GrantStmt *b)
{
	if (a->is_grant != b->is_grant)
		return false;
	if (a->objtype != b->objtype)
		return false;
	if (!equal(a->objects, b->objects))
		return false;
	if (!equali(a->privileges, b->privileges))
		return false;
	if (!equal(a->grantees, b->grantees))
		return false;

	return true;
}

static bool
_equalPrivGrantee(PrivGrantee *a, PrivGrantee *b)
{
	return equalstr(a->username, b->username)
		&& equalstr(a->groupname, b->groupname);
}

static bool
_equalFuncWithArgs(FuncWithArgs *a, FuncWithArgs *b)
{
	return equal(a->funcname, b->funcname)
		&& equal(a->funcargs, b->funcargs);
}

static bool
_equalInsertDefault(InsertDefault *a, InsertDefault *b)
{
	return true;
}

static bool
_equalClosePortalStmt(ClosePortalStmt *a, ClosePortalStmt *b)
{
	if (!equalstr(a->portalname, b->portalname))
		return false;

	return true;
}

static bool
_equalClusterStmt(ClusterStmt *a, ClusterStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equalstr(a->indexname, b->indexname))
		return false;

	return true;
}

static bool
_equalCopyStmt(CopyStmt *a, CopyStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equal(a->attlist, b->attlist))
		return false;
	if (a->is_from != b->is_from)
		return false;
	if (!equalstr(a->filename, b->filename))
		return false;
	if (!equal(a->options, b->options))
		return false;

	return true;
}

static bool
_equalCreateStmt(CreateStmt *a, CreateStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equal(a->tableElts, b->tableElts))
		return false;
	if (!equal(a->inhRelations, b->inhRelations))
		return false;
	if (!equal(a->constraints, b->constraints))
		return false;
	if (a->hasoids != b->hasoids)
		return false;

	return true;
}

static bool
_equalDefineStmt(DefineStmt *a, DefineStmt *b)
{
	if (a->defType != b->defType)
		return false;
	if (!equal(a->defnames, b->defnames))
		return false;
	if (!equal(a->definition, b->definition))
		return false;

	return true;
}

static bool
_equalDropStmt(DropStmt *a, DropStmt *b)
{
	if (!equal(a->objects, b->objects))
		return false;
	if (a->removeType != b->removeType)
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalTruncateStmt(TruncateStmt *a, TruncateStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;

	return true;
}

static bool
_equalCommentStmt(CommentStmt *a, CommentStmt *b)
{
	if (a->objtype != b->objtype)
		return false;
	if (!equal(a->objname, b->objname))
		return false;
	if (!equal(a->objargs, b->objargs))
		return false;
	if (!equalstr(a->comment, b->comment))
		return false;

	return true;
}

static bool
_equalFetchStmt(FetchStmt *a, FetchStmt *b)
{
	if (a->direction != b->direction)
		return false;
	if (a->howMany != b->howMany)
		return false;
	if (!equalstr(a->portalname, b->portalname))
		return false;
	if (a->ismove != b->ismove)
		return false;
	if (a->iswindow != b->iswindow)
		return false;

	return true;
}

static bool
_equalIndexStmt(IndexStmt *a, IndexStmt *b)
{
	if (!equalstr(a->idxname, b->idxname))
		return false;
	if (!equal(a->relation, b->relation))
		return false;
	if (!equalstr(a->accessMethod, b->accessMethod))
		return false;
	if (!equal(a->indexParams, b->indexParams))
		return false;
	if (!equal(a->whereClause, b->whereClause))
		return false;
	if (!equal(a->rangetable, b->rangetable))
		return false;
	if (a->unique != b->unique)
		return false;
	if (a->primary != b->primary)
		return false;
	if (a->isconstraint != b->isconstraint)
		return false;

	return true;
}

static bool
_equalCreateFunctionStmt(CreateFunctionStmt *a, CreateFunctionStmt *b)
{
	if (a->replace != b->replace)
		return false;
	if (!equal(a->funcname, b->funcname))
		return false;
	if (!equal(a->argTypes, b->argTypes))
		return false;
	if (!equal(a->returnType, b->returnType))
		return false;
	if (!equal(a->options, b->options))
		return false;
	if (!equal(a->withClause, b->withClause))
		return false;

	return true;
}

static bool
_equalRemoveAggrStmt(RemoveAggrStmt *a, RemoveAggrStmt *b)
{
	if (!equal(a->aggname, b->aggname))
		return false;
	if (!equal(a->aggtype, b->aggtype))
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalRemoveFuncStmt(RemoveFuncStmt *a, RemoveFuncStmt *b)
{
	if (!equal(a->funcname, b->funcname))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalRemoveOperStmt(RemoveOperStmt *a, RemoveOperStmt *b)
{
	if (!equal(a->opname, b->opname))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalRemoveOpClassStmt(RemoveOpClassStmt *a, RemoveOpClassStmt *b)
{
	if (!equal(a->opclassname, b->opclassname))
		return false;
	if (!equalstr(a->amname, b->amname))
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalRenameStmt(RenameStmt *a, RenameStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equalstr(a->oldname, b->oldname))
		return false;
	if (!equalstr(a->newname, b->newname))
		return false;
	if (a->renameType != b->renameType)
		return false;

	return true;
}

static bool
_equalRuleStmt(RuleStmt *a, RuleStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equalstr(a->rulename, b->rulename))
		return false;
	if (!equal(a->whereClause, b->whereClause))
		return false;
	if (a->event != b->event)
		return false;
	if (a->instead != b->instead)
		return false;
	if (a->replace != b->replace)
		return false;
	if (!equal(a->actions, b->actions))
		return false;

	return true;
}

static bool
_equalNotifyStmt(NotifyStmt *a, NotifyStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;

	return true;
}

static bool
_equalListenStmt(ListenStmt *a, ListenStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;

	return true;
}

static bool
_equalUnlistenStmt(UnlistenStmt *a, UnlistenStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;

	return true;
}

static bool
_equalTransactionStmt(TransactionStmt *a, TransactionStmt *b)
{
	if (a->command != b->command)
		return false;
	if (!equal(a->options, b->options))
		return false;

	return true;
}

static bool
_equalCompositeTypeStmt(CompositeTypeStmt *a, CompositeTypeStmt *b)
{
	if (!equal(a->typevar, b->typevar))
		return false;
	if (!equal(a->coldeflist, b->coldeflist))
		return false;

	return true;
}

static bool
_equalViewStmt(ViewStmt *a, ViewStmt *b)
{
	if (!equal(a->view, b->view))
		return false;
	if (!equal(a->aliases, b->aliases))
		return false;
	if (!equal(a->query, b->query))
		return false;
	if (a->replace != b->replace)
		return false;

	return true;
}

static bool
_equalLoadStmt(LoadStmt *a, LoadStmt *b)
{
	if (!equalstr(a->filename, b->filename))
		return false;

	return true;
}

static bool
_equalCreateDomainStmt(CreateDomainStmt *a, CreateDomainStmt *b)
{
	if (!equal(a->domainname, b->domainname))
		return false;
	if (!equal(a->typename, b->typename))
		return false;
	if (!equal(a->constraints, b->constraints))
		return false;

	return true;
}

static bool
_equalCreateOpClassStmt(CreateOpClassStmt *a, CreateOpClassStmt *b)
{
	if (!equal(a->opclassname, b->opclassname))
		return false;
	if (!equalstr(a->amname, b->amname))
		return false;
	if (!equal(a->datatype, b->datatype))
		return false;
	if (!equal(a->items, b->items))
		return false;
	if (a->isDefault != b->isDefault)
		return false;

	return true;
}

static bool
_equalCreateOpClassItem(CreateOpClassItem *a, CreateOpClassItem *b)
{
	if (a->itemtype != b->itemtype)
		return false;
	if (!equal(a->name, b->name))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (a->number != b->number)
		return false;
	if (a->recheck != b->recheck)
		return false;
	if (!equal(a->storedtype, b->storedtype))
		return false;

	return true;
}

static bool
_equalCreatedbStmt(CreatedbStmt *a, CreatedbStmt *b)
{
	if (!equalstr(a->dbname, b->dbname))
		return false;
	if (!equal(a->options, b->options))
		return false;

	return true;
}

static bool
_equalAlterDatabaseSetStmt(AlterDatabaseSetStmt *a, AlterDatabaseSetStmt *b)
{
	if (!equalstr(a->dbname, b->dbname))
		return false;
	if (!equalstr(a->variable, b->variable))
		return false;
	if (!equal(a->value, b->value))
		return false;

	return true;
}

static bool
_equalDropdbStmt(DropdbStmt *a, DropdbStmt *b)
{
	if (!equalstr(a->dbname, b->dbname))
		return false;

	return true;
}

static bool
_equalVacuumStmt(VacuumStmt *a, VacuumStmt *b)
{
	if (a->vacuum != b->vacuum)
		return false;
	if (a->full != b->full)
		return false;
	if (a->analyze != b->analyze)
		return false;
	if (a->freeze != b->freeze)
		return false;
	if (a->verbose != b->verbose)
		return false;
	if (!equal(a->relation, b->relation))
		return false;
	if (!equal(a->va_cols, b->va_cols))
		return false;

	return true;
}

static bool
_equalExplainStmt(ExplainStmt *a, ExplainStmt *b)
{
	if (!equal(a->query, b->query))
		return false;
	if (a->verbose != b->verbose)
		return false;
	if (a->analyze != b->analyze)
		return false;

	return true;
}

static bool
_equalCreateSeqStmt(CreateSeqStmt *a, CreateSeqStmt *b)
{
	if (!equal(a->sequence, b->sequence))
		return false;
	if (!equal(a->options, b->options))
		return false;

	return true;
}

static bool
_equalVariableSetStmt(VariableSetStmt *a, VariableSetStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (a->is_local != b->is_local)
		return false;

	return true;
}

static bool
_equalVariableShowStmt(VariableShowStmt *a, VariableShowStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;

	return true;
}

static bool
_equalVariableResetStmt(VariableResetStmt *a, VariableResetStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;

	return true;
}

static bool
_equalCreateTrigStmt(CreateTrigStmt *a, CreateTrigStmt *b)
{
	if (!equalstr(a->trigname, b->trigname))
		return false;
	if (!equal(a->relation, b->relation))
		return false;
	if (!equal(a->funcname, b->funcname))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (a->before != b->before)
		return false;
	if (a->row != b->row)
		return false;
	if (strcmp(a->actions, b->actions) != 0)
		return false;
	if (!equalstr(a->lang, b->lang))
		return false;
	if (!equalstr(a->text, b->text))
		return false;
	if (!equal(a->attr, b->attr))
		return false;
	if (!equalstr(a->when, b->when))
		return false;
	if (a->isconstraint != b->isconstraint)
		return false;
	if (a->deferrable != b->deferrable)
		return false;
	if (a->initdeferred != b->initdeferred)
		return false;
	if (!equal(a->constrrel, b->constrrel))
		return false;

	return true;
}

static bool
_equalDropPropertyStmt(DropPropertyStmt *a, DropPropertyStmt *b)
{
	if (!equal(a->relation, b->relation))
		return false;
	if (!equalstr(a->property, b->property))
		return false;
	if (a->removeType != b->removeType)
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalCreatePLangStmt(CreatePLangStmt *a, CreatePLangStmt *b)
{
	if (!equalstr(a->plname, b->plname))
		return false;
	if (!equal(a->plhandler, b->plhandler))
		return false;
	if (!equal(a->plvalidator, b->plvalidator))
		return false;
	if (a->pltrusted != b->pltrusted)
		return false;

	return true;
}

static bool
_equalDropPLangStmt(DropPLangStmt *a, DropPLangStmt *b)
{
	if (!equalstr(a->plname, b->plname))
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalCreateUserStmt(CreateUserStmt *a, CreateUserStmt *b)
{
	if (!equalstr(a->user, b->user))
		return false;
	if (!equal(a->options, b->options))
		return false;

	return true;
}

static bool
_equalAlterUserStmt(AlterUserStmt *a, AlterUserStmt *b)
{
	if (!equalstr(a->user, b->user))
		return false;
	if (!equal(a->options, b->options))
		return false;

	return true;
}

static bool
_equalAlterUserSetStmt(AlterUserSetStmt *a, AlterUserSetStmt *b)
{
	if (!equalstr(a->user, b->user))
		return false;
	if (!equalstr(a->variable, b->variable))
		return false;
	if (!equal(a->value, b->value))
		return false;

	return true;
}

static bool
_equalDropUserStmt(DropUserStmt *a, DropUserStmt *b)
{
	if (!equal(a->users, b->users))
		return false;

	return true;
}

static bool
_equalLockStmt(LockStmt *a, LockStmt *b)
{
	if (!equal(a->relations, b->relations))
		return false;
	if (a->mode != b->mode)
		return false;

	return true;
}

static bool
_equalConstraintsSetStmt(ConstraintsSetStmt *a, ConstraintsSetStmt *b)
{
	if (!equal(a->constraints, b->constraints))
		return false;
	if (a->deferred != b->deferred)
		return false;

	return true;
}

static bool
_equalCreateGroupStmt(CreateGroupStmt *a, CreateGroupStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->options, b->options))
		return false;

	return true;
}

static bool
_equalAlterGroupStmt(AlterGroupStmt *a, AlterGroupStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;
	if (a->action != b->action)
		return false;
	if (!equal(a->listUsers, b->listUsers))
		return false;

	return true;
}

static bool
_equalDropGroupStmt(DropGroupStmt *a, DropGroupStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;

	return true;
}

static bool
_equalReindexStmt(ReindexStmt *a, ReindexStmt *b)
{
	if (a->reindexType != b->reindexType)
		return false;
	if (!equal(a->relation, b->relation))
		return false;
	if (!equalstr(a->name, b->name))
		return false;
	if (a->force != b->force)
		return false;
	if (a->all != b->all)
		return false;

	return true;
}

static bool
_equalCreateSchemaStmt(CreateSchemaStmt *a, CreateSchemaStmt *b)
{
	if (!equalstr(a->schemaname, b->schemaname))
		return false;
	if (!equalstr(a->authid, b->authid))
		return false;
	if (!equal(a->schemaElts, b->schemaElts))
		return false;

	return true;
}

static bool
_equalCreateConversionStmt(CreateConversionStmt *a, CreateConversionStmt *b)
{
	if (!equal(a->conversion_name, b->conversion_name))
		return false;
	if (!equalstr(a->for_encoding_name, b->for_encoding_name))
		return false;
	if (!equalstr(a->to_encoding_name, b->to_encoding_name))
		return false;
	if (!equal(a->func_name, b->func_name))
		return false;
	if (a->def != b->def)
		return false;

	return true;
}

static bool
_equalCreateCastStmt(CreateCastStmt *a, CreateCastStmt *b)
{
	if (!equal(a->sourcetype, b->sourcetype))
		return false;
	if (!equal(a->targettype, b->targettype))
		return false;
	if (!equal(a->func, b->func))
		return false;
	if (a->context != b->context)
		return false;

	return true;
}

static bool
_equalDropCastStmt(DropCastStmt *a, DropCastStmt *b)
{
	if (!equal(a->sourcetype, b->sourcetype))
		return false;
	if (!equal(a->targettype, b->targettype))
		return false;
	if (a->behavior != b->behavior)
		return false;

	return true;
}

static bool
_equalPrepareStmt(PrepareStmt *a, PrepareStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->argtypes, b->argtypes))
		return false;
	if (!equali(a->argtype_oids, b->argtype_oids))
		return false;
	if (!equal(a->query, b->query))
		return false;

	return true;
}

static bool
_equalExecuteStmt(ExecuteStmt *a, ExecuteStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->into, b->into))
		return false;
	if (!equal(a->params, b->params))
		return false;

	return true;
}

static bool
_equalDeallocateStmt(DeallocateStmt *a, DeallocateStmt *b)
{
	if (!equalstr(a->name, b->name))
		return false;

	return true;
}

static bool
_equalAExpr(A_Expr *a, A_Expr *b)
{
	if (a->oper != b->oper)
		return false;
	if (!equal(a->name, b->name))
		return false;
	if (!equal(a->lexpr, b->lexpr))
		return false;
	if (!equal(a->rexpr, b->rexpr))
		return false;

	return true;
}

static bool
_equalColumnRef(ColumnRef *a, ColumnRef *b)
{
	if (!equal(a->fields, b->fields))
		return false;
	if (!equal(a->indirection, b->indirection))
		return false;

	return true;
}

static bool
_equalParamRef(ParamRef *a, ParamRef *b)
{
	if (a->number != b->number)
		return false;
	if (!equal(a->fields, b->fields))
		return false;
	if (!equal(a->indirection, b->indirection))
		return false;

	return true;
}

static bool
_equalAConst(A_Const *a, A_Const *b)
{
	if (!equal(&a->val, &b->val))
		return false;
	if (!equal(a->typename, b->typename))
		return false;

	return true;
}

static bool
_equalFuncCall(FuncCall *a, FuncCall *b)
{
	if (!equal(a->funcname, b->funcname))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (a->agg_star != b->agg_star)
		return false;
	if (a->agg_distinct != b->agg_distinct)
		return false;

	return true;
}

static bool
_equalAIndices(A_Indices *a, A_Indices *b)
{
	if (!equal(a->lidx, b->lidx))
		return false;
	if (!equal(a->uidx, b->uidx))
		return false;

	return true;
}

static bool
_equalExprFieldSelect(ExprFieldSelect *a, ExprFieldSelect *b)
{
	if (!equal(a->arg, b->arg))
		return false;
	if (!equal(a->fields, b->fields))
		return false;
	if (!equal(a->indirection, b->indirection))
		return false;

	return true;
}

static bool
_equalResTarget(ResTarget *a, ResTarget *b)
{
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->indirection, b->indirection))
		return false;
	if (!equal(a->val, b->val))
		return false;

	return true;
}

static bool
_equalTypeCast(TypeCast *a, TypeCast *b)
{
	if (!equal(a->arg, b->arg))
		return false;
	if (!equal(a->typename, b->typename))
		return false;

	return true;
}

static bool
_equalSortGroupBy(SortGroupBy *a, SortGroupBy *b)
{
	if (!equal(a->useOp, b->useOp))
		return false;
	if (!equal(a->node, b->node))
		return false;

	return true;
}

static bool
_equalAlias(Alias *a, Alias *b)
{
	if (!equalstr(a->aliasname, b->aliasname))
		return false;
	if (!equal(a->colnames, b->colnames))
		return false;

	return true;
}

static bool
_equalRangeVar(RangeVar *a, RangeVar *b)
{
	if (!equalstr(a->catalogname, b->catalogname))
		return false;
	if (!equalstr(a->schemaname, b->schemaname))
		return false;
	if (!equalstr(a->relname, b->relname))
		return false;
	if (a->inhOpt != b->inhOpt)
		return false;
	if (a->istemp != b->istemp)
		return false;
	if (!equal(a->alias, b->alias))
		return false;

	return true;
}

static bool
_equalRangeSubselect(RangeSubselect *a, RangeSubselect *b)
{
	if (!equal(a->subquery, b->subquery))
		return false;
	if (!equal(a->alias, b->alias))
		return false;

	return true;
}

static bool
_equalRangeFunction(RangeFunction *a, RangeFunction *b)
{
	if (!equal(a->funccallnode, b->funccallnode))
		return false;
	if (!equal(a->alias, b->alias))
		return false;
	if (!equal(a->coldeflist, b->coldeflist))
		return false;

	return true;
}

static bool
_equalTypeName(TypeName *a, TypeName *b)
{
	if (!equal(a->names, b->names))
		return false;
	if (a->typeid != b->typeid)
		return false;
	if (a->timezone != b->timezone)
		return false;
	if (a->setof != b->setof)
		return false;
	if (a->pct_type != b->pct_type)
		return false;
	if (a->typmod != b->typmod)
		return false;
	if (!equal(a->arrayBounds, b->arrayBounds))
		return false;

	return true;
}

static bool
_equalIndexElem(IndexElem *a, IndexElem *b)
{
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->funcname, b->funcname))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (!equal(a->opclass, b->opclass))
		return false;

	return true;
}

static bool
_equalColumnDef(ColumnDef *a, ColumnDef *b)
{
	if (!equalstr(a->colname, b->colname))
		return false;
	if (!equal(a->typename, b->typename))
		return false;
	if (a->inhcount != b->inhcount)
		return false;
	if (a->is_local != b->is_local)
		return false;
	if (a->is_not_null != b->is_not_null)
		return false;
	if (a->is_timestamp != b->is_timestamp)
		return false;
	if (!equal(a->raw_default, b->raw_default))
		return false;
	if (!equalstr(a->cooked_default, b->cooked_default))
		return false;
	if (!equal(a->constraints, b->constraints))
		return false;
	if (!equal(a->support, b->support))
		return false;

	return true;
}

static bool
_equalConstraint(Constraint *a, Constraint *b)
{
	if (a->contype != b->contype)
		return false;
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->raw_expr, b->raw_expr))
		return false;
	if (!equalstr(a->cooked_expr, b->cooked_expr))
		return false;
	if (!equal(a->keys, b->keys))
		return false;

	return true;
}

static bool
_equalDefElem(DefElem *a, DefElem *b)
{
	if (!equalstr(a->defname, b->defname))
		return false;
	if (!equal(a->arg, b->arg))
		return false;

	return true;
}

static bool
_equalTargetEntry(TargetEntry *a, TargetEntry *b)
{
	if (!equal(a->resdom, b->resdom))
		return false;
	if (!equal(a->fjoin, b->fjoin))
		return false;
	if (!equal(a->expr, b->expr))
		return false;

	return true;
}

static bool
_equalRangeTblEntry(RangeTblEntry *a, RangeTblEntry *b)
{
	if (a->rtekind != b->rtekind)
		return false;
	if (a->relid != b->relid)
		return false;
	if (!equal(a->subquery, b->subquery))
		return false;
	if (!equal(a->funcexpr, b->funcexpr))
		return false;
	if (!equal(a->coldeflist, b->coldeflist))
		return false;
	if (a->jointype != b->jointype)
		return false;
	if (!equal(a->joinaliasvars, b->joinaliasvars))
		return false;
	if (!equal(a->alias, b->alias))
		return false;
	if (!equal(a->eref, b->eref))
		return false;
	if (a->inh != b->inh)
		return false;
	if (a->inFromCl != b->inFromCl)
		return false;
	if (a->checkForRead != b->checkForRead)
		return false;
	if (a->checkForWrite != b->checkForWrite)
		return false;
	if (a->checkAsUser != b->checkAsUser)
		return false;

	return true;
}

static bool
_equalSortClause(SortClause *a, SortClause *b)
{
	if (a->tleSortGroupRef != b->tleSortGroupRef)
		return false;
	if (a->sortop != b->sortop)
		return false;

	return true;
}

static bool
_equalFkConstraint(FkConstraint *a, FkConstraint *b)
{
	if (!equalstr(a->constr_name, b->constr_name))
		return false;
	if (!equal(a->pktable, b->pktable))
		return false;
	if (!equal(a->fk_attrs, b->fk_attrs))
		return false;
	if (!equal(a->pk_attrs, b->pk_attrs))
		return false;
	if (a->fk_matchtype != b->fk_matchtype)
		return false;
	if (a->fk_upd_action != b->fk_upd_action)
		return false;
	if (a->fk_del_action != b->fk_del_action)
		return false;
	if (a->deferrable != b->deferrable)
		return false;
	if (a->initdeferred != b->initdeferred)
		return false;
	if (a->skip_validation != b->skip_validation)
		return false;

	return true;
}

static bool
_equalCaseExpr(CaseExpr *a, CaseExpr *b)
{
	if (a->casetype != b->casetype)
		return false;
	if (!equal(a->arg, b->arg))
		return false;
	if (!equal(a->args, b->args))
		return false;
	if (!equal(a->defresult, b->defresult))
		return false;

	return true;
}

static bool
_equalCaseWhen(CaseWhen *a, CaseWhen *b)
{
	if (!equal(a->expr, b->expr))
		return false;
	if (!equal(a->result, b->result))
		return false;

	return true;
}

static bool
_equalNullTest(NullTest *a, NullTest *b)
{
	if (!equal(a->arg, b->arg))
		return false;
	if (a->nulltesttype != b->nulltesttype)
		return false;
	return true;
}

static bool
_equalBooleanTest(BooleanTest *a, BooleanTest *b)
{
	if (!equal(a->arg, b->arg))
		return false;
	if (a->booltesttype != b->booltesttype)
		return false;
	return true;
}

static bool
_equalConstraintTest(ConstraintTest *a, ConstraintTest *b)
{
	if (!equal(a->arg, b->arg))
		return false;
	if (a->testtype != b->testtype)
		return false;
	if (!equalstr(a->name, b->name))
		return false;
	if (!equal(a->check_expr, b->check_expr))
		return false;
	return true;
}

/*
 * Stuff from pg_list.h
 */

static bool
_equalValue(Value *a, Value *b)
{
	if (a->type != b->type)
		return false;

	switch (a->type)
	{
		case T_Integer:
			return a->val.ival == b->val.ival;
		case T_Float:
		case T_String:
		case T_BitString:
			return strcmp(a->val.str, b->val.str) == 0;
		case T_Null:
			/* nothing to do */
			break;
		default:
			elog(ERROR, "_equalValue: unknown node type %d", a->type);
			break;
	}

	return true;
}

/*
 * Stuff from plannodes.h
 */
static bool
_equalSteM(SteM * a, SteM * b)
{
	return (equal(a->innerhashkey, b->innerhashkey) &&
	   BitArrayEqual(a->sourceBuiltOn, b->sourceBuiltOn, TCQMAXSOURCES));
}

/*
 * equal
 *	  returns whether two nodes are equal
 */
bool
equal(void *a, void *b)
{
	bool		retval = false;

	if (a == b)
		return true;

	/*
	 * note that a!=b, so only one of them can be NULL
	 */
	if (a == NULL || b == NULL)
		return false;

	/*
	 * are they the same type of nodes?
	 */
	if (nodeTag(a) != nodeTag(b))
		return false;

	switch (nodeTag(a))
	{
		case T_SubPlan:
			retval = _equalSubPlan(a, b);
			break;

		case T_Resdom:
			retval = _equalResdom(a, b);
			break;
		case T_Fjoin:
			retval = _equalFjoin(a, b);
			break;
		case T_Expr:
			retval = _equalExpr(a, b);
			break;
		case T_Var:
			retval = _equalVar(a, b);
			break;
		case T_Oper:
			retval = _equalOper(a, b);
			break;
		case T_Const:
			retval = _equalConst(a, b);
			break;
		case T_Param:
			retval = _equalParam(a, b);
			break;
		case T_Aggref:
			retval = _equalAggref(a, b);
			break;
		case T_SubLink:
			retval = _equalSubLink(a, b);
			break;
		case T_Func:
			retval = _equalFunc(a, b);
			break;
		case T_FieldSelect:
			retval = _equalFieldSelect(a, b);
			break;
		case T_ArrayRef:
			retval = _equalArrayRef(a, b);
			break;
		case T_RelabelType:
			retval = _equalRelabelType(a, b);
			break;
		case T_RangeTblRef:
			retval = _equalRangeTblRef(a, b);
			break;
		case T_FromExpr:
			retval = _equalFromExpr(a, b);
			break;
		case T_JoinExpr:
			retval = _equalJoinExpr(a, b);
			break;

		case T_RelOptInfo:
			retval = _equalRelOptInfo(a, b);
			break;
		case T_Path:
			retval = _equalPath(a, b);
			break;
		case T_IndexPath:
			retval = _equalIndexPath(a, b);
			break;
		case T_NestPath:
			retval = _equalNestPath(a, b);
			break;
		case T_MergePath:
			retval = _equalMergePath(a, b);
			break;
		case T_HashPath:
			retval = _equalHashPath(a, b);
			break;
		case T_PathKeyItem:
			retval = _equalPathKeyItem(a, b);
			break;
		case T_RestrictInfo:
			retval = _equalRestrictInfo(a, b);
			break;
		case T_JoinInfo:
			retval = _equalJoinInfo(a, b);
			break;
		case T_TidPath:
			retval = _equalTidPath(a, b);
			break;
		case T_AppendPath:
			retval = _equalAppendPath(a, b);
			break;
		case T_IndexOptInfo:
			retval = _equalIndexOptInfo(a, b);
			break;

		case T_List:
			{
				List	   *la = (List *) a;
				List	   *lb = (List *) b;
				List	   *l;

				/*
				 * Try to reject by length check before we grovel through
				 * all the elements...
				 */
				if (length(la) != length(lb))
					return false;
				foreach(l, la)
				{
					if (!equal(lfirst(l), lfirst(lb))) 
						return false;
					lb = lnext(lb);
				}
				retval = true;
			}
			break;

		case T_Integer:
		case T_Float:
		case T_String:
		case T_BitString:
		case T_Null:
			retval = _equalValue(a, b);
			break;

		case T_Query:
			retval = _equalQuery(a, b);
			break;
		case T_InsertStmt:
			retval = _equalInsertStmt(a, b);
			break;
		case T_DeleteStmt:
			retval = _equalDeleteStmt(a, b);
			break;
		case T_UpdateStmt:
			retval = _equalUpdateStmt(a, b);
			break;
		case T_SelectStmt:
			retval = _equalSelectStmt(a, b);
			break;
		case T_SetOperationStmt:
			retval = _equalSetOperationStmt(a, b);
			break;
		case T_AlterTableStmt:
			retval = _equalAlterTableStmt(a, b);
			break;
		case T_AlterStreamStmt:
			retval = _equalAlterStreamStmt(a, b);
			break;
		case T_GrantStmt:
			retval = _equalGrantStmt(a, b);
			break;
		case T_ClosePortalStmt:
			retval = _equalClosePortalStmt(a, b);
			break;
		case T_ClusterStmt:
			retval = _equalClusterStmt(a, b);
			break;
		case T_CopyStmt:
			retval = _equalCopyStmt(a, b);
			break;
		case T_CreateStmt:
			retval = _equalCreateStmt(a, b);
			break;
		case T_DefineStmt:
			retval = _equalDefineStmt(a, b);
			break;
		case T_DropStmt:
			retval = _equalDropStmt(a, b);
			break;
		case T_TruncateStmt:
			retval = _equalTruncateStmt(a, b);
			break;
		case T_CommentStmt:
			retval = _equalCommentStmt(a, b);
			break;
		case T_FetchStmt:
			retval = _equalFetchStmt(a, b);
			break;
		case T_IndexStmt:
			retval = _equalIndexStmt(a, b);
			break;
		case T_CreateFunctionStmt:
			retval = _equalCreateFunctionStmt(a, b);
			break;
		case T_RemoveAggrStmt:
			retval = _equalRemoveAggrStmt(a, b);
			break;
		case T_RemoveFuncStmt:
			retval = _equalRemoveFuncStmt(a, b);
			break;
		case T_RemoveOperStmt:
			retval = _equalRemoveOperStmt(a, b);
			break;
		case T_RemoveOpClassStmt:
			retval = _equalRemoveOpClassStmt(a, b);
			break;
		case T_RenameStmt:
			retval = _equalRenameStmt(a, b);
			break;
		case T_RuleStmt:
			retval = _equalRuleStmt(a, b);
			break;
		case T_NotifyStmt:
			retval = _equalNotifyStmt(a, b);
			break;
		case T_ListenStmt:
			retval = _equalListenStmt(a, b);
			break;
		case T_UnlistenStmt:
			retval = _equalUnlistenStmt(a, b);
			break;
		case T_TransactionStmt:
			retval = _equalTransactionStmt(a, b);
			break;
		case T_CompositeTypeStmt:
			retval = _equalCompositeTypeStmt(a, b);
			break;
		case T_ViewStmt:
			retval = _equalViewStmt(a, b);
			break;
		case T_LoadStmt:
			retval = _equalLoadStmt(a, b);
			break;
		case T_CreateDomainStmt:
			retval = _equalCreateDomainStmt(a, b);
			break;
		case T_CreateOpClassStmt:
			retval = _equalCreateOpClassStmt(a, b);
			break;
		case T_CreateOpClassItem:
			retval = _equalCreateOpClassItem(a, b);
			break;
		case T_CreatedbStmt:
			retval = _equalCreatedbStmt(a, b);
			break;
		case T_AlterDatabaseSetStmt:
			retval = _equalAlterDatabaseSetStmt(a, b);
			break;
		case T_DropdbStmt:
			retval = _equalDropdbStmt(a, b);
			break;
		case T_VacuumStmt:
			retval = _equalVacuumStmt(a, b);
			break;
		case T_ExplainStmt:
			retval = _equalExplainStmt(a, b);
			break;
		case T_CreateSeqStmt:
			retval = _equalCreateSeqStmt(a, b);
			break;
		case T_VariableSetStmt:
			retval = _equalVariableSetStmt(a, b);
			break;
		case T_VariableShowStmt:
			retval = _equalVariableShowStmt(a, b);
			break;
		case T_VariableResetStmt:
			retval = _equalVariableResetStmt(a, b);
			break;
		case T_CreateTrigStmt:
			retval = _equalCreateTrigStmt(a, b);
			break;
		case T_DropPropertyStmt:
			retval = _equalDropPropertyStmt(a, b);
			break;
		case T_CreatePLangStmt:
			retval = _equalCreatePLangStmt(a, b);
			break;
		case T_DropPLangStmt:
			retval = _equalDropPLangStmt(a, b);
			break;
		case T_CreateUserStmt:
			retval = _equalCreateUserStmt(a, b);
			break;
		case T_AlterUserStmt:
			retval = _equalAlterUserStmt(a, b);
			break;
		case T_AlterUserSetStmt:
			retval = _equalAlterUserSetStmt(a, b);
			break;
		case T_DropUserStmt:
			retval = _equalDropUserStmt(a, b);
			break;
		case T_LockStmt:
			retval = _equalLockStmt(a, b);
			break;
		case T_ConstraintsSetStmt:
			retval = _equalConstraintsSetStmt(a, b);
			break;
		case T_CreateGroupStmt:
			retval = _equalCreateGroupStmt(a, b);
			break;
		case T_AlterGroupStmt:
			retval = _equalAlterGroupStmt(a, b);
			break;
		case T_DropGroupStmt:
			retval = _equalDropGroupStmt(a, b);
			break;
		case T_ReindexStmt:
			retval = _equalReindexStmt(a, b);
			break;
		case T_CheckPointStmt:
			retval = true;
			break;
		case T_CreateSchemaStmt:
			retval = _equalCreateSchemaStmt(a, b);
			break;
		case T_CreateConversionStmt:
			retval = _equalCreateConversionStmt(a, b);
			break;
		case T_CreateCastStmt:
			retval = _equalCreateCastStmt(a, b);
			break;
		case T_DropCastStmt:
			retval = _equalDropCastStmt(a, b);
			break;
		case T_PrepareStmt:
			retval = _equalPrepareStmt(a, b);
			break;
		case T_ExecuteStmt:
			retval = _equalExecuteStmt(a, b);
			break;
		case T_DeallocateStmt:
			retval = _equalDeallocateStmt(a, b);
			break;

		case T_A_Expr:
			retval = _equalAExpr(a, b);
			break;
		case T_ColumnRef:
			retval = _equalColumnRef(a, b);
			break;
		case T_ParamRef:
			retval = _equalParamRef(a, b);
			break;
		case T_A_Const:
			retval = _equalAConst(a, b);
			break;
		case T_FuncCall:
			retval = _equalFuncCall(a, b);
			break;
		case T_A_Indices:
			retval = _equalAIndices(a, b);
			break;
		case T_ExprFieldSelect:
			retval = _equalExprFieldSelect(a, b);
			break;
		case T_ResTarget:
			retval = _equalResTarget(a, b);
			break;
		case T_TypeCast:
			retval = _equalTypeCast(a, b);
			break;
		case T_SortGroupBy:
			retval = _equalSortGroupBy(a, b);
			break;
		case T_Alias:
			retval = _equalAlias(a, b);
			break;
		case T_RangeVar:
			retval = _equalRangeVar(a, b);
			break;
		case T_RangeSubselect:
			retval = _equalRangeSubselect(a, b);
			break;
		case T_RangeFunction:
			retval = _equalRangeFunction(a, b);
			break;
		case T_TypeName:
			retval = _equalTypeName(a, b);
			break;
		case T_IndexElem:
			retval = _equalIndexElem(a, b);
			break;
		case T_ColumnDef:
			retval = _equalColumnDef(a, b);
			break;
		case T_Constraint:
			retval = _equalConstraint(a, b);
			break;
		case T_DefElem:
			retval = _equalDefElem(a, b);
			break;
		case T_TargetEntry:
			retval = _equalTargetEntry(a, b);
			break;
		case T_RangeTblEntry:
			retval = _equalRangeTblEntry(a, b);
			break;
		case T_SortClause:
			retval = _equalSortClause(a, b);
			break;
		case T_GroupClause:
			/* GroupClause is equivalent to SortClause */
			retval = _equalSortClause(a, b);
			break;
		case T_CaseExpr:
			retval = _equalCaseExpr(a, b);
			break;
		case T_CaseWhen:
			retval = _equalCaseWhen(a, b);
			break;
		case T_NullTest:
			retval = _equalNullTest(a, b);
			break;
		case T_BooleanTest:
			retval = _equalBooleanTest(a, b);
			break;
		case T_ConstraintTest:
			retval = _equalConstraintTest(a, b);
			break;
		case T_FkConstraint:
			retval = _equalFkConstraint(a, b);
			break;
		case T_PrivGrantee:
			retval = _equalPrivGrantee(a, b);
			break;
		case T_SteM:			/* @djoinSK */
			retval = _equalSteM(a, b);	/* @djoinSK */
			break;				/* @djoinSK */
		case T_FuncWithArgs:
			retval = _equalFuncWithArgs(a, b);
			break;
		case T_InsertDefault:
			retval = _equalInsertDefault(a, b);
			break;
		default:
			elog(WARNING, "equal: don't know whether nodes of type %d are equal",
				 nodeTag(a));
			break;
	}

	return retval;
}

