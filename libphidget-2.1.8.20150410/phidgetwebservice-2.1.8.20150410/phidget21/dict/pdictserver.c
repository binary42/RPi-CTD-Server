/*
 * This file is part of libphidget21
 *
 * Copyright 2006-2015 Phidgets Inc <patrick@phidgets.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see 
 * <http://www.gnu.org/licenses/>
 */

#include "stdafx.h"
#include "pdictserver.h"
#include "pdict.h"
#include "ptree.h"
#include "plist.h"
#include "utils.h"
#include <assert.h>

/*
 * XXX TODO:
 * - per-session heap utilization limits
 * - make application-close do cleanup
 * - hide process_line
 */

#define KEY "([a-zA-Z_/][a-zA-Z_0-9:%/.\\\\-]*)"
#define VALUE "(\"([][ a-zA-z0-9!@#$%^&*()_+{};':,./<>?\\\\-]*)\"|([][a-zA-z0-9!@#$%^&*()_+{};':,./<>?\\\\-]*))"
#define PATTERN "(\\\"([^\"]*)\\\"|([^ \t]*))"
#define CMDTAG "([a-zA-Z0-9_]*)" 
#define LISTENID "([a-zA-Z0-9_]+)" 
#define SP "[ \t]+"
#define OSP "[ \t]*"
#define SET_PATTERN "^" CMDTAG OSP "set" SP KEY OSP "=" OSP VALUE OSP \
"(for" SP "session)?" OSP "$"
#define LISTEN_PATTERN "^" CMDTAG OSP "listen" SP PATTERN SP LISTENID OSP "$"
#define IGNORE_PATTERN "^" CMDTAG OSP "ignore" SP LISTENID OSP "$"
#define REPORT_PATTERN "^" CMDTAG OSP "report" SP "([0-9]+)" OSP CMDTAG OSP "$"
#define WAIT_PATTERN "^" CMDTAG OSP "wait" SP "([0-9]+)" OSP "$"
#define FLUSH_PATTERN "^" CMDTAG OSP "flush" OSP "$"
#define WALK_PATTERN "^" CMDTAG OSP "(remove|get)" SP PATTERN OSP "$"
#define QUIT_PATTERN "^" CMDTAG OSP "quit" OSP "$"
#define NULL_PATTERN "^" CMDTAG OSP "need nulls" OSP "$"
#define GET_ID_PATTERN "^" CMDTAG OSP "get session id" OSP "$"
#define OK_PATTERN "^" CMDTAG OSP "report ack" OSP "$"
#define POLICY_FILE_REQUEST "^<policy-file-request */>$"

regex_t setex;
regex_t listenex;
regex_t ignoreex;
regex_t reportex;
regex_t waitex;
regex_t flushex;
regex_t walkex;
regex_t quitex;
regex_t nullex;
regex_t getidex;
regex_t okex;
regex_t policyex;

int debug_protocol = 0;

typedef struct {
	pds_session_t *na_pdss;
	const char *na_listenid;
	plist_node_t *na_pd_ids;
} notify_arg_t;

static int
nacmp(const void *i1, const void *i2)
{
	const notify_arg_t *n1 = i1;
	const notify_arg_t *n2 = i2;
	int res;
	
	if ((res = (size_t)n1->na_pdss - (size_t)n2->na_pdss) != 0)
		return res;
	return strcmp(n1->na_listenid, n2->na_listenid);
}

static int
ipmcmp(const void *i1, const void *i2)
{
	int res;

	//pu_log(PUL_INFO, 0, "Comparing IDs %s, %s in ipmcmp",((id_pde_map_t *)i1)->ipm_id,((id_pde_map_t *)i2)->ipm_id);
	if(((id_pde_map_t *)i1)->ipm_id[0] != 'l')
		abort();

	if ((res = strcmp(((id_pde_map_t *)i1)->ipm_id, ((id_pde_map_t *)i2)->ipm_id)) != 0)
		return res;

	//pu_log(PUL_INFO, 0, "Comparing Keys %s, %s in ipmcmp",((id_pde_map_t *)i1)->ipm_key, ((id_pde_map_t *)i2)->ipm_key);

	return strcmp(((id_pde_map_t *)i1)->ipm_key, ((id_pde_map_t *)i2)->ipm_key);
}

