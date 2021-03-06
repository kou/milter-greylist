/* $Id: spf.c,v 1.33 2011/08/17 01:06:50 manu Exp $ */

/*
 * Copyright (c) 2004-2007 Emmanuel Dreyfus
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
__RCSID("$Id: spf.c,v 1.33 2011/08/17 01:06:50 manu Exp $");
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "conf.h"
#include "spf.h"
#include "acl.h"
#include "milter-greylist.h"

#ifdef USE_DMALLOC
#include <dmalloc.h> 
#endif

#if (defined(HAVE_SPF) || defined(HAVE_SPF_ALT) || \
     defined(HAVE_SPF2_10) || defined(HAVE_SPF2))
static int spf_check_internal(acl_data_t *, acl_stage_t, 
			      struct acl_param *, struct mlfi_priv *);
static int spf_check_self(acl_data_t *, acl_stage_t, 
			  struct acl_param *, struct mlfi_priv *);
#endif

#ifdef HAVE_SPF
#include <spf.h>

#ifndef SPF_FALSE
#define SPF_FALSE 0
#endif


static int
spf_check_internal(ad, as, ap, priv)
	acl_data_t *ad;
	acl_stage_t as;
	struct acl_param *ap;
	struct mlfi_priv *priv;
{
	struct sockaddr *sa = SA(&priv->priv_addr);
	socklen_t salen = priv->priv_addrlen;
	char *helo = priv->priv_helo;
	char *fromp = priv->priv_from;
	peer_info_t *p = NULL;
	char addr[IPADDRSTRLEN];
	int result = 0;
	struct timeval tv1, tv2, tv3;
	enum spf_status status;

	if (conf.c_debug)
		gettimeofday(&tv1, NULL);

	if (sa->sa_family != AF_INET)	/* libspf doesn't support IPv6 */
		return result;
	if (!iptostring(sa, salen, addr, sizeof(addr)))
		return result;

	if ((p = SPF_init("milter-greylist", addr, 
	    NULL, NULL, NULL, SPF_FALSE, SPF_FALSE)) == NULL) {
		mg_log(LOG_ERR, "SPF_Init failed");
		goto out1;
	}
	SPF_smtp_helo(p, helo);
	SPF_smtp_from(p, from);
	p->RES = SPF_policy_main(p);

	if (conf.c_debug)
		mg_log(LOG_DEBUG, "SPF return code %d", p->RES);

	status = ad ? *(enum spf_status *)ad : MGSPF_PASS;
	switch (status) {
	case MGSPF_PASS:
		result = (p->RES == SPF_PASS);
		break;
	case MGSPF_FAIL:
		result = (p->RES == SPF_H_FAIL);
		break;
	case MGSPF_SOFTFAIL:
		result = (p->RES == SPF_S_FAIL);
		break;
	case MGSPF_NEUTRAL:
		result = (p->RES == SPF_NEUTRAL);
		break;
	case MGSPF_UNKNOWN:
		result = (p->RES == SPF_UNKNOWN || p->RES == SPF_UNMECH);
		break;
	case MGSPF_ERROR:
		result = (p->RES == SPF_ERROR);
		break;
	case MGSPF_NONE:
		result = (p->RES == SPF_NONE);
		break;
	default:
		mg_log(LOG_ERR, "Internal error: unexpected spf_status");
		exit(EX_SOFTWARE);
		break;
	}

	SPF_close(p);

out1:
	if (conf.c_debug) {
		gettimeofday(&tv2, NULL);
		timersub(&tv2, &tv1, &tv3);
		mg_log(LOG_DEBUG, "SPF lookup performed in %ld.%06lds",  
		    tv3.tv_sec, tv3.tv_usec);
	}
	
	return result;
}
#endif /* HAVE_SPF */


#if defined(HAVE_SPF_ALT) || defined(HAVE_SPF2_10) || defined(HAVE_SPF2)
/* SMTP needs at least 64 chars for local part and 255 for doamin... */
#ifndef NS_MAXDNAME
#define NS_MAXDNAME 1025
#endif
#endif

#ifdef HAVE_SPF_ALT
#include <spf_alt/spf.h>
#include <spf_alt/spf_dns_resolv.h>
#include <spf_alt/spf_lib_version.h>
#endif

#ifdef HAVE_SPF2_10
#include <spf2/spf.h>
#include <spf2/spf_dns_resolv.h>
#include <spf2/spf_lib_version.h>
#endif

