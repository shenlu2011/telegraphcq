/*
 * outfuncs.c
 *	  routines to convert a node to ascii representation
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	$Header: /project/eecs/db/cvsroot/postgres/src/backend/nodes/outfuncs.c,v 1.46 2005/06/13 02:59:27 phred Exp $
 *
 * NOTES
 *	  Every (plan) node in POSTGRES has an associated "out" routine which
 *	  knows how to create its ascii representation. These functions are
 *	  useful for debugging as well as for storing plans in the system
 *	  catalogs (eg. views).
 */
#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "parser/parse.h"
#include "utils/datum.h"


#define booltostr(x)  ((x) ? "true" : "false")

static void _outDatum(StringInfo str, Datum value, int typlen, bool typbyval);
static void _outNode(StringInfo str, void *obj);

/*
 * _outToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
static void
_outToken(StringInfo str, char *s)
{
	if (s == NULL || *s == '\0')
	{
		appendStringInfo(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by
	 * read.c (either in pg_strtok() or in nodeRead()), and therefore need
	 * a protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' ||
		*s == '\"' ||
		*s == '@' ||
		isdigit((unsigned char) *s) ||
		((*s == '+' || *s == '-') &&
		 (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s)
	{
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' ||
			*s == '(' || *s == ')' || *s == '{' || *s == '}' ||
			*s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

/*
 * _outIntList -
 *	   converts a List of integers
 */
static void
_outIntList(StringInfo str, List *list)
{
	List	   *l;

	appendStringInfoChar(str, '(');
	foreach(l, list)
		appendStringInfo(str, " %d", lfirsti(l));
	appendStringInfoChar(str, ')');
}

/*
 * _outOidList -
 *	   converts a List of OIDs
 */
static void
_outOidList(StringInfo str, List *list)
{
	List	   *l;

	appendStringInfoChar(str, '(');
	foreach(l, list)
		appendStringInfo(str, " %u", (Oid) lfirsti(l));
	appendStringInfoChar(str, ')');
}

static void
_outCreateStmt(StringInfo str, CreateStmt *node)
{
	appendStringInfo(str, " CREATE :relation ");
	_outNode(str, node->relation);

	appendStringInfo(str, "	:tableElts ");
	_outNode(str, node->tableElts);

	appendStringInfo(str, " :inhRelations ");
	_outNode(str, node->inhRelations);

	appendStringInfo(str, " :constraints ");
	_outNode(str, node->constraints);

	appendStringInfo(str, " :hasoids %s ",
					 booltostr(node->hasoids));
}

static void
_outCreateWrapperStmt(StringInfo str, CreateWrapperStmt * node)
{
	appendStringInfo(str, " :wrappername %s", node->wrappername);
	_outNode(str, node->functions);
	appendStringInfo(str, " :wrapperinfo %s", node->info);

}

static void						/* @BdtcqSK */
_outCreateStrmStmt(StringInfo str, CreateStrmStmt * nodeS)
{
	CreateStmt *node = (CreateStmt *) nodeS;

	appendStringInfo(str, " CREATE STREAM: relname ");
	_outNode(str, node->relation);

	appendStringInfo(str, "	:columns ");
	_outNode(str, node->tableElts);

	appendStringInfo(str, " :inhRelnames ");
	_outNode(str, node->inhRelations);

	appendStringInfo(str, " :constraints ");
	_outNode(str, node->constraints);

	appendStringInfo(str, " :hasoids %s ", booltostr(node->hasoids));
}	/* @EdtcqSK */

static void
_outIndexStmt(StringInfo str, IndexStmt *node)
{
	appendStringInfo(str, " INDEX :idxname ");
	_outToken(str, node->idxname);
	appendStringInfo(str, " :relation ");
	_outNode(str, node->relation);
	appendStringInfo(str, " :accessMethod ");
	_outToken(str, node->accessMethod);
	appendStringInfo(str, " :indexParams ");
	_outNode(str, node->indexParams);
	appendStringInfo(str, " :whereClause ");
	_outNode(str, node->whereClause);
	appendStringInfo(str, " :rangetable ");
	_outNode(str, node->rangetable);
	appendStringInfo(str, " :unique %s :primary %s :isconstraint %s ",
					 booltostr(node->unique),
					 booltostr(node->primary),
					 booltostr(node->isconstraint));
}

static void
_outNotifyStmt(StringInfo str, NotifyStmt *node)
{
	appendStringInfo(str, " NOTIFY :relation ");
	_outNode(str, node->relation);
}

static void
_outSelectStmt(StringInfo str, SelectStmt *node)
{
	/* XXX this is pretty durn incomplete */
	appendStringInfo(str, " SELECT :where ");
	_outNode(str, node->whereClause);
	appendStringInfo(str, ":from ");
	_outNode(str, node->fromClause);
	appendStringInfo(str, ":target  ");
	_outNode(str, node->targetList);

}

static void
_outFuncCall(StringInfo str, FuncCall *node)
{
	appendStringInfo(str, " FUNCCALL ");
	_outNode(str, node->funcname);
	appendStringInfo(str, " :args ");
	_outNode(str, node->args);
	appendStringInfo(str, " :agg_star %s :agg_distinct %s ",
					 booltostr(node->agg_star),
					 booltostr(node->agg_distinct));
}

static void
_outColumnDef(StringInfo str, ColumnDef *node)
{
	appendStringInfo(str, " COLUMNDEF :colname ");
	_outToken(str, node->colname);
	appendStringInfo(str, " :typename ");
	_outNode(str, node->typename);
	appendStringInfo(str, " :inhcount %d :is_local %s :is_not_null %s :raw_default ",
					 node->inhcount,
					 booltostr(node->is_local),
					 booltostr(node->is_not_null));
	_outNode(str, node->raw_default);
	appendStringInfo(str, " :is_timestamp %s ", /* @dtstampSK */
					 booltostr(node->is_timestamp));	/* @dtstampSK */
	appendStringInfo(str, " :cooked_default ");
	_outToken(str, node->cooked_default);
	appendStringInfo(str, " :constraints ");
	_outNode(str, node->constraints);
	appendStringInfo(str, " :support ");
	_outNode(str, node->support);
}

static void
_outTypeName(StringInfo str, TypeName *node)
{
	appendStringInfo(str, " TYPENAME :names ");
	_outNode(str, node->names);
	appendStringInfo(str, " :typeid %u :timezone %s :setof %s"
					 " :pct_type %s :typmod %d :arrayBounds ",
					 node->typeid,
					 booltostr(node->timezone),
					 booltostr(node->setof),
					 booltostr(node->pct_type),
					 node->typmod);
	_outNode(str, node->arrayBounds);
}

static void
_outTypeCast(StringInfo str, TypeCast *node)
{
	appendStringInfo(str, " TYPECAST :arg ");
	_outNode(str, node->arg);
	appendStringInfo(str, " :typename ");
	_outNode(str, node->typename);
}

static void
_outIndexElem(StringInfo str, IndexElem *node)
{
	appendStringInfo(str, " INDEXELEM :name ");
	_outToken(str, node->name);
	appendStringInfo(str, " :funcname ");
	_outNode(str, node->funcname);
	appendStringInfo(str, " :args ");
	_outNode(str, node->args);
	appendStringInfo(str, " :opclass ");
	_outNode(str, node->opclass);
}

static void						/* @BdwndwSK */
_outWindowExpr(StringInfo str, WindowExpr * node)
{
	appendStringInfo(str, " WINDOWEXPR :relation ");
	_outNode(str, node->relation);
	appendStringInfo(str, " :slideint ");
	_outNode(str, node->slideint);
	appendStringInfo(str, " :rangeint ");
	_outNode(str, node->rangeint);
	appendStringInfo(str, " :startts ");
	_outNode(str, node->startts);
	appendStringInfo(str, " :tsvar ");
	_outNode(str, node->tsvar);
}	/* @EdwndwSK */

