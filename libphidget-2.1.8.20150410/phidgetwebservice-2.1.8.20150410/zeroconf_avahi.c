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
#include "zeroconf.h"
#include "phidgetinterface.h"
#include "PhidgetWebservice21.h"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#ifdef ZEROCONF_RUNTIME_LINKING
typedef AvahiClient * (* avahi_client_new_type) (
    const AvahiPoll *poll_api /**< The abstract event loop API to use */,
    AvahiClientFlags flags /**< Some flags to modify the behaviour of  the client library */,
    AvahiClientCallback callback /**< A callback that is called whenever the state of the client changes. This may be NULL */,
    void *userdata /**< Some arbitrary user data pointer that will be passed to the callback function */,
    int *error /**< If creation of the client fails, this integer will contain the error cause. May be NULL if you aren't interested in the reason why avahi_client_new() failed. */);
typedef void (* avahi_client_free_type)(AvahiClient *client);
typedef const char * (* avahi_client_get_host_name_type) (AvahiClient *);
typedef AvahiEntryGroup* (* avahi_entry_group_new_type)(
    AvahiClient* c,
    AvahiEntryGroupCallback callback /**< This callback is called whenever the state of this entry group changes. May not be NULL. */,
    void *userdata /**< This arbitrary user data pointer will be passed to the callback functon */);
typedef int (* avahi_entry_group_free_type) (AvahiEntryGroup *);
typedef int (* avahi_entry_group_commit_type) (AvahiEntryGroup*);
typedef int (* avahi_entry_group_reset_type) (AvahiEntryGroup*);
typedef AvahiClient* (* avahi_entry_group_get_client_type) (AvahiEntryGroup*);
typedef int (* avahi_entry_group_add_service_type)(
    AvahiEntryGroup *group,
    AvahiIfIndex interface /**< The interface this service shall be announced on. We recommend to pass AVAHI_IF_UNSPEC here, to announce on all interfaces. */,
    AvahiProtocol protocol /**< The protocol this service shall be announced with, i.e. MDNS over IPV4 or MDNS over IPV6. We recommend to pass AVAHI_PROTO_UNSPEC here, to announce this service on all protocols the daemon supports. */,
    AvahiPublishFlags flags /**< Usually 0, unless you know what you do */,
    const char *name        /**< The name for the new service. May not be NULL. */,
    const char *type        /**< The service type for the new service, such as _http._tcp. May not be NULL. */,
    const char *domain      /**< The domain to register this domain in. We recommend to pass NULL here, to let the daemon decide */,   
    const char *host        /**< The host this services is residing on. We recommend to pass NULL here, the daemon will than automatically insert the local host name in that case */,
    uint16_t port           /**< The IP port number of this service */,
    ...);
