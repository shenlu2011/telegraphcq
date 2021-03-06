/* A Bison parser, made by GNU Bison 1.875.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     SQL_ALLOCATE = 258,
     SQL_AUTOCOMMIT = 259,
     SQL_BOOL = 260,
     SQL_BREAK = 261,
     SQL_CALL = 262,
     SQL_CARDINALITY = 263,
     SQL_CONNECT = 264,
     SQL_CONNECTION = 265,
     SQL_CONTINUE = 266,
     SQL_COUNT = 267,
     SQL_CURRENT = 268,
     SQL_DATA = 269,
     SQL_DATETIME_INTERVAL_CODE = 270,
     SQL_DATETIME_INTERVAL_PRECISION = 271,
     SQL_DESCRIPTOR = 272,
     SQL_DISCONNECT = 273,
     SQL_ENUM = 274,
     SQL_FOUND = 275,
     SQL_FREE = 276,
     SQL_GO = 277,
     SQL_GOTO = 278,
     SQL_IDENTIFIED = 279,
     SQL_INDICATOR = 280,
     SQL_KEY_MEMBER = 281,
     SQL_LENGTH = 282,
     SQL_LONG = 283,
     SQL_NAME = 284,
     SQL_NULLABLE = 285,
     SQL_OCTET_LENGTH = 286,
     SQL_OPEN = 287,
     SQL_RELEASE = 288,
     SQL_REFERENCE = 289,
     SQL_RETURNED_LENGTH = 290,
     SQL_RETURNED_OCTET_LENGTH = 291,
     SQL_SCALE = 292,
     SQL_SECTION = 293,
     SQL_SHORT = 294,
     SQL_SIGNED = 295,
     SQL_SQL = 296,
     SQL_SQLERROR = 297,
     SQL_SQLPRINT = 298,
     SQL_SQLWARNING = 299,
     SQL_START = 300,
     SQL_STOP = 301,
     SQL_STRUCT = 302,
     SQL_UNSIGNED = 303,
     SQL_VALUE = 304,
     SQL_VAR = 305,
     SQL_WHENEVER = 306,
     S_ADD = 307,
     S_AND = 308,
     S_ANYTHING = 309,
     S_AUTO = 310,
     S_CONST = 311,
     S_DEC = 312,
     S_DIV = 313,
     S_DOTPOINT = 314,
     S_EQUAL = 315,
     S_EXTERN = 316,
     S_INC = 317,
     S_LSHIFT = 318,
     S_MEMPOINT = 319,
     S_MEMBER = 320,
     S_MOD = 321,
     S_MUL = 322,
     S_NEQUAL = 323,
     S_OR = 324,
     S_REGISTER = 325,
     S_RSHIFT = 326,
     S_STATIC = 327,
     S_SUB = 328,
     S_VOLATILE = 329,
     S_TYPEDEF = 330,
     TYPECAST = 331,
     ABORT_TRANS = 332,
     ABSOLUTE = 333,
     ACCESS = 334,
     ACTION = 335,
     ADD = 336,
     AFTER = 337,
     AGGREGATE = 338,
     ALL = 339,
     ALTER = 340,
     ANALYSE = 341,
     ANALYZE = 342,
     AND = 343,
     ANY = 344,
     AS = 345,
     ASC = 346,
     ASSERTION = 347,
     ASSIGNMENT = 348,
     AT = 349,
     AUTHORIZATION = 350,
     BACKWARD = 351,
     BEFORE = 352,
     BEGIN_TRANS = 353,
     BETWEEN = 354,
     BIGINT = 355,
     BINARY = 356,
     BIT = 357,
     BOTH = 358,
     BOOLEAN = 359,
     BY = 360,
     CACHE = 361,
     CALLED = 362,
     CASCADE = 363,
     CASE = 364,
     CAST = 365,
     CHAIN = 366,
     CHAR_P = 367,
     CHARACTER = 368,
     CHARACTERISTICS = 369,
     CHECK = 370,
     CHECKPOINT = 371,
     CLASS = 372,
     CLOSE = 373,
     CLUSTER = 374,
     COALESCE = 375,
     COLLATE = 376,
     COLUMN = 377,
     COMMENT = 378,
     COMMIT = 379,
     COMMITTED = 380,
     CONSTRAINT = 381,
     CONSTRAINTS = 382,
     CONVERSION_P = 383,
     CONVERT = 384,
     COPY = 385,
     CREATE = 386,
     CREATEDB = 387,
     CREATEUSER = 388,
     CROSS = 389,
     CURRENT_DATE = 390,
     CURRENT_TIME = 391,
     CURRENT_TIMESTAMP = 392,
     CURRENT_USER = 393,
     CURSOR = 394,
     CYCLE = 395,
     DATABASE = 396,
     DAY_P = 397,
     DEALLOCATE = 398,
     DEC = 399,
     DECIMAL = 400,
     DECLARE = 401,
     DEFAULT = 402,
     DEFERRABLE = 403,
     DEFERRED = 404,
     DEFINER = 405,
     DELETE_P = 406,
     DELIMITER = 407,
     DELIMITERS = 408,
     DESC = 409,
     DISTINCT = 410,
     DO = 411,
     DOMAIN_P = 412,
     DOUBLE = 413,
     DROP = 414,
     EACH = 415,
     ELSE = 416,
     ENCODING = 417,
     ENCRYPTED = 418,
     END_TRANS = 419,
     ESCAPE = 420,
     EXCEPT = 421,
     EXCLUSIVE = 422,
     EXECUTE = 423,
     EXISTS = 424,
     EXPLAIN = 425,
     EXTERNAL = 426,
     EXTRACT = 427,
     FALSE_P = 428,
     FETCH = 429,
     FLOAT_P = 430,
     FOR = 431,
     FORCE = 432,
     FOREIGN = 433,
     FORWARD = 434,
     FREEZE = 435,
     FROM = 436,
     FULL = 437,
     FUNCTION = 438,
     GET = 439,
     GLOBAL = 440,
     GRANT = 441,
     GROUP_P = 442,
     HANDLER = 443,
     HAVING = 444,
     HOUR_P = 445,
     ILIKE = 446,
     IMMEDIATE = 447,
     IMMUTABLE = 448,
     IMPLICIT_P = 449,
     IN_P = 450,
     INCREMENT = 451,
     INDEX = 452,
     INHERITS = 453,
     INITIALLY = 454,
     INNER_P = 455,
     INOUT = 456,
     INPUT = 457,
     INSENSITIVE = 458,
     INSERT = 459,
     INSTEAD = 460,
     INT = 461,
     INTEGER = 462,
     INTERSECT = 463,
     INTERVAL = 464,
     INTO = 465,
     INVOKER = 466,
     IS = 467,
     ISNULL = 468,
     ISOLATION = 469,
     JOIN = 470,
     KEY = 471,
     LANCOMPILER = 472,
     LANGUAGE = 473,
     LEADING = 474,
     LEFT = 475,
     LEVEL = 476,
     LIKE = 477,
     LIMIT = 478,
     LISTEN = 479,
     LOAD = 480,
     LOCAL = 481,
     LOCATION = 482,
     LOCK_P = 483,
     MATCH = 484,
     MAXVALUE = 485,
     MINUTE_P = 486,
     MINVALUE = 487,
     MODE = 488,
     MONTH_P = 489,
     MOVE = 490,
     NAMES = 491,
     NATIONAL = 492,
     NATURAL = 493,
     NCHAR = 494,
     NEW = 495,
     NEXT = 496,
     NO = 497,
     NOCREATEDB = 498,
     NOCREATEUSER = 499,
     NONE = 500,
     NOT = 501,
     NOTHING = 502,
     NOTIFY = 503,
     NOTNULL = 504,
     NULL_P = 505,
     NULLIF = 506,
     NUMERIC = 507,
     OF = 508,
     OFF = 509,
     OFFSET = 510,
     OIDS = 511,
     OLD = 512,
     ON = 513,
     ONLY = 514,
     OPERATOR = 515,
     OPTION = 516,
     OR = 517,
     ORDER = 518,
     OUT_P = 519,
     OUTER_P = 520,
     OVERLAPS = 521,
     OVERLAY = 522,
     OWNER = 523,
     PARTIAL = 524,
     PASSWORD = 525,
     PATH_P = 526,
     PENDANT = 527,
     PLACING = 528,
     POSITION = 529,
     PRECISION = 530,
     PREPARE = 531,
     PRIMARY = 532,
     PRIOR = 533,
     PRIVILEGES = 534,
     PROCEDURE = 535,
     PROCEDURAL = 536,
     READ = 537,
     REAL = 538,
     RECHECK = 539,
     REFERENCES = 540,
     REINDEX = 541,
     RELATIVE = 542,
     RENAME = 543,
     REPLACE = 544,
     RESET = 545,
     RESTRICT = 546,
     RETURNS = 547,
     REVOKE = 548,
     RIGHT = 549,
     ROLLBACK = 550,
     ROW = 551,
     RULE = 552,
     SCHEMA = 553,
     SCROLL = 554,
     SECOND_P = 555,
     SECURITY = 556,
     SELECT = 557,
     SEQUENCE = 558,
     SERIALIZABLE = 559,
     SESSION = 560,
     SESSION_USER = 561,
     SET = 562,
     SETOF = 563,
     SHARE = 564,
     SHOW = 565,
     SIMILAR = 566,
     SIMPLE = 567,
     SMALLINT = 568,
     SOME = 569,
     STABLE = 570,
     START = 571,
     STATEMENT = 572,
     STATISTICS = 573,
     STDIN = 574,
     STDOUT = 575,
     STORAGE = 576,
     STRICT = 577,
     SUBSTRING = 578,
     SYSID = 579,
     TABLE = 580,
     TEMP = 581,
     TEMPLATE = 582,
     TEMPORARY = 583,
     THEN = 584,
     TIME = 585,
     TIMESTAMP = 586,
     TO = 587,
     TOAST = 588,
     TRAILING = 589,
     TRANSACTION = 590,
     TREAT = 591,
     TRIGGER = 592,
     TRIM = 593,
     TRUE_P = 594,
     TRUNCATE = 595,
     TRUSTED = 596,
     TYPE_P = 597,
     UNENCRYPTED = 598,
     UNION = 599,
     UNIQUE = 600,
     UNKNOWN = 601,
     UNLISTEN = 602,
     UNTIL = 603,
     UPDATE = 604,
     USAGE = 605,
     USER = 606,
     USING = 607,
     VACUUM = 608,
     VALID = 609,
     VALUES = 610,
     VARCHAR = 611,
     VARYING = 612,
     VERBOSE = 613,
     VERSION = 614,
     VIEW = 615,
     VOLATILE = 616,
     WHEN = 617,
     WHERE = 618,
     WITH = 619,
     WITHOUT = 620,
     WORK = 621,
     WRITE = 622,
     YEAR_P = 623,
     ZONE = 624,
     UNIONJOIN = 625,
     IDENT = 626,
     SCONST = 627,
     Op = 628,
     CSTRING = 629,
     CVARIABLE = 630,
     CPP_LINE = 631,
     IP = 632,
     BCONST = 633,
     XCONST = 634,
     ICONST = 635,
     PARAM = 636,
     FCONST = 637,
     POSTFIXOP = 638,
     UMINUS = 639
   };
#endif
#define SQL_ALLOCATE 258
#define SQL_AUTOCOMMIT 259
#define SQL_BOOL 260
#define SQL_BREAK 261
#define SQL_CALL 262
#define SQL_CARDINALITY 263
#define SQL_CONNECT 264
#define SQL_CONNECTION 265
#define SQL_CONTINUE 266
#define SQL_COUNT 267
#define SQL_CURRENT 268
#define SQL_DATA 269
#define SQL_DATETIME_INTERVAL_CODE 270
#define SQL_DATETIME_INTERVAL_PRECISION 271
#define SQL_DESCRIPTOR 272
#define SQL_DISCONNECT 273
#define SQL_ENUM 274
#define SQL_FOUND 275
#define SQL_FREE 276
#define SQL_GO 277
#define SQL_GOTO 278
#define SQL_IDENTIFIED 279
#define SQL_INDICATOR 280
#define SQL_KEY_MEMBER 281
#define SQL_LENGTH 282
#define SQL_LONG 283
#define SQL_NAME 284
#define SQL_NULLABLE 285
#define SQL_OCTET_LENGTH 286
#define SQL_OPEN 287
#define SQL_RELEASE 288
#define SQL_REFERENCE 289
#define SQL_RETURNED_LENGTH 290
#define SQL_RETURNED_OCTET_LENGTH 291
#define SQL_SCALE 292
#define SQL_SECTION 293
#define SQL_SHORT 294
#define SQL_SIGNED 295
#define SQL_SQL 296
#define SQL_SQLERROR 297
#define SQL_SQLPRINT 298
#define SQL_SQLWARNING 299
#define SQL_START 300
#define SQL_STOP 301
#define SQL_STRUCT 302
#define SQL_UNSIGNED 303
#define SQL_VALUE 304
#define SQL_VAR 305
#define SQL_WHENEVER 306
#define S_ADD 307
#define S_AND 308
#define S_ANYTHING 309
#define S_AUTO 310
#define S_CONST 311
#define S_DEC 312
#define S_DIV 313
#define S_DOTPOINT 314
#define S_EQUAL 315
#define S_EXTERN 316
#define S_INC 317
#define S_LSHIFT 318
#define S_MEMPOINT 319
#define S_MEMBER 320
#define S_MOD 321
#define S_MUL 322
#define S_NEQUAL 323
#define S_OR 324
#define S_REGISTER 325
#define S_RSHIFT 326
#define S_STATIC 327
#define S_SUB 328
#define S_VOLATILE 329
#define S_TYPEDEF 330
#define TYPECAST 331
#define ABORT_TRANS 332
#define ABSOLUTE 333
#define ACCESS 334
#define ACTION 335
#define ADD 336
#define AFTER 337
#define AGGREGATE 338
#define ALL 339
#define ALTER 340
#define ANALYSE 341
#define ANALYZE 342
#define AND 343
#define ANY 344
#define AS 345
#define ASC 346
#define ASSERTION 347
#define ASSIGNMENT 348
#define AT 349
#define AUTHORIZATION 350
#define BACKWARD 351
#define BEFORE 352
#define BEGIN_TRANS 353
#define BETWEEN 354
#define BIGINT 355
#define BINARY 356
#define BIT 357
#define BOTH 358
#define BOOLEAN 359
#define BY 360
#define CACHE 361
#define CALLED 362
#define CASCADE 363
#define CASE 364
#define CAST 365
#define CHAIN 366
#define CHAR_P 367
#define CHARACTER 368
#define CHARACTERISTICS 369
#define CHECK 370
#define CHECKPOINT 371
#define CLASS 372
#define CLOSE 373
#define CLUSTER 374
#define COALESCE 375
#define COLLATE 376
#define COLUMN 377
#define COMMENT 378
#define COMMIT 379
#define COMMITTED 380
#define CONSTRAINT 381
#define CONSTRAINTS 382
#define CONVERSION_P 383
#define CONVERT 384
#define COPY 385
#define CREATE 386
#define CREATEDB 387
#define CREATEUSER 388
#define CROSS 389
#define CURRENT_DATE 390
#define CURRENT_TIME 391
#define CURRENT_TIMESTAMP 392
#define CURRENT_USER 393
#define CURSOR 394
#define CYCLE 395
#define DATABASE 396
#define DAY_P 397
#define DEALLOCATE 398
#define DEC 399
#define DECIMAL 400
#define DECLARE 401
#define DEFAULT 402
#define DEFERRABLE 403
#define DEFERRED 404
#define DEFINER 405
#define DELETE_P 406
#define DELIMITER 407
#define DELIMITERS 408
#define DESC 409
#define DISTINCT 410
#define DO 411
#define DOMAIN_P 412
#define DOUBLE 413
#define DROP 414
#define EACH 415
#define ELSE 416
#define ENCODING 417
#define ENCRYPTED 418
#define END_TRANS 419
#define ESCAPE 420
#define EXCEPT 421
#define EXCLUSIVE 422
#define EXECUTE 423
#define EXISTS 424
#define EXPLAIN 425
#define EXTERNAL 426
#define EXTRACT 427
#define FALSE_P 428
#define FETCH 429
#define FLOAT_P 430
#define FOR 431
#define FORCE 432
#define FOREIGN 433
#define FORWARD 434
#define FREEZE 435
#define FROM 436
#define FULL 437
#define FUNCTION 438
#define GET 439
#define GLOBAL 440
#define GRANT 441
#define GROUP_P 442
#define HANDLER 443
#define HAVING 444
#define HOUR_P 445
#define ILIKE 446
#define IMMEDIATE 447
#define IMMUTABLE 448
#define IMPLICIT_P 449
#define IN_P 450
#define INCREMENT 451
#define INDEX 452
#define INHERITS 453
#define INITIALLY 454
#define INNER_P 455
#define INOUT 456
#define INPUT 457
#define INSENSITIVE 458
#define INSERT 459
#define INSTEAD 460
#define INT 461
#define INTEGER 462
#define INTERSECT 463
#define INTERVAL 464
#define INTO 465
#define INVOKER 466
#define IS 467
#define ISNULL 468
#define ISOLATION 469
#define JOIN 470
#define KEY 471
#define LANCOMPILER 472
#define LANGUAGE 473
#define LEADING 474
#define LEFT 475
#define LEVEL 476
#define LIKE 477
#define LIMIT 478
#define LISTEN 479
#define LOAD 480
#define LOCAL 481
#define LOCATION 482
#define LOCK_P 483
#define MATCH 484
#define MAXVALUE 485
#define MINUTE_P 486
#define MINVALUE 487
#define MODE 488
#define MONTH_P 489
#define MOVE 490
#define NAMES 491
#define NATIONAL 492
#define NATURAL 493
#define NCHAR 494
#define NEW 495
#define NEXT 496
#define NO 497
#define NOCREATEDB 498
#define NOCREATEUSER 499
#define NONE 500
#define NOT 501
#define NOTHING 502
#define NOTIFY 503
#define NOTNULL 504
#define NULL_P 505
#define NULLIF 506
#define NUMERIC 507
#define OF 508
#define OFF 509
#define OFFSET 510
#define OIDS 511
#define OLD 512
#define ON 513
#define ONLY 514
#define OPERATOR 515
#define OPTION 516
#define OR 517
#define ORDER 518
#define OUT_P 519
#define OUTER_P 520
#define OVERLAPS 521
#define OVERLAY 522
#define OWNER 523
#define PARTIAL 524
#define PASSWORD 525
#define PATH_P 526
#define PENDANT 527
#define PLACING 528
#define POSITION 529
#define PRECISION 530
#define PREPARE 531
#define PRIMARY 532
#define PRIOR 533
#define PRIVILEGES 534
#define PROCEDURE 535
#define PROCEDURAL 536
#define READ 537
#define REAL 538
#define RECHECK 539
#define REFERENCES 540
#define REINDEX 541
#define RELATIVE 542
#define RENAME 543
#define REPLACE 544
#define RESET 545
#define RESTRICT 546
#define RETURNS 547
#define REVOKE 548
#define RIGHT 549
#define ROLLBACK 550
#define ROW 551
#define RULE 552
#define SCHEMA 553
#define SCROLL 554
#define SECOND_P 555
#define SECURITY 556
#define SELECT 557
#define SEQUENCE 558
#define SERIALIZABLE 559
#define SESSION 560
#define SESSION_USER 561
#define SET 562
#define SETOF 563
#define SHARE 564
#define SHOW 565
#define SIMILAR 566
#define SIMPLE 567
#define SMALLINT 568
#define SOME 569
#define STABLE 570
#define START 571
#define STATEMENT 572
#define STATISTICS 573
#define STDIN 574
#define STDOUT 575
#define STORAGE 576
#define STRICT 577
#define SUBSTRING 578
#define SYSID 579
#define TABLE 580
#define TEMP 581
#define TEMPLATE 582
#define TEMPORARY 583
#define THEN 584
#define TIME 585
#define TIMESTAMP 586
#define TO 587
#define TOAST 588
#define TRAILING 589
#define TRANSACTION 590
#define TREAT 591
#define TRIGGER 592
#define TRIM 593
#define TRUE_P 594
#define TRUNCATE 595
#define TRUSTED 596
#define TYPE_P 597
#define UNENCRYPTED 598
#define UNION 599
#define UNIQUE 600
#define UNKNOWN 601
#define UNLISTEN 602
#define UNTIL 603
#define UPDATE 604
#define USAGE 605
#define USER 606
#define USING 607
#define VACUUM 608
#define VALID 609
#define VALUES 610
#define VARCHAR 611
#define VARYING 612
#define VERBOSE 613
#define VERSION 614
#define VIEW 615
#define VOLATILE 616
#define WHEN 617
#define WHERE 618
#define WITH 619
#define WITHOUT 620
#define WORK 621
#define WRITE 622
#define YEAR_P 623
#define ZONE 624
#define UNIONJOIN 625
#define IDENT 626
#define SCONST 627
#define Op 628
#define CSTRING 629
#define CVARIABLE 630
#define CPP_LINE 631
#define IP 632
#define BCONST 633
#define XCONST 634
#define ICONST 635
#define PARAM 636
#define FCONST 637
#define POSTFIXOP 638
#define UMINUS 639




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 139 "preproc.y"
typedef union YYSTYPE {
	double	dval;
	int		ival;
	char	*str;
	struct	when		action;
	struct	index		index;
	int		tagname;
	struct	this_type	type;
	enum	ECPGttype	type_enum;
	enum	ECPGdtype	dtype_enum;
	struct	fetch_desc	descriptor;
} YYSTYPE;
/* Line 1240 of yacc.c.  */
#line 817 "y.tab.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