static void
_outQuery(StringInfo str, Query *node)
{
	appendStringInfo(str, " QUERY :command %d :source %d :utility ",
					 (int) node->commandType, (int) node->querySource);

	/*
	 * Hack to work around missing outfuncs routines for a lot of the
	 * utility-statement node types.  (The only one we actually *need* for
	 * rules support is NotifyStmt.)  Someday we ought to support 'em all,
	 * but for the meantime do this to avoid getting lots of warnings when
	 * running with debug_print_parse on.
	 */
	if (node->utilityStmt)
	{
		switch (nodeTag(node->utilityStmt))
		{
			case T_CreateStmt:
			case T_IndexStmt:
			case T_NotifyStmt:
				_outNode(str, node->utilityStmt);
				break;
			default:
				appendStringInfo(str, "?");
				break;
		}
	}
	else
		appendStringInfo(str, "<>");

	appendStringInfo(str, " :resultRelation %d :into ",
					 node->resultRelation);
	_outNode(str, node->into);

	appendStringInfo(str, " :isPortal %s :isBinary %s"
					 " :hasAggs %s :hasSubLinks %s :rtable ",
					 booltostr(node->isPortal),
					 booltostr(node->isBinary),
					 booltostr(node->hasAggs),
					 booltostr(node->hasSubLinks));
	_outNode(str, node->rtable);

	appendStringInfo(str, " :jointree ");
	_outNode(str, node->jointree);

	appendStringInfo(str, " :rowMarks ");
	_outIntList(str, node->rowMarks);

	appendStringInfo(str, " :targetList ");
	_outNode(str, node->targetList);

	appendStringInfo(str, " :groupClause ");
	_outNode(str, node->groupClause);

	appendStringInfo(str, " :havingQual ");
	_outNode(str, node->havingQual);

	appendStringInfo(str, " :distinctClause ");
	_outNode(str, node->distinctClause);

	appendStringInfo(str, " :sortClause ");
	_outNode(str, node->sortClause);

	appendStringInfo(str, " :limitOffset ");
	_outNode(str, node->limitOffset);

	appendStringInfo(str, " :limitCount ");
	_outNode(str, node->limitCount);

	appendStringInfo(str, " :setOperations ");
	_outNode(str, node->setOperations);

	appendStringInfo(str, " :resultRelations ");
	_outIntList(str, node->resultRelations);

	/* planner-internal fields are not written out */

	/* XXX DEBUG ONLY: print out the planner internal fields -- Amol. */
    /******* DO NOT PRINT THESE THINGS OUT. */
    /* stringToNode which is used in some places doesn't like () followed
     * by anything but } and () will be printed out above by resultRelations
     * if the list is empty. This is bad design somewhere. */

    /*******
	appendStringInfo(str, " :base_rel_list ");
	_outNode(str, node->base_rel_list);

	appendStringInfo(str, " :other_rel_list ");
	_outNode(str, node->other_rel_list);

	appendStringInfo(str, " :join_rel_list ");
	_outNode(str, node->join_rel_list);

	appendStringInfo(str, " :equi_key_list ");
	_outNode(str, node->equi_key_list);

	appendStringInfo(str, " :query_pathkeys ");
	_outNode(str, node->query_pathkeys);
    *******/
}

static void
_outSortClause(StringInfo str, SortClause *node)
{
	appendStringInfo(str, " SORTCLAUSE :tleSortGroupRef %u :sortop %u ",
					 node->tleSortGroupRef, node->sortop);
}

static void
_outGroupClause(StringInfo str, GroupClause *node)
{
	appendStringInfo(str, " GROUPCLAUSE :tleSortGroupRef %u :sortop %u ",
					 node->tleSortGroupRef, node->sortop);
}

static void
_outSetOperationStmt(StringInfo str, SetOperationStmt *node)
{
	appendStringInfo(str, " SETOPERATIONSTMT :op %d :all %s :larg ",
					 (int) node->op,
					 booltostr(node->all));
	_outNode(str, node->larg);
	appendStringInfo(str, " :rarg ");
	_outNode(str, node->rarg);
	appendStringInfo(str, " :colTypes ");
	_outOidList(str, node->colTypes);
}

/*
 * print the basic stuff of all nodes that inherit from Plan
 *
 * NOTE: we deliberately omit the execution state (EState)
 */
static void
_outPlanInfo(StringInfo str, Plan *node)
{
	appendStringInfo(str,
					 ":startup_cost %.2f :total_cost %.2f :rows %.0f :width %d :qptargetlist ",
					 node->startup_cost,
					 node->total_cost,
					 node->plan_rows,
					 node->plan_width);
	_outNode(str, node->targetlist);

	appendStringInfo(str, " :qpqual ");
	_outNode(str, node->qual);

	appendStringInfo(str, " :lefttree ");
	_outNode(str, node->lefttree);

	appendStringInfo(str, " :righttree ");
	_outNode(str, node->righttree);

	appendStringInfo(str, " :extprm ");
	_outIntList(str, node->extParam);

	appendStringInfo(str, " :locprm ");
	_outIntList(str, node->locParam);

	appendStringInfo(str, " :initplan ");
	_outNode(str, node->initPlan);

	appendStringInfo(str, " :nprm %d ", node->nParamExec);
}

/*
 *	Stuff from plannodes.h
 */
static void
_outPlan(StringInfo str, Plan *node)
{
	appendStringInfo(str, " PLAN ");
	_outPlanInfo(str, (Plan *) node);
}

static void
_outResult(StringInfo str, Result *node)
{
	appendStringInfo(str, " RESULT ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :resconstantqual ");
	_outNode(str, node->resconstantqual);

}

/*
 *	Append is a subclass of Plan.
 */
static void
_outAppend(StringInfo str, Append *node)
{
	appendStringInfo(str, " APPEND ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :appendplans ");
	_outNode(str, node->appendplans);

	appendStringInfo(str, " :isTarget %s ",
					 booltostr(node->isTarget));
}

/*
 *	Join is a subclass of Plan
 */
static void
_outJoin(StringInfo str, Join *node)
{
	appendStringInfo(str, " JOIN ");
	_outPlanInfo(str, (Plan *) node);
	appendStringInfo(str, " :jointype %d :joinqual ",
					 (int) node->jointype);
	_outNode(str, node->joinqual);
}

/*
 *	NestLoop is a subclass of Join
 */
static void
_outNestLoop(StringInfo str, NestLoop *node)
{
	appendStringInfo(str, " NESTLOOP ");
	_outPlanInfo(str, (Plan *) node);
	appendStringInfo(str, " :jointype %d :joinqual ",
					 (int) node->join.jointype);
	_outNode(str, node->join.joinqual);
}

/*
 *	MergeJoin is a subclass of Join
 */
static void
_outMergeJoin(StringInfo str, MergeJoin *node)
{
	appendStringInfo(str, " MERGEJOIN ");
	_outPlanInfo(str, (Plan *) node);
	appendStringInfo(str, " :jointype %d :joinqual ",
					 (int) node->join.jointype);
	_outNode(str, node->join.joinqual);

	appendStringInfo(str, " :mergeclauses ");
	_outNode(str, node->mergeclauses);
}

/*
 *	HashJoin is a subclass of Join.
 */
static void
_outHashJoin(StringInfo str, HashJoin *node)
{
	appendStringInfo(str, " HASHJOIN ");
	_outPlanInfo(str, (Plan *) node);
	appendStringInfo(str, " :jointype %d :joinqual ",
					 (int) node->join.jointype);
	_outNode(str, node->join.joinqual);

	appendStringInfo(str, " :hashclauses ");
	_outNode(str, node->hashclauses);
	appendStringInfo(str, " :hashjoinop %u ",
					 node->hashjoinop);
}

/*
 *	SteMHashJoin is a subclass of Join. @dstemsAD
 *
 *	This is duplicate of the above except for the name of the module.
 */
static void
_outSteMHashJoin(StringInfo str, SteMHashJoin * node)
{
	appendStringInfo(str, " STEMHASHJOIN ");
	_outPlanInfo(str, (Plan *) node);
	appendStringInfo(str, " :jointype %d :joinqual ",
					 (int) node->join.jointype);
	_outNode(str, node->join.joinqual);

	appendStringInfo(str, " :hashclauses ");
	_outNode(str, node->hashclauses);
	appendStringInfo(str, " :hashjoinop %u ",
					 node->hashjoinop);
}

static void
_outSubPlan(StringInfo str, SubPlan *node)
{
	appendStringInfo(str, " SUBPLAN :plan ");
	_outNode(str, node->plan);

	appendStringInfo(str, " :planid %d :rtable ", node->plan_id);
	_outNode(str, node->rtable);

	appendStringInfo(str, " :setprm ");
	_outIntList(str, node->setParam);

	appendStringInfo(str, " :parprm ");
	_outIntList(str, node->parParam);

	appendStringInfo(str, " :slink ");
	_outNode(str, node->sublink);
}

