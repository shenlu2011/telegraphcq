/*-------------------------------------------------------------------------
 *
 * arrayfuncs.c
 *	  Support functions for arrays.
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /project/eecs/db/cvsroot/postgres/src/backend/utils/adt/arrayfuncs.c,v 1.2 2004/03/24 08:11:11 chungwu Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "access/tupmacs.h"
#include "catalog/catalog.h"
#include "catalog/pg_type.h"
#include "parser/parse_coerce.h"
#include "utils/array.h"
#include "utils/memutils.h"
#include "utils/syscache.h"


/*----------
 * A standard varlena array has the following internal structure:
 *	  <size>		- total number of bytes (also, TOAST info flags)
 *	  <ndim>		- number of dimensions of the array
 *	  <flags>		- bit mask of flags
 *	  <elemtype>	- element type OID
 *	  <dim>			- size of each array axis (C array of int)
 *	  <dim_lower>	- lower boundary of each dimension (C array of int)
 *	  <actual data> - whatever is the stored data
 * The actual data starts on a MAXALIGN boundary.  Individual items in the
 * array are aligned as specified by the array element type.
 *
 * NOTE: it is important that array elements of toastable datatypes NOT be
 * toasted, since the tupletoaster won't know they are there.  (We could
 * support compressed toasted items; only out-of-line items are dangerous.
 * However, it seems preferable to store such items uncompressed and allow
 * the toaster to compress the whole array as one input.)
 *
 * There is currently no support for NULL elements in arrays, either.
 * A reasonable (and backwards-compatible) way to add support would be to
 * add a nulls bitmap following the <dim_lower> array, which would be present
 * if needed; and its presence would be signaled by a bit in the flags word.
 *
 *
 * There are also some "fixed-length array" datatypes, such as NAME and
 * OIDVECTOR.  These are simply a sequence of a fixed number of items each
 * of a fixed-length datatype, with no overhead; the item size must be
 * a multiple of its alignment requirement, because we do no padding.
 * We support subscripting on these types, but array_in() and array_out()
 * only work with varlena arrays.
 *----------
 */


/* ----------
 * Local definitions
 * ----------
 */
#define ASSGN	 "="

#define RETURN_NULL(type)  do { *isNull = true; return (type) 0; } while (0)


static int	ArrayCount(char *str, int *dim, char typdelim);
static Datum *ReadArrayStr(char *arrayStr, int nitems, int ndim, int *dim,
			 FmgrInfo *inputproc, Oid typelem, int32 typmod,
			 char typdelim,
			 int typlen, bool typbyval, char typalign,
			 int *nbytes);
static void CopyArrayEls(char *p, Datum *values, int nitems,
			 int typlen, bool typbyval, char typalign,
			 bool freedata);
static void system_cache_lookup(Oid element_type, bool input, int *typlen,
					bool *typbyval, char *typdelim, Oid *typelem,
					Oid *proc, char *typalign);
static Datum ArrayCast(char *value, bool byval, int len);
static int ArrayCastAndSet(Datum src,
				int typlen, bool typbyval, char typalign,
				char *dest);
static int array_nelems_size(char *ptr, int nitems,
				  int typlen, bool typbyval, char typalign);
static char *array_seek(char *ptr, int nitems,
		   int typlen, bool typbyval, char typalign);
static int array_copy(char *destptr, int nitems, char *srcptr,
		   int typlen, bool typbyval, char typalign);
static int array_slice_size(int ndim, int *dim, int *lb, char *arraydataptr,
				 int *st, int *endp,
				 int typlen, bool typbyval, char typalign);
static void array_extract_slice(int ndim, int *dim, int *lb,
					char *arraydataptr,
					int *st, int *endp, char *destPtr,
					int typlen, bool typbyval, char typalign);
static void array_insert_slice(int ndim, int *dim, int *lb,
				   char *origPtr, int origdatasize,
				   char *destPtr,
				   int *st, int *endp, char *srcPtr,
				   int typlen, bool typbyval, char typalign);


/*---------------------------------------------------------------------
 * array_in :
 *		  converts an array from the external format in "string" to
 *		  its internal format.
 * return value :
 *		  the internal representation of the input array
 *--------------------------------------------------------------------
 */
Datum
array_in(PG_FUNCTION_ARGS)
{
	char	   *string = PG_GETARG_CSTRING(0);	/* external form */
	Oid			element_type = PG_GETARG_OID(1);		/* type of an array
														 * element */
	int32		typmod = PG_GETARG_INT32(2);	/* typmod for array
												 * elements */
	int			typlen;
	bool		typbyval;
	char		typdelim;
	Oid			typinput;
	Oid			typelem;
	char	   *string_save,
			   *p;
	FmgrInfo	inputproc;
	int			i,
				nitems;
	int32		nbytes;
	Datum	   *dataPtr;
	ArrayType  *retval;
	int			ndim,
				dim[MAXDIM],
				lBound[MAXDIM];
	char		typalign;

	/* Get info about element type, including its input conversion proc */
	system_cache_lookup(element_type, true, &typlen, &typbyval, &typdelim,
						&typelem, &typinput, &typalign);
	fmgr_info(typinput, &inputproc);

	/* Make a modifiable copy of the input */
	/* XXX why are we allocating an extra 2 bytes here? */
	string_save = (char *) palloc(strlen(string) + 3);
	strcpy(string_save, string);

	/*
	 * If the input string starts with dimension info, read and use that.
	 * Otherwise, we require the input to be in curly-brace style, and we
	 * prescan the input to determine dimensions.
	 *
	 * Dimension info takes the form of one or more [n] or [m:n] items. The
	 * outer loop iterates once per dimension item.
	 */
	p = string_save;
	ndim = 0;
	for (;;)
	{
		char	   *q;
		int			ub;

		/*
		 * Note: we currently allow whitespace between, but not within,
		 * dimension items.
		 */
		while (isspace((unsigned char) *p))
			p++;
		if (*p != '[')
			break;				/* no more dimension items */
		p++;
		if (ndim >= MAXDIM)
			elog(ERROR, "array_in: more than %d dimensions", MAXDIM);
		for (q = p; isdigit((unsigned char) *q); q++);
		if (q == p)				/* no digits? */
			elog(ERROR, "array_in: missing dimension value");
		if (*q == ':')
		{
			/* [m:n] format */
			*q = '\0';
			lBound[ndim] = atoi(p);
			p = q + 1;
			for (q = p; isdigit((unsigned char) *q); q++);
			if (q == p)			/* no digits? */
				elog(ERROR, "array_in: missing dimension value");
		}
		else
		{
			/* [n] format */
			lBound[ndim] = 1;
		}
		if (*q != ']')
			elog(ERROR, "array_in: missing ']' in array declaration");
		*q = '\0';
		ub = atoi(p);
		p = q + 1;
		if (ub < lBound[ndim])
			elog(ERROR, "array_in: upper_bound cannot be < lower_bound");
		dim[ndim] = ub - lBound[ndim] + 1;
		ndim++;
	}

	if (ndim == 0)
	{
		/* No array dimensions, so intuit dimensions from brace structure */
		if (*p != '{')
			elog(ERROR, "array_in: Need to specify dimension");
		ndim = ArrayCount(p, dim, typdelim);
		for (i = 0; i < ndim; i++)
			lBound[i] = 1;
	}
	else
	{
		/* If array dimensions are given, expect '=' operator */
		if (strncmp(p, ASSGN, strlen(ASSGN)) != 0)
			elog(ERROR, "array_in: missing assignment operator");
		p += strlen(ASSGN);
		while (isspace((unsigned char) *p))
			p++;
	}

#ifdef ARRAYDEBUG
	printf("array_in- ndim %d (", ndim);
	for (i = 0; i < ndim; i++)
	{
		printf(" %d", dim[i]);
	};
	printf(") for %s\n", string);
#endif

	nitems = ArrayGetNItems(ndim, dim);
	if (nitems == 0)
	{
		/* Return empty array */
		retval = (ArrayType *) palloc(sizeof(ArrayType));
		MemSet(retval, 0, sizeof(ArrayType));
		retval->size = sizeof(ArrayType);
		retval->elemtype = element_type;
		PG_RETURN_ARRAYTYPE_P(retval);
	}

	if (*p != '{')
		elog(ERROR, "array_in: missing left brace");

	dataPtr = ReadArrayStr(p, nitems, ndim, dim, &inputproc, typelem,
						   typmod, typdelim, typlen, typbyval, typalign,
						   &nbytes);
	nbytes += ARR_OVERHEAD(ndim);
	retval = (ArrayType *) palloc(nbytes);
	MemSet(retval, 0, nbytes);
	retval->size = nbytes;
	retval->ndim = ndim;
	retval->elemtype = element_type;
	memcpy((char *) ARR_DIMS(retval), (char *) dim,
		   ndim * sizeof(int));
	memcpy((char *) ARR_LBOUND(retval), (char *) lBound,
		   ndim * sizeof(int));

	CopyArrayEls(ARR_DATA_PTR(retval), dataPtr, nitems,
				 typlen, typbyval, typalign, true);
	pfree(dataPtr);
	pfree(string_save);
	PG_RETURN_ARRAYTYPE_P(retval);
}