#if defined(HAVE_SPF_ALT) || defined(HAVE_SPF2_10)
static int
spf_check_internal(ad, as, ap, priv)
	acl_data_t *ad;
	acl_stage_t as;
	struct acl_param *ap;
	struct mlfi_priv *priv;
{
	struct sockaddr *sa = SA(&priv->priv_addr);
	socklen_t salen = priv->priv_addrlen;
	char *helo = priv->priv_helo;
	char *fromp = priv->priv_from;
	SPF_config_t spfconf;
	SPF_dns_config_t dnsconf;
	char addr[IPADDRSTRLEN];
	char from[NS_MAXDNAME + 1];
	SPF_output_t out;
	int result = 0;
	struct timeval tv1, tv2, tv3;
	size_t len;
	enum spf_status status;

	if (conf.c_debug)
		gettimeofday(&tv1, NULL);

	if ((spfconf = SPF_create_config()) == NULL) {
		mg_log(LOG_ERR, "SPF_create_config failed");
		goto out1;
	}

	if ((dnsconf = SPF_dns_create_config_resolv(NULL, 0)) == NULL) {
		mg_log(LOG_ERR, "SPF_dns_create_config_resolv faile");
		goto out2;
	}

	/* 
	 * Get the IP address
	 */
	if (!iptostring(sa, salen, addr, sizeof(addr))) {
		mg_log(LOG_ERR, "SPF_set_ip_str failed");
		goto out3;
	}
	if (SPF_set_ip_str(spfconf, addr) != 0) {
		mg_log(LOG_ERR, "SPF_set_ip_str failed");
		goto out3;
	}

	/* HELO string */
	if (SPF_set_helo_dom(spfconf, helo) != 0) {
		mg_log(LOG_ERR, "SPF_set_helo failed");
		goto out3;
	}

	/* 
	 * And the enveloppe source e-mail
	 */
	if (fromp[0] == '<')
		fromp++; /* strip leading < */
	strncpy(from, fromp, NS_MAXDNAME);
	from[NS_MAXDNAME] = '\0';
	len = strlen(from);
	if (fromp[len - 1] == '>')
		from[len - 1] = '\0'; /* strip trailing > */

	if (SPF_set_env_from(spfconf, from) != 0) {
		mg_log(LOG_ERR, "SPF_set_env_from failed");
		goto out3;
	}

	/*
	 * Get the SPF result
	 */
	SPF_init_output(&out);
#if 0 &&((SPF_LIB_VERSION_MAJOR == 0) && (SPF_LIB_VERSION_MINOR <= 3))
	out = SPF_result(spfconf, dnsconf, NULL);
#else
	out = SPF_result(spfconf, dnsconf);
#endif

	status = ad ? *(enum spf_status *)ad : MGSPF_PASS;
	switch (status) {
	case MGSPF_PASS:
		result = (out.result == SPF_RESULT_PASS);
		break;
	case MGSPF_FAIL:
		result = (out.result == SPF_RESULT_FAIL);
		break;
	case MGSPF_SOFTFAIL:
		result = (out.result == SPF_RESULT_SOFTFAIL);
		break;
	case MGSPF_NEUTRAL:
		result = (out.result == SPF_RESULT_NEUTRAL);
		break;
	case MGSPF_UNKNOWN:
		result = (out.result == SPF_RESULT_UNKNOWN);
		break;
	case MGSPF_ERROR:
		result = (out.result == SPF_RESULT_ERROR);
		break;
	case MGSPF_NONE:
		result = (out.result == SPF_RESULT_NONE);
		break;
	default:
		mg_log(LOG_ERR, "Internal error: unexpected spf_status");
		exit(EX_SOFTWARE);
		break;
	}

	if (conf.c_debug)
		mg_log(LOG_DEBUG, "SPF return code %d", out.result);

	SPF_free_output(&out);
out3:
	SPF_dns_destroy_config_resolv(dnsconf);
out2:
	SPF_destroy_config(spfconf);
out1:
	if (conf.c_debug) {
		gettimeofday(&tv2, NULL);
		timersub(&tv2, &tv1, &tv3);
		mg_log(LOG_DEBUG, "SPF lookup performed in %ld.%06lds",  
		    tv3.tv_sec, tv3.tv_usec);
	}

	return result;
}

#endif /* HAVE_SPF_ALT */

#ifdef HAVE_SPF2
#include <spf2/spf.h>