static ptree_walk_res_t
write_pending(const void *node, int level, void *a, void *pwra)
{
	//void **arg = a;
	id_pde_map_t *ipm = (id_pde_map_t *)node;
	pds_session_t *pdss = (pds_session_t *)a;
	//ptree_node_t **pending = arg[1];
	int buflen;
	char errdesc[256];
	char *buf;
	void *ov;
	
	if (!pdss->pdss_write)
		goto freeing;
	buflen = pasprintf(&buf, "%s%s200-%s is pending, key %s "
					   "latest value \"%s\" (%s)\n", pdss->pdss_report_cmdtag ?
					   pdss->pdss_report_cmdtag : "", pdss->pdss_report_cmdtag ?
					   " " : "", ipm->ipm_id, ipm->ipm_key, ipm->ipm_val,
					   pdict_reason_str(ipm->ipm_reason));
	if (buf) {
		if (!pdss->pdss_write(pdss->pdss_wfd, buf, buflen +
							  pdss->pdss_client_cr_null, errdesc, sizeof (errdesc))) {
			//pu_log(PUL_WARN, pdss->pdss_id, "write error while reporting: %s",errdesc);
		}
		free(buf); buf = NULL;
	} else {
		pu_log(PUL_WARN, pdss->pdss_id, "some notifications dropped due to low-memory condition");
	}
freeing:
	ptree_inorder_walk_remove(&pdss->pdss_pending, &ov, pwra, ipmcmp);
	assert(node == ov);
	free((void *)ipm->ipm_key); //ipm->ipm_key = NULL;
	free((void *)ipm->ipm_val); //ipm->ipm_val = NULL;
	free(ov); ov = NULL;
	
	return 1;
}

//What the hell does this function do??
//it's called every time a key matching a listener changes
static void
notify(const char *k, const char *v, pdict_reason_t r, const char *pde_oldval,
	   void *a)
{
	notify_arg_t *na = a;
	id_pde_map_t *oldp;
	id_pde_map_t *ipm;
	id_pde_map_t cm;
	const char *oldVal;
	int res;
	
	na->na_pdss->pdss_pd_lock(na->na_pdss->pdss_pd_lock_arg);
	cm.ipm_id = na->na_listenid;
	cm.ipm_key = k;
	if (ptree_contains((void *)&cm, na->na_pdss->pdss_pending, ipmcmp, (void **)&ipm)) 
	{
		oldVal = ipm->ipm_val;
		ipm->ipm_val = strdup(v);
		free((void *)oldVal);
		ipm->ipm_reason = r;
		na->na_pdss->pdss_pd_unlock(na->na_pdss->pdss_pd_lock_arg);
		return;
	}
	if (!(ipm = malloc(sizeof (*ipm)))) {
		pu_log(PUL_WARN, na->na_pdss->pdss_id, "some notifications dropped due to low-memory condition");
		na->na_pdss->pdss_pd_unlock(na->na_pdss->pdss_pd_lock_arg);
		return;
	}
	ipm->ipm_id = na->na_listenid;
	ipm->ipm_key = strdup(k);
	ipm->ipm_val = strdup(v);
	ipm->ipm_reason = r;
	if ((res = ptree_replace(ipm, &na->na_pdss->pdss_pending, ipmcmp, (void **)&oldp)) != 0 && oldp) {
		pu_log(PUL_WARN, na->na_pdss->pdss_id, "some notifications dropped due to low-memory condition");
		free((void *)oldp->ipm_key); oldp->ipm_key = NULL;
		free((void *)oldp->ipm_val); oldp->ipm_val = NULL;
		free((void *)oldp); oldp = NULL;
	}
	if (!res) {
		free((void *)ipm->ipm_key); ipm->ipm_key = NULL;
		free((void *)ipm->ipm_val); ipm->ipm_val = NULL;
		free(ipm); ipm = NULL;
		pu_log(PUL_WARN, na->na_pdss->pdss_id, "some notifications dropped due to low-memory condition");
	}
	na->na_pdss->pdss_pd_unlock(na->na_pdss->pdss_pd_lock_arg);
}

static void
result(pds_session_t *pdss, const char *cmdtag, const char *fmt, ...)
{
	va_list va;
	char *formatted;
	char *resbuf;
	char errdesc[256];
	int len;
	char terminator = '\n';
	if(pdss->pdss_client_cr_null)
		terminator = '\0';
	
	va_start(va, fmt);
	pvasprintf(&formatted, fmt, va);
	va_end(va);
	if (!formatted) {
		if (!pdss->pdss_should_close)
			pu_log(PUL_WARN, pdss->pdss_id,"problem with pvasprintf; closing session");
		pdss->pdss_should_close = 1;
		return;
	}
	
	if (cmdtag)
		len = pasprintf(&resbuf, "%s %s%c", cmdtag, formatted, terminator);
	else
		len = pasprintf(&resbuf, "%s%c", formatted, terminator);
	if (!resbuf) {
		free(formatted); formatted = NULL;
		if (!pdss->pdss_should_close)
			pu_log(PUL_WARN, pdss->pdss_id,
				   "insufficient memory to compose command result; closing session");
		pdss->pdss_should_close = 1;
		return;
	}
	free(formatted); formatted = NULL;
	
	if (!pdss->pdss_write(pdss->pdss_wfd, resbuf, len +
						  pdss->pdss_client_cr_null, errdesc, sizeof (errdesc)) &&
	    !pdss->pdss_should_close) {
		char *errbuf;
		pasprintf(&errbuf,
				  "connection closed remotely; closing session (%s)", errdesc);
		if (errbuf) {
			pu_log(PUL_INFO, pdss->pdss_id, errbuf);
			free(errbuf); errbuf = NULL;
		}
		pdss->pdss_should_close = 1;
	}
	free(resbuf); resbuf = NULL;
	return;
}

