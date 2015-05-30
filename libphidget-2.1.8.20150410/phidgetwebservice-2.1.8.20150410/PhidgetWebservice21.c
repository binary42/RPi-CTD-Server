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
#include "utils.h"
#include "phidgetinterface.h"
#include "PhidgetWebservice21.h"
#include "md5.h"
#include "zeroconf.h"

#define WRITE(fd, msg) send(fd, msg, strlen(msg), MSG_NOSIGNAL)

/*
*	Phidget WebService Protocol version history
*	1.0 - Initial version
*
*	1.0.1
*		-first version to be enforced
*		-we changed around the device id numbers, so old webservice won't be able to talk to new
*
*	1.0.2
*		-Authorization is asynchronous, so we had to add Tags and now it's not compatible with old webservice
*		-Doesn't match the old version checking! So could be ugly for users, get an unexpected error rather then a version error
*		-Sends out all initial data, so it's just like opening locally
*		-supports interfacekit Raw sensor value
*		-supports labels on remoteIP managers
*
*	1.0.3
*		-supports servoType and setServoParameters for PhidgetServo and PhidgetAdvancedServo
*
*	1.0.4
*		-fixed RFID initialization - wasn't getting tag events if tag is on reader before open
*		-fixed RFID sometimes not attaching in Flash
*
*	1.0.5
*		-added dataRate for InterfaceKit
*
*	1.0.6
*		-added brightness for TextLCD
*		-support PhidgetSpatial
*		-support PhidgetIR
*		-1047 support (enable, index)
*
*	1.0.7
*		-1045 support
*		-1011 support
*		-1204 support
*		-support for report ack message
*
*	1.0.8
*		-support for 1065, 1002, 1040, 1046, 1056
*		-support for error events
*
*	1.0.9
*		-support for openLabelRemote and openLabelRemoteIP
*		-support for Unicode labels (UTF8)
*
*	1.0.10
*		-support for 1024, 1032
*/
const char *protocol_ver = "1.0.10";

/* 
 * TXT record version - this should be 1 for a long time
 *  - only need to change if we really change the TXT record format
 */
/*
 * phidget TXT Version Changelog
 *	1
 *		-Initial Version
 *	2
 *		-Added id and class so we can determine the difference between devices with the same name
 *	3
 *		-Added USB Product String
 */
const char *dnssd_phidget_txt_ver = "3";
/*
 * phidget_ws TXT Version Changelog
 *	1
 *		-Initial Version
 */
const char *dnssd_phidget_ws_txt_ver = "1";

char defaultName[1024] = ""; //computer name

int port = 5001;
char *serverName = NULL; //for mDNS
const char *password = NULL;

int print_debug_messages = FALSE;

//These are shared by all pdss's to synchronize access to the one and only pd
pdict_t *pd;
pthread_mutex_t pd_mutex;
pthread_mutex_t pd_mutex_mutex;
pthread_t locking_thread = 0;
int lock_count = 0;

char policy[1024];

char * get_policy_file() {
	sprintf(policy,"<?xml version=\"1.0\"?>\n"
				   "<!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\">\n"
				   "<cross-domain-policy>\n"
				   "   <allow-access-from domain=\"*\" to-ports=\"%d\" />\n"
				   "</cross-domain-policy>\n",port);
	return policy;
}

//these are only ever called on the one and only pd_mutex - presumably.
void
pd_lock(void *pd_lock) {
	pthread_mutex_lock(&pd_mutex_mutex);
	if(locking_thread != pthread_self())
	{		
		pthread_mutex_unlock(&pd_mutex_mutex);
		(void) pthread_mutex_lock(pd_lock);
		pthread_mutex_lock(&pd_mutex_mutex);
		locking_thread = pthread_self();
	}
	lock_count++;
	pthread_mutex_unlock(&pd_mutex_mutex);
}

