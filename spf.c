/* $Id: spf.c,v 1.2 2004/03/30 14:17:47 manu Exp $ */

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

#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#ifdef __RCSID
__RCSID("$Id");
#endif
#endif

#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "spf.h"
#include "except.h"

#ifdef HAVE_SPF
int
spf_check(in, from)
	struct in_addr *in;
	char *from;
{
	return EXF_NONE;	/* Unimplemented */
}
#endif /* HAVE_SPF */


#ifdef HAVE_SPF_ALT
#include <spf_alt/spf.h>
#include <spf_alt/spf_dns_resolv.h>

SPF_config_t spfconf = NULL;
SPF_dns_config_t dnsconf = NULL;

int
spf_alt_check(in, from)
	struct in_addr *in;
	char *from;
{
	char addr[IPADDRLEN + 1];
	SPF_output_t out;
	int result = EXF_NONE;

	if (spfconf == NULL)
		spfconf = SPF_create_config();
	if (dnsconf == NULL)
		dnsconf = SPF_dns_create_config_resolv(NULL, 0);

	if ((spfconf == NULL) || (dnsconf == NULL)) {
		syslog(LOG_ERR, "spf_alt_check init failed");
		return EXF_NONE;
	}

	if (SPF_set_ip_str(spfconf, addr) != 0) {
		syslog(LOG_ERR, "SPF_set_ip_str failed");
		return EXF_NONE;
	}

	if (SPF_set_env_from(spfconf, from) != 0) {
		syslog(LOG_ERR, "SPF_set_env_from failed");
		return EXF_NONE;
	}

	SPF_init_output(&out);
	out = SPF_result(spfconf, dnsconf, NULL);

	if (out.result == SPF_RESULT_PASS) 
		result = EXF_SPF;

	SPF_reset_config(spfconf);
	SPF_free_output(&out);

	return result;
}

#endif /* HAVE_SPF_ALT */