static void *
report(void *arg)
{
	pds_session_t *pdss = arg;
	char *cmdtag = NULL;
	int period = 0;
	
	pthread_mutex_lock(&pdss->pdss_lock);
	do {
		/* XXX not periodic */
		/* XXX if expires with nothing pending, wait on condvar */
		period = pdss->pdss_report_period;
		if (period) {
			pthread_mutex_unlock(&pdss->pdss_lock);
			usleep(period * 1000);
			pthread_mutex_lock(&pdss->pdss_lock);
			period = pdss->pdss_report_period;
		}
		if ((cmdtag && pdss->pdss_report_cmdtag && strcmp(cmdtag,
														  pdss->pdss_report_cmdtag) != 0) || (!cmdtag &&
																							  pdss->pdss_report_cmdtag)) {
			if (cmdtag && strcmp(cmdtag, "unknown") != 0)
			{
				free(cmdtag); cmdtag = NULL;
			}
			cmdtag = strdup(pdss->pdss_report_cmdtag);
			if (!cmdtag)
				cmdtag = "unknown";
		} else if (cmdtag && !pdss->pdss_report_cmdtag) {
			if (strcmp(cmdtag, "unknown") != 0)
				free(cmdtag);
			cmdtag = NULL;
		}
		if (!period)
			break;
		pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
		if (!pdss->pdss_pending) {
			pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
			continue;
		}
		result(pdss, cmdtag, "200-periodic report follows:");
		pthread_mutex_unlock(&pdss->pdss_lock);
		ptree_walk(pdss->pdss_pending, PTREE_POSTORDER, write_pending, ipmcmp, pdss);
		assert(!pdss->pdss_pending);
		pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
		result(pdss, cmdtag, "200-that's all for now");
		pthread_mutex_lock(&pdss->pdss_lock);
		period = pdss->pdss_report_period;
		pthread_cond_signal(&pdss->pdss_report_cv);
		pdss->pdss_nreport++;
	} while (period && !pdss->pdss_should_close);
	if (cmdtag && strcmp(cmdtag, "unknown") != 0)
		free(cmdtag);
	cmdtag = NULL;
	if (pdss->pdss_report_cmdtag) {
		free(pdss->pdss_report_cmdtag); pdss->pdss_report_cmdtag = NULL;
	}
	pthread_mutex_unlock(&pdss->pdss_lock);
	pthread_cond_signal(&pdss->pdss_report_cv);
	
	//Don't just set this to 0! we won't be able to join it.
	//pdss->pdss_report_thread = 0;
	
	pu_log(PUL_INFO, 0, "Exiting report thread");
	return 0;
}

static int
set_report_period(pds_session_t *pdss, const char *cmdtag,
				  const char *report_cmdtag, unsigned int period)
{
	pthread_mutex_lock(&pdss->pdss_lock);
	pdss->pdss_report_period = period;
	if (pdss->pdss_report_cmdtag) {
		free(pdss->pdss_report_cmdtag); pdss->pdss_report_cmdtag = NULL;
	}
	if (report_cmdtag)
		pdss->pdss_report_cmdtag = strdup(report_cmdtag);
	if (pdss->pdss_report_period) {
		if (!pdss->pdss_report_thread) {
			if (pthread_create(&pdss->pdss_report_thread, 0, report, pdss) != 0) {
				result(pdss, cmdtag, "502 temporarily failure establishing report thread");
				pthread_mutex_unlock(&pdss->pdss_lock);
				return 0;
			}
		}
		result(pdss, cmdtag, "200 inter-report period"
			   " adjusted to %dms", pdss->pdss_report_period);
	} else if (!pdss->pdss_report_thread)
		result(pdss, cmdtag, "200 periodic reports off");
	else 
		result(pdss, cmdtag, "200 periodic reports will"
			   " stop after next report");
	pthread_mutex_unlock(&pdss->pdss_lock);
	return 1;
}

/*
 * Waits for a report to be issued, if periodic reports are enabled and
 * notifications are pending.  Caller must hold pdss_lock and pd_lock.
 */