static int
spf_check_internal(ad, as, ap, priv)
	acl_data_t *ad;
	acl_stage_t as;
	struct acl_param *ap;
	struct mlfi_priv *priv;
{
	struct sockaddr *sa = SA(&priv->priv_addr);
	char *helo = priv->priv_helo;
	char *fromp = priv->priv_from;
	SPF_server_t *spf_server;
	SPF_request_t *spf_request;
	SPF_response_t *spf_response;
	char from[NS_MAXDNAME + 1];
	int res, result = 0;
	struct timeval tv1, tv2, tv3;
	size_t len;
	enum spf_status status;

	if (conf.c_debug)
		gettimeofday(&tv1, NULL);

	if ((spf_server = SPF_server_new(SPF_DNS_CACHE, 0)) == NULL) {
		mg_log(LOG_ERR, "SPF_server_new failed");
		goto out1;
	}

	if ((spf_request = SPF_request_new(spf_server)) == NULL) {
		mg_log(LOG_ERR, "SPF_request_new failed");
		goto out2;
	}

	/*
	 * Get the IP address
	 */
	switch (sa->sa_family) {
	case AF_INET:
		res = SPF_request_set_ipv4(spf_request, *SADDR4(sa));
		break;
#ifdef AF_INET6
	case AF_INET6:
		res = SPF_request_set_ipv6(spf_request, *SADDR6(sa));
		break;
#endif
	default:
		mg_log(LOG_ERR, "unknown address family %d", sa->sa_family);
		goto out3;
	}
	if (res != 0) {
		mg_log(LOG_ERR, "SPF_request_set_ip_str failed");
		goto out3;
	}

	/* HELO string */
	if (SPF_request_set_helo_dom(spf_request, helo) != 0) {
		mg_log(LOG_ERR, "SPF_request_set_helo_dom failed");
		goto out3;
	}

	/*
	 * And the enveloppe source e-mail
	 */
	if (fromp[0] == '<')
		fromp++; /* strip leading < */
	strncpy(from, fromp, NS_MAXDNAME);
	from[NS_MAXDNAME] = '\0';
	len = strlen(from);
	if (fromp[len - 1] == '>')
		from[len - 1] = '\0'; /* strip trailing > */

	if (SPF_request_set_env_from(spf_request, from) != 0) {
		mg_log(LOG_ERR, "SPF_request_set_env_from failed");
		goto out3;
	}

	/*
	 * Get the SPF result
	 */
	SPF_request_query_mailfrom(spf_request, &spf_response);
	res = SPF_response_result(spf_response);

	status = ad ? *(enum spf_status *)ad : MGSPF_PASS;
	switch (status) {
	case MGSPF_PASS:
		result = (res == SPF_RESULT_PASS);
		break;
	case MGSPF_FAIL:
		result = (res == SPF_RESULT_FAIL);
		break;
	case MGSPF_SOFTFAIL:
		result = (res == SPF_RESULT_SOFTFAIL);
		break;
	case MGSPF_NEUTRAL:
		result = (res == SPF_RESULT_NEUTRAL);
		break;
	case MGSPF_UNKNOWN:
		result = 
		    (res == SPF_RESULT_PERMERROR || res == SPF_RESULT_INVALID);
		break;
	case MGSPF_ERROR:
		result = (res == SPF_RESULT_TEMPERROR);
		break;
	case MGSPF_NONE:
		result = (res == SPF_RESULT_NONE);
		break;
	default:
		mg_log(LOG_ERR, "Internal error: unexpected spf_status");
		exit(EX_SOFTWARE);
		break;
	}

	if (conf.c_debug)
		mg_log(LOG_DEBUG, "SPF return code %d", res);

	SPF_response_free(spf_response);
out3:
	SPF_request_free(spf_request);
out2:
	SPF_server_free(spf_server);
out1:
	if (conf.c_debug) {
		gettimeofday(&tv2, NULL);
		timersub(&tv2, &tv1, &tv3);
		mg_log(LOG_DEBUG, "SPF lookup performed in %ld.%06lds",
		    tv3.tv_sec, tv3.tv_usec);
	}

	return result;
}


#endif /* HAVE_SPF2 */



#if (defined(HAVE_SPF) || defined(HAVE_SPF_ALT) || \
     defined(HAVE_SPF2_10) || defined(HAVE_SPF2))
