.\\" auto-generated by docbook2man-spec $Revision: 1.25 $
.TH "PREPARE" "l" "2002-11-22" "SQL - Language Statements" "SQL Commands"
.SH NAME
PREPARE \- create a prepared query
.SH SYNOPSIS
.sp
.nf
   PREPARE \fIplan_name\fR [ (\fIdatatype\fR [, ...] ) ] AS \fIquery\fR
  
.sp
.fi
.SS "INPUTS"
.PP
.TP
\fB\fIplan_name\fB\fR
An arbitrary name given to this particular prepared query. It
must be unique within a single session, and is used to execute
or remove a previously prepared query.
.TP
\fB\fIdatatype\fB\fR
The data-type of a parameter to the prepared query.
To refer to the parameters in the prepared query itself,
use $1, $2, etc.
.PP
.SS "OUTPUTS"
.PP
.TP
\fBPREPARE\fR
The query has been prepared successfully.
.PP
.SH "DESCRIPTION"
.PP
\fBPREPARE\fR creates a prepared query. A prepared
query is a server-side object that can be used to optimize
performance. When the \fBPREPARE\fR statement is
executed, the specified query is parsed, rewritten, and
planned. When a subsequent \fBEXECUTE\fR statement is
issued, the prepared query need only be executed. Thus, the
parsing, rewriting, and planning stages are only performed once,
instead of every time the query is executed.
.PP
Prepared queries can take parameters: values that are
substituted into the query when it is executed. To specify the
parameters to a prepared query, include a list of data-types with
the \fBPREPARE\fR statement. In the query itself, you
can refer to the parameters by position using
$1, $2, etc. When executing
the query, specify the actual values for these parameters in the
\fBEXECUTE\fR statement -- refer to EXECUTE [\fBexecute\fR(l)]
for more information.
.PP
Prepared queries are stored locally (in the current backend), and
only exist for the duration of the current database session. When
the client exits, the prepared query is forgotten, and so it must be
re-created before being used again. This also means that a single
prepared query cannot be used by multiple simultaneous database
clients; however, each client can create their own prepared query
to use.
.PP
Prepared queries have the largest performance advantage when a
single backend is being used to execute a large number of similar
queries. The performance difference will be particularly
significant if the queries are complex to plan or rewrite. For
example, if the query involves a join of many tables or requires
the application of several rules. If the query is relatively simple
to plan and rewrite but relatively expensive to execute, the
performance advantage of prepared queries will be less noticeable.
.SS "NOTES"
.PP
In some situations, the query plan produced by
PostgreSQL for a prepared query may be
inferior to the plan produced if the query were submitted and
executed normally. This is because when the query is planned (and
the optimizer attempts to determine the optimal query plan), the
actual values of any parameters specified in the query are
unavailable. PostgreSQL collects
statistics on the distribution of data in the table, and can use
constant values in a query to make guesses about the likely
result of executing the query. Since this data is unavailable when
planning prepared queries with parameters, the chosen plan may be
sub-optimal.
.PP
For more information on query planning and the statistics
collected by PostgreSQL for query
optimization purposes, see the ANALYZE [\fBanalyze\fR(l)] documentation.
.SH "COMPATIBILITY"
.SS "SQL92"
.PP
SQL92 includes a \fBPREPARE\fR statement, but it is
only for use in embedded SQL clients. The
\fBPREPARE\fR statement implemented by
PostgreSQL also uses a somewhat
different syntax.