/*
 *	Scan is a subclass of Node
 */
static void
_outScan(StringInfo str, Scan *node)
{
	appendStringInfo(str, " SCAN ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :scanrelid %u ", node->scanrelid);
}

/*
 *	SeqScan is a subclass of Scan
 */
static void
_outSeqScan(StringInfo str, SeqScan *node)
{
	appendStringInfo(str, " SEQSCAN ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :scanrelid %u ", node->scanrelid);
}

/*
 *	IndexScan is a subclass of Scan
 */
static void
_outIndexScan(StringInfo str, IndexScan *node)
{
	appendStringInfo(str, " INDEXSCAN ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :scanrelid %u :indxid ", node->scan.scanrelid);
	_outOidList(str, node->indxid);

	appendStringInfo(str, " :indxqual ");
	_outNode(str, node->indxqual);

	appendStringInfo(str, " :indxqualorig ");
	_outNode(str, node->indxqualorig);

	appendStringInfo(str, " :indxorderdir %d ", node->indxorderdir);
}

/*
 *	TidScan is a subclass of Scan
 */
static void
_outTidScan(StringInfo str, TidScan *node)
{
	appendStringInfo(str, " TIDSCAN ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :scanrelid %u ", node->scan.scanrelid);
	appendStringInfo(str, " :needrescan %d ", node->needRescan);

	appendStringInfo(str, " :tideval ");
	_outNode(str, node->tideval);

}

/*
 *	ScanQueue
 */
static void
_outScanQueue(StringInfo str, ScanQueue * node)
{
	appendStringInfo(str, " SCANQUEUE ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :dontProject %d ", node->dontProject);
}


/*
 *	Fjord
 */

static void
_outFjord(StringInfo str, Fjord * f)
{
	appendStringInfo(str, " FJORD ");
	appendStringInfo(str, " :operator %d ", f->operator);
	appendStringInfo(str, " :usedByQueries:");
	_outIntList(str, f->usedByQueries);
	appendStringInfo(str, " :numOutputs %d ",f->numOutputs);
}



/*
 *
 * ScanModule
 */

static void
_outScanModule(StringInfo str, ScanModule * node)
{
	appendStringInfo(str, " ScanModule ");
	_outFjord(str, &node->fjord);
	_outNode(str, node->scan);
}

/*
 *	StrmScan
 */
static void
_outStrmScan(StringInfo str, StrmScan * node)
{
	appendStringInfo(str, " StrmScan ");
	_outScanQueue(str, (ScanQueue *) node);
    if (node->seqscan) {
        _outSeqScan(str, node->seqscan);
    } else {
        appendStringInfo(str, " :strmscan (NIL) ");
    }
	appendStringInfo(str, " :streamoid %d :blocking %d ",
					 node->streamoid, node->blocking);

}

/*
 *	SubqueryScan is a subclass of Scan
 */
static void
_outSubqueryScan(StringInfo str, SubqueryScan *node)
{
	appendStringInfo(str, " SUBQUERYSCAN ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :scanrelid %u :subplan ", node->scan.scanrelid);
	_outNode(str, node->subplan);
}

/*
 *	FunctionScan is a subclass of Scan
 */
static void
_outFunctionScan(StringInfo str, FunctionScan *node)
{
	appendStringInfo(str, " FUNCTIONSCAN ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :scanrelid %u ", node->scan.scanrelid);
}

/*
 *	Material is a subclass of Plan
 */
static void
_outMaterial(StringInfo str, Material *node)
{
	appendStringInfo(str, " MATERIAL ");
	_outPlanInfo(str, (Plan *) node);
}

/*
 *	Sort is a subclass of Plan
 */
static void
_outSort(StringInfo str, Sort *node)
{
	appendStringInfo(str, " SORT ");
	_outPlanInfo(str, (Plan *) node);
	appendStringInfo(str, " :keycount %d ", node->keycount);
}

static void
_outAgg(StringInfo str, Agg *node)
{
	appendStringInfo(str, " AGG ");
	_outPlanInfo(str, (Plan *) node);
}

static void
_outHashGroup(StringInfo str, HashGroup *node)
{
	int i;
	
	appendStringInfo(str, " HASH-GROUP ");

	_outPlanInfo(str, (Plan *) node);
	
	appendStringInfo(str, " :numCols %d :grpColIdx ",
					 node->agg.numCols);

	for (i = 0; i < node->agg.numCols; i++)
		appendStringInfo(str, "%d ", (int) node->agg.grpColIdx[i]);
}

static void
_outGroup(StringInfo str, Group *node)
{
	appendStringInfo(str, " GRP ");
	_outPlanInfo(str, (Plan *) node);

	/* the actual Group fields */
	appendStringInfo(str, " :numCols %d :tuplePerGroup %s ",
					 node->numCols,
					 booltostr(node->tuplePerGroup));
}

static void
_outWindow(StringInfo str, Window *node)
{
	appendStringInfo(str, " WND ");
	_outPlanInfo(str, (Plan *) node);

	/* the actual Window fields */
	appendStringInfo(str, " :wndClause ");
	_outNode(str, node->wndClause);
}

static void
_outUnique(StringInfo str, Unique *node)
{
	int			i;

	appendStringInfo(str, " UNIQUE ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :numCols %d :uniqColIdx ",
					 node->numCols);
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, "%d ", (int) node->uniqColIdx[i]);
}

static void
_outSetOp(StringInfo str, SetOp *node)
{
	int			i;

	appendStringInfo(str, " SETOP ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :cmd %d :numCols %d :dupColIdx ",
					 (int) node->cmd, node->numCols);
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, "%d ", (int) node->dupColIdx[i]);
	appendStringInfo(str, " :flagColIdx %d ",
					 (int) node->flagColIdx);
}

static void
_outLimit(StringInfo str, Limit *node)
{
	appendStringInfo(str, " LIMIT ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :limitOffset ");
	_outNode(str, node->limitOffset);
	appendStringInfo(str, " :limitCount ");
	_outNode(str, node->limitCount);
}

/*
 *	Hash is a subclass of Plan
 */
static void
_outHash(StringInfo str, Hash *node)
{
	appendStringInfo(str, " HASH ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :hashkey ");
	_outNode(str, node->hashkey);
}

/*																	 //@BdeddySK
 *	Eddy is a subclass of Plan
 */
static void
_outEddy(StringInfo str, Eddy * node)
{
	int			i;

	appendStringInfo(str, " EDDY ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :sources numSources: %d", node->numSources);

	for (i = 0; i < node->numSources; i++)
	{
		if (node->sources[i] == NULL)
			continue;
		_outNode(str, (node->sources)[i]);
	}

	appendStringInfo(str, " :modules numModules:%d ", node->numModules);

	for (i = 0; i < node->numModules; i++)
	{
		if (node->modules[i] == NULL)
			continue;
		_outNode(str, (node->modules)[i]);
	}

	appendStringInfo(str, " :cacq %s ", booltostr(node->cacq)); /* @dcacqSK */
	appendStringInfo(str, " :rangetable ");		/* @dcacqSK */
	_outNode(str, node->rangetable);	/* @dcacqSK */
}

/*@BdfluxMS */
/*
 * FSched is a subclass of Plan
 */
static void
_outFSched(StringInfo str, FSched * node)
{
	int			i;

	appendStringInfo(str, " FSCHED ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :dataflow segments numSegs:%d ", node->numSegs);

	for (i = 0; i < node->numSegs; i++)
		_outNode(str, (node->segs)[i]);

	appendStringInfo(str, " :top segment");
	_outNode(str, (node->topSeg));
}

/*@EdfluxMS */

static void
_outBitArray(StringInfo str, BitArray ba, BitIndex num)
{
    int i;

    for(i = 0; i < num; i++)
    {
        appendStringInfo(str, BitArrayBitIsSet(ba, i) ? "1" : "0");
    }
}

/*
 *	 SteM is a subclass of Plan
 */
