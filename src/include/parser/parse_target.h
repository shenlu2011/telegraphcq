/*-------------------------------------------------------------------------
 *
 * parse_target.h
 *	  handle target lists
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: parse_target.h,v 1.1.1.2 2003/04/17 23:02:40 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_TARGET_H
#define PARSE_TARGET_H

#include "parser/parse_node.h"


extern List *transformTargetList(ParseState *pstate, List *targetlist);
extern TargetEntry *transformTargetEntry(ParseState *pstate,
					 Node *node, Node *expr,
					 char *colname, bool resjunk);
extern void updateTargetListEntry(ParseState *pstate, TargetEntry *tle,
					  char *colname, int attrno,
					  List *indirection);
extern List *checkInsertTargets(ParseState *pstate, List *cols,
				   List **attrnos);

#endif   /* PARSE_TARGET_H */
