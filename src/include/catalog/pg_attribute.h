/*-------------------------------------------------------------------------
 *
 * pg_attribute.h
 *	  definition of the system "attribute" relation (pg_attribute)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 2003, Regents of the University of California
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: pg_attribute.h,v 1.7 2005/01/23 23:26:58 yangsta Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *	  utils/cache/relcache.c requires hard-coded tuple descriptors
 *	  for some of the system catalogs.	So if the schema for any of
 *	  these changes, be sure and change the appropriate Schema_xxx
 *	  macros!  -cim 2/5/91
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_ATTRIBUTE_H
#define PG_ATTRIBUTE_H

/* ----------------
 *		postgres.h contains the system type definintions and the
 *		CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *		can be read by both genbki.sh and the C compiler.
 * ----------------
 */

/* ----------------
 *		pg_attribute definition.  cpp turns this into
 *		typedef struct FormData_pg_attribute
 *
 *		If you change the following, make sure you change the structs for
 *		system attributes in catalog/heap.c also.
 * ----------------
 */
CATALOG(pg_attribute) BOOTSTRAP BKI_WITHOUT_OIDS
{
	Oid			attrelid;		/* OID of relation containing this
								 * attribute */
	NameData	attname;		/* name of attribute */

	/*
	 * atttypid is the OID of the instance in Catalog Class pg_type that
	 * defines the data type of this attribute (e.g. int4).  Information
	 * in that instance is redundant with the attlen, attbyval, and
	 * attalign attributes of this instance, so they had better match or
	 * Postgres will fail.
	 */
	Oid			atttypid;

	/*
	 * attstattarget is the target number of statistics datapoints to
	 * collect during VACUUM ANALYZE of this column.  A zero here
	 * indicates that we do not wish to collect any stats about this
	 * column. A "-1" here indicates that no value has been explicitly set
	 * for this column, so ANALYZE should use the default setting.
	 */
	int4		attstattarget;

	/*
	 * attlen is a copy of the typlen field from pg_type for this
	 * attribute.  See atttypid comments above.
	 */
	int2		attlen;

	/*
	 * attnum is the "attribute number" for the attribute:	A value that
	 * uniquely identifies this attribute within its class. For user
	 * attributes, Attribute numbers are greater than 0 and not greater
	 * than the number of attributes in the class. I.e. if the Class
	 * pg_class says that Class XYZ has 10 attributes, then the user
	 * attribute numbers in Class pg_attribute must be 1-10.
	 *
	 * System attributes have attribute numbers less than 0 that are unique
	 * within the class, but not constrained to any particular range.
	 *
	 * Note that (attnum - 1) is often used as the index to an array.
	 */
	int2		attnum;

	/*
	 * attndims is the declared number of dimensions, if an array type,
	 * otherwise zero.
	 */
	int4		attndims;

	/*
	 * fastgetattr() uses attcacheoff to cache byte offsets of attributes
	 * in heap tuples.	The value actually stored in pg_attribute (-1)
	 * indicates no cached value.  But when we copy these tuples into a
	 * tuple descriptor, we may then update attcacheoff in the copies.
	 * This speeds up the attribute walking process.
	 */
	int4		attcacheoff;

	/*
	 * atttypmod records type-specific data supplied at table creation
	 * time (for example, the max length of a varchar field).  It is
	 * passed to type-specific input and output functions as the third
	 * argument. The value will generally be -1 for types that do not need
	 * typmod.
	 */
	int4		atttypmod;

	/*
	 * attbyval is a copy of the typbyval field from pg_type for this
	 * attribute.  See atttypid comments above.
	 */
	bool		attbyval;

	/*----------
	 * attstorage tells for VARLENA attributes, what the heap access
	 * methods can do to it if a given tuple doesn't fit into a page.
	 * Possible values are
	 *		'p': Value must be stored plain always
	 *		'e': Value can be stored in "secondary" relation (if relation
	 *			 has one, see pg_class.reltoastrelid)
	 *		'm': Value can be stored compressed inline
	 *		'x': Value can be stored compressed inline or in "secondary"
	 * Note that 'm' fields can also be moved out to secondary storage,
	 * but only as a last resort ('e' and 'x' fields are moved first).
	 *----------
	 */
	char		attstorage;

	/* This flag indicates that the attribute is really a set */
	bool		attisset;

	/*
	 * attalign is a copy of the typalign field from pg_type for this
	 * attribute.  See atttypid comments above.
	 */
	char		attalign;

	/* This flag represents the "NOT NULL" constraint */
	bool		attnotnull;

	/* Has DEFAULT value or not */
	bool		atthasdef;

	/* Is dropped (ie, logically invisible) or not */
	bool		attisdropped;

	/* Has a local definition (hence, do not drop when attinhcount is 0) */
	bool		attislocal;

	/* Number of times inherited from direct parent relation(s) */
	int4		attinhcount;
} FormData_pg_attribute;