/*-----------------------------------------------------------------------------
 * ArrayCount
 *	 Counts the number of dimensions and the *dim array for an array string.
 *		 The syntax for array input is C-like nested curly braces
 *-----------------------------------------------------------------------------
 */
static int
ArrayCount(char *str, int *dim, char typdelim)
{
	int			nest_level = 0,
				i;
	int			ndim = 0,
				temp[MAXDIM];
	bool		scanning_string = false;
	bool		eoArray = false;
	char	   *ptr;

	for (i = 0; i < MAXDIM; ++i)
		temp[i] = dim[i] = 0;

	if (strncmp(str, "{}", 2) == 0)
		return 0;

	ptr = str;
	while (!eoArray)
	{
		bool		itemdone = false;

		while (!itemdone)
		{
			switch (*ptr)
			{
				case '\0':
					/* Signal a premature end of the string */
					elog(ERROR, "malformed array constant: %s", str);
					break;
				case '\\':
					/* skip the escaped character */
					if (*(ptr + 1))
						ptr++;
					else
						elog(ERROR, "malformed array constant: %s", str);
					break;
				case '\"':
					scanning_string = !scanning_string;
					break;
				case '{':
					if (!scanning_string)
					{
						if (nest_level >= MAXDIM)
							elog(ERROR, "array_in: illformed array constant");
						temp[nest_level] = 0;
						nest_level++;
						if (ndim < nest_level)
							ndim = nest_level;
					}
					break;
				case '}':
					if (!scanning_string)
					{
						if (nest_level == 0)
							elog(ERROR, "array_in: illformed array constant");
						nest_level--;
						if (nest_level == 0)
							eoArray = itemdone = true;
						else
						{
							/*
							 * We don't set itemdone here; see comments in
							 * ReadArrayStr
							 */
							temp[nest_level - 1]++;
						}
					}
					break;
				default:
					if (*ptr == typdelim && !scanning_string)
						itemdone = true;
					break;
			}
			if (!itemdone)
				ptr++;
		}
		temp[ndim - 1]++;
		ptr++;
	}
	for (i = 0; i < ndim; ++i)
		dim[i] = temp[i];

	return ndim;
}

/*---------------------------------------------------------------------------
 * ReadArrayStr :
 *	 parses the array string pointed by "arrayStr" and converts it to
 *	 internal format. The external format expected is like C array
 *	 declaration. Unspecified elements are initialized to zero for fixed length
 *	 base types and to empty varlena structures for variable length base
 *	 types.  (This is pretty bogus; NULL would be much safer.)
 * result :
 *	 returns a palloc'd array of Datum representations of the array elements.
 *	 If element type is pass-by-ref, the Datums point to palloc'd values.
 *	 *nbytes is set to the amount of data space needed for the array,
 *	 including alignment padding but not including array header overhead.
 *	 CAUTION: the contents of "arrayStr" may be modified!
 *---------------------------------------------------------------------------
 */
static Datum *
ReadArrayStr(char *arrayStr,
			 int nitems,
			 int ndim,
			 int *dim,
			 FmgrInfo *inputproc,
			 Oid typelem,
			 int32 typmod,
			 char typdelim,
			 int typlen,
			 bool typbyval,
			 char typalign,
			 int *nbytes)
{
	int			i,
				nest_level = 0;
	Datum	   *values;
	char	   *ptr;
	bool		scanning_string = false;
	bool		eoArray = false;
	int			indx[MAXDIM],
				prod[MAXDIM];

	mda_get_prod(ndim, dim, prod);
	values = (Datum *) palloc(nitems * sizeof(Datum));
	MemSet(values, 0, nitems * sizeof(Datum));
	MemSet(indx, 0, sizeof(indx));

	/* read array enclosed within {} */
	ptr = arrayStr;
	while (!eoArray)
	{
		bool		itemdone = false;
		int			i = -1;
		char	   *itemstart;

		/* skip leading whitespace */
		while (isspace((unsigned char) *ptr))
			ptr++;
		itemstart = ptr;

		while (!itemdone)
		{
			switch (*ptr)
			{
				case '\0':
					/* Signal a premature end of the string */
					elog(ERROR, "malformed array constant: %s", arrayStr);
					break;
				case '\\':
					{
						char	   *cptr;

						/* Crunch the string on top of the backslash. */
						for (cptr = ptr; *cptr != '\0'; cptr++)
							*cptr = *(cptr + 1);
						if (*ptr == '\0')
							elog(ERROR, "malformed array constant: %s", arrayStr);
						break;
					}
				case '\"':
					{
						char	   *cptr;

						scanning_string = !scanning_string;
						/* Crunch the string on top of the quote. */
						for (cptr = ptr; *cptr != '\0'; cptr++)
							*cptr = *(cptr + 1);
						/* Back up to not miss following character. */
						ptr--;
						break;
					}
				case '{':
					if (!scanning_string)
					{
						if (nest_level >= ndim)
							elog(ERROR, "array_in: illformed array constant");
						nest_level++;
						indx[nest_level - 1] = 0;
						/* skip leading whitespace */
						while (isspace((unsigned char) *(ptr + 1)))
							ptr++;
						itemstart = ptr + 1;
					}
					break;
				case '}':
					if (!scanning_string)
					{
						if (nest_level == 0)
							elog(ERROR, "array_in: illformed array constant");
						if (i == -1)
							i = ArrayGetOffset0(ndim, indx, prod);
						indx[nest_level - 1] = 0;
						nest_level--;
						if (nest_level == 0)
							eoArray = itemdone = true;
						else
						{
							/*
							 * tricky coding: terminate item value string
							 * at first '}', but don't process it till we
							 * see a typdelim char or end of array.  This
							 * handles case where several '}'s appear
							 * successively in a multidimensional array.
							 */
							*ptr = '\0';
							indx[nest_level - 1]++;
						}
					}
					break;
				default:
					if (*ptr == typdelim && !scanning_string)
					{
						if (i == -1)
							i = ArrayGetOffset0(ndim, indx, prod);
						itemdone = true;
						indx[ndim - 1]++;
					}
					break;
			}
			if (!itemdone)
				ptr++;
		}
		*ptr++ = '\0';
		if (i < 0 || i >= nitems)
			elog(ERROR, "array_in: illformed array constant");
		values[i] = FunctionCall3(inputproc,
								  CStringGetDatum(itemstart),
								  ObjectIdGetDatum(typelem),
								  Int32GetDatum(typmod));
	}

	/*
	 * Initialize any unset items and compute total data space needed
	 */
	if (typlen > 0)
	{
		*nbytes = nitems * att_align(typlen, typalign);
		if (!typbyval)
			for (i = 0; i < nitems; i++)
				if (values[i] == (Datum) 0)
				{
					values[i] = PointerGetDatum(palloc(typlen));
					MemSet(DatumGetPointer(values[i]), 0, typlen);
				}
	}
	else
	{
		Assert(!typbyval);
		*nbytes = 0;
		for (i = 0; i < nitems; i++)
		{
			if (values[i] != (Datum) 0)
			{
				/* let's just make sure data is not toasted */
				if (typlen == -1)
					values[i] = PointerGetDatum(PG_DETOAST_DATUM(values[i]));
				*nbytes = att_addlength(*nbytes, typlen, values[i]);
				*nbytes = att_align(*nbytes, typalign);
			}
			else if (typlen == -1)
			{
				/* dummy varlena value (XXX bogus, see notes above) */
				values[i] = PointerGetDatum(palloc(sizeof(int32)));
				VARATT_SIZEP(DatumGetPointer(values[i])) = sizeof(int32);
				*nbytes += sizeof(int32);
				*nbytes = att_align(*nbytes, typalign);
			}
			else
			{
				/* dummy cstring value */
				Assert(typlen == -2);
				values[i] = PointerGetDatum(palloc(1));
				*((char *) DatumGetPointer(values[i])) = '\0';
				*nbytes += 1;
				*nbytes = att_align(*nbytes, typalign);
			}
		}
	}
	return values;
}


