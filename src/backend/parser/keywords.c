
/*-------------------------------------------------------------------------
 *
 * keywords.c
 *	  lexical token lookup for reserved words in PostgreSQL
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/parser/keywords.c,v 1.23 2005/06/13 23:44:53 phred Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "nodes/parsenodes.h"
#include "parser/keywords.h"
#include "parser/parse.h"

/* NB: This file is also used by pg_dump. */

/*
 * List of (keyword-name, keyword-token-value) pairs.
 *
 * !!WARNING!!: This list must be sorted, because binary
 *		 search is used to locate entries.
 */
static const ScanKeyword ScanKeywords[] = {
	/* name, value */
	{"abort", ABORT_TRANS},
	{"absolute", ABSOLUTE},
	{"access", ACCESS},
	{"action", ACTION},
	{"add", ADD},
	{"after", AFTER},
	{"aggregate", AGGREGATE},
	{"all", ALL},
	{"alter", ALTER},
	{"analyse", ANALYSE},		/* British spelling */
	{"analyze", ANALYZE},
	{"and", AND},
	{"any", ANY},
	{"anyone", ANYONE},
	{"archived", ARCHIVED},		/* @dtcqSK */
	{"as", AS},
	{"asc", ASC},
	{"assertion", ASSERTION},
 	{"assignment", ASSIGNMENT},
	{"at", AT},
	{"authorization", AUTHORIZATION},
	{"backward", BACKWARD},
	{"before", BEFORE},
	{"begin", BEGIN_TRANS},
	{"between", BETWEEN},
	{"bigint", BIGINT},
	{"binary", BINARY},
	{"bindings", BINDINGS},
	{"bit", BIT},
    {"block", BLOCK},
	{"boolean", BOOLEAN},
	{"both", BOTH},
	{"by", BY},
	{"cache", CACHE},
	{"called", CALLED},
	{"cascade", CASCADE},
	{"case", CASE},
	{"cast", CAST},
	{"chain", CHAIN},
	{"char", CHAR_P},
	{"character", CHARACTER},
	{"characteristics", CHARACTERISTICS},
	{"check", CHECK},
	{"checkpoint", CHECKPOINT},
	{"class", CLASS},
	{"close", CLOSE},
	{"cluster", CLUSTER},
	{"coalesce", COALESCE},
	{"collate", COLLATE},
	{"column", COLUMN},
	{"comment", COMMENT},
	{"commit", COMMIT},
	{"committed", COMMITTED},
	{"constraint", CONSTRAINT},
	{"constraints", CONSTRAINTS},
	{"conversion", CONVERSION_P},
	{"convert", CONVERT},
	{"copy", COPY},
	{"counts", COUNTS},
	{"cqeddy", CQEDDY},
	{"create", CREATE},
	{"createdb", CREATEDB},
	{"createuser", CREATEUSER},
	{"cross", CROSS},
	{"current_date", CURRENT_DATE},
	{"current_time", CURRENT_TIME},
	{"current_timestamp", CURRENT_TIMESTAMP},
	{"current_user", CURRENT_USER},
	{"cursor", CURSOR},
	{"cycle", CYCLE},
	{"database", DATABASE},
	{"day", DAY_P},
	{"deallocate", DEALLOCATE},
	{"dec", DEC},
	{"decimal", DECIMAL},
	{"declare", DECLARE},
	{"declust", DECLUST},
	{"default", DEFAULT},
	{"deferrable", DEFERRABLE},
	{"deferred", DEFERRED},
	{"definer", DEFINER},
	{"delete", DELETE_P},
	{"delimiter", DELIMITER},
	{"delimiters", DELIMITERS},
	{"desc", DESC},
	{"distinct", DISTINCT},
	{"do", DO},
	{"domain", DOMAIN_P},
	{"double", DOUBLE},
	{"drop", DROP},
	{"dstream", DSTREAM},
	{"each", EACH},
	{"else", ELSE},
	{"encoding", ENCODING},
	{"encrypted", ENCRYPTED},
	{"end", END_TRANS},
	{"escape", ESCAPE},
	{"event", EVENT},
	{"except", EXCEPT},
	{"exclusive", EXCLUSIVE},
	{"execute", EXECUTE},
	{"exists", EXISTS},
	{"explain", EXPLAIN},
	{"external", EXTERNAL},
	{"extract", EXTRACT},
	{"false", FALSE_P},
	{"fetch", FETCH},
	{"float", FLOAT_P},
	{"follows", FOLLOWS},
	{"for", FOR},
	{"force", FORCE},
	{"foreign", FOREIGN},
	{"forward", FORWARD},
	{"freeze", FREEZE},
	{"from", FROM},
	{"full", FULL},
	{"function", FUNCTION},
	{"get", GET},
	{"global", GLOBAL},
	{"grant", GRANT},
	{"group", GROUP_P},
	{"handler", HANDLER},
	{"having", HAVING},
	{"hour", HOUR_P},
	{"ilike", ILIKE},
	{"immediate", IMMEDIATE},
	{"immutable", IMMUTABLE},
	{"implicit", IMPLICIT_P},
	{"in", IN_P},
	{"increment", INCREMENT},
	{"index", INDEX},
	{"inherits", INHERITS},
	{"initially", INITIALLY},
	{"inner", INNER_P},
	{"inout", INOUT},
	{"input", INPUT},
	{"insensitive", INSENSITIVE},
	{"insert", INSERT},
	{"instead", INSTEAD},
	{"int", INT},
	{"integer", INTEGER},
	{"intersect", INTERSECT},
	{"interval", INTERVAL},
	{"into", INTO},
	{"invoker", INVOKER},
	{"is", IS},
	{"isnull", ISNULL},
	{"isolation", ISOLATION},
	{"istream", ISTREAM},
	{"join", JOIN},
	{"keep", KEEP},
	{"key", KEY},
	{"lancompiler", LANCOMPILER},
	{"language", LANGUAGE},
	{"leading", LEADING},
	{"left", LEFT},
	{"level", LEVEL},
	{"like", LIKE},
	{"limit", LIMIT},
	{"listen", LISTEN},
	{"load", LOAD},
	{"local", LOCAL},
	{"localtime", LOCALTIME},
	{"localtimestamp", LOCALTIMESTAMP},
	{"location", LOCATION},
	{"lock", LOCK_P},
	{"match", MATCH},
	{"maxvalue", MAXVALUE},
	{"minute", MINUTE_P},
	{"minvalue", MINVALUE},
	{"mode", MODE},
	{"month", MONTH_P},
	{"move", MOVE},
	{"myhist", MYHIST},
	{"names", NAMES},
	{"national", NATIONAL},
	{"natural", NATURAL},
	{"nchar", NCHAR},
	{"new", NEW},
	{"next", NEXT},
	{"no", NO},
	{"nocreatedb", NOCREATEDB},
	{"nocreateuser", NOCREATEUSER},
	{"none", NONE},
	{"not", NOT},
	{"notfollows", NOTFOLLOWS},
	{"nothing", NOTHING},
	{"notify", NOTIFY},
	{"notnull", NOTNULL},
	{"notprecedes", NOTPRECEDES},
	{"null", NULL_P},
	{"nullif", NULLIF},
	{"numeric", NUMERIC},
	{"of", OF},
	{"off", OFF},
	{"offset", OFFSET},
	{"oids", OIDS},
	{"old", OLD},
	{"on", ON},
	{"only", ONLY},
	{"operator", OPERATOR},
	{"option", OPTION},
	{"optional", OPTIONAL},
	{"or", OR},
	{"order", ORDER},
	{"out", OUT_P},
	{"outer", OUTER_P},
	{"overlaps", OVERLAPS},
	{"overlay", OVERLAY},
	{"overload", OVERLOAD},
	{"owner", OWNER},
	{"partial", PARTIAL},
	{"partition", PARTITION},
	{"password", PASSWORD},
	{"path", PATH_P},
	{"pendant", PENDANT},
	{"placing", PLACING},
	{"position", POSITION},
	{"precedes", PRECEDES},
	{"precision", PRECISION},
	{"prepare", PREPARE},
	{"primary", PRIMARY},
	{"prior", PRIOR},
	{"privileges", PRIVILEGES},
	{"procedural", PROCEDURAL},
	{"procedure", PROCEDURE},
	{"range", RANGE},
	{"read", READ},
	{"real", REAL},
	{"recheck", RECHECK},
	{"recursive_next", RECURSIVE_NEXT},
	{"recursive_result", RECURSIVE_RESULT},
	{"references", REFERENCES},
	{"reghist", REGHIST},
	{"reindex", REINDEX},
	{"relative", RELATIVE},
	{"rename", RENAME},
	{"replace", REPLACE},
	{"reset", RESET},
	{"restrict", RESTRICT},
	{"returns", RETURNS},
	{"revoke", REVOKE},
	{"right", RIGHT},
	{"rollback", ROLLBACK},
	{"row", ROW},
	{"rstream", RSTREAM},
	{"rule", RULE},
	{"sample", SAMPLE},
	{"schema", SCHEMA},
	{"scroll", SCROLL},
	{"second", SECOND_P},
	{"security", SECURITY},
	{"select", SELECT},
	{"sequence", SEQUENCE},
	{"serializable", SERIALIZABLE},
	{"session", SESSION},
	{"session_user", SESSION_USER},
	{"set", SET},
	{"setof", SETOF},
	{"share", SHARE},
	{"show", SHOW},
	{"similar", SIMILAR},
	{"simple", SIMPLE},
	{"slices", SLICES},
	{"slide", SLIDE},
	{"slidetimeout", SLIDETIMEOUT},                           /* @tcqoc */
	{"smallint", SMALLINT},
	{"some", SOME},
	{"stable", STABLE},
	{"start", START},
	{"statement", STATEMENT},
	{"statistics", STATISTICS},
	{"stdin", STDIN},
	{"stdout", STDOUT},
	{"storage", STORAGE},
	{"stream", STREAM},			/* @dtcqSK */
	{"strict", STRICT},
	{"substring", SUBSTRING},
	{"sysid", SYSID},
	{"table", TABLE},
	{"temp", TEMP},
	{"template", TEMPLATE},
	{"temporary", TEMPORARY},
	{"then", THEN},
	{"time", TIME},
	{"timestamp", TIMESTAMP},
	{"timestampcolumn", TIMESTAMPCOLUMN},		/* @dtcqSK */
	{"to", TO},
	{"toast", TOAST},
	{"trailing", TRAILING},
	{"transaction", TRANSACTION},
	{"treat", TREAT},
	{"trigger", TRIGGER},
	{"trim", TRIM},
	{"true", TRUE_P},
	{"truncate", TRUNCATE},
	{"trusted", TRUSTED},
	{"type", TYPE_P},
	{"unarchived", UNARCHIVED},
	{"unarchived_drop", UNARCHIVED_DROP},
	{"unarchived_spill", UNARCHIVED_SPILL},
	{"unencrypted", UNENCRYPTED},
	{"union", UNION},
	{"unique", UNIQUE},
	{"unknown", UNKNOWN},
	{"unlisten", UNLISTEN},
	{"until", UNTIL},
	{"update", UPDATE},
	{"usage", USAGE},
	{"user", USER},
	{"using", USING},
	{"vacuum", VACUUM},
	{"valid", VALID},
	{"validator", VALIDATOR},
	{"values", VALUES},
	{"varchar", VARCHAR},
	{"varying", VARYING},
	{"verbose", VERBOSE},
	{"version", VERSION},
	{"view", VIEW},
	{"volatile", VOLATILE},
	{"wavelet", WAVELET},
	{"when", WHEN},
	{"where", WHERE},
	{"window", WINDOW},			/* @dwndwSK */
	{"with", WITH},
	{"without", WITHOUT},
	{"work", WORK},
	{"wrapper", WRAPPER},		/* @dwrapSK */
	{"wrapperinfo", WRAPPERINFO},		/* @dwrapSK */
	{"write", WRITE},
	{"year", YEAR_P},
	{"zone", ZONE},
};