static void
_outSteM(StringInfo str, SteM * node)
{
	appendStringInfo(str, " STEM ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :innerhashkey ");
	_outNode(str, node->innerhashkey);

	appendStringInfo(str, " :outerhashkey ");
	_outNode(str, node->outerhashkey);

	appendStringInfo(str, " :stemclauses ");
	_outNode(str, node->stemclauses);

	/* appendStringInfo(str, " :sourceBuiltOn %d ", STEM_SOURCE(node)); */
    appendStringInfo(str, " :sourceBuiltOn ");
    _outBitArray(str, node->sourceBuiltOn, TCQMAXSOURCES);

	if (node->innerNode)
	{
		appendStringInfo(str, " :innerNode ");
		_outNode(str, node->innerNode);
	}
}

/*
 *	Filter is a subclass of Plan
 */
static void
_outFilter(StringInfo str, Filter * node)
{
	appendStringInfo(str, " FILTER ");
	_outPlanInfo(str, (Plan *) node);

	appendStringInfo(str, " :filterqual ");
	_outNode(str, node->filterqual);
}	/* @EdeddySK */

/*																	 //@BdcacqSK
 *	GSFilter is a subclass of Fjord
 */
static void
_outGSFilter(StringInfo str, GSFilter * node)
{
	appendStringInfo(str, " GSFILTER ");
	_outPlanInfo(str, (Plan *) node);

	_outFjord(str, &node->fjord);

	appendStringInfo(str, " :eqoper ");
	_outNode(str, node->eqoper);

	appendStringInfo(str, " :neoper ");
	_outNode(str, node->neoper);
	appendStringInfo(str, " :ltoper ");
	_outNode(str, node->ltoper);
	appendStringInfo(str, " :gtoper ");
	_outNode(str, node->gtoper);
	appendStringInfo(str, " :var ");
	_outNode(str, node->node);

	appendStringInfo(str, " :ltmap ");
	_outNode(str, node->ltmap);

	appendStringInfo(str, " :rtmap ");
	_outNode(str, node->rtmap);
	appendStringInfo(str, " :eqmap ");
	_outNode(str, node->eqmap);
	appendStringInfo(str, " :nemap ");
	_outNode(str, node->nemap);

}	/* @EdcacqSK */

/*
 *  FSteMClauses
 */
static void
_outFSteMClauses(StringInfo str, FSteMClauses *fsc) 
{
	appendStringInfo(str, " FSTEMCLAUSES ");
	appendStringInfo(str, " :stemclauses ");
	_outNode(str, fsc->stemclauses);
	appendStringInfo(str, " :interested-queries ");
	_outIntList(str, fsc->interested_queries);
}

/*
 *  FSteMOuterHashkey
 */
static void
_outFSteMOuterHashkey(StringInfo str, FSteMOuterHashkey *foh) 
{
	List *l;
	
	appendStringInfo(str, " FSTEMOUTERHASHKEY ");
	appendStringInfo(str, " :outerhashkey ");
	_outNode(str, foh->outerhashkey);
	appendStringInfo(str, " :fstemclauses_list ");
	appendStringInfoChar(str, '(');
	foreach(l, foh->fstemclauses_list)
		{
			_outFSteMClauses(str,lfirst(l));
		}
	appendStringInfoChar(str, ')');
}

/*
 *	 SteM is a subclass of Fjord
 */
static void
_outFSteM(StringInfo str, FSteM * node)
{
	int			 i;
	char		 buf[256];
	List	   	*l;

	appendStringInfo(str, " FSTEM ");
	if (NULL == node)
 	{
		appendStringInfo(str, " is NULL ");
		return;
	}

	_outPlanInfo(str, (Plan *) node);

	_outFjord(str, &node->fjord);

	appendStringInfoChar(str, '(');
	foreach(l, node->fstemouterhashkeys)
	{
		_outFSteMOuterHashkey(str,lfirst(l));
	}
	appendStringInfoChar(str, ')');
	
	appendStringInfo(str, " :protostem ");
	_outNode(str, node->protostem);

	for (i = 0; i < FSTEM_NUM_SUBSTEMS; i++)
	{
		snprintf(buf, sizeof(buf), " :substem %d ", i);
		appendStringInfo(str, buf);
		if (NULL == node->substems[i].stem)
			appendStringInfo(str, " is NULL ");
		else
			_outNode(str, node->substems[i].stem);
	}
}

/*
 * FOutput is a subclass of Fjord
 */
static void
_outFOutput(StringInfo str, FOutput * node)
{
	appendStringInfo(str, " FOUTPUT ");
	if (NULL == node) 
	{
		appendStringInfo(str, " is NULL ");
		return;
	}

	_outPlanInfo(str, (Plan *) node);

	_outFjord(str, &node->fjord);

	appendStringInfo(str, " :signature: ");

	_outBitArray (str, node->signature, TCQMAXSOURCES);
}

/*
 * FSplit is a subclass of Fjord
 */
static void
_outFSplit(StringInfo str, FSplit * node)
{
	appendStringInfo(str, " FSPLIT ");
	if (NULL == node) 
	{
		appendStringInfo(str, " is NULL ");
		return;
	}

	_outPlanInfo(str, (Plan *) node);

	_outFjord(str, &node->fjord);
}

/*
 *	Filter is a subclass of Fjord
 */
static void
_outFFilter(StringInfo str, FFilter * node)
{
	appendStringInfo(str, " FFILTER ");
	if (NULL == node) 
	{
		appendStringInfo(str, " is NULL ");
		return;
	}

	_outPlanInfo(str, (Plan *) node);

	_outFjord(str, &node->fjord);
	
	appendStringInfo(str, " :filterqual ");
	_outNode(str, node->filterqual);
}	

/* 
 * FEventAgg is a sublcass of Fjord
 */
static void
_outFEventAgg(StringInfo str, FEventAgg * node)
{

  appendStringInfo(str, " _outFEventAgg: Not doing anything here for now ");
  


}




/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

/*
 *	Resdom is a subclass of Node
 */
static void
_outResdom(StringInfo str, Resdom *node)
{
	appendStringInfo(str,
				 " RESDOM :resno %d :restype %u :restypmod %d :resname ",
					 node->resno,
					 node->restype,
					 node->restypmod);
	_outToken(str, node->resname);
	appendStringInfo(str, " :reskey %u :reskeyop %u :ressortgroupref %u :resjunk %s ",
					 node->reskey,
					 node->reskeyop,
					 node->ressortgroupref,
					 booltostr(node->resjunk));
}

static void
_outFjoin(StringInfo str, Fjoin *node)
{
	int			i;

	appendStringInfo(str, " FJOIN :initialized %s :nNodes %d ",
					 booltostr(node->fj_initialized),
					 node->fj_nNodes);

	appendStringInfo(str, " :innerNode ");
	_outNode(str, node->fj_innerNode);

	appendStringInfo(str, " :results @ 0x%p :alwaysdone",
					 node->fj_results);

	for (i = 0; i < node->fj_nNodes; i++)
		appendStringInfo(str,
						 booltostr(node->fj_alwaysDone[i]));
}

/*
 *	Expr is a subclass of Node
 */
static void
_outExpr(StringInfo str, Expr *node)
{
	char	   *opstr = NULL;

	appendStringInfo(str, " EXPR :typeOid %u ",
					 node->typeOid);

	switch (node->opType)
	{
		case OP_EXPR:
			opstr = "op";
			break;
		case DISTINCT_EXPR:
			opstr = "distinct";
			break;
		case FUNC_EXPR:
			opstr = "func";
			break;
		case OR_EXPR:
			opstr = "or";
			break;
		case AND_EXPR:
			opstr = "and";
			break;
		case NOT_EXPR:
			opstr = "not";
			break;
		case SUBPLAN_EXPR:
			opstr = "subp";
			break;
	}
	appendStringInfo(str, " :opType ");
	_outToken(str, opstr);
	appendStringInfo(str, " :oper ");
	_outNode(str, node->oper);

	appendStringInfo(str, " :args ");
	_outNode(str, node->args);
}

/*
 *	Var is a subclass of Expr
 */
static void
_outVar(StringInfo str, Var *node)
{
	appendStringInfo(str,
				" VAR :varno %u :varattno %d :vartype %u :vartypmod %d ",
					 node->varno,
					 node->varattno,
					 node->vartype,
					 node->vartypmod);

	appendStringInfo(str, " :varlevelsup %u :varnoold %u :varoattno %d",
					 node->varlevelsup,
					 node->varnoold,
					 node->varoattno);
}