/*----------
 * Copy data into an array object from a temporary array of Datums.
 *
 * p: pointer to start of array data area
 * values: array of Datums to be copied
 * nitems: number of Datums to be copied
 * typbyval, typlen, typalign: info about element datatype
 * freedata: if TRUE and element type is pass-by-ref, pfree data values
 * referenced by Datums after copying them.
 *
 * If the input data is of varlena type, the caller must have ensured that
 * the values are not toasted.	(Doing it here doesn't work since the
 * caller has already allocated space for the array...)
 *----------
 */
static void
CopyArrayEls(char *p,
			 Datum *values,
			 int nitems,
			 int typlen,
			 bool typbyval,
			 char typalign,
			 bool freedata)
{
	int			i;

	if (typbyval)
		freedata = false;

	for (i = 0; i < nitems; i++)
	{
		p += ArrayCastAndSet(values[i], typlen, typbyval, typalign, p);
		if (freedata)
			pfree(DatumGetPointer(values[i]));
	}
}

/*-------------------------------------------------------------------------
 * array_out :
 *		   takes the internal representation of an array and returns a string
 *		  containing the array in its external format.
 *-------------------------------------------------------------------------
 */
Datum
array_out(PG_FUNCTION_ARGS)
{
	ArrayType  *v = PG_GETARG_ARRAYTYPE_P(0);
	Oid			element_type;
	int			typlen;
	bool		typbyval;
	char		typdelim;
	Oid			typoutput,
				typelem;
	FmgrInfo	outputproc;
	char		typalign;
	char	   *p,
			   *tmp,
			   *retval,
			  **values;
	bool	   *needquotes;
	int			nitems,
				overall_length,
				i,
				j,
				k,
				indx[MAXDIM];
	int			ndim,
			   *dim;

	element_type = ARR_ELEMTYPE(v);
	system_cache_lookup(element_type, false, &typlen, &typbyval,
						&typdelim, &typelem, &typoutput, &typalign);
	fmgr_info(typoutput, &outputproc);

	ndim = ARR_NDIM(v);
	dim = ARR_DIMS(v);
	nitems = ArrayGetNItems(ndim, dim);

	if (nitems == 0)
	{
		retval = pstrdup("{}");
		PG_RETURN_CSTRING(retval);
	}

	/*
	 * Convert all values to string form, count total space needed
	 * (including any overhead such as escaping backslashes), and detect
	 * whether each item needs double quotes.
	 */
	values = (char **) palloc(nitems * sizeof(char *));
	needquotes = (bool *) palloc(nitems * sizeof(bool));
	p = ARR_DATA_PTR(v);
	overall_length = 1;			/* [TRH] don't forget to count \0 at end. */
	for (i = 0; i < nitems; i++)
	{
		Datum		itemvalue;
		bool		nq;

		itemvalue = fetch_att(p, typbyval, typlen);
		values[i] = DatumGetCString(FunctionCall3(&outputproc,
												  itemvalue,
											   ObjectIdGetDatum(typelem),
												  Int32GetDatum(-1)));
		p = att_addlength(p, typlen, PointerGetDatum(p));
		p = (char *) att_align(p, typalign);

		/* count data plus backslashes; detect chars needing quotes */
		nq = (values[i][0] == '\0');	/* force quotes for empty string */
		for (tmp = values[i]; *tmp; tmp++)
		{
			char		ch = *tmp;

			overall_length += 1;
			if (ch == '"' || ch == '\\')
			{
				nq = true;
#ifndef TCL_ARRAYS
				overall_length += 1;
#endif
			}
			else if (ch == '{' || ch == '}' || ch == typdelim ||
					 isspace((unsigned char) ch))
				nq = true;
		}

		needquotes[i] = nq;

		/* Count the pair of double quotes, if needed */
		if (nq)
			overall_length += 2;

		/* and the comma */
		overall_length += 1;
	}

	/*
	 * count total number of curly braces in output string
	 */
	for (i = j = 0, k = 1; i < ndim; k *= dim[i++], j += k);

	retval = (char *) palloc(overall_length + 2 * j);
	p = retval;

#define APPENDSTR(str)	(strcpy(p, (str)), p += strlen(p))
#define APPENDCHAR(ch)	(*p++ = (ch), *p = '\0')

	APPENDCHAR('{');
	for (i = 0; i < ndim; indx[i++] = 0);
	j = 0;
	k = 0;
	do
	{
		for (i = j; i < ndim - 1; i++)
			APPENDCHAR('{');

		if (needquotes[k])
		{
			APPENDCHAR('"');
#ifndef TCL_ARRAYS
			for (tmp = values[k]; *tmp; tmp++)
			{
				char		ch = *tmp;

				if (ch == '"' || ch == '\\')
					*p++ = '\\';
				*p++ = ch;
			}
			*p = '\0';
#else
			APPENDSTR(values[k]);
#endif
			APPENDCHAR('"');
		}
		else
			APPENDSTR(values[k]);
		pfree(values[k++]);

		for (i = ndim - 1; i >= 0; i--)
		{
			indx[i] = (indx[i] + 1) % dim[i];
			if (indx[i])
			{
				APPENDCHAR(typdelim);
				break;
			}
			else
				APPENDCHAR('}');
		}
		j = i;
	} while (j != -1);

#undef APPENDSTR
#undef APPENDCHAR

	pfree(values);
	pfree(needquotes);

	PG_RETURN_CSTRING(retval);
}

/*-------------------------------------------------------------------------
 * array_length_coerce :
 *		  Apply the element type's length-coercion routine to each element
 *		  of the given array.
 *-------------------------------------------------------------------------
 */