/*
 * someone should figure out how to do this properly. (The problem is
 * the size of the C struct is not the same as the size of the tuple
 * because of alignment padding at the end of the struct.)
 */
#define ATTRIBUTE_TUPLE_SIZE \
	(offsetof(FormData_pg_attribute,attinhcount) + sizeof(int4))

/* ----------------
 *		Form_pg_attribute corresponds to a pointer to a tuple with
 *		the format of pg_attribute relation.
 * ----------------
 */
typedef FormData_pg_attribute *Form_pg_attribute;

/* ----------------
 *		compiler constants for pg_attribute
 * ----------------
 */

#define Natts_pg_attribute				18
#define Anum_pg_attribute_attrelid		1
#define Anum_pg_attribute_attname		2
#define Anum_pg_attribute_atttypid		3
#define Anum_pg_attribute_attstattarget 4
#define Anum_pg_attribute_attlen		5
#define Anum_pg_attribute_attnum		6
#define Anum_pg_attribute_attndims		7
#define Anum_pg_attribute_attcacheoff	8
#define Anum_pg_attribute_atttypmod		9
#define Anum_pg_attribute_attbyval		10
#define Anum_pg_attribute_attstorage	11
#define Anum_pg_attribute_attisset		12
#define Anum_pg_attribute_attalign		13
#define Anum_pg_attribute_attnotnull	14
#define Anum_pg_attribute_atthasdef		15
#define Anum_pg_attribute_attisdropped	16
#define Anum_pg_attribute_attislocal	17
#define Anum_pg_attribute_attinhcount	18



/* ----------------
 *		SCHEMA_ macros for declaring hardcoded tuple descriptors.
 *		these are used in utils/cache/relcache.c
 * ----------------
#define SCHEMA_NAME(x) CppConcat(Name_,x)
#define SCHEMA_DESC(x) CppConcat(Desc_,x)
#define SCHEMA_NATTS(x) CppConcat(Natts_,x)
#define SCHEMA_DEF(x) \
	FormData_pg_attribute \
	SCHEMA_DESC(x) [ SCHEMA_NATTS(x) ] = \
	{ \
		CppConcat(Schema_,x) \
	}
 */

/* ----------------
 *		initial contents of pg_attribute
 *
 * NOTE: only "bootstrapped" relations need to be declared here.
 *
 * NOTE: if changing pg_attribute column set, also see the hard-coded
 * entries for system attributes in catalog/heap.c.
 * ----------------
 */

/* ----------------
 *		pg_type schema
 * ----------------
 */
#define Schema_pg_type \
{ 1247, {"typname"},	   19, -1, NAMEDATALEN, 1, 0, -1, -1, false, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typnamespace"},  26, -1,	4,	2, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typowner"},	   23, 0,	4,	3, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typlen"},		   21, 0,	2,	4, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1247, {"typbyval"},	   16, 0,	1,	5, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1247, {"typtype"},	   18, -1,	1,	6, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1247, {"typisdefined"},  16, -1,	1,	7, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1247, {"typdelim"},	   18, 0,	1,	8, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1247, {"typrelid"},	   26, 0,	4,	9, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typelem"},	   26, 0,	4, 10, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typinput"},	   24, 0,	4, 11, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typoutput"},	   24, 0,	4, 12, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typalign"},	   18, 0,	1, 13, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1247, {"typstorage"},    18, 0,	1, 14, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1247, {"typnotnull"},    16, 0,	1, 15, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1247, {"typbasetype"},   26, 0,	4, 16, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typtypmod"},	   23, 0,	4, 17, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typndims"},	   23, 0,	4, 18, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1247, {"typdefaultbin"}, 25, 0,  -1, 19, 0, -1, -1, false, 'x', false, 'i', false, false, false, true, 0 }, \
{ 1247, {"typdefault"},    25, 0,  -1, 20, 0, -1, -1, false, 'x', false, 'i', false, false, false, true, 0 }