void
pd_unlock(void *pd_lock) {
	pthread_mutex_lock(&pd_mutex_mutex);
	if(locking_thread == pthread_self())
	{
		lock_count--;
		if(lock_count == 0)
		{
			(void) pthread_mutex_unlock(pd_lock);
			locking_thread = 0;
		}
	}
	pthread_mutex_unlock(&pd_mutex_mutex);
}

int authenticate(pds_session_t *pdss)
{
	char buf[100];
	char errdesc[1024];
	char *line, *line_no_tag;
	char *versionstring;
	int res;
	
	char tagstr[20];
	int tag;

tryagain:
	//wait for input from client
	pdss->pdss_errdesc[0] = 0;
	res = pd_getline(pdss->pdss_readbuf, sizeof (pdss->pdss_readbuf),
		&pdss->pdss_bufcur, &pdss->pdss_buflen, pdss->pdss_read,
		pdss->pdss_close, pdss->pdss_rfd, &line,
		pdss->pdss_errdesc, sizeof (pdss->pdss_errdesc));
		
	if(!res)
	{
		DPRINT(PUL_WARN, "Error in pd_getline: %s", pdss->pdss_errdesc);
		free(line); line=NULL;
		return EPHIDGET_NETWORK;
	}

	//Parse out tag
	if (line[0] == 'T') {
		tag = atoi(line + 1);
		line_no_tag = strchr(line, ' ')+1;
		snprintf(tagstr, sizeof(tagstr), "T%d ", tag);
	}
	else
	{
		line_no_tag = line;
		//tag = -1;
		tagstr[0] = '\0';
	}
	
	//995 = 'tell me about authentication' - it is the only correct initial packet except for the exceptions within...
	//it should be '995 authenticate, version=1.0.0'
	if(strncmp(line_no_tag, "995", 3))
	{
		//from flash
		if(!strcmp(line, "need nulls"))
		{
			pdss->pdss_client_cr_null = 1;
			
			snprintf(buf, sizeof(buf), "%s200 appending NULLs to lines\n",tagstr);
			if(!(pu_write(pdss->pdss_wfd, buf, strlen(buf) + pdss->pdss_client_cr_null, errdesc, sizeof(errdesc))))
			{
				DPRINT(PUL_ERR, "Error in pu_write: %s", errdesc);
				free(line);
				return EPHIDGET_UNEXPECTED;
			}
			goto tryagain;
		}
		if(!strcmp(line, "<policy-file-request/>"))
		{
			pdss->pdss_client_cr_null = 1;
			DPRINT(PUL_INFO, "Sending policy file...");
			get_policy_file();
			if(!(pu_write(pdss->pdss_wfd, policy, strlen(policy) + pdss->pdss_client_cr_null, errdesc, sizeof(errdesc))))
			{
				DPRINT(PUL_ERR, "Error in pu_write: %s", errdesc);
				free(line);
				return EPHIDGET_UNEXPECTED;
			}
			free(line);
			return EPHIDGET_OK; //no authenticate, etc. for this!
		}
		DPRINT(PUL_ERR, "Recieved unexpected data in authenticate: %s", line);
		free(line);
		return EPHIDGET_UNEXPECTED;
	}

	//Check for right version - 995 authenticate, version=(version)
	if((versionstring=strstr(line_no_tag, "version="))!=NULL)
	{
		if(strcmp(versionstring+8, protocol_ver))
		{
			DPRINT(PUL_WARN, "Version mismatch - webservice verion: %s, client version: %s",protocol_ver,versionstring);
			DPRINT(PUL_WARN, " Either the webservice, phidget21, or both need to be updated to these versions match");
			goto badversion;
		}
	}
	else
	{
		DPRINT(PUL_WARN, "WARNING: Old phidget21 client detected, you need to update the phidget21 that your client is using.");
badversion:
		snprintf(buf, sizeof(buf), "%s994 Bad Version, expecting: %s\n",tagstr, protocol_ver);
		if(!(pu_write(pdss->pdss_wfd, buf, strlen(buf) + pdss->pdss_client_cr_null, errdesc, sizeof(errdesc))))
		{
			DPRINT(PUL_ERR, "Error in pu_write: %s", errdesc);
			free(line);
			return EPHIDGET_UNEXPECTED;
		}
		free(line);
		return EPHIDGET_UNEXPECTED;
	}
	
	//Done with line
	free(line); line=NULL;
	
	if(password != NULL)
	{
		int randomHash = rand();
		md5_state_t state;
		md5_byte_t digest[16];
		
		int di;

		DPRINT(PUL_INFO, "Authenticating...");
		
		snprintf(buf, sizeof(buf), "%s999 %d\n",tagstr, randomHash);
		if(!(pu_write(pdss->pdss_wfd, buf, strlen(buf) + pdss->pdss_client_cr_null, errdesc, sizeof(errdesc))))
		{
			DPRINT(PUL_ERR, "Error in pu_write: %s", errdesc);
			return EPHIDGET_UNEXPECTED;
		}
		
		pdss->pdss_errdesc[0] = 0;
		res = pd_getline(pdss->pdss_readbuf, sizeof (pdss->pdss_readbuf),
			&pdss->pdss_bufcur, &pdss->pdss_buflen, pdss->pdss_read,
			pdss->pdss_close, pdss->pdss_rfd, &line,
			pdss->pdss_errdesc, sizeof (pdss->pdss_errdesc));
			
		if(!res)
		{
			DPRINT(PUL_WARN, "Error in pd_getline: %s", pdss->pdss_errdesc);
			free(line);
			return EPHIDGET_NETWORK;
		}

		//Parse out tag
		if (line[0] == 'T') {
			tag = atoi(line + 1);
			line_no_tag = strchr(line, ' ')+1;
			snprintf(tagstr, sizeof(tagstr), "T%d ", tag);
		}
		else
		{
			line_no_tag = line;
			//tag = -1;
			tagstr[0] = '\0';
		}
		
		if(strncmp(line_no_tag, "997", 3))
		{
			DPRINT(PUL_ERR, "Recieved unexpected data in authenticate: %s", line);
			free(line);
			return EPHIDGET_UNEXPECTED;
		}
		
		sprintf(buf,"%d%s",randomHash,password);
		
		md5_init(&state);
		md5_append(&state, (const md5_byte_t *)buf, strlen(buf));
		md5_finish(&state, digest);
		
		for (di = 0; di < 16; ++di)
			sprintf((buf) + di * 2, "%02x", digest[di]);
			
		DPRINT(PUL_INFO, "calculated password hash: %s",(buf));
		DPRINT(PUL_INFO, "recieved password hash: %s",(line_no_tag+4));
		
		if(strncmp(buf,(line_no_tag+4),100)) {					
			snprintf(buf, sizeof(buf), "%s998 Authorization failed\n",tagstr);
			if(!(pu_write(pdss->pdss_wfd, buf, strlen(buf) + pdss->pdss_client_cr_null, errdesc, sizeof(errdesc))))
			{
				DPRINT(PUL_ERR, "Error in pu_write: %s", errdesc);
				free(line);
				return EPHIDGET_UNEXPECTED;
			}
			free(line);
			return EPHIDGET_UNEXPECTED;
		}
		
		//done with line
		free(line); line=NULL;
		
		snprintf(buf, sizeof(buf), "%s996 Authentication passed, version=%s\n",tagstr,protocol_ver);
		if(!(pu_write(pdss->pdss_wfd, buf, strlen(buf) + pdss->pdss_client_cr_null, errdesc, sizeof(errdesc))))
		{
			DPRINT(PUL_ERR, "Error in pu_write: %s", errdesc);
			return EPHIDGET_UNEXPECTED;
		}
	}
	else
	{
		DPRINT(PUL_INFO, "No Authentication");
		snprintf(buf, sizeof(buf), "%s996 No need to authenticate, version=%s\n",tagstr,protocol_ver);
		if(!(pu_write(pdss->pdss_wfd, buf, strlen(buf) + pdss->pdss_client_cr_null, errdesc, sizeof(errdesc))))
		{
			DPRINT(PUL_ERR, "Error in pu_write: %s", errdesc);
			return EPHIDGET_UNEXPECTED;
		}
	}
	return EPHIDGET_OK;
}

