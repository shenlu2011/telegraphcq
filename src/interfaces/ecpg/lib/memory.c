/* $Header: /project/eecs/db/cvsroot/postgres/src/interfaces/ecpg/lib/memory.c,v 1.2 2004/03/24 08:11:30 chungwu Exp $ */

#include "postgres_fe.h"

#include "ecpgtype.h"
#include "ecpglib.h"
#include "ecpgerrno.h"
#include "extern.h"

void
ECPGfree(void *ptr)
{
	free(ptr);
}

char *
ECPGalloc(long size, int lineno)
{
	char	   *new = (char *) calloc(1L, size);

	if (!new)
	{
		ECPGraise(lineno, ECPG_OUT_OF_MEMORY, NULL);
		return NULL;
	}

	memset(new, '\0', size);
	return (new);
}

char *
ECPGstrdup(const char *string, int lineno)
{
	char	   *new = strdup(string);

	if (!new)
	{
		ECPGraise(lineno, ECPG_OUT_OF_MEMORY, NULL);
		return NULL;
	}

	return (new);
}

/* keep a list of memory we allocated for the user */
static struct auto_mem
{
	void	   *pointer;
	struct auto_mem *next;
}	*auto_allocs = NULL;

void
ECPGadd_mem(void *ptr, int lineno)
{
	struct auto_mem *am = (struct auto_mem *) ECPGalloc(sizeof(struct auto_mem), lineno);

	am->pointer = ptr;
	am->next = auto_allocs;
	auto_allocs = am;
}

void
ECPGfree_auto_mem(void)
{
	struct auto_mem *am;

	/* free all memory we have allocated for the user */
	for (am = auto_allocs; am;)
	{
		struct auto_mem *act = am;

		am = am->next;
		ECPGfree(act->pointer);
		ECPGfree(act);
	}

	auto_allocs = NULL;
}

void
ECPGclear_auto_mem(void)
{
	struct auto_mem *am;

	/* free just our own structure */
	for (am = auto_allocs; am;)
	{
		struct auto_mem *act = am;

		am = am->next;
		ECPGfree(act);
	}

	auto_allocs = NULL;
}
