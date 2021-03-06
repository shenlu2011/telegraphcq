.\\" auto-generated by docbook2man-spec $Revision: 1.25 $
.TH "ALTER USER" "l" "2002-11-22" "SQL - Language Statements" "SQL Commands"
.SH NAME
ALTER USER \- change a database user account
.SH SYNOPSIS
.sp
.nf
ALTER USER \fIusername\fR [ [ WITH ] \fIoption\fR [ ... ] ]

where \fIoption\fR can be:

      [ ENCRYPTED | UNENCRYPTED ] PASSWORD '\fIpassword\fR' 
    | CREATEDB | NOCREATEDB
    | CREATEUSER | NOCREATEUSER 
    | VALID UNTIL '\fIabstime\fR'

ALTER USER \fIusername\fR SET \fIvariable\fR { TO | = } { \fIvalue\fR | DEFAULT }
ALTER USER \fIusername\fR RESET \fIvariable\fR
.sp
.fi
.SH "DESCRIPTION"
.PP
\fBALTER USER\fR is used to change the attributes of a
PostgreSQL user account. Attributes not
mentioned in the command retain their previous settings.
.PP
The first variant of this command in the synopsis changes certain
global user privileges and authentication settings. (See below for
details.) Only a database superuser can change privileges and
password expiration with this command. Ordinary users can only
change their own password.
.PP
The second and the third variant change a user's session default for
a specified configuration variable. Whenever the user subsequently
starts a new session, the specified value becomes the session default,
overriding whatever setting is present in \fIpostgresql.conf\fR
or has been received from the postmaster.
Ordinary users can change their own session defaults.
Superusers can change anyone's session defaults.
.SS "PARAMETERS"
.PP
.TP
\fB\fIusername\fB\fR
The name of the user whose attributes are to be altered.
.TP
\fB\fIpassword\fB\fR
The new password to be used for this account.
.TP
\fBENCRYPTED\fR
.TP
\fBUNENCRYPTED\fR
These key words control whether the password is stored
encrypted in pg_shadow. (See
CREATE USER [\fBcreate_user\fR(l)]
for more information about this choice.)
.TP
\fBCREATEDB\fR
.TP
\fBNOCREATEDB\fR
These clauses define a user's ability to create databases. If
CREATEDB is specified, the user being
defined will be allowed to create his own databases. Using
NOCREATEDB will deny a user the ability to
create databases.
.TP
\fBCREATEUSER\fR
.TP
\fBNOCREATEUSER\fR
These clauses determine whether a user will be permitted to
create new users himself. This option will also make the user
a superuser who can override all access restrictions.
.TP
\fB\fIabstime\fB\fR
The date (and, optionally, the time)
at which this user's password is to expire.
.TP
\fB\fIvariable\fB\fR
.TP
\fB\fIvalue\fB\fR
Set this user's session default for the specified configuration
variable to the given value. If
\fIvalue\fR is DEFAULT
or, equivalently, RESET is used, the
user-specific variable setting is removed and the user will
inherit the system-wide default setting in new sessions. Use
RESET ALL to clear all settings.

See SET [\fBset\fR(l)] and the
\fIAdministrator's Guide\fR for more
information about allowed variable names and values.
.PP
.SH "DIAGNOSTICS"
.PP
.TP
\fBALTER USER\fR
Message returned if the alteration was successful.
.TP
\fBERROR: ALTER USER: user "username" does not exist\fR
Error message returned if the specified user is not known to
the database.
.PP
.SH "NOTES"
.PP
Use CREATE USER [\fBcreate_user\fR(l)]
to add new users, and DROP USER [\fBdrop_user\fR(l)] to remove a user.
.PP
\fBALTER USER\fR cannot change a user's group memberships.
Use ALTER GROUP [\fBalter_group\fR(l)]
to do that.
.PP
Using ALTER DATABASE [\fBalter_database\fR(l)], it is also possible to tie a
session default to a specific database rather than a user.
.SH "EXAMPLES"
.PP
Change a user password:
.sp
.nf
ALTER USER davide WITH PASSWORD 'hu8jmn3';
.sp
.fi
.PP
Change a user's valid until date:
.sp
.nf
ALTER USER manuel VALID UNTIL 'Jan 31 2030';
.sp
.fi
.PP
Change a user's valid until date, specifying that his
authorization should expire at midday on 4th May 1998 using
the time zone which is one hour ahead of UTC:
.sp
.nf
ALTER USER chris VALID UNTIL 'May 4 12:00:00 1998 +1';
.sp
.fi
.PP
Give a user the ability to create other users and new databases:
.sp
.nf
ALTER USER miriam CREATEUSER CREATEDB;
.sp
.fi
.SH "COMPATIBILITY"
.PP
The \fBALTER USER\fR statement is a
PostgreSQL extension. The SQL standard
leaves the definition of users to the implementation.
.SH "SEE ALSO"
CREATE USER [\fBcreate_user\fR(l)], DROP USER [\fBdrop_user\fR(l)], SET [\fBset\fR(l)]