/*
 * This is called from stream_server_accept via a function pointer on a new connection
 * initialized a new pdss (phidget dictionary server session), authenticates the connection
 * and starts the pds_session_serve thread.
 */
static void
accept_cb(int fd, const char *claddr, int clport)
{
	const pds_session_t *pdss;
	char errdesc[1024];
	pthread_t t;
	int err;
	pthread_attr_t *attrPtr = NULL;
	
	pu_log(PUL_INFO, 0, "Accepted new connection");

	if (!(pdss = pds_session_alloc(pd, pd_lock, pd_unlock, &pd_mutex, fd, pu_read, fd, pu_write, pu_close,
		pu_log, NULL, authenticate, errdesc, sizeof (errdesc)))) {
		/* XXX log error */
		pu_log(PUL_INFO, 0, "pds_session_alloc");
		//(void) WRITE(fd, "500 temporarily unavailable\n");
		pu_close(fd, errdesc, sizeof (errdesc));
		return;
	}
	
#ifndef _WINDOWS
	{
		pthread_attr_t attr;
		if((err = pthread_attr_init(&attr)) == 0)
		{
			if((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) == 0)
			{
				attrPtr = &attr;
			}
			else
			{
				DPRINT(PUL_ERR, "pthread_attr_setdetachstate failed with error: %d",err);
			}
		}
		else
		{
			DPRINT(PUL_ERR, "pthread_attr_init failed with error: %d",err);
		}
	}
#endif
	
	if((err = pthread_create(&t, attrPtr, (void *(*)(void *))pds_session_serve, (void *)pdss)) != 0)
	{
		switch(err)
		{
			case EAGAIN:
				DPRINT(PUL_ERR, "Couldn't create pds_session_serve thread: EAGAIN");
				break;
			default:
				DPRINT(PUL_ERR, "Couldn't create pds_session_serve thread: %d",err);
				break;
		}
		//(void) WRITE(fd, "500 temporarily unavailable\n");
		pu_close(fd, errdesc, sizeof (errdesc));
		return;
	}
}