DATA(insert ( 1247 typname			19 -1 NAMEDATALEN	1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1247 typnamespace		26 -1 4   2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typowner			23 0  4   3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typlen			21 0  2   4 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1247 typbyval			16 0  1   5 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1247 typtype			18 -1 1   6 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1247 typisdefined		16 -1 1   7 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1247 typdelim			18 0  1   8 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1247 typrelid			26 0  4   9 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typelem			26 0  4  10 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typinput			24 0  4  11 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typoutput		24 0  4  12 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typalign			18 0  1  13 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1247 typstorage		18 0  1  14 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1247 typnotnull		16 0  1  15 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1247 typbasetype		26 0  4  16 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typtypmod		23 0  4  17 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typndims			23 0  4  18 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 typdefaultbin	25 0 -1  19 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1247 typdefault		25 0 -1  20 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1247 ctid				27 0  6  -1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1247 oid				26 0  4  -2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 xmin				28 0  4  -3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 cmin				29 0  4  -4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 xmax				28 0  4  -5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 cmax				29 0  4  -6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1247 tableoid			26 0  4  -7 0 -1 -1 t p f i t f f t 0));

/* ----------------
 *		pg_database
 * ----------------
 */
DATA(insert ( 1262 datname			19 0 NAMEDATALEN   1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1262 datdba			23 0  4   2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 encoding			23 0  4   3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 datistemplate	16 0  1   4 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1262 datallowconn		16 0  1   5 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1262 datlastsysoid	26 0  4   6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 datvacuumxid		28 0  4   7 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 datfrozenxid		28 0  4   8 0 -1 -1 t p f i t f f t 0));
/* do not mark datpath as toastable; GetRawDatabaseInfo won't cope */
DATA(insert ( 1262 datpath			25 0 -1   9 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1262 datconfig	  1009 0 -1  10 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1262 datacl		  1034 0 -1  11 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1262 ctid				27 0  6  -1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1262 oid				26 0  4  -2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 xmin				28 0  4  -3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 cmin				29 0  4  -4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 xmax				28 0  4  -5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 cmax				29 0  4  -6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1262 tableoid			26 0  4  -7 0 -1 -1 t p f i t f f t 0));

/* ----------------
 *		pg_proc
 * ----------------
 */
#define Schema_pg_proc \
{ 1255, {"proname"},			19, -1, NAMEDATALEN,  1, 0, -1, -1, false, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1255, {"pronamespace"},		26, -1, 4,	2, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1255, {"proowner"},			23, 0,	4,	3, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1255, {"prolang"},			26, 0,	4,	4, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1255, {"proisagg"},			16, -1, 1,	5, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1255, {"prosecdef"},			16, 0,	1,	6, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1255, {"proisstrict"},		16, 0,	1,	7, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1255, {"proretset"},			16, 0,	1,	8, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1255, {"provolatile"},		18, 0,	1,	9, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1255, {"pronargs"},			21, 0,	2, 10, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1255, {"prorettype"},			26, 0,	4, 11, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1255, {"proargtypes"},		30, 0, INDEX_MAX_KEYS*4, 12, 0, -1, -1, false, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1255, {"prosrc"},				25, 0, -1, 13, 0, -1, -1, false, 'x', false, 'i', false, false, false, true, 0 }, \
{ 1255, {"probin"},				17, 0, -1, 14, 0, -1, -1, false, 'x', false, 'i', false, false, false, true, 0 }, \
{ 1255, {"proacl"},			  1034, 0, -1, 15, 0, -1, -1, false, 'x', false, 'i', false, false, false, true, 0 }

DATA(insert ( 1255 proname			19 -1 NAMEDATALEN	1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1255 pronamespace		26 -1 4   2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 proowner			23 0  4   3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 prolang			26 0  4   4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 proisagg			16 -1 1   5 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1255 prosecdef		16 0  1   6 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1255 proisstrict		16 0  1   7 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1255 proretset		16 0  1   8 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1255 provolatile		18 0  1   9 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1255 pronargs			21 0  2  10 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1255 prorettype		26 0  4  11 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 proargtypes		30 0 INDEX_MAX_KEYS*4 12 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1255 prosrc			25 0 -1  13 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1255 probin			17 0 -1  14 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1255 proacl		  1034 0 -1  15 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1255 ctid				27 0  6  -1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1255 oid				26 0  4  -2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 xmin				28 0  4  -3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 cmin				29 0  4  -4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 xmax				28 0  4  -5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 cmax				29 0  4  -6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1255 tableoid			26 0  4  -7 0 -1 -1 t p f i t f f t 0));

