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

#ifndef __PHIDGETWEBSERVICE21
#define __PHIDGETWEBSERVICE21



extern pthread_mutex_t pd_mutex;
 void
pd_lock(void *pd_lock);
 void
pd_unlock(void *pd_lock);

int add_key(pds_session_t *pdss, const char *key, const char *val);
int remove_key(pds_session_t *pdss, const char *key);
int add_listener(pds_session_t *pdss, const char *kpat, pdl_notify_func_t notfiy, void *ptr);

extern int port;
extern char *serverName;
extern const char *password;
extern const char *protocol_ver;

#endif