Datum
array_length_coerce(PG_FUNCTION_ARGS)
{
	ArrayType  *v = PG_GETARG_ARRAYTYPE_P(0);
	int32		len = PG_GETARG_INT32(1);
	bool		isExplicit = PG_GETARG_BOOL(2);
	FmgrInfo   *fmgr_info = fcinfo->flinfo;
	FmgrInfo   *element_finfo;
	FunctionCallInfoData locfcinfo;

	/* If no typmod is provided, shortcircuit the whole thing */
	if (len < 0)
		PG_RETURN_ARRAYTYPE_P(v);

	/*
	 * We arrange to look up the element type's coercion function only
	 * once per series of calls.
	 */
	if (fmgr_info->fn_extra == NULL)
	{
		Oid			funcId;
		int			nargs;

		fmgr_info->fn_extra = MemoryContextAlloc(fmgr_info->fn_mcxt,
												 sizeof(FmgrInfo));
		element_finfo = (FmgrInfo *) fmgr_info->fn_extra;

		funcId = find_typmod_coercion_function(ARR_ELEMTYPE(v), &nargs);

		if (OidIsValid(funcId))
			fmgr_info_cxt(funcId, element_finfo, fmgr_info->fn_mcxt);
		else
			element_finfo->fn_oid = InvalidOid;
	}
	else
		element_finfo = (FmgrInfo *) fmgr_info->fn_extra;

	/*
	 * If we didn't find a coercion function, return the array unmodified
	 * (this should not happen in the normal course of things, but might
	 * happen if this function is called manually).
	 */
	if (element_finfo->fn_oid == InvalidOid)
		PG_RETURN_ARRAYTYPE_P(v);

	/*
	 * Use array_map to apply the function to each array element.
	 *
	 * Note: we pass isExplicit whether or not the function wants it ...
	 */
	MemSet(&locfcinfo, 0, sizeof(locfcinfo));
	locfcinfo.flinfo = element_finfo;
	locfcinfo.nargs = 3;
	locfcinfo.arg[0] = PointerGetDatum(v);
	locfcinfo.arg[1] = Int32GetDatum(len);
	locfcinfo.arg[2] = BoolGetDatum(isExplicit);

	return array_map(&locfcinfo, ARR_ELEMTYPE(v), ARR_ELEMTYPE(v));
}

/*-----------------------------------------------------------------------------
 * array_dims :
 *		  returns the dimensions of the array pointed to by "v", as a "text"
 *----------------------------------------------------------------------------
 */
Datum
array_dims(PG_FUNCTION_ARGS)
{
	ArrayType  *v = PG_GETARG_ARRAYTYPE_P(0);
	text	   *result;
	char	   *p;
	int			nbytes,
				i;
	int		   *dimv,
			   *lb;

	/* Sanity check: does it look like an array at all? */
	if (ARR_NDIM(v) <= 0 || ARR_NDIM(v) > MAXDIM)
		PG_RETURN_NULL();

	nbytes = ARR_NDIM(v) * 33 + 1;

	/*
	 * 33 since we assume 15 digits per number + ':' +'[]'
	 *
	 * +1 allows for temp trailing null
	 */

	result = (text *) palloc(nbytes + VARHDRSZ);
	p = VARDATA(result);

	dimv = ARR_DIMS(v);
	lb = ARR_LBOUND(v);

	for (i = 0; i < ARR_NDIM(v); i++)
	{
		sprintf(p, "[%d:%d]", lb[i], dimv[i] + lb[i] - 1);
		p += strlen(p);
	}
	VARATT_SIZEP(result) = strlen(VARDATA(result)) + VARHDRSZ;

	PG_RETURN_TEXT_P(result);
}

/*---------------------------------------------------------------------------
 * array_ref :
 *	  This routine takes an array pointer and an index array and returns
 *	  the referenced item as a Datum.  Note that for a pass-by-reference
 *	  datatype, the returned Datum is a pointer into the array object.
 *---------------------------------------------------------------------------
 */
Datum
array_ref(ArrayType *array,
		  int nSubscripts,
		  int *indx,
		  int arraylen,
		  int elmlen,
		  bool elmbyval,
		  char elmalign,
		  bool *isNull)
{
	int			i,
				ndim,
			   *dim,
			   *lb,
				offset,
				fixedDim[1],
				fixedLb[1];
	char	   *arraydataptr,
			   *retptr;

	if (array == (ArrayType *) NULL)
		RETURN_NULL(Datum);

	if (arraylen > 0)
	{
		/*
		 * fixed-length arrays -- these are assumed to be 1-d, 0-based
		 */
		ndim = 1;
		fixedDim[0] = arraylen / elmlen;
		fixedLb[0] = 0;
		dim = fixedDim;
		lb = fixedLb;
		arraydataptr = (char *) array;
	}
	else
	{
		/* detoast input array if necessary */
		array = DatumGetArrayTypeP(PointerGetDatum(array));

		ndim = ARR_NDIM(array);
		dim = ARR_DIMS(array);
		lb = ARR_LBOUND(array);
		arraydataptr = ARR_DATA_PTR(array);
	}

	/*
	 * Return NULL for invalid subscript
	 */
	if (ndim != nSubscripts || ndim <= 0 || ndim > MAXDIM)
		RETURN_NULL(Datum);
	for (i = 0; i < ndim; i++)
		if (indx[i] < lb[i] || indx[i] >= (dim[i] + lb[i]))
			RETURN_NULL(Datum);

	/*
	 * OK, get the element
	 */
	offset = ArrayGetOffset(nSubscripts, dim, lb, indx);

	retptr = array_seek(arraydataptr, offset, elmlen, elmbyval, elmalign);

	*isNull = false;
	return ArrayCast(retptr, elmbyval, elmlen);
}

/*-----------------------------------------------------------------------------
 * array_get_slice :
 *		   This routine takes an array and a range of indices (upperIndex and
 *		   lowerIndx), creates a new array structure for the referred elements
 *		   and returns a pointer to it.
 *
 * NOTE: we assume it is OK to scribble on the provided index arrays
 * lowerIndx[] and upperIndx[].  These are generally just temporaries.
 *-----------------------------------------------------------------------------
 */
ArrayType *
array_get_slice(ArrayType *array,
				int nSubscripts,
				int *upperIndx,
				int *lowerIndx,
				int arraylen,
				int elmlen,
				bool elmbyval,
				char elmalign,
				bool *isNull)
{
	int			i,
				ndim,
			   *dim,
			   *lb,
			   *newlb;
	int			fixedDim[1],
				fixedLb[1];
	char	   *arraydataptr;
	ArrayType  *newarray;
	int			bytes,
				span[MAXDIM];

	if (array == (ArrayType *) NULL)
		RETURN_NULL(ArrayType *);

	if (arraylen > 0)
	{
		/*
		 * fixed-length arrays -- currently, cannot slice these because
		 * parser labels output as being of the fixed-length array type!
		 * Code below shows how we could support it if the parser were
		 * changed to label output as a suitable varlena array type.
		 */
		elog(ERROR, "Slices of fixed-length arrays not implemented");

		/*
		 * fixed-length arrays -- these are assumed to be 1-d, 0-based XXX
		 * where would we get the correct ELEMTYPE from?
		 */
		ndim = 1;
		fixedDim[0] = arraylen / elmlen;
		fixedLb[0] = 0;
		dim = fixedDim;
		lb = fixedLb;
		arraydataptr = (char *) array;
	}
	else
	{
		/* detoast input array if necessary */
		array = DatumGetArrayTypeP(PointerGetDatum(array));

		ndim = ARR_NDIM(array);
		dim = ARR_DIMS(array);
		lb = ARR_LBOUND(array);
		arraydataptr = ARR_DATA_PTR(array);
	}

	/*
	 * Check provided subscripts.  A slice exceeding the current array
	 * limits is silently truncated to the array limits.  If we end up
	 * with an empty slice, return NULL (should it be an empty array
	 * instead?)
	 */
	if (ndim < nSubscripts || ndim <= 0 || ndim > MAXDIM)
		RETURN_NULL(ArrayType *);

	for (i = 0; i < nSubscripts; i++)
	{
		if (lowerIndx[i] < lb[i])
			lowerIndx[i] = lb[i];
		if (upperIndx[i] >= (dim[i] + lb[i]))
			upperIndx[i] = dim[i] + lb[i] - 1;
		if (lowerIndx[i] > upperIndx[i])
			RETURN_NULL(ArrayType *);
	}
	/* fill any missing subscript positions with full array range */
	for (; i < ndim; i++)
	{
		lowerIndx[i] = lb[i];
		upperIndx[i] = dim[i] + lb[i] - 1;
		if (lowerIndx[i] > upperIndx[i])
			RETURN_NULL(ArrayType *);
	}

	mda_get_range(ndim, span, lowerIndx, upperIndx);

	bytes = array_slice_size(ndim, dim, lb, arraydataptr,
							 lowerIndx, upperIndx,
							 elmlen, elmbyval, elmalign);
	bytes += ARR_OVERHEAD(ndim);

	newarray = (ArrayType *) palloc(bytes);
	newarray->size = bytes;
	newarray->ndim = ndim;
	newarray->flags = 0;
	newarray->elemtype = ARR_ELEMTYPE(array);
	memcpy(ARR_DIMS(newarray), span, ndim * sizeof(int));

	/*
	 * Lower bounds of the new array are set to 1.	Formerly (before 7.3)
	 * we copied the given lowerIndx values ... but that seems confusing.
	 */
	newlb = ARR_LBOUND(newarray);
	for (i = 0; i < ndim; i++)
		newlb[i] = 1;

	array_extract_slice(ndim, dim, lb, arraydataptr,
						lowerIndx, upperIndx, ARR_DATA_PTR(newarray),
						elmlen, elmbyval, elmalign);

	return newarray;
}

