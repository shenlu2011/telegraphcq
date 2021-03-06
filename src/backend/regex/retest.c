/*
 * a simple regexp debug program
 *
 * $Header: /project/eecs/db/cvsroot/postgres/src/backend/regex/retest.c,v 1.2 2004/03/24 08:11:05 chungwu Exp $
 */

#include "postgres.h"
#include "regex/regex.h"

int
main()
{
	int			sts;
	regex_t		re;
	char		buf[1024];
	char	   *p;

	printf("type in regexp string: ");
	if (!fgets(buf, sizeof(buf), stdin))
		exit(0);
	p = strchr(buf, '\n');
	if (p)
		*p = '\0';

	sts = pg_regcomp(&re, buf, 1);
	printf("regcomp: parses \"%s\" and returns %d\n", buf, sts);
	for (;;)
	{
		printf("type in target string: ");
		if (!fgets(buf, sizeof(buf), stdin))
			exit(0);
		p = strchr(buf, '\n');
		if (p)
			*p = '\0';

		sts = pg_regexec(&re, buf, 0, 0, 0);
		printf("regexec: returns %d\n", sts);
	}
}

void
elog(int lev, const char *fmt,...)
{
}