static void
_flush(pds_session_t *pdss)
{
	unsigned int nreport;
	
	nreport = pdss->pdss_nreport;
	while (pdss->pdss_report_thread && pdss->pdss_pending &&
		   nreport == pdss->pdss_nreport) {
		pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
		pthread_cond_wait(&pdss->pdss_report_cv,
						  &pdss->pdss_lock);
		pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
	}
}

static int
remove_persistent_change_listener_cb(const void *k, const void *v, void *arg)
{
	if (!pdict_remove_persistent_change_listener(arg, (int)k))
		pu_log(PUL_WARN, 0, "insufficient memory");
	return 1;
}

static int
_count(const void *k, const void *v, void *arg)
{
	int *np = arg;
	
	(*np)++;
	return 1;
}

static ptree_walk_res_t
remove_pending_id(const void *v1, int level, void *a, void *pwra)
{
	void **arg = a;
	const id_pde_map_t *ipm = v1;
	const char *id = (const char *)arg[0];
	ptree_node_t **pending = arg[1];
	void *ov;
	
	if (strcmp(id, ipm->ipm_id) == 0) {
		ptree_inorder_walk_remove(pending, &ov, pwra, ipmcmp);
		assert(v1 == ov);
		free((void *)ipm->ipm_key);
		free((void *)ipm->ipm_val);
		free(ov);
	}
	return PTREE_WALK_CONTINUE;
}

static int
add_to_wa_list(const char *k, const char *v, void *arg)
{
	wa_t *wa = arg;
	
	if (regexec(&wa->wa_regex, k, 0, NULL, 0) == 0)
		return plist_add((void *)k, (void *)v, &wa->wa_l);
	return 1;
}

static int
print_wa_list(const void *k, const void *v, void *arg)
{
	wa_t *wa = arg;
	
	result(wa->wa_pdss, wa->wa_cmdtag, "200-key %s value %s", k, v);
	
	return 1;
}

static int
remove_wa_list(const void *k, const void *v, void *arg)
{
	wa_t *wa = arg;
	char *ov;
	
	assert(pdict_ent_remove(wa->wa_pdss->pdss_pd, k, &ov));
	assert(ov);
	free(ov); ov = NULL;
	
	return 1;
}

static int
is_key_in_list(const void *k, const void *v, void *arg)
{
	if(!strcmp((const char *)k, (const char *)arg))
		return 0;
	return 1;
}