/*
 *	Const is a subclass of Expr
 */
static void
_outConst(StringInfo str, Const *node)
{
	appendStringInfo(str,
					 " CONST :consttype %u :constlen %d :constbyval %s"
					 " :constisnull %s :constvalue ",
					 node->consttype,
					 node->constlen,
					 booltostr(node->constbyval),
					 booltostr(node->constisnull));

	if (node->constisnull)
		appendStringInfo(str, "<>");
	else
		_outDatum(str, node->constvalue, node->constlen, node->constbyval);
}

/*
 *	Aggref
 */
static void
_outAggref(StringInfo str, Aggref *node)
{
	appendStringInfo(str, " AGGREG :aggfnoid %u :aggtype %u :target ",
					 node->aggfnoid, node->aggtype);
	_outNode(str, node->target);

	appendStringInfo(str, " :aggstar %s :aggdistinct %s ",
					 booltostr(node->aggstar),
					 booltostr(node->aggdistinct));
	/* aggno is not dumped */
}

/*
 *	SubLink
 */
static void
_outSubLink(StringInfo str, SubLink *node)
{
	appendStringInfo(str,
					 " SUBLINK :subLinkType %d :useor %s :lefthand ",
					 node->subLinkType,
					 booltostr(node->useor));
	_outNode(str, node->lefthand);

	appendStringInfo(str, " :oper ");
	_outNode(str, node->oper);

	appendStringInfo(str, " :subselect ");
	_outNode(str, node->subselect);
}

/*
 *	ArrayRef is a subclass of Expr
 */
static void
_outArrayRef(StringInfo str, ArrayRef *node)
{
	appendStringInfo(str,
		 " ARRAYREF :refrestype %u :refattrlength %d :refelemlength %d ",
					 node->refrestype,
					 node->refattrlength,
					 node->refelemlength);

	appendStringInfo(str,
				   ":refelembyval %s :refelemalign %c :refupperindexpr ",
					 booltostr(node->refelembyval),
					 node->refelemalign);
	_outNode(str, node->refupperindexpr);

	appendStringInfo(str, " :reflowerindexpr ");
	_outNode(str, node->reflowerindexpr);

	appendStringInfo(str, " :refexpr ");
	_outNode(str, node->refexpr);

	appendStringInfo(str, " :refassgnexpr ");
	_outNode(str, node->refassgnexpr);
}

/*
 *	Func is a subclass of Expr
 */
static void
_outFunc(StringInfo str, Func *node)
{
	appendStringInfo(str,
	" FUNC :funcid %u :funcresulttype %u :funcretset %s :funcformat %d ",
					 node->funcid,
					 node->funcresulttype,
					 booltostr(node->funcretset),
					 (int) node->funcformat);
}

/*
 *	Oper is a subclass of Expr
 */
static void
_outOper(StringInfo str, Oper *node)
{
	appendStringInfo(str,
				" OPER :opno %u :opid %u :opresulttype %u :opretset %s ",
					 node->opno,
					 node->opid,
					 node->opresulttype,
					 booltostr(node->opretset));
}

/*
 *	Param is a subclass of Expr
 */
static void
_outParam(StringInfo str, Param *node)
{
	appendStringInfo(str, " PARAM :paramkind %d :paramid %d :paramname ",
					 node->paramkind,
					 node->paramid);
	_outToken(str, node->paramname);
	appendStringInfo(str, " :paramtype %u ", node->paramtype);
}

/*
 *	FieldSelect
 */
static void
_outFieldSelect(StringInfo str, FieldSelect *node)
{
	appendStringInfo(str, " FIELDSELECT :arg ");
	_outNode(str, node->arg);

	appendStringInfo(str, " :fieldnum %d :resulttype %u :resulttypmod %d ",
				   node->fieldnum, node->resulttype, node->resulttypmod);
}

/*
 *	RelabelType
 */
static void
_outRelabelType(StringInfo str, RelabelType *node)
{
	appendStringInfo(str, " RELABELTYPE :arg ");
	_outNode(str, node->arg);
	appendStringInfo(str,
				   " :resulttype %u :resulttypmod %d :relabelformat %d ",
					 node->resulttype,
					 node->resulttypmod,
					 (int) node->relabelformat);
}

/*
 *	RangeTblRef
 */
static void
_outRangeTblRef(StringInfo str, RangeTblRef *node)
{
	appendStringInfo(str, " RANGETBLREF %d ",
					 node->rtindex);
}

/*
 *	FromExpr
 */
static void
_outFromExpr(StringInfo str, FromExpr *node)
{
	appendStringInfo(str, " FROMEXPR :fromlist ");
	_outNode(str, node->fromlist);
	appendStringInfo(str, " :quals ");
	_outNode(str, node->quals);
}

/*
 *	JoinExpr
 */
static void
_outJoinExpr(StringInfo str, JoinExpr *node)
{
	appendStringInfo(str, " JOINEXPR :jointype %d :isNatural %s :larg ",
					 (int) node->jointype,
					 booltostr(node->isNatural));
	_outNode(str, node->larg);
	appendStringInfo(str, " :rarg ");
	_outNode(str, node->rarg);
	appendStringInfo(str, " :using ");
	_outNode(str, node->using);
	appendStringInfo(str, " :quals ");
	_outNode(str, node->quals);
	appendStringInfo(str, " :alias ");
	_outNode(str, node->alias);
	appendStringInfo(str, " :rtindex %d ", node->rtindex);
}

/*																	 //@BdcacqSK
 *	ConstTree
 */
static void
_outConstTree(StringInfo str, ConstTree * node)
{
	if (node == NULL)
	{
		appendStringInfo(str, " CONSTTREE NULL ");
		return;
	}

	appendStringInfo(str, " CONSTTREE :c ");
	_outNode(str, node->c);

	appendStringInfo(str, " :queries ");
	_outIntList(str, node->queries);

	appendStringInfo(str, " :eqqueries ");
	_outIntList(str, node->eqqueries);
	appendStringInfo(str, " :left ");
	_outNode(str, node->left);

	appendStringInfo(str, " :right ");
	_outNode(str, node->right);
}	/* @EdcacqSK */

/*
 *	TargetEntry is a subclass of Node.
 */
static void
_outTargetEntry(StringInfo str, TargetEntry *node)
{
	appendStringInfo(str, " TARGETENTRY :resdom ");
	_outNode(str, node->resdom);

	appendStringInfo(str, " :expr ");
	_outNode(str, node->expr);
}

static void
_outAlias(StringInfo str, Alias *node)
{
	appendStringInfo(str, " ALIAS :aliasname ");
	_outToken(str, node->aliasname);
	appendStringInfo(str, " :colnames ");
	_outNode(str, node->colnames);
}

static void
_outRangeTblEntry(StringInfo str, RangeTblEntry *node)
{
	/* put alias + eref first to make dump more legible */
	appendStringInfo(str, " RTE :alias ");
	_outNode(str, node->alias);
	appendStringInfo(str, " :eref ");
	_outNode(str, node->eref);
	appendStringInfo(str, " :rtekind %d ",
					 (int) node->rtekind);
	switch (node->rtekind)
	{
		case RTE_RELATION:
		case RTE_SPECIAL:
			appendStringInfo(str, ":relid %u", node->relid);
			break;
		case RTE_SUBQUERY:
			appendStringInfo(str, ":subquery ");
			_outNode(str, node->subquery);
			break;
		case RTE_FUNCTION:
			appendStringInfo(str, ":funcexpr ");
			_outNode(str, node->funcexpr);
			appendStringInfo(str, " :coldeflist ");
			_outNode(str, node->coldeflist);
			break;
		case RTE_JOIN:
			appendStringInfo(str, ":jointype %d :joinaliasvars ",
							 (int) node->jointype);
			_outNode(str, node->joinaliasvars);
			break;
		default:
			elog(ERROR, "bogus rte kind %d", (int) node->rtekind);
			break;
	}
	appendStringInfo(str, " :inh %s :inFromCl %s :checkForRead %s"
					 " :checkForWrite %s :checkAsUser %u",
					 booltostr(node->inh),
					 booltostr(node->inFromCl),
					 booltostr(node->checkForRead),
					 booltostr(node->checkForWrite),
					 node->checkAsUser);
}

/*
 *	Path is a subclass of Node.
 */