static void print_help()
{
	printf("'phidgetwebservice21' is a Phidget and Dictionary server from Phidgets Inc. "
		"See www.phidgets.com for more information.\n\n");
	printf("Usage: phidgetwebservice21 [OPTION]\n\n");
	printf("All parameters are optional. The default parameters are: port=5001, "
		"ServerName=(Computer Name) and no password\n\n");
	printf("Options:\n");
	printf("  -p      Port\n");
	printf("  -n      Server Name\n");
	printf("  -P      Password\n");
	printf("  -v      Debug mode\n");
	printf("  -h      Display this help\n");
}

void sighandler (int n)
{
	pu_log(PUL_INFO, 0, "got a signal. exiting.");
	stop_phidgets();
	exit(0);
}

int
main(int argc, char* argv[])
{
	const pds_session_t *pdss;
	char errdesc[1024];
	int i;

#ifdef _WINDOWS
	{
		WSADATA wsad;
		WORD wsav;

		wsav = MAKEWORD(2, 2);
		WSAStartup(wsav, &wsad);
	}
#else
	signal (SIGPIPE, SIG_IGN);
#endif

#ifndef WINCE
	signal (SIGTERM, sighandler);
	signal (SIGINT, sighandler);
#endif

	//TODO: enable -- options (or get rid of them)
	for(i=0;i<argc;i++)
	{
		if((argv[i][0] == '-') && (strlen(argv[i]) >= 2))
		{
			switch(argv[i][1])
			{
				case 'p': // Port
					if((i+1)<argc && argv[i+1])
						port = strtol(argv[i+1], NULL, 10);
					break;
				case 'n': // Server name
					if((i+1)<argc && argv[i+1])
						serverName = strdup(argv[i+1]);
					break;
				case 'h': // Help
					print_help();
					exit(0);
				case 'P': // Password
					if((i+1)<argc && argv[i+1])
						password = argv[i+1];
					break;
				case 'v': // Verbose
					logging_enabled = TRUE;
					if(strlen(argv[i]) == 3 && argv[i][2] == 'v')
						CPhidget_enableLogging(PHIDGET_LOG_VERBOSE, NULL);
					else
						CPhidget_enableLogging(PHIDGET_LOG_INFO, NULL);
					break;
				default:
					printf("Invalid option %s specified.\n\n",argv[i]);
					print_help();
					exit(0);
			}
		}
	}
	
	if(!serverName)
		serverName = strdup(defaultName);
	

	if (!(pd = pdict_alloc())) {
		pu_log(PUL_ERR, 0, "pdict_alloc failure");
		return 1;
	}	
	if (pthread_mutex_init(&pd_mutex, NULL) != 0) {
		pu_log(PUL_ERR, 0, "pthread_mutex_init failure");
		return 1;
	}
	if (pthread_mutex_init(&pd_mutex_mutex, NULL) != 0) {
		pu_log(PUL_ERR, 0, "pthread_mutex_init failure");
		return 1;
	}

	if (!pds_init()) {
		pu_log(PUL_ERR, 0, "pds_init failure");
		return 1;
	}

	if (!(pdss = pds_session_alloc(pd, pd_lock, pd_unlock, &pd_mutex, 0,
	    pu_read, 1, pu_write, pu_close,
		pu_log, NULL, NULL, errdesc, sizeof (errdesc)))) {
		pu_log(PUL_ERR, 0, "pds_session_alloc failure");
		return 1;
	}
	/*pthread_create(&t, NULL, (void *(*)(void *))pds_session_serve,
	    (void *)pdss);*/

	start_phidget((pds_session_t *)pdss);

	if (!stream_server_accept(port, accept_cb, errdesc, sizeof (errdesc))) {
		pu_log(PUL_ERR, 0, "stream_server_accept failure: %s\n", errdesc);
		return 1;
	}
	return EPHIDGET_OK;
}