char *
acl_print_spf(ad, buf, len)
	acl_data_t *ad;
	char *buf;
	size_t len;
{
	char *tmpstr;
	enum spf_status status;

	status = ad ? *(enum spf_status *)ad : MGSPF_PASS;
	switch (status) {
	case MGSPF_PASS:
		tmpstr = "pass";
		break;
	case MGSPF_FAIL:
		tmpstr = "fail";
		break;
	case MGSPF_SOFTFAIL:
		tmpstr = "softfail";
		break;
	case MGSPF_NEUTRAL:
		tmpstr = "neutral";
		break;
	case MGSPF_UNKNOWN:
		tmpstr = "unknown";
		break;
	case MGSPF_ERROR:
		tmpstr = "error";
		break;
	case MGSPF_NONE:
		tmpstr = "none";
		break;
	case MGSPF_SELF:
		tmpstr = "self";
		break;
	default:
		mg_log(LOG_ERR, "Internal error: unexpected spf_status");
		exit(EX_SOFTWARE);
		break;
	}
	snprintf(buf, len, "%s", tmpstr);
	return buf;
}

void
acl_add_spf(ad, data)
	acl_data_t *ad;
	void *data;
{
	ad->spf_status = *(enum spf_status *)data;
	return;
}

int
spf_check(ad, as, ap, priv)
	acl_data_t *ad;
	acl_stage_t as;
	struct acl_param *ap;
	struct mlfi_priv *priv;
{
	enum spf_status status;

	status = ad ? *(enum spf_status *)ad : MGSPF_PASS;
	switch (status) {
	case MGSPF_PASS:
	case MGSPF_FAIL:
	case MGSPF_SOFTFAIL:
	case MGSPF_NEUTRAL:
	case MGSPF_UNKNOWN:
	case MGSPF_ERROR:
	case MGSPF_NONE:
		return spf_check_internal(ad, as, ap, priv);
		break;
	case MGSPF_SELF:
		return spf_check_self(ad, as, ap, priv);		
		break;
	default:
		break;
	}

	mg_log(LOG_ERR, "Internal error: unexpected spf_status");
	exit(EX_SOFTWARE);

	return 0;
}

/* 
 * Check if the SPF record is wide open: the simpliest
 * way of doing it is to check whether our own IP 
 * validates the record.
 * That is a problem for Postfix installations, as the
 * local IP is not available. For now this clause is 
 * not available on Postfix.
 */
static int
spf_check_self(ad, as, ap, priv)
	acl_data_t *ad;
	acl_stage_t as;
	struct acl_param *ap;
	struct mlfi_priv *priv;
{
	int retval = 0;
	sockaddr_t saved_addr;
	acl_data_t tmp_ad;
	char *ip;

	(void)memcpy(&saved_addr, &priv->priv_addr, sizeof(saved_addr));

	switch (conf.c_localaddr.ss_family) {
	case AF_INET:
		(void)memcpy(&priv->priv_addr, &conf.c_localaddr, 
			     sizeof(struct sockaddr_in));
		break;
#ifdef AF_INET6
	case AF_INET6:
		(void)memcpy(&priv->priv_addr, &conf.c_localaddr, 
			     sizeof(struct sockaddr_in6));
		break;
#endif /* AF_INET6 */
	default:
#ifndef USE_POSTFIX
		ip = local_ipstr(priv);
		if (strncmp(ip, "IPv6:", strlen("IPv6:")) == 0) {
#ifdef AF_INET6
			if (inet_pton(AF_INET6, ip + strlen("IPv6:"), 
				    SADDR6(SA(&priv->priv_addr))) <= 0) {
				mg_log(LOG_ERR, 
				       "Invalid IPv6 local address %s", ip);
				exit(EX_SOFTWARE);
			}
#else /* AF_INET6 */
			mg_log(LOG_ERR, 
			    "IPv6 support not compiled but local IP is IPv6");
			exit(EX_SOFTWARE);
#endif /* AF_INET6 */
		} else {
			if (inet_pton(AF_INET, ip, 
				    SADDR4(SA(&priv->priv_addr))) <= 0) {
				mg_log(LOG_ERR, 
				       "Invalid IPv4 local address %s", ip);
				exit(EX_SOFTWARE);
			}
		}
#else /* USE_POSTFIX */
		mg_log(LOG_ERR, "spf self used without localaddr specified");
		return 0;
#endif /* USE_POSTFIX */
		break;
	}

	if (ad == NULL)
		(void)memset(&tmp_ad, 0, sizeof(tmp_ad));
	else
		(void)memcpy(&tmp_ad, ad, sizeof(tmp_ad));

	tmp_ad.spf_status = MGSPF_PASS;

	retval = spf_check_internal(&tmp_ad, as, ap, priv);

	(void)memcpy(&priv->priv_addr, &saved_addr, sizeof(priv->priv_addr));

	return retval;
}

#endif