/* ----------------
 *		pg_shadow
 * ----------------
 */
DATA(insert ( 1260 usename			19	-1 NAMEDATALEN	1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1260 usesysid			23	-1	4	2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1260 usecreatedb		16	0	1	3 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1260 usesuper			16	0	1	4 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1260 usecatupd		16	0	1	5 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1260 passwd			25	0  -1	6 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1260 valuntil			702 0	4	7 0 -1 -1 t p f i f f f t 0));
DATA(insert ( 1260 useconfig	  1009	0  -1	8 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1260 ctid				27 0  6  -1 0 -1 -1 f p f i t f f t 0));
/* no OIDs in pg_shadow */
DATA(insert ( 1260 xmin				28 0  4  -3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1260 cmin				29 0  4  -4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1260 xmax				28 0  4  -5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1260 cmax				29 0  4  -6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1260 tableoid			26 0  4  -7 0 -1 -1 t p f i t f f t 0));

/* ----------------
 *		pg_group
 * ----------------
 */
DATA(insert ( 1261 groname			19 -1 NAMEDATALEN  1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1261 grosysid			23 -1  4   2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1261 grolist		  1007 0 -1   3 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1261 ctid				27 0  6  -1 0 -1 -1 f p f i t f f t 0));
/* no OIDs in pg_group */
DATA(insert ( 1261 xmin				28 0  4  -3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1261 cmin				29 0  4  -4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1261 xmax				28 0  4  -5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1261 cmax				29 0  4  -6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1261 tableoid			26 0  4  -7 0 -1 -1 t p f i t f f t 0));

/* ----------------
 *		pg_attribute
 * ----------------
 */
#define Schema_pg_attribute \
{ 1249, {"attrelid"},	  26, -1,	4,	1, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1249, {"attname"},	  19, -1, NAMEDATALEN,	2, 0, -1, -1, false, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1249, {"atttypid"},	  26, 0,	4,	3, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1249, {"attstattarget"}, 23, 0,	4,	4, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1249, {"attlen"},		  21, 0,	2,	5, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1249, {"attnum"},		  21, 0,	2,	6, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1249, {"attndims"},	  23, 0,	4,	7, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1249, {"attcacheoff"},  23, 0,	4,	8, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1249, {"atttypmod"},	  23, 0,	4,	9, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1249, {"attbyval"},	  16, 0,	1, 10, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"attstorage"},   18, 0,	1, 11, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"attisset"},	  16, 0,	1, 12, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"attalign"},	  18, 0,	1, 13, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"attnotnull"},   16, 0, 1, 14, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"atthasdef"},	  16, 0, 1, 15, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"attisdropped"}, 16, 0, 1, 16, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"attislocal"},   16, 0, 1, 17, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1249, {"attinhcount"},  23, 0, 4, 18, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }

DATA(insert ( 1249 attrelid			26 -1  4   1 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 attname			19 -1 NAMEDATALEN  2 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1249 atttypid			26 0  4   3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 attstattarget	23 0  4   4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 attlen			21 0  2   5 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1249 attnum			21 0  2   6 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1249 attndims			23 0  4   7 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 attcacheoff		23 0  4   8 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 atttypmod		23 0  4   9 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 attbyval			16 0  1  10 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 attstorage		18 0  1  11 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 attisset			16 0  1  12 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 attalign			18 0  1  13 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 attnotnull		16 0  1  14 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 atthasdef		16 0  1  15 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 attisdropped		16 0  1  16 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 attislocal		16 0  1  17 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1249 attinhcount		23 0  4  18 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 ctid				27 0  6  -1 0 -1 -1 f p f i t f f t 0));
/* no OIDs in pg_attribute */
DATA(insert ( 1249 xmin				28 0  4  -3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 cmin				29 0  4  -4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 xmax				28 0  4  -5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 cmax				29 0  4  -6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1249 tableoid			26 0  4  -7 0 -1 -1 t p f i t f f t 0));

/* ----------------
 *		pg_class
 * ----------------
 */