/*-----------------------------------------------------------------------------
 * array_set :
 *		  This routine sets the value of an array location (specified by
 *		  an index array) to a new value specified by "dataValue".
 * result :
 *		  A new array is returned, just like the old except for the one
 *		  modified entry.
 *
 * For one-dimensional arrays only, we allow the array to be extended
 * by assigning to the position one above or one below the existing range.
 * (We could be more flexible if we had a way to represent NULL elements.)
 *
 * NOTE: For assignments, we throw an error for invalid subscripts etc,
 * rather than returning a NULL as the fetch operations do.  The reasoning
 * is that returning a NULL would cause the user's whole array to be replaced
 * with NULL, which will probably not make him happy.
 *-----------------------------------------------------------------------------
 */
ArrayType *
array_set(ArrayType *array,
		  int nSubscripts,
		  int *indx,
		  Datum dataValue,
		  int arraylen,
		  int elmlen,
		  bool elmbyval,
		  char elmalign,
		  bool *isNull)
{
	int			i,
				ndim,
				dim[MAXDIM],
				lb[MAXDIM],
				offset;
	ArrayType  *newarray;
	char	   *elt_ptr;
	bool		extendbefore = false;
	bool		extendafter = false;
	int			olddatasize,
				newsize,
				olditemlen,
				newitemlen,
				overheadlen,
				lenbefore,
				lenafter;

	if (array == (ArrayType *) NULL)
		RETURN_NULL(ArrayType *);

	if (arraylen > 0)
	{
		/*
		 * fixed-length arrays -- these are assumed to be 1-d, 0-based. We
		 * cannot extend them, either.
		 */
		if (nSubscripts != 1)
			elog(ERROR, "Invalid array subscripts");
		if (indx[0] < 0 || indx[0] * elmlen >= arraylen)
			elog(ERROR, "Invalid array subscripts");
		newarray = (ArrayType *) palloc(arraylen);
		memcpy(newarray, array, arraylen);
		elt_ptr = (char *) newarray + indx[0] * elmlen;
		ArrayCastAndSet(dataValue, elmlen, elmbyval, elmalign, elt_ptr);
		return newarray;
	}

	/* make sure item to be inserted is not toasted */
	if (elmlen == -1)
		dataValue = PointerGetDatum(PG_DETOAST_DATUM(dataValue));

	/* detoast input array if necessary */
	array = DatumGetArrayTypeP(PointerGetDatum(array));

	ndim = ARR_NDIM(array);
	if (ndim != nSubscripts || ndim <= 0 || ndim > MAXDIM)
		elog(ERROR, "Invalid array subscripts");

	/* copy dim/lb since we may modify them */
	memcpy(dim, ARR_DIMS(array), ndim * sizeof(int));
	memcpy(lb, ARR_LBOUND(array), ndim * sizeof(int));

	/*
	 * Check subscripts
	 */
	for (i = 0; i < ndim; i++)
	{
		if (indx[i] < lb[i])
		{
			if (ndim == 1 && indx[i] == lb[i] - 1)
			{
				dim[i]++;
				lb[i]--;
				extendbefore = true;
			}
			else
				elog(ERROR, "Invalid array subscripts");
		}
		if (indx[i] >= (dim[i] + lb[i]))
		{
			if (ndim == 1 && indx[i] == (dim[i] + lb[i]))
			{
				dim[i]++;
				extendafter = true;
			}
			else
				elog(ERROR, "Invalid array subscripts");
		}
	}

	/*
	 * Compute sizes of items and areas to copy
	 */
	overheadlen = ARR_OVERHEAD(ndim);
	olddatasize = ARR_SIZE(array) - overheadlen;
	if (extendbefore)
	{
		lenbefore = 0;
		olditemlen = 0;
		lenafter = olddatasize;
	}
	else if (extendafter)
	{
		lenbefore = olddatasize;
		olditemlen = 0;
		lenafter = 0;
	}
	else
	{
		offset = ArrayGetOffset(nSubscripts, dim, lb, indx);
		elt_ptr = array_seek(ARR_DATA_PTR(array), offset,
							 elmlen, elmbyval, elmalign);
		lenbefore = (int) (elt_ptr - ARR_DATA_PTR(array));
		olditemlen = att_addlength(0, elmlen, PointerGetDatum(elt_ptr));
		olditemlen = att_align(olditemlen, elmalign);
		lenafter = (int) (olddatasize - lenbefore - olditemlen);
	}

	newitemlen = att_addlength(0, elmlen, dataValue);
	newitemlen = att_align(newitemlen, elmalign);

	newsize = overheadlen + lenbefore + newitemlen + lenafter;

	/*
	 * OK, do the assignment
	 */
	newarray = (ArrayType *) palloc(newsize);
	newarray->size = newsize;
	newarray->ndim = ndim;
	newarray->flags = 0;
	newarray->elemtype = ARR_ELEMTYPE(array);
	memcpy(ARR_DIMS(newarray), dim, ndim * sizeof(int));
	memcpy(ARR_LBOUND(newarray), lb, ndim * sizeof(int));
	memcpy((char *) newarray + overheadlen,
		   (char *) array + overheadlen,
		   lenbefore);
	memcpy((char *) newarray + overheadlen + lenbefore + newitemlen,
		   (char *) array + overheadlen + lenbefore + olditemlen,
		   lenafter);

	ArrayCastAndSet(dataValue, elmlen, elmbyval, elmalign,
					(char *) newarray + overheadlen + lenbefore);

	return newarray;
}

/*----------------------------------------------------------------------------
 * array_set_slice :
 *		  This routine sets the value of a range of array locations (specified
 *		  by upper and lower index values ) to new values passed as
 *		  another array
 * result :
 *		  A new array is returned, just like the old except for the
 *		  modified range.
 *
 * NOTE: we assume it is OK to scribble on the provided index arrays
 * lowerIndx[] and upperIndx[].  These are generally just temporaries.
 *
 * NOTE: For assignments, we throw an error for silly subscripts etc,
 * rather than returning a NULL as the fetch operations do.  The reasoning
 * is that returning a NULL would cause the user's whole array to be replaced
 * with NULL, which will probably not make him happy.
 *----------------------------------------------------------------------------
 */
