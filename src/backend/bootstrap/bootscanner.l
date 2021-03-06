%{
/*-------------------------------------------------------------------------
 *
 * bootscanner.l
 *	  a lexical scanner for the bootstrap parser
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/bootstrap/bootscanner.l,v 1.2 2004/03/24 08:10:50 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <time.h>

#include "access/attnum.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/skey.h"
#include "access/strat.h"
#include "access/tupdesc.h"
#include "bootstrap/bootstrap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_class.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/pg_list.h"
#include "nodes/primnodes.h"
#include "parser/scansup.h"
#include "rewrite/prs2lock.h"
#include "storage/block.h"
#include "storage/fd.h"
#include "storage/itemptr.h"
#include "storage/off.h"
#include "utils/nabstime.h"
#include "utils/rel.h"

/* Not needed now that this file is compiled as part of bootparse. */
/* #include "bootstrap_tokens.h" */


int		yyline;  /* keep track of the line number for error reporting */

%}

%option 8bit
%option never-interactive
%option nounput
%option noyywrap


D		[0-9]
oct		\\{D}{D}{D}
Exp		[Ee][-+]?{D}+
id		([A-Za-z0-9_]|{oct}|\-)+
sid		\"([^\"])*\"
arrayid [A-Za-z0-9_]+\[{D}*\]

%%

open			{ return(OPEN); }

close			{ return(XCLOSE); }

create			{ return(XCREATE); }

OID				{ return(OBJ_ID); }
bootstrap		{ return(XBOOTSTRAP); }
"shared_relation"	{ return(XSHARED_RELATION); }
"without_oids"	{ return(XWITHOUT_OIDS); }
_null_			{ return(NULLVAL); }

insert			{ return(INSERT_TUPLE); }

","				{ return(COMMA); }
"="				{ return(EQUALS); }
"("				{ return(LPAREN); }
")"				{ return(RPAREN); }

[\n]			{ yyline++; }
[\t]			;
" "				;

^\#[^\n]* ; /* drop everything after "#" for comments */


"declare"		{ return(XDECLARE); }
"build"			{ return(XBUILD); }
"indices"		{ return(INDICES); }
"unique"		{ return(UNIQUE); }
"index"			{ return(INDEX); }
"on"			{ return(ON); }
"using"			{ return(USING); }

{arrayid}		{
					yylval.ival = EnterString(MapArrayTypeName((char*)yytext));
					return(ID);
				}
{id}			{
					char   *newid = scanstr((char*)yytext);
					yylval.ival = EnterString(newid);
					pfree(newid);
					return(ID);
				}
{sid}			{
					char   *newid;
					yytext[strlen(yytext)-1] = '\0'; /* strip off quotes */
					newid = scanstr((char*)yytext+1);
					yylval.ival = EnterString(newid);
					pfree(newid);
					yytext[strlen(yytext)] = '"'; /* restore quotes */
					return(ID);
				}

(-)?{D}+"."{D}*({Exp})? |
(-)?{D}*"."{D}+({Exp})? |
(-)?{D}+{Exp}			{
							yylval.ival = EnterString((char*)yytext);
							return(CONST_P);
						}

.				{
					elog(ERROR, "syntax error at line %d: unexpected character %s", yyline, yytext);
				}



%%

void
yyerror(const char *str)
{
	elog(ERROR, "syntax error at line %d: unexpected token %s", yyline, str);
}