void
pds_process_line(pds_session_t *pdss, char *line)
{
	regmatch_t pmatch[9];
	char *cmdtag = NULL;
	char *cmdstr = NULL;
	char *key = NULL;
	char *val = NULL;
	char *buf = NULL;
	char *listenid = NULL;
	int reportperiod;
	int forsession;
	int res;
	
	//DPRINT(line);
	
	if ((res = regexec(&setex, line, 7, pmatch, 0)) == 0) {
		getmatchsub(line, &cmdtag, pmatch, 1);
		if (cmdtag && strcmp(cmdtag, "unknown") == 0) {
			result(pdss, cmdtag,
				   "304 unknown is a reserved command tag");
			free(cmdtag); cmdtag = NULL;
			return;
		}
		if (!getmatchsub(line, &key, pmatch, 2) || !key)
			goto fail;
		
		if (!getmatchsub(line, &val, pmatch, 5) &&
		    !getmatchsub(line, &val, pmatch, 4))
			goto fail;
		if (!val)
			goto fail;
		if ((forsession = getmatchsub(line, NULL, pmatch, 6)) > 0) {
			/* mark for expiration, but make sure it's only added once! */
			if(plist_walk(pdss->pdss_expire, is_key_in_list, key))
			{
				char *e;
				if (!(e = strdup(key)))
					goto fail;
				plist_add(e, NULL, &pdss->pdss_expire);
			}
		}
		pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
		res = pdict_add(pdss->pdss_pd, key, val, NULL);
		pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
		if (!res)
			result(pdss, cmdtag, "304 set failed");
		else
			result(pdss, cmdtag, "200 set successful");
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		free(key); key = NULL;
		free(val); val = NULL;
		return;
	}
	if ((res = regexec(&listenex, line, 6, pmatch, 0)) == 0) {
		notify_arg_t *nap;
		notify_arg_t na;
		
		getmatchsub(line, &cmdtag, pmatch, 1);
		if (cmdtag && strcmp(cmdtag, "unknown") == 0) {
			result(pdss, cmdtag,
				   "304 unknown is a reserved command tag");
			free(cmdtag); cmdtag = NULL;
			return;
		}
		if ((!getmatchsub(line, &buf, pmatch, 4) && !getmatchsub(line, &buf, pmatch, 3)) || !buf)
			goto fail;
		getmatchsub(line, &listenid, pmatch, 5);
		na.na_pdss = pdss;
		na.na_listenid = listenid;
		pthread_mutex_lock(&pdss->pdss_lock);
		pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
		if (!ptree_contains(&na, pdss->pdss_notify_args, nacmp, (void **)&nap)) {
			if (!(nap = malloc(sizeof (*nap))))
			{
				pu_log(PUL_WARN, pdss->pdss_id, "insufficient memory");
				pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
				pthread_mutex_unlock(&pdss->pdss_lock);
				goto fail;
			}
			nap->na_pdss = pdss;
			nap->na_listenid = listenid;
			nap->na_pd_ids = NULL;
			if (!ptree_replace(nap, &pdss->pdss_notify_args, nacmp, NULL)) {
				free(nap); nap = NULL;
				pu_log(PUL_WARN, pdss->pdss_id, "insufficient memory");
				pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
				pthread_mutex_unlock(&pdss->pdss_lock);
				free(listenid); listenid = NULL;
				free(buf); buf = NULL;
				goto fail;
			}
		}
		
		if (!(res = pdict_add_persistent_change_listener(pdss->pdss_pd, buf, notify, (void *)nap))) {
			if (!nap->na_pd_ids) {
				ptree_remove(nap, &pdss->pdss_notify_args, nacmp, NULL);
				free((void *)nap->na_listenid); nap->na_listenid = NULL;
				free(nap); nap = NULL;
			}
			result(pdss, cmdtag, "303 listen not established--bad pattern?");
		} else {
			if (!plist_add((void *)res, NULL, &nap->na_pd_ids)) {
				pdict_remove_persistent_change_listener(pdss->pdss_pd, res);
				if (!nap->na_pd_ids) {
					ptree_remove(nap, &pdss->pdss_notify_args, nacmp, NULL);
					free((void *)nap->na_listenid); nap->na_listenid = NULL;
					free(nap); nap = NULL;
				}
				free(buf); buf = NULL;
				if (cmdtag) {
					free(cmdtag); cmdtag = NULL;
				}
				pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
				pthread_mutex_unlock(&pdss->pdss_lock);
				goto fail;
			} else {
				result(pdss, cmdtag,"200 listening, id %s",
					   listenid);
			}
		}
		pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
		pthread_mutex_unlock(&pdss->pdss_lock);
		free(buf); buf = NULL;
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		return;
	}
	if ((res = regexec(&reportex, line, 5, pmatch, 0)) == 0) {
		getmatchsub(line, &cmdtag, pmatch, 1);
		if (cmdtag && strcmp(cmdtag, "unknown") == 0) {
			result(pdss, cmdtag,
				   "304 unknown is a reserved command tag");
			free(cmdtag); cmdtag = NULL;
			return;
		}
		if (!getmatchsub(line, &buf, pmatch, 2) || !buf)
			goto fail;
		if ((reportperiod = atoi(buf)) < 0 || reportperiod > 10000) {
			result(pdss, cmdtag, "301 invalid report/wait period"
				   "--specify milliseconds or 0 for off (max 10s)");
			if (cmdtag) {
				free(cmdtag); cmdtag = NULL;
			}
			free(buf); buf = NULL;
			return;
		}
		free(buf); buf = NULL;
		getmatchsub(line, &buf, pmatch, 3);
		set_report_period(pdss, cmdtag, buf, reportperiod);
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		if (buf) {
			free(buf); buf = NULL;
		}
		return;
	}
	if ((res = regexec(&waitex, line, 5, pmatch, 0)) == 0) {
		getmatchsub(line, &cmdtag, pmatch, 1);
		if (cmdtag && strcmp(cmdtag, "unknown") == 0) {
			result(pdss, cmdtag,
				   "304 unknown is a reserved command tag");
			free(cmdtag); cmdtag = NULL;
			return;
		}
		if (!getmatchsub(line, &buf, pmatch, 2) || !buf)
			goto fail;
		if (atoi(buf) < 0 || atoi(buf) > 10000) {
			result(pdss, cmdtag, "301 invalid wait period"
				   "--specify milliseconds (max 10s)");
			if (cmdtag) {
				free(cmdtag); cmdtag = NULL;
			}
			return;
		}
		usleep(atoi(buf) * 1000);
		result(pdss, cmdtag, "200 nothin' doin'");
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		return;
	}
	if ((res = regexec(&flushex, line, 5, pmatch, 0)) == 0) {
		getmatchsub(line, &cmdtag, pmatch, 1);
		if (cmdtag && strcmp(cmdtag, "unknown") == 0) {
			result(pdss, cmdtag,
				   "304 unknown is a reserved command tag");
			free(cmdtag); cmdtag = NULL;
			return;
		}
		pthread_mutex_lock(&pdss->pdss_lock);
		pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
		_flush(pdss);
		pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
		pthread_mutex_unlock(&pdss->pdss_lock);
		result(pdss, cmdtag, "200 glug glug");
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		return;
	}
	if ((res = regexec(&walkex, line, 5, pmatch, 0)) == 0) {
		wa_t wa;
		
		getmatchsub(line, &cmdtag, pmatch, 1);
		if (cmdtag && strcmp(cmdtag, "unknown") == 0) {
			result(pdss, cmdtag,
				   "304 unknown is a reserved command tag");
			free(cmdtag); cmdtag = NULL;
			return;
		}
		if (!getmatchsub(line, &cmdstr, pmatch, 2) || !cmdstr)
			goto fail;
		if (!getmatchsub(line, &buf, pmatch, 3) || !buf) {
			free(cmdstr); cmdstr = NULL;
			goto fail;
		}
		if ((regcomp(&wa.wa_regex, buf, REG_EXTENDED)) != 0) {
			free(cmdstr); cmdstr = NULL;
			free(buf); buf = NULL;
			result(pdss, cmdtag, "305 expression error"); 
			if (cmdtag) {
				free(cmdtag); cmdtag = NULL;
			}
			return;
		}
		wa.wa_pdss = pdss;
		wa.wa_cmdtag = cmdtag;
		wa.wa_l = NULL;
		pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
		if (!pdict_walk(pdss->pdss_pd, add_to_wa_list, &wa)) {
			int e = errno;
			pu_log(PUL_WARN, pdss->pdss_id, "temporary failure: %s", strerror(e));
			result(pdss, cmdtag, "300 temporary failure: %s",
				   strerror(e)); 
		} else {
			if (strcmp(cmdstr, "remove") == 0)
				plist_walk(wa.wa_l, remove_wa_list, &wa);
			else
				plist_walk(wa.wa_l, print_wa_list, &wa);
			result(pdss, cmdtag, "200 done");
		}
		plist_clear(&wa.wa_l);
		pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
		regfree(&wa.wa_regex);
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		free(cmdstr); cmdstr = NULL;
		free(buf); buf = NULL;
		return;
	}
	if ((res = regexec(&ignoreex, line, 5, pmatch, 0)) == 0) {
		notify_arg_t *nap;
		notify_arg_t na;
		void *arg[2];
		int n;
		
		getmatchsub(line, &cmdtag, pmatch, 1);
		if (cmdtag && strcmp(cmdtag, "unknown") == 0) {
			result(pdss, cmdtag,
				   "304 unknown is a reserved command tag");
			if (cmdtag) {
				free(cmdtag); cmdtag = NULL;
			}
			return;
		}
		if (!getmatchsub(line, &listenid, pmatch, 2) || !listenid)
			goto fail;
		pthread_mutex_lock(&pdss->pdss_lock);
		pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
		na.na_pdss = pdss;
		na.na_listenid = listenid;
		if (!ptree_remove(&na, &pdss->pdss_notify_args, nacmp,
						  (void **)&nap)) {
			pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
			pthread_mutex_unlock(&pdss->pdss_lock);
			result(pdss, cmdtag, "306 nonexistent key/id");
			if (cmdtag) {
				free(cmdtag); cmdtag = NULL;
			}
			return;
		}
		n = 0;
		plist_walk(nap->na_pd_ids, _count, &n);
		plist_walk(nap->na_pd_ids, remove_persistent_change_listener_cb,
				   pdss->pdss_pd);
		arg[0] = (void *)nap->na_listenid;
		arg[1] = &pdss->pdss_pending;
		ptree_walk(pdss->pdss_pending, PTREE_POSTORDER, remove_pending_id, ipmcmp, arg);
		free((void *)nap->na_listenid); nap->na_listenid = NULL;
		plist_clear(&nap->na_pd_ids);
		free(nap); nap = NULL;
		assert(!ptree_contains(&na, pdss->pdss_notify_args, nacmp, NULL));
		pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
		pthread_mutex_unlock(&pdss->pdss_lock);
		
		result(pdss, cmdtag, "200 %d listener%s ignored", n, n > 1 ? "s"
			   : "");
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		free(listenid);
		return;
	}
	if ((res = regexec(&quitex, line, 5, pmatch, 0)) == 0) {
		getmatchsub(line, &cmdtag, pmatch, 1);
		result(pdss, cmdtag, "200 goodbye");
		pdss->pdss_should_close = 1;
		pdss->pdss_close(pdss->pdss_wfd, NULL, 0);
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		return;
	}
	if ((res = regexec(&getidex, line, 5, pmatch, 0)) == 0) {
		getmatchsub(line, &cmdtag, pmatch, 1);
		result(pdss, cmdtag, "200 %d", pdss->pdss_id);
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		return;
	}
	if ((res = regexec(&okex, line, 5, pmatch, 0)) == 0) {
		if (cmdtag) {
			free(cmdtag); cmdtag = NULL;
		}
		return;
	}
	
	result(pdss, NULL, "400 input unrecognized: %s", line);
	return;
fail:
	result(pdss, cmdtag, "300 command failed: %s", strerror(errno));
	if (cmdtag)
		free (cmdtag);
}

