/* $Id: acl.h,v 1.2 2004/12/16 23:08:13 manu Exp $ */

/*
 * Copyright (c) 2004 Emmanuel Dreyfus
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by Emmanuel Dreyfus
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,  
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ACL_H_
#define _ACL_H_

#include "config.h"
#ifdef HAVE_OLD_QUEUE_H
#include "queue.h"
#else 
#include <sys/queue.h>
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <regex.h>

#include "pending.h"
#include "milter-greylist.h"

#define ACL_WRLOCK WRLOCK(acl_lock) 
#define ACL_RDLOCK RDLOCK(acl_lock) 
#define ACL_UNLOCK UNLOCK(acl_lock)

TAILQ_HEAD(acllist, acl_entry);

typedef enum { 
	A_GREYLIST,
	A_WHITELIST
} acl_type_t;

#define a_addr a_netblock.nb_addr
#define a_addrlen a_netblock.nb_addrlen
#define a_mask a_netblock.nb_mask

struct acl_entry {
	acl_type_t a_type;
	struct {
		struct sockaddr *nb_addr;
		socklen_t nb_addrlen;
		ipaddr *nb_mask;
	} a_netblock;
	char *a_from;
	char *a_rcpt;
	char *a_domain;
	regex_t *a_from_re;
	char *a_from_re_copy;
	regex_t *a_rcpt_re;
	char *a_rcpt_re_copy;
	regex_t *a_domain_re;
	char *a_domain_re_copy;
	TAILQ_ENTRY(acl_entry) a_list;
};

extern int testmode;
extern pthread_rwlock_t acl_lock;

void acl_init(void);
void acl_clear(void);
void acl_add_netblock(struct sockaddr *, socklen_t, int);
void acl_add_domain(char *);
void acl_add_domain_regex(char *);
void acl_add_from(char *);
void acl_add_rcpt(char *);
void acl_add_from_regex(char *);
void acl_add_rcpt_regex(char *);
struct acl_entry *acl_register_entry_first (acl_type_t);
struct acl_entry *acl_register_entry_last (acl_type_t);
int acl_filter(struct sockaddr *, socklen_t, char *, char *, char *, char *);
char *acl_entry(struct acl_entry  *);
void acl_dump(void);

/* acl_filter() return codes */
#define	EXF_UNSET	0
#define	EXF_GREYLIST	(1 << 0)
#define EXF_WHITELIST	(1 << 1)

#define	EXF_DEFAULT	(1 << 2)
#define	EXF_ADDR	(1 << 3)
#define	EXF_DOMAIN	(1 << 4)
#define	EXF_FROM	(1 << 5)
#define	EXF_RCPT	(1 << 6)
#define	EXF_AUTO	(1 << 7)
#define	EXF_NONE	(1 << 8)
#define	EXF_AUTH	(1 << 9)
#define	EXF_SPF		(1 << 10)
#define	EXF_NONIPV4	(1 << 11)
#define	EXF_STARTTLS	(1 << 12)
#define EXF_ACCESSDB	(1 << 13)
#endif /* _ACL_H_ */