ArrayType *
array_set_slice(ArrayType *array,
				int nSubscripts,
				int *upperIndx,
				int *lowerIndx,
				ArrayType *srcArray,
				int arraylen,
				int elmlen,
				bool elmbyval,
				char elmalign,
				bool *isNull)
{
	int			i,
				ndim,
				dim[MAXDIM],
				lb[MAXDIM],
				span[MAXDIM];
	ArrayType  *newarray;
	int			nsrcitems,
				olddatasize,
				newsize,
				olditemsize,
				newitemsize,
				overheadlen,
				lenbefore,
				lenafter;

	if (array == (ArrayType *) NULL)
		RETURN_NULL(ArrayType *);
	if (srcArray == (ArrayType *) NULL)
		return array;

	if (arraylen > 0)
	{
		/*
		 * fixed-length arrays -- not got round to doing this...
		 */
		elog(ERROR, "Updates on slices of fixed-length arrays not implemented");
	}

	/* detoast arrays if necessary */
	array = DatumGetArrayTypeP(PointerGetDatum(array));
	srcArray = DatumGetArrayTypeP(PointerGetDatum(srcArray));

	/* note: we assume srcArray contains no toasted elements */

	ndim = ARR_NDIM(array);
	if (ndim < nSubscripts || ndim <= 0 || ndim > MAXDIM)
		elog(ERROR, "Invalid array subscripts");

	/* copy dim/lb since we may modify them */
	memcpy(dim, ARR_DIMS(array), ndim * sizeof(int));
	memcpy(lb, ARR_LBOUND(array), ndim * sizeof(int));

	/*
	 * Check provided subscripts.  A slice exceeding the current array
	 * limits throws an error, *except* in the 1-D case where we will
	 * extend the array as long as no hole is created. An empty slice is
	 * an error, too.
	 */
	for (i = 0; i < nSubscripts; i++)
	{
		if (lowerIndx[i] > upperIndx[i])
			elog(ERROR, "Invalid array subscripts");
		if (lowerIndx[i] < lb[i])
		{
			if (ndim == 1 && upperIndx[i] >= lb[i] - 1)
			{
				dim[i] += lb[i] - lowerIndx[i];
				lb[i] = lowerIndx[i];
			}
			else
				elog(ERROR, "Invalid array subscripts");
		}
		if (upperIndx[i] >= (dim[i] + lb[i]))
		{
			if (ndim == 1 && lowerIndx[i] <= (dim[i] + lb[i]))
				dim[i] = upperIndx[i] - lb[i] + 1;
			else
				elog(ERROR, "Invalid array subscripts");
		}
	}
	/* fill any missing subscript positions with full array range */
	for (; i < ndim; i++)
	{
		lowerIndx[i] = lb[i];
		upperIndx[i] = dim[i] + lb[i] - 1;
		if (lowerIndx[i] > upperIndx[i])
			elog(ERROR, "Invalid array subscripts");
	}

	/*
	 * Make sure source array has enough entries.  Note we ignore the
	 * shape of the source array and just read entries serially.
	 */
	mda_get_range(ndim, span, lowerIndx, upperIndx);
	nsrcitems = ArrayGetNItems(ndim, span);
	if (nsrcitems > ArrayGetNItems(ARR_NDIM(srcArray), ARR_DIMS(srcArray)))
		elog(ERROR, "Source array too small");

	/*
	 * Compute space occupied by new entries, space occupied by replaced
	 * entries, and required space for new array.
	 */
	newitemsize = array_nelems_size(ARR_DATA_PTR(srcArray), nsrcitems,
									elmlen, elmbyval, elmalign);
	overheadlen = ARR_OVERHEAD(ndim);
	olddatasize = ARR_SIZE(array) - overheadlen;
	if (ndim > 1)
	{
		/*
		 * here we do not need to cope with extension of the array; it
		 * would be a lot more complicated if we had to do so...
		 */
		olditemsize = array_slice_size(ndim, dim, lb, ARR_DATA_PTR(array),
									   lowerIndx, upperIndx,
									   elmlen, elmbyval, elmalign);
		lenbefore = lenafter = 0;		/* keep compiler quiet */
	}
	else
	{
		/*
		 * here we must allow for possibility of slice larger than orig
		 * array
		 */
		int			oldlb = ARR_LBOUND(array)[0];
		int			oldub = oldlb + ARR_DIMS(array)[0] - 1;
		int			slicelb = Max(oldlb, lowerIndx[0]);
		int			sliceub = Min(oldub, upperIndx[0]);
		char	   *oldarraydata = ARR_DATA_PTR(array);

		lenbefore = array_nelems_size(oldarraydata, slicelb - oldlb,
									  elmlen, elmbyval, elmalign);
		if (slicelb > sliceub)
			olditemsize = 0;
		else
			olditemsize = array_nelems_size(oldarraydata + lenbefore,
											sliceub - slicelb + 1,
											elmlen, elmbyval, elmalign);
		lenafter = olddatasize - lenbefore - olditemsize;
	}

	newsize = overheadlen + olddatasize - olditemsize + newitemsize;

	newarray = (ArrayType *) palloc(newsize);
	newarray->size = newsize;
	newarray->ndim = ndim;
	newarray->flags = 0;
	newarray->elemtype = ARR_ELEMTYPE(array);
	memcpy(ARR_DIMS(newarray), dim, ndim * sizeof(int));
	memcpy(ARR_LBOUND(newarray), lb, ndim * sizeof(int));

	if (ndim > 1)
	{
		/*
		 * here we do not need to cope with extension of the array; it
		 * would be a lot more complicated if we had to do so...
		 */
		array_insert_slice(ndim, dim, lb, ARR_DATA_PTR(array), olddatasize,
						   ARR_DATA_PTR(newarray),
						   lowerIndx, upperIndx, ARR_DATA_PTR(srcArray),
						   elmlen, elmbyval, elmalign);
	}
	else
	{
		memcpy((char *) newarray + overheadlen,
			   (char *) array + overheadlen,
			   lenbefore);
		memcpy((char *) newarray + overheadlen + lenbefore,
			   ARR_DATA_PTR(srcArray),
			   newitemsize);
		memcpy((char *) newarray + overheadlen + lenbefore + newitemsize,
			   (char *) array + overheadlen + lenbefore + olditemsize,
			   lenafter);
	}

	return newarray;
}

/*
 * array_map()
 *
 * Map an array through an arbitrary function.	Return a new array with
 * same dimensions and each source element transformed by fn().  Each
 * source element is passed as the first argument to fn(); additional
 * arguments to be passed to fn() can be specified by the caller.
 * The output array can have a different element type than the input.
 *
 * Parameters are:
 * * fcinfo: a function-call data structure pre-constructed by the caller
 *	 to be ready to call the desired function, with everything except the
 *	 first argument position filled in.  In particular, flinfo identifies
 *	 the function fn(), and if nargs > 1 then argument positions after the
 *	 first must be preset to the additional values to be passed.  The
 *	 first argument position initially holds the input array value.
 * * inpType: OID of element type of input array.  This must be the same as,
 *	 or binary-compatible with, the first argument type of fn().
 * * retType: OID of element type of output array.	This must be the same as,
 *	 or binary-compatible with, the result type of fn().
 *
 * NB: caller must assure that input array is not NULL.  Currently,
 * any additional parameters passed to fn() may not be specified as NULL
 * either.
 */
