.\\" auto-generated by docbook2man-spec $Revision: 1.25 $
.TH "DROP OPERATOR" "l" "2002-11-22" "SQL - Language Statements" "SQL Commands"
.SH NAME
DROP OPERATOR \- remove a user-defined operator
.SH SYNOPSIS
.sp
.nf
DROP OPERATOR \fIid\fR ( \fIlefttype\fR | NONE , \fIrighttype\fR | NONE ) [ CASCADE | RESTRICT ]
  
.sp
.fi
.SS "INPUTS"
.PP
.TP
\fB\fIid\fB\fR
The identifier (optionally schema-qualified) of an existing operator.
.TP
\fB\fIlefttype\fB\fR
The type of the operator's left argument; write NONE if the
operator has no left argument.
.TP
\fB\fIrighttype\fB\fR
The type of the operator's right argument; write NONE if the
operator has no right argument.
.TP
\fBCASCADE\fR
Automatically drop objects that depend on the operator.
.TP
\fBRESTRICT\fR
Refuse to drop the operator if there are any dependent objects.
This is the default.
.PP
.SS "OUTPUTS"
.PP
.TP
\fBDROP OPERATOR\fR
The message returned if the command is successful.
.TP
\fBERROR: RemoveOperator: binary operator '\fIoper\fB' taking '\fIlefttype\fB' and '\fIrighttype\fB' does not exist\fR
This message occurs if the specified binary operator does not exist.
.TP
\fBERROR: RemoveOperator: left unary operator '\fIoper\fB' taking '\fIlefttype\fB' does not exist\fR
This message occurs if the left unary operator
specified does not exist.
.TP
\fBERROR: RemoveOperator: right unary operator '\fIoper\fB' taking '\fIrighttype\fB' does not exist\fR
This message occurs if the right unary operator
specified does not exist.
.PP
.SH "DESCRIPTION"
.PP
\fBDROP OPERATOR\fR drops an existing operator from the
database.
To execute this command you must be the owner of the operator.
.PP
The left or right type of a left or right unary
operator, respectively, must be specified as NONE.
.SS "NOTES"
.PP
The \fBDROP OPERATOR\fR statement is a 
PostgreSQL
language extension.
.PP
Refer to
CREATE OPERATOR [\fBcreate_operator\fR(l)]
for information on how to create operators.
.SH "USAGE"
.PP
Remove power operator a^n for int4:
.sp
.nf
DROP OPERATOR ^ (int4, int4);
   
.sp
.fi
.PP
Remove left unary negation operator (! b) for \fBboolean\fR:
.sp
.nf
DROP OPERATOR ! (none, bool);
   
.sp
.fi
.PP
Remove right unary factorial operator (i !) for
int4:
.sp
.nf
DROP OPERATOR ! (int4, none);
   
.sp
.fi
.SH "COMPATIBILITY"
.SS "SQL92"
.PP
There is no \fBDROP OPERATOR\fR in SQL92.