typedef char *(* avahi_alternative_service_name_type)(const char *s);
typedef void (* avahi_free_type)(void *p);
typedef const char *(* avahi_strerror_type)(int error);
typedef int (* avahi_client_errno_type) (AvahiClient*);
typedef AvahiThreadedPoll *(* avahi_threaded_poll_new_type)(void);
typedef void (* avahi_threaded_poll_free_type)(AvahiThreadedPoll *p);
typedef const AvahiPoll* (* avahi_threaded_poll_get_type)(AvahiThreadedPoll *p);
typedef int (* avahi_threaded_poll_start_type)(AvahiThreadedPoll *p);
typedef int (* avahi_threaded_poll_stop_type)(AvahiThreadedPoll *p);
typedef void (* avahi_threaded_poll_quit_type)(AvahiThreadedPoll *p);
typedef void (* avahi_threaded_poll_lock_type)(AvahiThreadedPoll *p);
typedef void (* avahi_threaded_poll_unlock_type)(AvahiThreadedPoll *p);
#else
#define avahi_client_new_ptr avahi_client_new
#define avahi_client_free_ptr avahi_client_free
#define avahi_client_get_host_name_ptr avahi_client_get_host_name
#define avahi_entry_group_new_ptr avahi_entry_group_new
#define avahi_entry_group_free_ptr avahi_entry_group_free
#define avahi_entry_group_commit_ptr avahi_entry_group_commit
#define avahi_entry_group_get_client_ptr avahi_entry_group_get_client
#define avahi_entry_group_reset_ptr avahi_entry_group_reset
#define avahi_entry_group_add_service_ptr avahi_entry_group_add_service
#define avahi_alternative_service_name_ptr avahi_alternative_service_name
#define avahi_free_ptr avahi_free
#define avahi_strerror_ptr avahi_strerror
#define avahi_client_errno_ptr avahi_client_errno
#define avahi_threaded_poll_new_ptr avahi_threaded_poll_new
#define avahi_threaded_poll_free_ptr avahi_threaded_poll_free
#define avahi_threaded_poll_get_ptr avahi_threaded_poll_get
#define avahi_threaded_poll_start_ptr avahi_threaded_poll_start
#define avahi_threaded_poll_stop_ptr avahi_threaded_poll_stop
#define avahi_threaded_poll_quit_ptr avahi_threaded_poll_quit
#define avahi_threaded_poll_lock_ptr avahi_threaded_poll_lock
#define avahi_threaded_poll_unlock_ptr avahi_threaded_poll_unlock
#endif
avahi_client_new_type avahi_client_new_ptr = NULL;
avahi_client_free_type avahi_client_free_ptr = NULL;
avahi_client_get_host_name_type avahi_client_get_host_name_ptr = NULL;
avahi_entry_group_new_type avahi_entry_group_new_ptr = NULL;
avahi_entry_group_free_type avahi_entry_group_free_ptr = NULL;
avahi_entry_group_commit_type avahi_entry_group_commit_ptr = NULL;
avahi_entry_group_reset_type avahi_entry_group_reset_ptr = NULL;
avahi_entry_group_add_service_type avahi_entry_group_add_service_ptr = NULL;
avahi_entry_group_get_client_type avahi_entry_group_get_client_ptr = NULL;
avahi_alternative_service_name_type avahi_alternative_service_name_ptr = NULL;
avahi_free_type avahi_free_ptr = NULL;
avahi_strerror_type avahi_strerror_ptr = NULL;
avahi_client_errno_type avahi_client_errno_ptr = NULL;
avahi_threaded_poll_new_type avahi_threaded_poll_new_ptr = NULL;
avahi_threaded_poll_free_type avahi_threaded_poll_free_ptr = NULL;
avahi_threaded_poll_get_type avahi_threaded_poll_get_ptr = NULL;
avahi_threaded_poll_start_type avahi_threaded_poll_start_ptr = NULL;
avahi_threaded_poll_stop_type avahi_threaded_poll_stop_ptr = NULL;
avahi_threaded_poll_quit_type avahi_threaded_poll_quit_ptr = NULL;
avahi_threaded_poll_lock_type avahi_threaded_poll_lock_ptr = NULL;
avahi_threaded_poll_unlock_type avahi_threaded_poll_unlock_ptr = NULL;

int Dns_sdInitialized = FALSE; 

static AvahiThreadedPoll *threaded_poll = NULL;
static AvahiClient *client = NULL;
static AvahiEntryGroup *zeroconf_dict_ref = NULL;

extern char *serverName;

void *avahiLibHandle = NULL;

int InitializeZeroconf();
int UninitializeZeroconf();
static int zeroconf_advertise_phidget_name(CPhidgetHandle phid, char *mdns_name, int lock);
static int zeroconf_advertise_ws_int(int lock);