#define Schema_pg_class \
{ 1259, {"relname"},	      19, -1, NAMEDATALEN, 1, 0, -1, -1, false, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"relnamespace"},     26, -1,	4,	2, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reltype"},	      26, 0,	4,	3, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"relowner"},	      23, 0,	4,	4, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"relam"},		      26, 0,	4,	5, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"relfilenode"},      26, 0,	4,	6, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"relpages"},	      23, 0,	4,	7, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reltuples"},	      700, 0,	4,	8, 0, -1, -1, false, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reltoastrelid"},    26, 0,	4,	9, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reltoastidxid"},    26, 0,	4, 10, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"relhasindex"},      16, 0,	1, 11, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1259, {"relisshared"},      16, 0,	1, 12, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1259, {"relkind"},	      18, -1,	1, 13, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1259, {"relnatts"},	      21, 0,	2, 14, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1259, {"relchecks"},	      21, 0,	2, 15, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1259, {"reltriggers"},      21, 0,	2, 16, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1259, {"relukeys"},	      21, 0,	2, 17, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1259, {"relfkeys"},	      21, 0,	2, 18, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1259, {"relrefs"},	      21, 0,	2, 19, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1259, {"reloverloadtype"},  21, 0,	2, 20, 0, -1, -1, true, 'p', false, 's', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam1"},23, 0,	4, 21, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam2"},23, 0,	4, 22, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam3"},23, 0,	4, 23, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam4"},23, 0,	4, 24, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam5"},23, 0,	4, 25, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam6"},23, 0,	4, 26, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam7"},23, 0,	4, 27, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"reloverloadparam8"},23, 0,	4, 28, 0, -1, -1, true, 'p', false, 'i', true, false, false, true, 0 }, \
{ 1259, {"relhasoids"},       16, 0,	1, 29, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1259, {"relhaspkey"},       16, 0,	1, 30, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1259, {"relhasrules"},      16, 0,	1, 31, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1259, {"relhassubclass"},   16, 0,	1, 32, 0, -1, -1, true, 'p', false, 'c', true, false, false, true, 0 }, \
{ 1259, {"relacl"},		    1034, 0,  -1, 33, 0, -1, -1, false, 'x', false, 'i', false, false, false, true, 0 }

DATA(insert ( 1259 relname				19 -1 NAMEDATALEN	1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1259 relnamespace			26 -1 4   2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reltype				26 0  4   3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 relowner				23 0  4   4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 relam				26 0  4   5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 relfilenode			26 0  4   6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 relpages				23 0  4   7 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reltuples		   700 0  4   8 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1259 reltoastrelid		26 0  4   9 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reltoastidxid		26 0  4  10 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 relhasindex			16 0  1  11 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1259 relisshared			16 0  1  12 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1259 relkind				18 -1 1  13 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1259 relnatts				21 0  2  14 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1259 relchecks			21 0  2  15 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1259 reltriggers			21 0  2  16 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1259 relukeys				21 0  2  17 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1259 relfkeys				21 0  2  18 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1259 relrefs				21 0  2  19 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1259 reloverloadtype		21 0  2  20 0 -1 -1 t p f s t f f t 0));
DATA(insert ( 1259 reloverloadparam1	23 0  4  21 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reloverloadparam2	23 0  4  22 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reloverloadparam3	23 0  4  23 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reloverloadparam4	23 0  4  24 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reloverloadparam5	23 0  4  25 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reloverloadparam6	23 0  4  26 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reloverloadparam7	23 0  4  27 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 reloverloadparam8	23 0  4  28 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 relhasoids			16 0  1  29 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1259 relhaspkey			16 0  1  30 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1259 relhasrules			16 0  1  31 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1259 relhassubclass		16 0  1  32 0 -1 -1 t p f c t f f t 0));
DATA(insert ( 1259 relacl			  1034 0 -1  33 0 -1 -1 f x f i f f f t 0));
DATA(insert ( 1259 ctid					27 0  6  -1 0 -1 -1 f p f i t f f t 0));
DATA(insert ( 1259 oid					26 0  4  -2 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 xmin					28 0  4  -3 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 cmin					29 0  4  -4 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 xmax					28 0  4  -5 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 cmax					29 0  4  -6 0 -1 -1 t p f i t f f t 0));
DATA(insert ( 1259 tableoid				26 0  4  -7 0 -1 -1 t p f i t f f t 0));

/* ----------------
 *		pg_xactlock - this is not a real relation, but is a placeholder
 *				  to allow a relation OID to be used for transaction
 *				  waits.  We need a pg_xactlock entry in pg_class only to
 *				  ensure that that OID can never be allocated to a real
 *				  table; and this entry is just to link to that one.
 * ----------------
 */
DATA(insert ( 376 xactlockfoo		26 0  4   1 0 -1 -1 t p f i t f f t 0));

#endif   /* PG_ATTRIBUTE_H */
