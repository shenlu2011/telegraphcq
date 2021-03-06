/*-------------------------------------------------------------------------
 *
 * parse_agg.h
 *
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: parse_agg.h,v 1.1.1.2 2003/04/17 23:02:39 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_AGG_H
#define PARSE_AGG_H

#include "parser/parse_node.h"

extern void parseCheckAggregates(ParseState *pstate, Query *qry, Node *qual);

#endif   /* PARSE_AGG_H */