/*
 * ScanKeywordLookup - see if a given word is a keyword
 *
 * Returns a pointer to the ScanKeyword table entry, or NULL if no match.
 *
 * The match is done case-insensitively.  Note that we deliberately use a
 * dumbed-down case conversion that will only translate 'A'-'Z' into 'a'-'z',
 * even if we are in a locale where tolower() would produce more or different
 * translations.  This is to conform to the SQL99 spec, which says that
 * keywords are to be matched in this way even though non-keyword identifiers
 * receive a different case-normalization mapping.
 */
const ScanKeyword *
ScanKeywordLookup(const char *text)
{
	int			len,
				i;
	char		word[NAMEDATALEN];
	const ScanKeyword *low;
	const ScanKeyword *high;

	len = strlen(text);
	/* We assume all keywords are shorter than NAMEDATALEN. */
	if (len >= NAMEDATALEN)
		return NULL;

	/*
	 * Apply an ASCII-only downcasing.	We must not use tolower() since it
	 * may produce the wrong translation in some locales (eg, Turkish),
	 * and we don't trust isupper() very much either.  In an ASCII-based
	 * encoding the tests against A and Z are sufficient, but we also
	 * check isupper() so that we will work correctly under EBCDIC.  The
	 * actual case conversion step should work for either ASCII or EBCDIC.
	 */
	for (i = 0; i < len; i++)
	{
		char		ch = text[i];

		if (ch >= 'A' && ch <= 'Z' && isupper((unsigned char) ch))
			ch += 'a' - 'A';
		word[i] = ch;
	}
	word[len] = '\0';

	/*
	 * Now do a binary search using plain strcmp() comparison.
	 */
	low = &ScanKeywords[0];
	high = endof(ScanKeywords) - 1;
	while (low <= high)
	{
		const ScanKeyword *middle;
		int			difference;

		middle = low + (high - low) / 2;
		difference = strcmp(middle->name, word);
		if (difference == 0)
			return middle;
		else if (difference < 0)
			low = middle + 1;
		else
			high = middle - 1;
	}

	return NULL;
}