static void phidget_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {

    /* Called whenever the entry group state changes */
	const char *name;
	char mdns_name[1024];
	CPhidgetHandle phid = userdata;
	
    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED :
            /* The entry group has been established successfully */
			CPhidget_getDeviceName(phid, &name);
			snprintf(mdns_name, sizeof(mdns_name), "%s (%d)", name, phid->serialNumber);
            pu_log(PUL_INFO, 0, "Service '%s' successfully established.", mdns_name);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION : {
            char *n;
            
            /* A service name collision happened. Let's pick a new name */
			CPhidget_getDeviceName(phid, &name);
			snprintf(mdns_name, sizeof(mdns_name), "%s (%d)", name, phid->serialNumber);
            n = avahi_alternative_service_name_ptr(mdns_name);
            
            pu_log(PUL_WARN, 0, "Service name collision, renaming service to '%s'", n);
            
            /* And recreate the services */
			zeroconf_advertise_phidget_name(phid, n, 0);
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE :

            pu_log(PUL_ERR, 0, "Entry group failure: %s", avahi_strerror_ptr(avahi_client_errno_ptr(avahi_entry_group_get_client_ptr(g))));

            /* Some kind of failure happened while we were registering our services */
            avahi_threaded_poll_quit_ptr(threaded_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
			break;
        case AVAHI_ENTRY_GROUP_REGISTERING:
			break;
    }
}

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
    assert(g == zeroconf_dict_ref || zeroconf_dict_ref == NULL);

    /* Called whenever the entry group state changes */

    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED :
            /* The entry group has been established successfully */
            pu_log(PUL_INFO, 0, "Service '%s' successfully established.", serverName);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION : {
            char *n;
            
            /* A service name collision happened. Let's pick a new name */
            n = avahi_alternative_service_name_ptr(serverName);
			free((void *)serverName);
            serverName = strdup(n);
            avahi_free_ptr(n);
            
            pu_log(PUL_WARN, 0, "Service name collision, renaming service to '%s'", serverName);
            
            /* And recreate the services */
            zeroconf_advertise_ws_int(0);
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE :

            pu_log(PUL_ERR, 0, "Entry group failure: %s", avahi_strerror_ptr(avahi_client_errno_ptr(avahi_entry_group_get_client_ptr(g))));

            /* Some kind of failure happened while we were registering our services */
            avahi_threaded_poll_quit_ptr(threaded_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

int zeroconf_advertise_ws()
{
	return zeroconf_advertise_ws_int(1);
}
static int zeroconf_advertise_ws_int(int lock)
{
	InitializeZeroconf();
	if(Dns_sdInitialized)
	{
		char txtvers[50], protocolvers[50], auth[5];
		int ret;

		if(lock)
			avahi_threaded_poll_lock_ptr(threaded_poll);    
		/* If this is the first time we're called, let's create a new entry group */
		if (!zeroconf_dict_ref)
			if (!(zeroconf_dict_ref = avahi_entry_group_new_ptr(client, entry_group_callback, NULL))) {
				pu_log(PUL_ERR, 0, "avahi_entry_group_new() failed: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
				goto fail;
			}
				
		snprintf(txtvers, sizeof(txtvers), "txtvers=%s", dnssd_phidget_ws_txt_ver);
		snprintf(protocolvers, sizeof(protocolvers), "protocolvers=%s", protocol_ver);
		snprintf(auth, sizeof(auth), "auth=%s", (password==NULL?"n":"y"));
				
		/* Add the ws service */
		if ((ret = avahi_entry_group_add_service_ptr(zeroconf_dict_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 
			0, serverName, "_phidget_ws._tcp", NULL, NULL, port, txtvers, protocolvers, auth, NULL)) < 0)
		{
			pu_log(PUL_ERR, 0, "Failed to add _phidget_ws._tcp service: %s", avahi_strerror_ptr(ret));
			goto fail;
		}
		/* Tell the server to register the service */
		if ((ret = avahi_entry_group_commit_ptr(zeroconf_dict_ref)) < 0) {
			pu_log(PUL_ERR, 0, "Failed to commit entry_group: %s\n", avahi_strerror_ptr(ret));
			goto fail;
		}
fail:
		if(lock)
			avahi_threaded_poll_unlock_ptr(threaded_poll);
	}
	return EPHIDGET_OK;
}

int zeroconf_unadvertise_phidget(CPhidgetHandle phid)
{
	if(Dns_sdInitialized)
	{
		//avahi_threaded_poll_lock_ptr(threaded_poll);
		if(phid->dnsServiceRef)
		{
			avahi_entry_group_reset_ptr(phid->dnsServiceRef);
			avahi_entry_group_free_ptr(phid->dnsServiceRef);
			phid->dnsServiceRef = NULL;
		}
		//avahi_threaded_poll_unlock_ptr(threaded_poll);
	}
	return EPHIDGET_OK;
}

int zeroconf_advertise_phidget(CPhidgetHandle phid)
{
		const char *name;
		char mdns_name[1024];
		CPhidget_getDeviceName(phid, &name);
		snprintf(mdns_name, sizeof(mdns_name), "%s (%d)", name, phid->serialNumber);
		return zeroconf_advertise_phidget_name(phid, mdns_name, 1);
}

int zeroconf_advertise_phidget_name(CPhidgetHandle phid, char *mdns_name, int lock)
{
	if(Dns_sdInitialized)
	{
		int ret;
		const char *name;

		char txt_ver[20], txt_name[128], txt_type[50], txt_serial[20], txt_server_id[1024], txt_label[200];
		char txt_txtver[20], txt_protocolver[25], txt_auth[10], txt_id[10], txt_class[13], txt_usbProduct[71];

		CPhidget_getDeviceName(phid, &name);
		snprintf(txt_ver, sizeof(txt_ver), "version=%d", phid->deviceVersion);
		snprintf(txt_name, sizeof(txt_name), "name=%s", name);
		snprintf(txt_type, sizeof(txt_type), "type=%s", phid->deviceType);
		snprintf(txt_serial, sizeof(txt_serial), "serial=%d", phid->serialNumber);
		snprintf(txt_server_id, sizeof(txt_server_id), "server_id=%s", serverName);
		snprintf(txt_label, sizeof(txt_label), "label=%s", phid->label);
		snprintf(txt_txtver, sizeof(txt_txtver), "txtvers=%s", dnssd_phidget_txt_ver);
		snprintf(txt_protocolver, sizeof(txt_protocolver), "protocolvers=%s", protocol_ver);
		snprintf(txt_auth, sizeof(txt_auth), "auth=%s", (password==NULL?"n":"y"));
		snprintf(txt_id, sizeof(txt_id), "id=%d", phid->deviceIDSpec);
		snprintf(txt_class, sizeof(txt_class), "class=%d", phid->deviceID);
		snprintf(txt_usbProduct, sizeof(txt_usbProduct), "usbstr=%s", phid->usbProduct);

		if(lock)
			avahi_threaded_poll_lock_ptr(threaded_poll);    
		/* If this is the first time we're called, let's create a new entry group */
		if (!phid->dnsServiceRef)
			if (!(phid->dnsServiceRef = avahi_entry_group_new_ptr(client, phidget_group_callback, phid))) {
				pu_log(PUL_ERR, 0, "avahi_entry_group_new() failed: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
				goto fail;
			}
				
		/* Add the ws service */
		if ((ret = avahi_entry_group_add_service_ptr(phid->dnsServiceRef, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 
			0, mdns_name, "_phidget._tcp", NULL, NULL, port, 
			txt_ver, txt_name, txt_type, txt_serial, txt_server_id, txt_label, txt_txtver, txt_protocolver, txt_auth, txt_id, txt_class, txt_usbProduct, NULL)) < 0)
		{
			pu_log(PUL_ERR, 0, "Failed to add _phidget._tcp service: %s", avahi_strerror_ptr(ret));
			goto fail;
		}
		/* Tell the server to register the service */
		if ((ret = avahi_entry_group_commit_ptr(phid->dnsServiceRef)) < 0) {
			pu_log(PUL_ERR, 0, "Failed to commit entry_group: %s\n", avahi_strerror_ptr(ret));
			goto fail;
		}
fail:
		if(lock)
			avahi_threaded_poll_unlock_ptr(threaded_poll);
	}
	return EPHIDGET_OK;
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);

    /* Called whenever the client or server state changes */

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
        
            /* The server has startup successfully and registered its host
             * name on the network */
			Dns_sdInitialized = TRUE;
            break;

        case AVAHI_CLIENT_FAILURE:
            
            pu_log(PUL_ERR, 0, "Client failure: %s\n", avahi_strerror_ptr(avahi_client_errno_ptr(c)));
            avahi_threaded_poll_quit_ptr(threaded_poll);
            
            break;

        case AVAHI_CLIENT_S_COLLISION:
        
            /* Let's drop our registered services. When the server is back
             * in AVAHI_SERVER_RUNNING state we will register them
             * again with the new host name. */
            
        case AVAHI_CLIENT_S_REGISTERING:

            /* The server records are now being established. This
             * might be caused by a host name change. We need to wait
             * for our own records to register until the host name is
             * properly esatblished. */
            
            //if (group)
              //  avahi_entry_group_reset_ptr(group);
            
            break;

        case AVAHI_CLIENT_CONNECTING:
            ;
    }
}

int InitializeZeroconf()
{
    int error;
    //int ret = 1;
	int timeout = 50; //500ms
#ifdef ZEROCONF_RUNTIME_LINKING

	avahiLibHandle = dlopen("libavahi-client.so",RTLD_LAZY);
	if(!avahiLibHandle)
	{
		avahiLibHandle = dlopen("libavahi-client.so.3",RTLD_LAZY);
		if(!avahiLibHandle)
		{
			pu_log(PUL_INFO, 0, "dlopen returned: %s", dlerror());
			pu_log(PUL_INFO, 0, "Assuming that zeroconf is not supported on this machine.");
			return EPHIDGET_UNSUPPORTED;
		}
	}
	
	if(!(avahi_client_new_ptr = (avahi_client_new_type)dlsym(avahiLibHandle, "avahi_client_new"))) goto dlsym_err;
	if(!(avahi_client_free_ptr = (avahi_client_free_type)dlsym(avahiLibHandle, "avahi_client_free"))) goto dlsym_err;
	if(!(avahi_client_get_host_name_ptr = (avahi_client_get_host_name_type)dlsym(avahiLibHandle, "avahi_client_get_host_name"))) goto dlsym_err;
	if(!(avahi_entry_group_new_ptr = (avahi_entry_group_new_type)dlsym(avahiLibHandle, "avahi_entry_group_new"))) goto dlsym_err;
	if(!(avahi_entry_group_free_ptr = (avahi_entry_group_free_type)dlsym(avahiLibHandle, "avahi_entry_group_free"))) goto dlsym_err;
	if(!(avahi_entry_group_commit_ptr = (avahi_entry_group_commit_type)dlsym(avahiLibHandle, "avahi_entry_group_commit"))) goto dlsym_err;
	if(!(avahi_entry_group_reset_ptr = (avahi_entry_group_reset_type)dlsym(avahiLibHandle, "avahi_entry_group_reset"))) goto dlsym_err;
	if(!(avahi_entry_group_add_service_ptr = (avahi_entry_group_add_service_type)dlsym(avahiLibHandle, "avahi_entry_group_add_service"))) goto dlsym_err;
	if(!(avahi_entry_group_get_client_ptr = (avahi_entry_group_get_client_type)dlsym(avahiLibHandle, "avahi_entry_group_get_client"))) goto dlsym_err;
	if(!(avahi_alternative_service_name_ptr = (avahi_alternative_service_name_type)dlsym(avahiLibHandle, "avahi_alternative_service_name"))) goto dlsym_err;
	if(!(avahi_free_ptr = (avahi_free_type)dlsym(avahiLibHandle, "avahi_free"))) goto dlsym_err;
	if(!(avahi_strerror_ptr = (avahi_strerror_type)dlsym(avahiLibHandle, "avahi_strerror"))) goto dlsym_err;
	if(!(avahi_client_errno_ptr = (avahi_client_errno_type)dlsym(avahiLibHandle, "avahi_client_errno"))) goto dlsym_err;
	if(!(avahi_threaded_poll_new_ptr = (avahi_threaded_poll_new_type)dlsym(avahiLibHandle, "avahi_threaded_poll_new"))) goto dlsym_err;
	if(!(avahi_threaded_poll_free_ptr = (avahi_threaded_poll_free_type)dlsym(avahiLibHandle, "avahi_threaded_poll_free"))) goto dlsym_err;
	if(!(avahi_threaded_poll_get_ptr = (avahi_threaded_poll_get_type)dlsym(avahiLibHandle, "avahi_threaded_poll_get"))) goto dlsym_err;
	if(!(avahi_threaded_poll_start_ptr = (avahi_threaded_poll_start_type)dlsym(avahiLibHandle, "avahi_threaded_poll_start"))) goto dlsym_err;
	if(!(avahi_threaded_poll_stop_ptr = (avahi_threaded_poll_stop_type)dlsym(avahiLibHandle, "avahi_threaded_poll_stop"))) goto dlsym_err;
	if(!(avahi_threaded_poll_quit_ptr = (avahi_threaded_poll_quit_type)dlsym(avahiLibHandle, "avahi_threaded_poll_quit"))) goto dlsym_err;
	if(!(avahi_threaded_poll_lock_ptr = (avahi_threaded_poll_lock_type)dlsym(avahiLibHandle, "avahi_threaded_poll_lock"))) goto dlsym_err;
	if(!(avahi_threaded_poll_unlock_ptr = (avahi_threaded_poll_unlock_type)dlsym(avahiLibHandle, "avahi_threaded_poll_unlock"))) goto dlsym_err;
	
	goto dlsym_good;

dlsym_err:
		pu_log(PUL_INFO, 0, "dlsym returned: %s", dlerror());
		pu_log(PUL_INFO, 0, "Assuming that zeroconf is not supported on this machine.");
		return EPHIDGET_UNSUPPORTED;
		
dlsym_good:
		
#endif

    /* Allocate main loop object */
    if (!(threaded_poll = avahi_threaded_poll_new_ptr())) {
        pu_log(PUL_ERR, 0, "Failed to create threaded poll object.");
        return EPHIDGET_UNEXPECTED;
    }
	
    /* Allocate a new client */
    client = avahi_client_new_ptr(avahi_threaded_poll_get_ptr(threaded_poll), 0, client_callback, NULL, &error);
	
	if(!strcmp(serverName, "")) 
	{
		free(serverName);
		serverName = strdup(avahi_client_get_host_name_ptr(client));
	}

    /* Check wether creating the client object succeeded */
    if (!client) {
        pu_log(PUL_ERR, 0, "Failed to create client: %s", avahi_strerror_ptr(error));
        return EPHIDGET_UNEXPECTED;
    }
	
	avahi_threaded_poll_start_ptr(threaded_poll);
	
	while(!Dns_sdInitialized)
	{
		usleep(10000);
		timeout--;
		if(!timeout)
		{
			UninitializeZeroconf();
			pu_log(PUL_ERR, 0, "InitializeZeroconf Seems bad...");
			return EPHIDGET_UNEXPECTED;
		}
	}
	
	pu_log(PUL_INFO, 0, "InitializeZeroconf Seems good...");

	return EPHIDGET_OK;
}

int UninitializeZeroconf()
{
#ifdef ZEROCONF_RUNTIME_LINKING
#endif

    /* Cleanup things */
	if(Dns_sdInitialized)
	{
		if (threaded_poll)
		{
			avahi_threaded_poll_stop_ptr(threaded_poll);
			avahi_client_free_ptr(client);
			avahi_threaded_poll_free_ptr(threaded_poll);
			threaded_poll = NULL;
			client = NULL;
		}
	}
		
	Dns_sdInitialized = FALSE;
	return EPHIDGET_OK;
}