static void
_outPath(StringInfo str, Path *node)
{
	appendStringInfo(str,
	 " PATH :pathtype %d :startup_cost %.2f :total_cost %.2f :pathkeys ",
					 node->pathtype,
					 node->startup_cost,
					 node->total_cost);
	_outNode(str, node->pathkeys);
}

/*
 *	IndexPath is a subclass of Path.
 */
static void
_outIndexPath(StringInfo str, IndexPath *node)
{
	appendStringInfo(str,
					 " INDEXPATH :pathtype %d :startup_cost %.2f :total_cost %.2f :pathkeys ",
					 node->path.pathtype,
					 node->path.startup_cost,
					 node->path.total_cost);
	_outNode(str, node->path.pathkeys);

	appendStringInfo(str, " :indexinfo ");
	_outNode(str, node->indexinfo);

	appendStringInfo(str, " :indexqual ");
	_outNode(str, node->indexqual);

	appendStringInfo(str, " :indexscandir %d :joinrelids ",
					 (int) node->indexscandir);
	_outIntList(str, node->joinrelids);

	appendStringInfo(str, " :alljoinquals %s :rows %.2f ",
					 booltostr(node->alljoinquals),
					 node->rows);
}

/*
 *	TidPath is a subclass of Path.
 */
static void
_outTidPath(StringInfo str, TidPath *node)
{
	appendStringInfo(str,
					 " TIDPATH :pathtype %d :startup_cost %.2f :total_cost %.2f :pathkeys ",
					 node->path.pathtype,
					 node->path.startup_cost,
					 node->path.total_cost);
	_outNode(str, node->path.pathkeys);

	appendStringInfo(str, " :tideval ");
	_outNode(str, node->tideval);

	appendStringInfo(str, " :unjoined_relids ");
	_outIntList(str, node->unjoined_relids);
}

/*
 *	AppendPath is a subclass of Path.
 */
static void
_outAppendPath(StringInfo str, AppendPath *node)
{
	appendStringInfo(str,
					 " APPENDPATH :pathtype %d :startup_cost %.2f :total_cost %.2f :pathkeys ",
					 node->path.pathtype,
					 node->path.startup_cost,
					 node->path.total_cost);
	_outNode(str, node->path.pathkeys);

	appendStringInfo(str, " :subpaths ");
	_outNode(str, node->subpaths);
}

/*
 *	NestPath is a subclass of Path
 */
static void
_outNestPath(StringInfo str, NestPath *node)
{
	appendStringInfo(str,
					 " NESTPATH :pathtype %d :startup_cost %.2f :total_cost %.2f :pathkeys ",
					 node->path.pathtype,
					 node->path.startup_cost,
					 node->path.total_cost);
	_outNode(str, node->path.pathkeys);
	appendStringInfo(str, " :jointype %d :outerjoinpath ",
					 (int) node->jointype);
	_outNode(str, node->outerjoinpath);
	appendStringInfo(str, " :innerjoinpath ");
	_outNode(str, node->innerjoinpath);
	appendStringInfo(str, " :joinrestrictinfo ");
	_outNode(str, node->joinrestrictinfo);
}

/*
 *	MergePath is a subclass of NestPath.
 */
static void
_outMergePath(StringInfo str, MergePath *node)
{
	appendStringInfo(str,
					 " MERGEPATH :pathtype %d :startup_cost %.2f :total_cost %.2f :pathkeys ",
					 node->jpath.path.pathtype,
					 node->jpath.path.startup_cost,
					 node->jpath.path.total_cost);
	_outNode(str, node->jpath.path.pathkeys);
	appendStringInfo(str, " :jointype %d :outerjoinpath ",
					 (int) node->jpath.jointype);
	_outNode(str, node->jpath.outerjoinpath);
	appendStringInfo(str, " :innerjoinpath ");
	_outNode(str, node->jpath.innerjoinpath);
	appendStringInfo(str, " :joinrestrictinfo ");
	_outNode(str, node->jpath.joinrestrictinfo);

	appendStringInfo(str, " :path_mergeclauses ");
	_outNode(str, node->path_mergeclauses);

	appendStringInfo(str, " :outersortkeys ");
	_outNode(str, node->outersortkeys);

	appendStringInfo(str, " :innersortkeys ");
	_outNode(str, node->innersortkeys);
}

/*
 *	HashPath is a subclass of NestPath.
 */
static void
_outHashPath(StringInfo str, HashPath *node)
{
	appendStringInfo(str,
					 " HASHPATH :pathtype %d :startup_cost %.2f :total_cost %.2f :pathkeys ",
					 node->jpath.path.pathtype,
					 node->jpath.path.startup_cost,
					 node->jpath.path.total_cost);
	_outNode(str, node->jpath.path.pathkeys);
	appendStringInfo(str, " :jointype %d :outerjoinpath ",
					 (int) node->jpath.jointype);
	_outNode(str, node->jpath.outerjoinpath);
	appendStringInfo(str, " :innerjoinpath ");
	_outNode(str, node->jpath.innerjoinpath);
	appendStringInfo(str, " :joinrestrictinfo ");
	_outNode(str, node->jpath.joinrestrictinfo);

	appendStringInfo(str, " :path_hashclauses ");
	_outNode(str, node->path_hashclauses);
}

/*
 *	PathKeyItem is a subclass of Node.
 */
static void
_outPathKeyItem(StringInfo str, PathKeyItem *node)
{
	appendStringInfo(str, " PATHKEYITEM :sortop %u :key ",
					 node->sortop);
	_outNode(str, node->key);
}

/*
 *	RestrictInfo is a subclass of Node.
 */
static void
_outRestrictInfo(StringInfo str, RestrictInfo *node)
{
	appendStringInfo(str, " RESTRICTINFO :clause ");
	_outNode(str, node->clause);

	appendStringInfo(str, " :ispusheddown %s :subclauseindices ",
					 booltostr(node->ispusheddown));
	_outNode(str, node->subclauseindices);

	appendStringInfo(str, " :mergejoinoperator %u ", node->mergejoinoperator);
	appendStringInfo(str, " :left_sortop %u ", node->left_sortop);
	appendStringInfo(str, " :right_sortop %u ", node->right_sortop);
	appendStringInfo(str, " :hashjoinoperator %u ", node->hashjoinoperator);
}

/*
 *	JoinInfo is a subclass of Node.
 */
static void
_outJoinInfo(StringInfo str, JoinInfo *node)
{
	appendStringInfo(str, " JINFO :unjoined_relids ");
	_outIntList(str, node->unjoined_relids);

	appendStringInfo(str, " :jinfo_restrictinfo ");
	_outNode(str, node->jinfo_restrictinfo);
}

/*
 * Print the value of a Datum given its type.
 */
static void
_outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
	Size		length,
				i;
	char	   *s;

	length = datumGetSize(value, typbyval, typlen);

	if (typbyval)
	{
		s = (char *) (&value);
		appendStringInfo(str, " %u [ ", (unsigned int) length);
		for (i = 0; i < (Size) sizeof(Datum); i++)
			appendStringInfo(str, "%d ", (int) (s[i]));
		appendStringInfo(str, "] ");
	}
	else
	{
		s = (char *) DatumGetPointer(value);
		if (!PointerIsValid(s))
			appendStringInfo(str, " 0 [ ] ");
		else
		{
			appendStringInfo(str, " %u [ ", (unsigned int) length);
			for (i = 0; i < length; i++)
				appendStringInfo(str, "%d ", (int) (s[i]));
			appendStringInfo(str, "] ");
		}
	}
}

static void
_outAExpr(StringInfo str, A_Expr *node)
{
	appendStringInfo(str, " AEXPR ");
	switch (node->oper)
	{
		case AND:
			appendStringInfo(str, "AND ");
			break;
		case OR:
			appendStringInfo(str, "OR ");
			break;
		case NOT:
			appendStringInfo(str, "NOT ");
			break;
		case OP:
			_outNode(str, node->name);
			appendStringInfo(str, " ");
			break;
		default:
			appendStringInfo(str, "?? ");
			break;
	}
	_outNode(str, node->lexpr);
	appendStringInfo(str, " ");
	_outNode(str, node->rexpr);
}

