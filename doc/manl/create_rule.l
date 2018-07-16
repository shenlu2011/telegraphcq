.\\" auto-generated by docbook2man-spec $Revision: 1.25 $
.TH "CREATE RULE" "l" "2002-11-22" "SQL - Language Statements" "SQL Commands"
.SH NAME
CREATE RULE \- define a new rewrite rule
.SH SYNOPSIS
.sp
.nf
CREATE [ OR REPLACE ] RULE \fIname\fR AS ON \fIevent\fR
    TO \fItable\fR [ WHERE \fIcondition\fR ]
    DO [ INSTEAD ] \fIaction\fR

where \fIaction\fR can be:

NOTHING
| \fIquery\fR
| ( \fIquery\fR ; \fIquery\fR ... )
  
.sp
.fi
.SS "INPUTS"
.PP
.TP
\fB\fIname\fB\fR
The name of a rule to create. This must be distinct from the name
of any other rule for the same table.
.TP
\fB\fIevent\fB\fR
Event is one of SELECT,
UPDATE, DELETE
or INSERT.
.TP
\fB\fItable\fB\fR
The name (optionally schema-qualified) of the table or view the rule
applies to.
.TP
\fB\fIcondition\fB\fR
Any SQL conditional expression (returning \fBboolean\fR).
The condition expression may not
refer to any tables except new and
old, and may not contain aggregate functions.
.TP
\fB\fIquery\fB\fR
The query or queries making up the
\fIaction\fR
can be any SQL SELECT, INSERT,
UPDATE, DELETE, or
NOTIFY statement.
.PP
.PP
Within the \fIcondition\fR
and \fIaction\fR, the special
table names new and old may be
used to refer to values in the referenced table.
new is valid in ON INSERT and ON UPDATE rules
to refer to the new row being inserted or updated.
old is valid in ON UPDATE and ON DELETE
rules to refer to the existing row being updated or deleted.
.SS "OUTPUTS"
.PP
.TP
\fBCREATE RULE\fR
Message returned if the rule is successfully created.
.PP
.SH "DESCRIPTION"
.PP
\fBCREATE RULE\fR defines a new rule applying to a specified
table or view.
\fBCREATE OR REPLACE RULE\fR will either create a
new rule, or replace an existing rule of the same name for the same
table.
.PP
The PostgreSQL 
\fIrule system\fR allows one to define an
alternate action to be performed on inserts, updates, or deletions
from database tables. Rules are used to
implement table views as well.
.PP
The semantics of a rule is that at the time an individual instance (row)
is
accessed, inserted, updated, or deleted, there is an old instance (for
selects, updates and deletes) and a new instance (for inserts and
updates). All the rules for the given event type and the given target
table are examined successively (in order by name). If the
\fIcondition\fR specified in the
WHERE clause (if any) is true, the 
\fIaction\fR part of the rule is
executed. The \fIaction\fR is
done instead of the original query if INSTEAD is specified; otherwise
it is done after the original query in the case of ON INSERT, or before
the original query in the case of ON UPDATE or ON DELETE.
Within both the \fIcondition\fR
and \fIaction\fR, values from
fields in the old instance and/or the new instance are substituted for
old.\fIattribute-name\fR
and new.\fIattribute-name\fR.
.PP
The \fIaction\fR part of the
rule can consist of one or more queries. To write multiple queries,
surround them with parentheses. Such queries will be performed in the
specified order. The \fIaction\fR can also be NOTHING indicating
no action. Thus, a DO INSTEAD NOTHING rule suppresses the original
query from executing (when its condition is true); a DO NOTHING rule
is useless.
.PP
The \fIaction\fR part of the rule
executes with the same command and transaction identifier as the user
command that caused activation.
.PP
It is important to realize that a rule is really a query transformation
mechanism, or query macro. The entire query is processed to convert it
into a series of queries that include the rule actions. This occurs
before evaluation of the query starts. So, conditional rules are
handled by adding the rule condition to the WHERE clause of the action(s)
derived from the rule. The above description of a rule as an operation
that executes for each row is thus somewhat misleading. If you actually
want an operation that fires independently for each physical row, you
probably want to use a trigger not a rule. Rules are most useful for
situations that call for transforming entire queries independently of
the specific data being handled.
.SS "RULES AND VIEWS"
.PP
Presently, ON SELECT rules must be unconditional INSTEAD rules and must
have actions that consist of a single SELECT query. Thus, an ON SELECT
rule effectively turns the table into a view, whose visible
contents are the rows returned by the rule's SELECT query rather than
whatever had been stored in the table (if anything). It is considered
better style to write a CREATE VIEW command than to create a real table
and define an ON SELECT rule for it.
.PP
CREATE VIEW [\fBcreate_view\fR(l)] creates a dummy table (with no underlying
storage) and associates an ON SELECT rule with it. The system will not
allow updates to the view, since it knows there is no real table there.
You can create the
illusion of an updatable view by defining ON INSERT, ON UPDATE, and
ON DELETE rules (or any subset of those that's sufficient
for your purposes) to replace update actions on the view with
appropriate updates on other tables.
.PP
There is a catch if you try to use conditional
rules for view updates: there \fBmust\fR be an unconditional
INSTEAD rule for each action you wish to allow on the view. If the
rule is conditional, or is not INSTEAD, then the system will still reject
attempts to perform the update action, because it thinks it might end up
trying to perform the action on the dummy table in some cases.
If you want to
handle all the useful cases in conditional rules, you can; just add an
unconditional DO INSTEAD NOTHING rule to ensure that the system
understands it will never be called on to update the dummy table. Then
make the conditional rules non-INSTEAD; in the cases where they fire,
they add to the default INSTEAD NOTHING action.
.SS "NOTES"
.PP
You must have rule definition access to a table in order
to define a rule on it. Use \fBGRANT\fR
and \fBREVOKE\fR to change permissions.
.PP
It is very important to take care to avoid circular rules.
For example, though each
of the following two rule definitions are accepted by
PostgreSQL, the
select command will cause PostgreSQL to 
report an error because the query cycled too many times:
.sp
.nf
CREATE RULE "_RETURN" AS
    ON SELECT TO emp
    DO INSTEAD 
	SELECT * FROM toyemp;

CREATE RULE "_RETURN" AS
    ON SELECT TO toyemp
    DO INSTEAD 
	SELECT * FROM emp;
.sp
.fi
This attempt to select from EMP will cause
PostgreSQL to issue an error
because the queries cycled too many times:
.sp
.nf
SELECT * FROM emp;
.sp
.fi
.PP
Presently, if a rule contains a NOTIFY query, the NOTIFY will be executed
unconditionally --- that is, the NOTIFY will be issued even if there are
not any rows that the rule should apply to. For example, in
.sp
.nf
CREATE RULE notify_me AS ON UPDATE TO mytable DO NOTIFY mytable;

UPDATE mytable SET name = 'foo' WHERE id = 42;
      
.sp
.fi
one NOTIFY event will be sent during the UPDATE, whether or not there
are any rows with id = 42. This is an implementation restriction that
may be fixed in future releases.
.SH "COMPATIBILITY"
.SS "SQL92"
.PP
\fBCREATE RULE\fR is a PostgreSQL
language extension.
There is no \fBCREATE RULE\fR statement in SQL92.