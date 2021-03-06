/*
 * op function for ltree and lquery
 * Teodor Sigaev <teodor@stack.net>
 */

#include "ltree.h"
#include <ctype.h>

PG_FUNCTION_INFO_V1(ltq_regex);
PG_FUNCTION_INFO_V1(ltq_rregex);

typedef struct
{
	lquery_level *q;
	int			nq;
	ltree_level *t;
	int			nt;
	int			posq;
	int			post;
}	FieldNot;

static char *
getlexem(char *start, char *end, int *len)
{
	char	   *ptr;

	while (start < end && *start == '_')
		start++;

	ptr = start;
	if (ptr == end)
		return NULL;

	while (ptr < end && *ptr != '_')
		ptr++;

	*len = ptr - start;
	return start;
}

bool
			compare_subnode(ltree_level * t, char *qn, int len, int (*cmpptr) (const char *, const char *, size_t), bool anyend)
{
	char	   *endt = t->name + t->len;
	char	   *endq = qn + len;
	char	   *tn;
	int			lent,
				lenq;
	bool		isok;

	while ((qn = getlexem(qn, endq, &lenq)) != NULL)
	{
		tn = t->name;
		isok = false;
		while ((tn = getlexem(tn, endt, &lent)) != NULL)
		{
			if (
				(
				 lent == lenq ||
				 (lent > lenq && anyend)
				 ) &&
				(*cmpptr) (qn, tn, lenq) == 0)
			{

				isok = true;
				break;
			}
			tn += lent;
		}

		if (!isok)
			return false;
		qn += lenq;
	}

	return true;
}

static bool
checkLevel(lquery_level * curq, ltree_level * curt)
{
	int			(*cmpptr) (const char *, const char *, size_t);
	lquery_variant *curvar = LQL_FIRST(curq);
	int			i;

	for (i = 0; i < curq->numvar; i++)
	{
		cmpptr = (curvar->flag & LVAR_INCASE) ? strncasecmp : strncmp;

		if (curvar->flag & LVAR_SUBLEXEM)
		{
			if (compare_subnode(curt, curvar->name, curvar->len, cmpptr, (curvar->flag & LVAR_ANYEND)))
				return true;
		}
		else if (
				 (
				  curvar->len == curt->len ||
				(curt->len > curvar->len && (curvar->flag & LVAR_ANYEND))
				  ) &&
				 (*cmpptr) (curvar->name, curt->name, curvar->len) == 0)
		{

			return true;
		}
		curvar = LVAR_NEXT(curvar);
	}
	return false;
}

/*
void
printFieldNot(FieldNot *fn ) {
	while(fn->q) {
		elog(NOTICE,"posQ:%d lenQ:%d posT:%d lenT:%d", fn->posq,fn->nq,fn->post,fn->nt);
		fn++;
	}
}
*/

static bool
checkCond(lquery_level * curq, int query_numlevel, ltree_level * curt, int tree_numlevel, FieldNot * ptr)
{
	uint32		low_pos = 0,
				high_pos = 0,
				cur_tpos = 0;
	int			tlen = tree_numlevel,
				qlen = query_numlevel;
	int			isok;
	lquery_level *prevq = NULL;
	ltree_level *prevt = NULL;

	while (tlen > 0 && qlen > 0)
	{
		if (curq->numvar)
		{
			prevt = curt;
			while (cur_tpos < low_pos)
			{
				curt = LEVEL_NEXT(curt);
				tlen--;
				cur_tpos++;
				if (tlen == 0)
					return false;
				if (ptr && ptr->q)
					ptr->nt++;
			}

			if (ptr && curq->flag & LQL_NOT)
			{
				if (!(prevq && prevq->numvar == 0))
					prevq = curq;
				if (ptr->q == NULL)
				{
					ptr->t = prevt;
					ptr->q = prevq;
					ptr->nt = 1;
					ptr->nq = 1 + ((prevq == curq) ? 0 : 1);
					ptr->posq = query_numlevel - qlen - ((prevq == curq) ? 0 : 1);
					ptr->post = cur_tpos;
				}
				else
				{
					ptr->nt++;
					ptr->nq++;
				}

				if (qlen == 1 && ptr->q->numvar == 0)
					ptr->nt = tree_numlevel - ptr->post;
				curt = LEVEL_NEXT(curt);
				tlen--;
				cur_tpos++;
				if (high_pos < cur_tpos)
					high_pos++;
			}
			else
			{
				isok = false;
				while (cur_tpos <= high_pos && tlen > 0 && !isok)
				{
					isok = checkLevel(curq, curt);
					curt = LEVEL_NEXT(curt);
					tlen--;
					cur_tpos++;
					if (!isok && ptr)
						ptr->nt++;
				}
				if (!isok)
					return false;

				if (ptr && ptr->q)
				{
					if (checkCond(ptr->q, ptr->nq, ptr->t, ptr->nt, NULL))
						return false;
					ptr->q = NULL;
				}
				low_pos = cur_tpos;
				high_pos = cur_tpos;
			}
		}
		else
		{
			low_pos = cur_tpos + curq->low;
			high_pos = cur_tpos + curq->high;
			if (ptr && ptr->q)
			{
				ptr->nq++;
				if (qlen == 1)
					ptr->nt = tree_numlevel - ptr->post;
			}
		}

		prevq = curq;
		curq = LQL_NEXT(curq);
		qlen--;
	}

	if (low_pos > tree_numlevel || tree_numlevel > high_pos)
		return false;

	while (qlen > 0)
	{
		if (curq->numvar)
		{
			if (!(curq->flag & LQL_NOT))
				return false;
		}
		else
		{
			low_pos = cur_tpos + curq->low;
			high_pos = cur_tpos + curq->high;
		}

		curq = LQL_NEXT(curq);
		qlen--;
	}

	if (low_pos > tree_numlevel || tree_numlevel > high_pos)
		return false;

	if (ptr && ptr->q && checkCond(ptr->q, ptr->nq, ptr->t, ptr->nt, NULL))
		return false;

	return true;
}

Datum
ltq_regex(PG_FUNCTION_ARGS)
{
	ltree	   *tree = PG_GETARG_LTREE(0);
	lquery	   *query = PG_GETARG_LQUERY(1);
	bool		res = false;

	if (query->flag & LQUERY_HASNOT)
	{
		FieldNot	fn;

		fn.q = NULL;

		res = checkCond(LQUERY_FIRST(query), query->numlevel,
						LTREE_FIRST(tree), tree->numlevel, &fn);
	}
	else
	{
		res = checkCond(LQUERY_FIRST(query), query->numlevel,
						LTREE_FIRST(tree), tree->numlevel, NULL);
	}

	PG_FREE_IF_COPY(tree, 0);
	PG_FREE_IF_COPY(query, 1);
	PG_RETURN_BOOL(res);
}

Datum
ltq_rregex(PG_FUNCTION_ARGS)
{
	PG_RETURN_DATUM(DirectFunctionCall2(ltq_regex,
										PG_GETARG_DATUM(1),
										PG_GETARG_DATUM(0)
										));
}
