/*-------------------------------------------------------------------------
 *
 * libpq_be.h
 *	  This file contains definitions for structures and externs used
 *	  by the postmaster during client authentication.
 *
 *	  Note that this is backend-internal and is NOT exported to clients.
 *	  Structs that need to be client-visible are in pqcomm.h.
 *
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: libpq-be.h,v 1.1.1.2 2003/04/17 23:02:16 sailesh Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef LIBPQ_BE_H
#define LIBPQ_BE_H

#include <sys/types.h>

#include "libpq/hba.h"
#include "libpq/pqcomm.h"

#ifdef USE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif


/*
 * This is used by the postmaster in its communication with frontends.	It
 * contains all state information needed during this communication before the
 * backend is run.
 */

typedef struct Port
{
	int			sock;			/* File descriptor */
	SockAddr	laddr;			/* local addr (postmaster) */
	SockAddr	raddr;			/* remote addr (client) */
	char		md5Salt[4];		/* Password salt */
	char		cryptSalt[2];	/* Password salt */

	/*
	 * Information that needs to be held during the fe/be authentication
	 * handshake.
	 */

	ProtocolVersion proto;
	char		database[SM_DATABASE + 1];
	char		user[SM_DATABASE_USER + 1];
	char		options[SM_OPTIONS + 1];
	char		tty[SM_TTY + 1];
	char		auth_arg[MAX_AUTH_ARG];
	UserAuth	auth_method;

	/*
	 * SSL structures
	 */
#ifdef USE_SSL
	SSL		   *ssl;
	X509	   *peer;
	char		peer_dn[128 + 1];
	char		peer_cn[SM_USER + 1];
	unsigned long count;
#endif
} Port;


extern ProtocolVersion FrontendProtocol;

#endif   /* LIBPQ_BE_H */