int add_key(pds_session_t *pdss, const char *key, const char *val)
{
	int res;
	char *null_val = "\001";
	pd_lock(&pd_mutex);
	if(val[0] == '\0')
		res = pdict_add(pd, key, null_val, NULL);
	else
		res = pdict_add(pd, key, val, NULL);
	pd_unlock(&pd_mutex);
	if (!res)
		return EPHIDGET_UNEXPECTED;
	else
		return EPHIDGET_OK;
}

int remove_key(pds_session_t *pdss, const char *key)
{
	char *keypattern = malloc(1024);
	pd_lock(&pd_mutex);
	snprintf(keypattern, 1024, "remove %s",key);
	//TODO: we need to provide a valid write handle in case proccess_line fails
	pds_process_line(pdss, keypattern);
	free(keypattern); keypattern = NULL;
	pd_unlock(&pd_mutex);
		return EPHIDGET_OK;
}

int add_listener(pds_session_t *pdss, const char *kpat, pdl_notify_func_t notfiyme, void *ptr)
{
	int res = 0;
	pd_lock(&pd_mutex);
	res = pdict_add_persistent_change_listener(pd, kpat, notfiyme, ptr);
	pd_unlock(&pd_mutex);
	if (!res)
		return EPHIDGET_UNEXPECTED;
	else
		return EPHIDGET_OK;
}
