/* $Id: dump.c,v 1.2 2004/03/17 22:21:36 manu Exp $ */

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

#include <sys/cdefs.h>
#ifdef __RCSID  
__RCSID("$Id: dump.c,v 1.2 2004/03/17 22:21:36 manu Exp $");
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <errno.h>
#include <sysexits.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "sync.h"
#include "dump.h"
#include "autowhite.h"
#include "milter-greylist.h"

pthread_cond_t dump_sleepflag;

char *dumpfile = DUMPFILE;
int dump_parse(void);
int dump_dirty = 0;

int
dump_init(void) {
	int error;

	if ((error = pthread_cond_init(&dump_sleepflag, NULL)) == 0)
		return error;

	return 0;
}

void
dumper_start(void) {
	pthread_t tid;

	if (pthread_create(&tid, NULL, (void *)dumper, NULL) != 0) {
		syslog(LOG_ERR, 
		    "cannot start dumper thread: %s", strerror(errno));
		exit(EX_OSERR);
	}
	return;
}
	
/* ARGSUSED0 */
void
dumper(dontcare) 
	void *dontcare;
{
	FILE *dump;
	int dumpfd;
	struct timeval tv1, tv2, tv3;
	pthread_mutex_t mutex;
	char newdumpfile[MAXPATHLEN + 1];
	int done;

	if (pthread_mutex_init(&mutex, NULL) != 0) {
		syslog(LOG_ERR, "pthread_mutex_init failed: %s\n",
		    strerror(errno));
		exit(EX_OSERR);
	}

	while (1) {
		if (pthread_cond_wait(&dump_sleepflag, &mutex) != 0)
		    syslog(LOG_ERR, "pthread_cond_wait failed: %s\n",
			strerror(errno));

		if (debug) {
			(void)gettimeofday(&tv1, NULL);
			syslog(LOG_DEBUG, "dumping %d modifications", 
			    dump_dirty);
			/* 
			 * dump_dirty is not protected by a lock,
			 * hence it could be modified between the 
			 * display and the actual dump. This debug
			 * message does not give an accurate information
			 */
			dump_dirty = 0;
		}

		/* 
		 * Dump the database in a temporary file and 
		 * then replace the old one by the new one.
		 * On decent systems, rename(2) garantees that 
		 * even if the machine crashes, we will not 
		 * loose both files.
		 */
		snprintf(newdumpfile, MAXPATHLEN, "%s-XXXXXXXX", dumpfile);

		if ((dumpfd = mkstemp(newdumpfile)) == -1) {
			syslog(LOG_ERR, "mkstemp(\"%s\") failed: %s", 
			    newdumpfile, strerror(errno));
			exit(EX_OSERR);
		}

		if ((dump = fdopen(dumpfd, "w")) == NULL) {
			syslog(LOG_ERR, "cannot write dumpfile \"%s\": %s", 
			    newdumpfile, strerror(errno));
			exit(EX_OSERR);
		}

		dump_header(dump);
		done = 0;
		done += pending_textdump(dump);
		done += autowhite_textdump(dump);

		fclose(dump);

		if (rename(newdumpfile, dumpfile) != 0) {
			syslog(LOG_ERR, "cannot replace \"%s\" by \"%s\": %s\n",
			    dumpfile, newdumpfile, strerror(errno));
			exit(EX_OSERR);
		}

		if (debug) {
			(void)gettimeofday(&tv2, NULL);
			timersub(&tv2, &tv1, &tv3);
			syslog(LOG_DEBUG, "dumping %d records in %ld.%06lds",
			    done, tv3.tv_sec, tv3.tv_usec);
		}

	}

	/* NOTREACHED */
	syslog(LOG_ERR, "dumper unexpectedly exitted");
	exit(EX_SOFTWARE);

	return;
}

void
dump_reload(void) {
	FILE *dump;

	/* 
	 * Re-import a saved greylist
	 */
	if ((dump = fopen(dumpfile, "r")) == NULL) {
		syslog(LOG_ERR, "cannot read dumpfile \"%s\"", dumpfile);
		syslog(LOG_ERR, "starting with an empty greylist");
	} else {
		dump_in = dump;
		PENDING_WRLOCK;
		AUTOWHITE_WRLOCK;
		dump_parse();

		/* 
		 * dump_dirty has been bumped on each pending_get call,
		 * whereas there is nothing to flush. Fix that.
		 */
		dump_dirty = 0;

		AUTOWHITE_UNLOCK;
		PENDING_UNLOCK;
		fclose(dump);
	}

	return;
}

void
dump_flush(void) {
	if (pthread_cond_signal(&dump_sleepflag) != 0) {
		syslog(LOG_ERR, "cannot wakeup dumper: %s", strerror(errno));
		exit(EX_SOFTWARE);
	}

	return;
}

void
dump_header(stream)
	FILE *stream;
{
	struct timeval tv;
	char textdate[DATELEN + 1];

	gettimeofday(&tv, NULL);
	strftime(textdate, DATELEN, "%Y-%m-%d %T",
	    localtime((time_t *)&tv.tv_sec));

	fprintf(stream, "#\n# milter-greylist databases, "
	    "dumped by milter-greylist-%s on %s.\n",
	    PACKAGE_VERSION, textdate);
	fprintf(stream, "# DO NOT EDIT while milter-greylist is running, "
	    "changes will be overwritten.\n#\n\n");

	return;
}