static void
_outValue(StringInfo str, Value *value)
{
	switch (value->type)
	{
		case T_Integer:
			appendStringInfo(str, " %ld ", value->val.ival);
			break;
		case T_Float:

			/*
			 * We assume the value is a valid numeric literal and so does
			 * not need quoting.
			 */
			appendStringInfo(str, " %s ", value->val.str);
			break;
		case T_String:
			appendStringInfo(str, " \"");
			_outToken(str, value->val.str);
			appendStringInfo(str, "\" ");
			break;
		case T_BitString:
			/* internal representation already has leading 'b' */
			appendStringInfo(str, " %s ", value->val.str);
			break;
		default:
			elog(WARNING, "_outValue: don't know how to print type %d ",
				 value->type);
			break;
	}
}

static void
_outRangeVar(StringInfo str, RangeVar *node)
{
	appendStringInfo(str, " RANGEVAR :relation ");

	/*
	 * we deliberately ignore catalogname here, since it is presently not
	 * semantically meaningful
	 */
	_outToken(str, node->schemaname);
	appendStringInfo(str, " . ");
	_outToken(str, node->relname);
	appendStringInfo(str, " :inhopt %d :istemp %s",
					 (int) node->inhOpt,
					 booltostr(node->istemp));
	appendStringInfo(str, " :alias ");
	_outNode(str, node->alias);
}

static void
_outColumnRef(StringInfo str, ColumnRef *node)
{
	appendStringInfo(str, " COLUMNREF :fields ");
	_outNode(str, node->fields);
	appendStringInfo(str, " :indirection ");
	_outNode(str, node->indirection);
}

static void
_outParamRef(StringInfo str, ParamRef *node)
{
	appendStringInfo(str, " PARAMREF :number %d :fields ", node->number);
	_outNode(str, node->fields);
	appendStringInfo(str, " :indirection ");
	_outNode(str, node->indirection);
}

static void
_outAConst(StringInfo str, A_Const *node)
{
	appendStringInfo(str, "CONST ");
	_outValue(str, &(node->val));
	appendStringInfo(str, " :typename ");
	_outNode(str, node->typename);
}

static void
_outExprFieldSelect(StringInfo str, ExprFieldSelect *node)
{
	appendStringInfo(str, " EXPRFIELDSELECT :arg ");
	_outNode(str, node->arg);
	appendStringInfo(str, " :fields ");
	_outNode(str, node->fields);
	appendStringInfo(str, " :indirection ");
	_outNode(str, node->indirection);
}

static void
_outConstraint(StringInfo str, Constraint *node)
{
	appendStringInfo(str, " CONSTRAINT :name ");
	_outToken(str, node->name);
	appendStringInfo(str, " :type ");

	switch (node->contype)
	{
		case CONSTR_PRIMARY:
			appendStringInfo(str, "PRIMARY_KEY :keys ");
			_outNode(str, node->keys);
			break;

		case CONSTR_CHECK:
			appendStringInfo(str, "CHECK :raw ");
			_outNode(str, node->raw_expr);
			appendStringInfo(str, " :cooked ");
			_outToken(str, node->cooked_expr);
			break;

		case CONSTR_DEFAULT:
			appendStringInfo(str, "DEFAULT :raw ");
			_outNode(str, node->raw_expr);
			appendStringInfo(str, " :cooked ");
			_outToken(str, node->cooked_expr);
			break;

		case CONSTR_NOTNULL:
			appendStringInfo(str, "NOT_NULL");
			break;

		case CONSTR_TIMESTAMPCOLUMN:	/* @dtstampSK */
			appendStringInfo(str, "TIMESTAMP :keys ");	/* @dtstampSK */
			_outNode(str, node->keys);	/* @dtstampSK */
			break;

		case CONSTR_UNIQUE:
			appendStringInfo(str, "UNIQUE :keys ");
			_outNode(str, node->keys);
			break;

		default:
			appendStringInfo(str, "<unrecognized_constraint>");
			break;
	}
}

static void
_outFkConstraint(StringInfo str, FkConstraint *node)
{
	appendStringInfo(str, " FKCONSTRAINT :constr_name ");
	_outToken(str, node->constr_name);
	appendStringInfo(str, " :pktable ");
	_outNode(str, node->pktable);
	appendStringInfo(str, " :fk_attrs ");
	_outNode(str, node->fk_attrs);
	appendStringInfo(str, " :pk_attrs ");
	_outNode(str, node->pk_attrs);
	appendStringInfo(str, " :fk_matchtype %c :fk_upd_action %c :fk_del_action %c :deferrable %s :initdeferred %s :skip_validation %s",
					 node->fk_matchtype,
					 node->fk_upd_action,
					 node->fk_del_action,
					 booltostr(node->deferrable),
					 booltostr(node->initdeferred),
					 booltostr(node->skip_validation));
}

static void
_outCaseExpr(StringInfo str, CaseExpr *node)
{
	appendStringInfo(str, " CASE :casetype %u :arg ",
					 node->casetype);
	_outNode(str, node->arg);

	appendStringInfo(str, " :args ");
	_outNode(str, node->args);

	appendStringInfo(str, " :defresult ");
	_outNode(str, node->defresult);
}

static void
_outCaseWhen(StringInfo str, CaseWhen *node)
{
	appendStringInfo(str, " WHEN ");
	_outNode(str, node->expr);

	appendStringInfo(str, " :then ");
	_outNode(str, node->result);
}

/*
 *	NullTest
 */
static void
_outNullTest(StringInfo str, NullTest *node)
{
	appendStringInfo(str, " NULLTEST :arg ");
	_outNode(str, node->arg);
	appendStringInfo(str, " :nulltesttype %d ",
					 (int) node->nulltesttype);
}

/*
 *	BooleanTest
 */
static void
_outBooleanTest(StringInfo str, BooleanTest *node)
{
	appendStringInfo(str, " BOOLEANTEST :arg ");
	_outNode(str, node->arg);
	appendStringInfo(str, " :booltesttype %d ",
					 (int) node->booltesttype);
}

/*
 * RangeVar
 */
static void
_outResTarget(StringInfo str, ResTarget *node)
{
	/* More to do here. */
	appendStringInfo(str, " RESTARGET :col ");
	if (node->name != NULL)
		appendStringInfo(str, node->name);
	if (node->val != NULL)
		_outNode(str, node->val);
}

/*
 *	ConstraintTest
 */
static void
_outConstraintTest(StringInfo str, ConstraintTest *node)
{
	appendStringInfo(str, " CONSTRAINTTEST :arg ");
	_outNode(str, node->arg);
	appendStringInfo(str, " :testtype %d :name ",
					 (int) node->testtype);
	_outToken(str, node->name);
	appendStringInfo(str, " :check_expr ");
	_outNode(str, node->check_expr);
}


/*
 *	RelOptInfo			@dstemAD
 *
 *	FIXME: NOT COMPLETE
 */
static void
_outRelOptInfo(StringInfo str, RelOptInfo *node)
{
	appendStringInfo(str, " RELOPTINFO ");

	appendStringInfo(str, " :relids ");
	_outIntList(str, node->relids);

	appendStringInfo(str, " :targetlist ");
	_outNode(str, node->targetlist);

	appendStringInfo(str, " :pathlist ");
	_outNode(str, node->pathlist);

	appendStringInfo(str, " :baserestrictinfo ");
	_outNode(str, node->baserestrictinfo);

	appendStringInfo(str, " :outerjoinset ");
	_outIntList(str, node->outerjoinset);

	appendStringInfo(str, " :joininfo ");
	_outNode(str, node->joininfo);
}

static void
_outBinding(StringInfo str, Binding * node)
{
	appendStringInfo(str, " Binding ");
	appendStringInfo(str, " isOptional: %s  ", booltostr(node->isOptional));
	appendStringInfo(str, " column: ");
	_outNode(str, node->column);
	appendStringInfo(str, " default:  ");
	_outNode(str, node->def);
}