Datum
array_map(FunctionCallInfo fcinfo, Oid inpType, Oid retType)
{
	ArrayType  *v;
	ArrayType  *result;
	Datum	   *values;
	Datum		elt;
	int		   *dim;
	int			ndim;
	int			nitems;
	int			i;
	int			nbytes = 0;
	int			inp_typlen;
	bool		inp_typbyval;
	char		inp_typalign;
	int			typlen;
	bool		typbyval;
	char		typalign;
	char		typdelim;
	Oid			typelem;
	Oid			proc;
	char	   *s;

	/* Get input array */
	if (fcinfo->nargs < 1)
		elog(ERROR, "array_map: invalid nargs: %d", fcinfo->nargs);
	if (PG_ARGISNULL(0))
		elog(ERROR, "array_map: null input array");
	v = PG_GETARG_ARRAYTYPE_P(0);

	Assert(ARR_ELEMTYPE(v) == inpType);

	ndim = ARR_NDIM(v);
	dim = ARR_DIMS(v);
	nitems = ArrayGetNItems(ndim, dim);

	/* Check for empty array */
	if (nitems <= 0)
		PG_RETURN_ARRAYTYPE_P(v);

	/* Lookup source and result types. Unneeded variables are reused. */
	system_cache_lookup(inpType, false, &inp_typlen, &inp_typbyval,
						&typdelim, &typelem, &proc, &inp_typalign);
	system_cache_lookup(retType, false, &typlen, &typbyval,
						&typdelim, &typelem, &proc, &typalign);

	/* Allocate temporary array for new values */
	values = (Datum *) palloc(nitems * sizeof(Datum));

	/* Loop over source data */
	s = (char *) ARR_DATA_PTR(v);
	for (i = 0; i < nitems; i++)
	{
		/* Get source element */
		elt = fetch_att(s, inp_typbyval, inp_typlen);

		s = att_addlength(s, inp_typlen, PointerGetDatum(s));
		s = (char *) att_align(s, inp_typalign);

		/*
		 * Apply the given function to source elt and extra args.
		 *
		 * We assume the extra args are non-NULL, so need not check whether
		 * fn() is strict.	Would need to do more work here to support
		 * arrays containing nulls, too.
		 */
		fcinfo->arg[0] = elt;
		fcinfo->argnull[0] = false;
		fcinfo->isnull = false;
		values[i] = FunctionCallInvoke(fcinfo);
		if (fcinfo->isnull)
			elog(ERROR, "array_map: cannot handle NULL in array");

		/* Ensure data is not toasted */
		if (typlen == -1)
			values[i] = PointerGetDatum(PG_DETOAST_DATUM(values[i]));

		/* Update total result size */
		nbytes = att_addlength(nbytes, typlen, values[i]);
		nbytes = att_align(nbytes, typalign);
	}

	/* Allocate and initialize the result array */
	nbytes += ARR_OVERHEAD(ndim);
	result = (ArrayType *) palloc(nbytes);
	MemSet(result, 0, nbytes);

	result->size = nbytes;
	result->ndim = ndim;
	result->elemtype = retType;
	memcpy(ARR_DIMS(result), ARR_DIMS(v), 2 * ndim * sizeof(int));

	/*
	 * Note: do not risk trying to pfree the results of the called
	 * function
	 */
	CopyArrayEls(ARR_DATA_PTR(result), values, nitems,
				 typlen, typbyval, typalign, false);
	pfree(values);

	PG_RETURN_ARRAYTYPE_P(result);
}

/*----------
 * construct_array	--- simple method for constructing an array object
 *
 * elems: array of Datum items to become the array contents
 * nelems: number of items
 * elmtype, elmlen, elmbyval, elmalign: info for the datatype of the items
 *
 * A palloc'd 1-D array object is constructed and returned.  Note that
 * elem values will be copied into the object even if pass-by-ref type.
 * NULL element values are not supported.
 *
 * NOTE: it would be cleaner to look up the elmlen/elmbval/elmalign info
 * from the system catalogs, given the elmtype.  However, in most current
 * uses the type is hard-wired into the caller and so we can save a lookup
 * cycle by hard-wiring the type info as well.
 *----------
 */
ArrayType *
construct_array(Datum *elems, int nelems,
				Oid elmtype,
				int elmlen, bool elmbyval, char elmalign)
{
	ArrayType  *result;
	int			nbytes;
	int			i;

	/* compute required space */
	if (elmlen > 0)
		nbytes = nelems * att_align(elmlen, elmalign);
	else
	{
		Assert(!elmbyval);
		nbytes = 0;
		for (i = 0; i < nelems; i++)
		{
			/* make sure data is not toasted */
			if (elmlen == -1)
				elems[i] = PointerGetDatum(PG_DETOAST_DATUM(elems[i]));
			nbytes = att_addlength(nbytes, elmlen, elems[i]);
			nbytes = att_align(nbytes, elmalign);
		}
	}

	/* Allocate and initialize 1-D result array */
	nbytes += ARR_OVERHEAD(1);
	result = (ArrayType *) palloc(nbytes);

	result->size = nbytes;
	result->ndim = 1;
	result->flags = 0;
	result->elemtype = elmtype;
	ARR_DIMS(result)[0] = nelems;
	ARR_LBOUND(result)[0] = 1;

	CopyArrayEls(ARR_DATA_PTR(result), elems, nelems,
				 elmlen, elmbyval, elmalign, false);

	return result;
}

/*----------
 * deconstruct_array  --- simple method for extracting data from an array
 *
 * array: array object to examine (must not be NULL)
 * elmtype, elmlen, elmbyval, elmalign: info for the datatype of the items
 * elemsp: return value, set to point to palloc'd array of Datum values
 * nelemsp: return value, set to number of extracted values
 *
 * If array elements are pass-by-ref data type, the returned Datums will
 * be pointers into the array object.
 *
 * NOTE: it would be cleaner to look up the elmlen/elmbval/elmalign info
 * from the system catalogs, given the elmtype.  However, in most current
 * uses the type is hard-wired into the caller and so we can save a lookup
 * cycle by hard-wiring the type info as well.
 *----------
 */
void
deconstruct_array(ArrayType *array,
				  Oid elmtype,
				  int elmlen, bool elmbyval, char elmalign,
				  Datum **elemsp, int *nelemsp)
{
	Datum	   *elems;
	int			nelems;
	char	   *p;
	int			i;

	Assert(ARR_ELEMTYPE(array) == elmtype);

	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));
	if (nelems <= 0)
	{
		*elemsp = NULL;
		*nelemsp = 0;
		return;
	}
	*elemsp = elems = (Datum *) palloc(nelems * sizeof(Datum));
	*nelemsp = nelems;

	p = ARR_DATA_PTR(array);
	for (i = 0; i < nelems; i++)
	{
		elems[i] = fetch_att(p, elmbyval, elmlen);
		p = att_addlength(p, elmlen, PointerGetDatum(p));
		p = (char *) att_align(p, elmalign);
	}
}


/*-----------------------------------------------------------------------------
 * array_eq :
 *		  compares two arrays for equality
 * result :
 *		  returns true if the arrays are equal, false otherwise.
 *
 * XXX bitwise equality is pretty bogus ...
 *-----------------------------------------------------------------------------
 */
Datum
array_eq(PG_FUNCTION_ARGS)
{
	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	bool		result = true;

	if (ARR_SIZE(array1) != ARR_SIZE(array2))
		result = false;
	else if (memcmp(array1, array2, ARR_SIZE(array1)) != 0)
		result = false;

	/* Avoid leaking memory when handed toasted input. */
	PG_FREE_IF_COPY(array1, 0);
	PG_FREE_IF_COPY(array2, 1);

	PG_RETURN_BOOL(result);
}


/***************************************************************************/
/******************|		  Support  Routines			  |*****************/
/***************************************************************************/

