/* $Id: urlcheck.h,v 1.15 2012/09/20 08:31:49 manu Exp $ */

/*
 * Copyright (c) 2006-2007 Emmanuel Dreyfus
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

#include <curl/curl.h>
#include "prop.h"

struct urlcheck_cnx {
	CURL *uc_hdl;
	int uc_pipe_req[2];
	int uc_pipe_rep[2];
	pid_t uc_pid;
	time_t uc_old;
	pthread_mutex_t uc_lock;
};

struct urlcheck_entry {
	char u_name[QSTRLEN + 1];
	char u_url[QSTRLEN + 1];
	int u_maxcnx;
	int u_flags;
	struct urlcheck_cnx *u_cnxpool;
	LIST_ENTRY(urlcheck_entry) u_list;
};

/* For u_flags */
#define U_POSTMSG	0x01
#define U_GETPROP	0x02
#define U_CLEARPROP	0x04
#define U_FORK		0x10
#define U_NOENCODE	0x20

#define URLCHECK_HELPER_TIMEOUT 60

extern int urlcheck_gflags;

struct urlcheck_entry *urlcheck_byname(char *);
void urlcheck_init(void);
void urlcheck_def_add(char *, char *, int, int);
void urlcheck_clear(void);
int urlcheck_validate(acl_data_t *, acl_stage_t,
		      struct acl_param *, struct mlfi_priv *);
