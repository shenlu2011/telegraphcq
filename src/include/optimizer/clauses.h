/*-------------------------------------------------------------------------
 *
 * clauses.h
 *	  prototypes for clauses.c.
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: clauses.h,v 1.3 2003/07/16 09:42:51 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef CLAUSES_H
#define CLAUSES_H

#include "nodes/relation.h"

extern Expr *make_clause(int type, Node *oper, List *args);

extern bool is_opclause(Node *clause);
extern Expr *make_opclause(Oper *op, Var *leftop, Var *rightop);
extern Var *get_leftop(Expr *clause);
extern Var *get_rightop(Expr *clause);

extern bool is_funcclause(Node *clause);
extern Expr *make_funcclause(Func *func, List *funcargs);

extern bool or_clause(Node *clause);
extern Expr *make_orclause(List *orclauses);

extern bool not_clause(Node *clause);
extern Expr *make_notclause(Expr *notclause);
extern Expr *get_notclausearg(Expr *notclause);

extern bool and_clause(Node *clause);
extern Expr *make_andclause(List *andclauses);
extern Node *make_and_qual(Node *qual1, Node *qual2);
extern Expr *make_ands_explicit(List *andclauses);
extern List *make_ands_implicit(Expr *clause);

extern bool contain_agg_clause(Node *clause);
extern bool contain_distinct_agg_clause(Node *clause);
extern List *pull_agg_clause(Node *clause);

extern bool expression_returns_set(Node *clause);

extern bool contain_subplans(Node *clause);
extern List *pull_subplans(Node *clause);
extern void check_subplans_for_ungrouped_vars(Query *query);

extern bool contain_mutable_functions(Node *clause);
extern bool contain_volatile_functions(Node *clause);

extern bool is_pseudo_constant_clause(Node *clause);
extern List *pull_constant_clauses(List *quals, List **constantQual);

extern bool has_distinct_on_clause(Query *query);

extern void clause_get_relids_vars(Node *clause, Relids *relids, List **vars);
extern int	NumRelids(Node *clause);
extern void CommuteClause(Expr *clause);

extern Node *eval_const_expressions(Node *node);

extern bool expression_tree_walker(Node *node, bool (*walker) (),
											   void *context);
extern Node *expression_tree_mutator(Node *node, Node *(*mutator) (),
												 void *context);

/* flags bits for query_tree_walker and query_tree_mutator */
#define QTW_IGNORE_SUBQUERIES	0x01	/* subqueries in rtable */
#define QTW_IGNORE_JOINALIASES	0x02	/* JOIN alias var lists */

extern bool query_tree_walker(Query *query, bool (*walker) (),
										  void *context, int flags);
extern void query_tree_mutator(Query *query, Node *(*mutator) (),
										   void *context, int flags);

#define is_subplan(clause)	((clause) != NULL && \
							 IsA(clause, Expr) && \
							 ((Expr *) (clause))->opType == SUBPLAN_EXPR)

#endif   /* CLAUSES_H */
