.\\" auto-generated by docbook2man-spec $Revision: 1.25 $
.TH "CREATE SCHEMA" "l" "2002-11-22" "SQL - Language Statements" "SQL Commands"
.SH NAME
CREATE SCHEMA \- define a new schema
.SH SYNOPSIS
.sp
.nf
CREATE SCHEMA \fIschemaname\fR [ AUTHORIZATION \fIusername\fR ] [ \fIschema_element\fR [ ... ] ]
CREATE SCHEMA AUTHORIZATION \fIusername\fR [ \fIschema_element\fR [ ... ] ]
  
.sp
.fi
.SS "INPUTS"
.PP
.TP
\fB\fIschemaname\fB\fR
The name of a schema to be created. If this is omitted, the user name
is used as the schema name.
.TP
\fB\fIusername\fB\fR
The name of the user who will own the schema. If omitted,
defaults to the user executing the command. Only superusers
may create schemas owned by users other than themselves.
.TP
\fB\fIschema_element\fB\fR
An SQL statement defining an object to be created within the schema.
Currently, only \fBCREATE TABLE\fR, \fBCREATE VIEW\fR, 
and \fBGRANT\fR are accepted as clauses within
\fBCREATE SCHEMA\fR. Other kinds of objects may be created
in separate commands after the schema is created.
.PP
.SS "OUTPUTS"
.PP
.TP
\fBCREATE SCHEMA\fR
Message returned if the command is successful.
.TP
\fBERROR: namespace "\fIschemaname\fB" already exists\fR
If the schema specified already exists.
.PP
.SH "DESCRIPTION"
.PP
\fBCREATE SCHEMA\fR will enter a new schema
into the current database.
The schema name must be distinct from the name of any existing schema
in the current database.
.PP
A schema is essentially a namespace:
it contains named objects (tables, data types, functions, and operators)
whose names may duplicate those of other objects existing in other
schemas. Named objects are accessed either by ``qualifying''
their names with the schema name as a prefix, or by setting a search
path that includes the desired schema(s).
.PP
Optionally, \fBCREATE SCHEMA\fR can include subcommands
to create objects within the new schema. The subcommands are treated
essentially the same as separate commands issued after creating the
schema, except that if the AUTHORIZATION clause is used,
all the created objects will be owned by that user.
.SS "NOTES"
.PP
To create a schema, the invoking user must have CREATE
privilege for the current database. (Of course, superusers bypass
this check.)
.PP
Use \fBDROP SCHEMA\fR to remove a schema.
.SH "EXAMPLES"
.PP
Create a schema:
.sp
.nf
CREATE SCHEMA myschema;
  
.sp
.fi
.PP
Create a schema for user joe --- the schema will also
be named joe:
.sp
.nf
CREATE SCHEMA AUTHORIZATION joe;
  
.sp
.fi
.PP
Create a schema and create a table and view within it:
.sp
.nf
CREATE SCHEMA hollywood
    CREATE TABLE films (title text, release date, awards text[])
    CREATE VIEW winners AS
        SELECT title, release FROM films WHERE awards IS NOT NULL;
  
.sp
.fi
Notice that the individual subcommands do not end with semicolons.
.PP
The following is an equivalent way of accomplishing the same result:
.sp
.nf
CREATE SCHEMA hollywood;
CREATE TABLE hollywood.films (title text, release date, awards text[]);
CREATE VIEW hollywood.winners AS
    SELECT title, release FROM hollywood.films WHERE awards IS NOT NULL;
  
.sp
.fi
.SH "COMPATIBILITY"
.SS "SQL92"
.PP
SQL92 allows a DEFAULT CHARACTER SET clause in
\fBCREATE SCHEMA\fR, as well as more subcommand types
than are presently accepted by PostgreSQL.
.PP
SQL92 specifies that the subcommands in \fBCREATE SCHEMA\fR
may appear in any order. The present
PostgreSQL implementation does not handle all
cases of forward references in subcommands; it may sometimes be necessary
to reorder the subcommands to avoid forward references.
.PP
In SQL92, the owner of a schema always owns all objects within it.
PostgreSQL allows schemas to contain objects
owned by users other than the schema owner. This can happen only if the
schema owner grants CREATE rights on his schema to someone
else.