int
pds_init(void)
{
	static int pds_init_done = 0;
	int res;
	
	if (pds_init_done)
		return 1;
	
	if ((res = regcomp(&setex, SET_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr, "set command pattern compilation error %d\n",
				res);
		abort();
	}
	if ((res = regcomp(&listenex, LISTEN_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr, "listen command pattern compilation error %d\n",
				res);
		abort();
	}
	if ((res = regcomp(&ignoreex, IGNORE_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"ignore command pattern compilation error %d\n", res);
		abort();
	}
	if ((res = regcomp(&reportex, REPORT_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"report command pattern compilation error %d\n", res);
		abort();
	}
	if ((res = regcomp(&waitex, WAIT_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"wait command pattern compilation error %d\n", res);
		abort();
	}
	if ((res = regcomp(&flushex, FLUSH_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"flush command pattern compilation error %d\n", res);
		abort();
	}
	if ((res = regcomp(&walkex, WALK_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"walk command pattern compilation error %d\n", res);
		abort();
	}
	if ((res = regcomp(&quitex, QUIT_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"quit command pattern compilation error %d\n", res);
		abort();
	}
	if ((res = regcomp(&nullex, NULL_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"need nulls command pattern compilation error %d\n", res);
		abort();
	}
	if ((res = regcomp(&getidex, GET_ID_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"get session id command pattern compilation error %d\n",
		        res);
		abort();
	}
	if ((res = regcomp(&okex, OK_PATTERN, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"ok command pattern compilation error %d\n",
		        res);
		abort();
	}
	if ((res = regcomp(&policyex, POLICY_FILE_REQUEST, REG_EXTENDED)) != 0) {
		fprintf(stderr,
				"policy file request command pattern compilation error %d\n",
		        res);
		abort();
	}
	
	pds_init_done = 1;
	return 1;
}

static ptree_walk_res_t
free_na_cb(const void *v1, int level, void *a, void *pwra)
{
	pds_session_t *pdss = (pds_session_t *)a;
	notify_arg_t *na = (notify_arg_t *)v1;
	void *ov;
	
	ptree_inorder_walk_remove(&pdss->pdss_notify_args, &ov, pwra, nacmp);
	assert(na == ov);

	plist_walk(na->na_pd_ids, remove_persistent_change_listener_cb, na->na_pdss->pdss_pd);
	free((void *)na->na_listenid); na->na_listenid = NULL;
	plist_clear(&na->na_pd_ids);

	free(ov); ov = NULL;
	
	return PTREE_WALK_CONTINUE;
}

static int
expired_key_cb(const void *k, const void *v, void *arg)
{
	pdict_t *pd = arg;
	char *ov = NULL;
	
	pdict_ent_remove(pd, k, &ov);
	free((void *)k); k = NULL;
	if (ov) {
		free(ov); ov = NULL;
	}
	
	return 1;
}


/*static int
 print_key(const void *k, const void *v, void *arg)
 {
 printf(" Key: %s, Val: %s\n",(char *)k,(char *)v);
 return 1;
 }*/

/*
 * This is started by accept_cb as a thread on a new connection.
 * Calls pd_getline and sends the data it gets to pds_process_line
 * until pd_getline > 0 and !pdss->pdss_should_close
 */
int
pds_session_serve(const pds_session_t *arg)
{
	pds_session_t *pdss = (pds_session_t *)arg;
	char *line = NULL;
	int res;
	
#ifdef _MACOSX
	signal (SIGPIPE, SIG_IGN);
#endif
	
	DPRINT(PUL_INFO, "Started new pds_session_serve thread");
	
	pdss->pdss_errdesc[0] = 0;
	
	/* first thing - Authenticate */
	if(pdss->pdss_auth)
	{
		if(pdss->pdss_auth((pds_session_t *)pdss)){
			pu_log(PUL_WARN, pdss->pdss_id, "Authentication failed or bad version - closing connection");
			goto authfailed;
		}
	}
	
	while ((res = pd_getline(pdss->pdss_readbuf, sizeof (pdss->pdss_readbuf),
							 &pdss->pdss_bufcur, &pdss->pdss_buflen, pdss->pdss_read,
							 pdss->pdss_close, pdss->pdss_rfd, &line, pdss->pdss_errdesc,
							 sizeof (pdss->pdss_errdesc))) > 0 && !pdss->pdss_should_close)
	{
		pds_process_line((pds_session_t *)pdss, line);
		free(line); line=NULL;
	}
	
authfailed:
	
	free(line); line=NULL;
	
	//Shut down the report thread
	pdss->pdss_should_close = 1;

	pthread_mutex_lock(&pdss->pdss_lock);
	if (pdss->pdss_report_thread)
	{
		void *status;
		//pthread_cond_wait(&pdss->pdss_report_cv, &pdss->pdss_lock);
		pthread_mutex_unlock(&pdss->pdss_lock);
		pthread_join(pdss->pdss_report_thread, &status);
		pthread_mutex_lock(&pdss->pdss_lock);
	}
	
	/* remove keys set to expire at end of session */
	pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
	plist_walk(pdss->pdss_expire, expired_key_cb, pdss->pdss_pd);
	plist_clear(&pdss->pdss_expire);
	
	/* write_pending will free pending notifications - it will NOT send them because we freed pdss_write */
	pdss->pdss_write = NULL;

	ptree_walk(pdss->pdss_pending, PTREE_POSTORDER, write_pending, ipmcmp, pdss);
	assert(!pdss->pdss_pending);
	//ptree_clear(&pdss->pdss_pending);
	//pdss->pdss_pending = NULL;
	
	//clear the listener list - need to lock the dictionary, 
	//because an add/remove could be enumerating the list as we free it otherwise.
	ptree_walk(pdss->pdss_notify_args, PTREE_POSTORDER, free_na_cb, nacmp, pdss);
	//ptree_clear(&pdss->pdss_notify_args);
	assert(!pdss->pdss_notify_args);
	
	//Close the socket
	pdss->pdss_close(pdss->pdss_wfd, NULL, 0);
	if(pdss->pdss_wfd != pdss->pdss_rfd)
		pdss->pdss_close(pdss->pdss_rfd, NULL, 0);
	pdss->pdss_wfd = pdss->pdss_rfd = INVALID_SOCKET;
	pdss->pdss_close = NULL;
	
	pu_log(PUL_INFO, pdss->pdss_id, "done - session closed");
	
	pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
	pthread_mutex_unlock(&pdss->pdss_lock);
	
	//make sure no cb threads are running
	while(pdss->cb_threads_count)
		SLEEP(10);

	/* now free the pdss!! */
	pds_session_free(pdss);
	
	pu_log(PUL_INFO, 0, "Exiting pds_session_serve thread");
	return 0;
}

void pds_session_free(pds_session_t *pdss)
{
	pthread_mutex_destroy(&pdss->pdss_lock);
	pthread_cond_destroy(&pdss->pdss_report_cv);
	free(pdss);
}

const pds_session_t *
pds_session_alloc(pdict_t *pd, void (*pd_lock)(void *),
				  void (*pd_unlock)(void *), void *pd_lock_arg, int readfd,
				  int(*readfunc)(int, void *, unsigned int, char *errdesc, int edlen),
				  int writefd, int(*writefunc)(int, const void *, unsigned int, char *errdesc,
											   int edlen), int (*closefunc)(int, char *errdesc, int edlen),
				  void (*logfunc)(pu_log_level_t, int s, const char *, ...),
				  void(*cleanupfunc)(void), 
				  int(*authfunc)(pds_session_t *pdss),
				  char *errdesc, int edlen)
{
	pds_session_t *pdss;
	static int id = 1;
	
	pds_init();
	
	if (!pd_lock || !pd_unlock) {
		snprintf(errdesc, edlen, "dictionary locking functions invalid");
		return 0;
	}
	
	if (!(pdss = malloc(sizeof (*pdss))))
		return NULL;
	memset(pdss, 0, sizeof (*pdss));
	pdss->pdss_id = id++;
	pdss->pdss_rfd = readfd;
	pdss->pdss_read = readfunc;
	pdss->pdss_wfd = writefd;
	pdss->pdss_write = writefunc;
	pdss->pdss_close = closefunc;
	pdss->pdss_log = logfunc;
	pdss->pdss_cleanup = cleanupfunc;
	pdss->pdss_auth = authfunc;
	pthread_mutex_init(&pdss->pdss_lock, NULL);
	pthread_cond_init(&pdss->pdss_report_cv, NULL);
	pdss->pdss_pd = pd;
	pdss->pdss_pd_lock_arg = pd_lock_arg;
	pdss->pdss_pd_lock = pd_lock;
	pdss->pdss_pd_unlock = pd_unlock;
	pdss->cb_threads_count = 0;
	
	return pdss;
}

int
pds_session_id(const pds_session_t *pdss)
{
	return pdss->pdss_id;
}

/*
 * Ensure notifications are noticed by the given session.
 */
void
pds_session_flush(pds_session_t *pdss)
{
	pthread_mutex_lock(&pdss->pdss_lock);
	pdss->pdss_pd_lock(pdss->pdss_pd_lock_arg);
	_flush(pdss);
	pdss->pdss_pd_unlock(pdss->pdss_pd_lock_arg);
	pthread_mutex_unlock(&pdss->pdss_lock);
}
