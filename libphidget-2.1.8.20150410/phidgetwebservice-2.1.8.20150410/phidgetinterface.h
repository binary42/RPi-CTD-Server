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

#ifndef _PHIDGETINTERFACE
#define _PHIDGETINTERFACE

#include "cphidgetlist.h"
#include "pdictserver.h"

typedef struct _CClientInfo CClientInfo, *CClientInfoHandle;
typedef struct _CClientList CClientList, *CClientListHandle;
typedef struct _CNetworkPhidgetInfo CNetworkPhidgetInfo, *CNetworkPhidgetInfoHandle;
typedef struct _CNetworkPhidgetList CNetworkPhidgetList, *CNetworkPhidgetListHandle;

struct _CClientInfo
{
	CPhidgetSocketClientHandle client;
	CNetworkPhidgetListHandle phidgets;
};

struct _CClientList
{
	struct _CClientList *next;
	CClientInfoHandle clientInfo;
};

struct _CNetworkPhidgetInfo
{
	CClientListHandle clients;
	CPhidgetHandle phidget;
};

struct _CNetworkPhidgetList
{
	struct _CNetworkPhidgetList *next;
	CNetworkPhidgetInfoHandle phidgetInfo;
};

#ifdef USE_ZEROCONF
int updateDNSTXTRecords();
int createDNSTXTRecord(CPhidgetHandle phid, const unsigned char *txt_buf, int *len);
int dns_callback_thread(void *ref);
#endif

int start_phidget(pds_session_t *pdss);
int stop_phidgets();
int open_phidget(void *pdss, int deviceID, long serialNumber, const char *label, const char *ipaddr, const char *port);
int close_phidget(void *pdss, int deviceID, long serialNumber, const char *label, const char *ipaddr, const char *port);

int findNetworkPhidgetInfoHandleInList(CNetworkPhidgetListHandle list, long serialNumber, const char *label, int deviceID, CNetworkPhidgetInfoHandle *phid);
int findClientInfoHandleInList(CClientListHandle list, const char *ipaddr, const char *port, CClientInfoHandle *client);

extern CClientListHandle ConnectedClients;
extern CNetworkPhidgetListHandle OpenPhidgets;
extern regex_t phidgetsetex;
extern regex_t phidgetopencloseex;
extern pthread_mutex_t PhidgetsAndClientsMutex;

extern CPhidgetManagerHandle phidm;

#endif