static void
system_cache_lookup(Oid element_type,
					bool input,
					int *typlen,
					bool *typbyval,
					char *typdelim,
					Oid *typelem,
					Oid *proc,
					char *typalign)
{
	HeapTuple	typeTuple;
	Form_pg_type typeStruct;

	typeTuple = SearchSysCache(TYPEOID,
							   ObjectIdGetDatum(element_type),
							   0, 0, 0);
	if (!HeapTupleIsValid(typeTuple))
		elog(ERROR, "cache lookup failed for type %u", element_type);
	typeStruct = (Form_pg_type) GETSTRUCT(typeTuple);

	*typlen = typeStruct->typlen;
	*typbyval = typeStruct->typbyval;
	*typdelim = typeStruct->typdelim;
	*typelem = typeStruct->typelem;
	*typalign = typeStruct->typalign;
	if (input)
		*proc = typeStruct->typinput;
	else
		*proc = typeStruct->typoutput;
	ReleaseSysCache(typeTuple);
}

/*
 * Fetch array element at pointer, converted correctly to a Datum
 */
static Datum
ArrayCast(char *value, bool byval, int len)
{
	return fetch_att(value, byval, len);
}

/*
 * Copy datum to *dest and return total space used (including align padding)
 */
static int
ArrayCastAndSet(Datum src,
				int typlen,
				bool typbyval,
				char typalign,
				char *dest)
{
	int			inc;

	if (typlen > 0)
	{
		if (typbyval)
			store_att_byval(dest, src, typlen);
		else
			memmove(dest, DatumGetPointer(src), typlen);
		inc = att_align(typlen, typalign);
	}
	else
	{
		Assert(!typbyval);
		inc = att_addlength(0, typlen, src);
		memmove(dest, DatumGetPointer(src), inc);
		inc = att_align(inc, typalign);
	}

	return inc;
}

/*
 * Compute total size of the nitems array elements starting at *ptr
 */
static int
array_nelems_size(char *ptr, int nitems,
				  int typlen, bool typbyval, char typalign)
{
	char	   *origptr;
	int			i;

	/* fixed-size elements? */
	if (typlen > 0)
		return nitems * att_align(typlen, typalign);

	Assert(!typbyval);
	origptr = ptr;
	for (i = 0; i < nitems; i++)
	{
		ptr = att_addlength(ptr, typlen, PointerGetDatum(ptr));
		ptr = (char *) att_align(ptr, typalign);
	}
	return ptr - origptr;
}

/*
 * Advance ptr over nitems array elements
 */
static char *
array_seek(char *ptr, int nitems,
		   int typlen, bool typbyval, char typalign)
{
	return ptr + array_nelems_size(ptr, nitems,
								   typlen, typbyval, typalign);
}

/*
 * Copy nitems array elements from srcptr to destptr
 *
 * Returns number of bytes copied
 */
static int
array_copy(char *destptr, int nitems, char *srcptr,
		   int typlen, bool typbyval, char typalign)
{
	int			numbytes = array_nelems_size(srcptr, nitems,
											 typlen, typbyval, typalign);

	memmove(destptr, srcptr, numbytes);
	return numbytes;
}

/*
 * Compute space needed for a slice of an array
 *
 * We assume the caller has verified that the slice coordinates are valid.
 */
static int
array_slice_size(int ndim, int *dim, int *lb, char *arraydataptr,
				 int *st, int *endp,
				 int typlen, bool typbyval, char typalign)
{
	int			st_pos,
				span[MAXDIM],
				prod[MAXDIM],
				dist[MAXDIM],
				indx[MAXDIM];
	char	   *ptr;
	int			i,
				j,
				inc;
	int			count = 0;

	mda_get_range(ndim, span, st, endp);

	/* Pretty easy for fixed element length ... */
	if (typlen > 0)
		return ArrayGetNItems(ndim, span) * att_align(typlen, typalign);

	/* Else gotta do it the hard way */
	st_pos = ArrayGetOffset(ndim, dim, lb, st);
	ptr = array_seek(arraydataptr, st_pos,
					 typlen, typbyval, typalign);
	mda_get_prod(ndim, dim, prod);
	mda_get_offset_values(ndim, dist, prod, span);
	for (i = 0; i < ndim; i++)
		indx[i] = 0;
	j = ndim - 1;
	do
	{
		ptr = array_seek(ptr, dist[j],
						 typlen, typbyval, typalign);
		inc = att_addlength(0, typlen, PointerGetDatum(ptr));
		inc = att_align(inc, typalign);
		ptr += inc;
		count += inc;
	} while ((j = mda_next_tuple(ndim, indx, span)) != -1);
	return count;
}

/*
 * Extract a slice of an array into consecutive elements at *destPtr.
 *
 * We assume the caller has verified that the slice coordinates are valid
 * and allocated enough storage at *destPtr.
 */
static void
array_extract_slice(int ndim,
					int *dim,
					int *lb,
					char *arraydataptr,
					int *st,
					int *endp,
					char *destPtr,
					int typlen,
					bool typbyval,
					char typalign)
{
	int			st_pos,
				prod[MAXDIM],
				span[MAXDIM],
				dist[MAXDIM],
				indx[MAXDIM];
	char	   *srcPtr;
	int			i,
				j,
				inc;

	st_pos = ArrayGetOffset(ndim, dim, lb, st);
	srcPtr = array_seek(arraydataptr, st_pos,
						typlen, typbyval, typalign);
	mda_get_prod(ndim, dim, prod);
	mda_get_range(ndim, span, st, endp);
	mda_get_offset_values(ndim, dist, prod, span);
	for (i = 0; i < ndim; i++)
		indx[i] = 0;
	j = ndim - 1;
	do
	{
		srcPtr = array_seek(srcPtr, dist[j],
							typlen, typbyval, typalign);
		inc = array_copy(destPtr, 1, srcPtr,
						 typlen, typbyval, typalign);
		destPtr += inc;
		srcPtr += inc;
	} while ((j = mda_next_tuple(ndim, indx, span)) != -1);
}

/*
 * Insert a slice into an array.
 *
 * ndim/dim/lb are dimensions of the dest array, which has data area
 * starting at origPtr.  A new array with those same dimensions is to
 * be constructed; its data area starts at destPtr.
 *
 * Elements within the slice volume are taken from consecutive locations
 * at srcPtr; elements outside it are copied from origPtr.
 *
 * We assume the caller has verified that the slice coordinates are valid
 * and allocated enough storage at *destPtr.
 */
static void
array_insert_slice(int ndim,
				   int *dim,
				   int *lb,
				   char *origPtr,
				   int origdatasize,
				   char *destPtr,
				   int *st,
				   int *endp,
				   char *srcPtr,
				   int typlen,
				   bool typbyval,
				   char typalign)
{
	int			st_pos,
				prod[MAXDIM],
				span[MAXDIM],
				dist[MAXDIM],
				indx[MAXDIM];
	char	   *origEndpoint = origPtr + origdatasize;
	int			i,
				j,
				inc;

	st_pos = ArrayGetOffset(ndim, dim, lb, st);
	inc = array_copy(destPtr, st_pos, origPtr,
					 typlen, typbyval, typalign);
	destPtr += inc;
	origPtr += inc;
	mda_get_prod(ndim, dim, prod);
	mda_get_range(ndim, span, st, endp);
	mda_get_offset_values(ndim, dist, prod, span);
	for (i = 0; i < ndim; i++)
		indx[i] = 0;
	j = ndim - 1;
	do
	{
		/* Copy/advance over elements between here and next part of slice */
		inc = array_copy(destPtr, dist[j], origPtr,
						 typlen, typbyval, typalign);
		destPtr += inc;
		origPtr += inc;
		/* Copy new element at this slice position */
		inc = array_copy(destPtr, 1, srcPtr,
						 typlen, typbyval, typalign);
		destPtr += inc;
		srcPtr += inc;
		/* Advance over old element at this slice position */
		origPtr = array_seek(origPtr, 1,
							 typlen, typbyval, typalign);
	} while ((j = mda_next_tuple(ndim, indx, span)) != -1);

	/* don't miss any data at the end */
	memcpy(destPtr, origPtr, origEndpoint - origPtr);
}