/*
 * _outNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
static void
_outNode(StringInfo str, void *obj)
{
	if (obj == NULL)
	{
		appendStringInfo(str, "<>");
		return;
	}

	if (IsA(obj, List))
	{
		List	   *l;

		appendStringInfoChar(str, '(');
		foreach(l, (List *) obj)
		{
			_outNode(str, lfirst(l));
			if (lnext(l))
				appendStringInfoChar(str, ' ');
		}
		appendStringInfoChar(str, ')');
	}
	else if (IsA(obj, Integer) || IsA(obj, Float) || IsA(obj, String) || IsA(obj, BitString))
	{
		/* nodeRead does not want to see { } around these! */
		_outValue(str, obj);
	}
	else
	{
		appendStringInfoChar(str, '{');
		switch (nodeTag(obj))
		{
		case T_Binding:
		  _outBinding(str, obj);
		  break;
		case T_CreateStmt:
				_outCreateStmt(str, obj);
				break;
			case T_CreateWrapperStmt:
				_outCreateWrapperStmt(str, obj);
				break;
			case T_CreateStrmStmt:
				_outCreateStrmStmt(str, obj);
				break;
			case T_IndexStmt:
				_outIndexStmt(str, obj);
				break;
			case T_NotifyStmt:
				_outNotifyStmt(str, obj);
				break;
			case T_SelectStmt:
				_outSelectStmt(str, obj);
				break;
			case T_ColumnDef:
				_outColumnDef(str, obj);
				break;
			case T_TypeName:
				_outTypeName(str, obj);
				break;
			case T_TypeCast:
				_outTypeCast(str, obj);
				break;
			case T_IndexElem:
				_outIndexElem(str, obj);
				break;
			case T_WindowExpr:	/* @BdwndwSK */
				_outWindowExpr(str, obj);
				break;			/* @EdwndwSK */
			case T_Query:
				_outQuery(str, obj);
				break;
			case T_SortClause:
				_outSortClause(str, obj);
				break;
			case T_GroupClause:
				_outGroupClause(str, obj);
				break;
			case T_SetOperationStmt:
				_outSetOperationStmt(str, obj);
				break;
			case T_Plan:
				_outPlan(str, obj);
				break;
			case T_Result:
				_outResult(str, obj);
				break;
			case T_Append:
				_outAppend(str, obj);
				break;
			case T_Join:
				_outJoin(str, obj);
				break;
			case T_NestLoop:
				_outNestLoop(str, obj);
				break;
			case T_MergeJoin:
				_outMergeJoin(str, obj);
				break;
			case T_HashJoin:
				_outHashJoin(str, obj);
				break;
			case T_Scan:
				_outScan(str, obj);
				break;
			case T_SeqScan:
				_outSeqScan(str, obj);
				break;
			case T_IndexScan:
				_outIndexScan(str, obj);
				break;
			case T_TidScan:
				_outTidScan(str, obj);
				break;
			case T_ScanQueue:
				_outScanQueue(str, obj);
				break;
			case T_StrmScan:
				_outStrmScan(str, obj);
				break;
			case T_SubqueryScan:
				_outSubqueryScan(str, obj);
				break;
			case T_FunctionScan:
				_outFunctionScan(str, obj);
				break;
			case T_Material:
				_outMaterial(str, obj);
				break;
			case T_Sort:
				_outSort(str, obj);
				break;
			case T_Agg:
				_outAgg(str, obj);
				break;
				/* @BdfluxMS */
			case T_HashGroup:
				_outHashGroup(str, obj);
				break;
				/* @EdfluxMS */
			case T_Group:
				_outGroup(str, obj);
				break;
			case T_Window:
				_outWindow(str, obj);
				break;
			case T_Unique:
				_outUnique(str, obj);
				break;
			case T_SetOp:
				_outSetOp(str, obj);
				break;
			case T_Limit:
				_outLimit(str, obj);
				break;
			case T_Hash:
				_outHash(str, obj);
				break;
			case T_SubPlan:
				_outSubPlan(str, obj);
				break;
			case T_Resdom:
				_outResdom(str, obj);
				break;
			case T_Fjoin:
				_outFjoin(str, obj);
				break;
			case T_Expr:
				_outExpr(str, obj);
				break;
			case T_Var:
				_outVar(str, obj);
				break;
			case T_Const:
				_outConst(str, obj);
				break;
			case T_Aggref:
				_outAggref(str, obj);
				break;
			case T_SubLink:
				_outSubLink(str, obj);
				break;
			case T_ArrayRef:
				_outArrayRef(str, obj);
				break;
			case T_Func:
				_outFunc(str, obj);
				break;
			case T_Oper:
				_outOper(str, obj);
				break;
			case T_Param:
				_outParam(str, obj);
				break;
			case T_FieldSelect:
				_outFieldSelect(str, obj);
				break;
			case T_RelabelType:
				_outRelabelType(str, obj);
				break;
			case T_RangeTblRef:
				_outRangeTblRef(str, obj);
				break;
			case T_FromExpr:
				_outFromExpr(str, obj);
				break;
			case T_JoinExpr:
				_outJoinExpr(str, obj);
				break;
			case T_ConstTree:	/* @BdcacqSK */
				_outConstTree(str, obj);
				break;			/* @EdcacqSK */
			case T_TargetEntry:
				_outTargetEntry(str, obj);
				break;
			case T_Alias:
				_outAlias(str, obj);
				break;
			case T_RangeTblEntry:
				_outRangeTblEntry(str, obj);
				break;
			case T_Path:
				_outPath(str, obj);
				break;
			case T_IndexPath:
				_outIndexPath(str, obj);
				break;
			case T_TidPath:
				_outTidPath(str, obj);
				break;
			case T_AppendPath:
				_outAppendPath(str, obj);
				break;
			case T_NestPath:
				_outNestPath(str, obj);
				break;
			case T_MergePath:
				_outMergePath(str, obj);
				break;
			case T_HashPath:
				_outHashPath(str, obj);
				break;
			case T_PathKeyItem:
				_outPathKeyItem(str, obj);
				break;
			case T_RestrictInfo:
				_outRestrictInfo(str, obj);
				break;
			case T_JoinInfo:
				_outJoinInfo(str, obj);
				break;
			case T_A_Expr:
				_outAExpr(str, obj);
				break;
			case T_RangeVar:
				_outRangeVar(str, obj);
				break;
			case T_ColumnRef:
				_outColumnRef(str, obj);
				break;
			case T_ParamRef:
				_outParamRef(str, obj);
				break;
			case T_A_Const:
				_outAConst(str, obj);
				break;
			case T_ExprFieldSelect:
				_outExprFieldSelect(str, obj);
				break;
			case T_Constraint:
				_outConstraint(str, obj);
				break;
			case T_FkConstraint:
				_outFkConstraint(str, obj);
				break;
			case T_CaseExpr:
				_outCaseExpr(str, obj);
				break;
			case T_CaseWhen:
				_outCaseWhen(str, obj);
				break;
			case T_NullTest:
				_outNullTest(str, obj);
				break;
			case T_BooleanTest:
				_outBooleanTest(str, obj);
				break;
			case T_ConstraintTest:
				_outConstraintTest(str, obj);
				break;
			case T_FuncCall:
				_outFuncCall(str, obj);
				break;
			case T_Eddy:		
				_outEddy(str, obj);
				break;
			case T_FSched:
				_outFSched(str, obj);
				break;
			case T_Filter:
				_outFilter(str, obj);
				break;			
			case T_GSFilter:	
				_outGSFilter(str, obj);
				break;			
			case T_ScanModule:
				_outScanModule(str, obj);
				break;
			case T_ResTarget:
				_outResTarget(str, obj);
				break;
			case T_SteM:
				_outSteM(str, obj);	
				break;			
			case T_FSteM:		
				_outFSteM(str, obj);
				break;
			case T_FOutput:
				_outFOutput(str,obj);
				break;
			case T_FSplit:                                      /* @dsigmodSK */
				_outFSplit(str,obj);                            /* @dsigmodSK */
				break;                                          /* @dsigmodSK */
			case T_FFilter:                                     /* @dsigmodSK */
				_outFFilter(str,obj);							/* @dsigmodSK */
				break;                                          /* @dsigmodSK */
			case T_SteMHashJoin:
				_outSteMHashJoin(str, obj);
				break;
		case T_FEventAgg:
		  _outFEventAgg(str, obj);
		  break;
			case T_RelOptInfo:	
				_outRelOptInfo(str, obj);
				break;

			default:
				elog(WARNING, "_outNode: don't know how to print type %d ",
					 nodeTag(obj));
				break;
		}
		appendStringInfoChar(str, '}');
	}
}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToString(void *obj)
{
	StringInfoData str;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfo(&str);
	_outNode(&str, obj);
	return str.data;
}


