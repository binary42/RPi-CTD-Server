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

#ifndef _ZEROCONF_H_
#define _ZEROCONF_H_

int zeroconf_advertise_ws();
int zeroconf_unadvertise_phidget(CPhidgetHandle phid);
int zeroconf_advertise_phidget(CPhidgetHandle phid);

int InitializeZeroconf();
int UninitializeZeroconf();

extern const char *dnssd_phidget_ws_txt_ver;
extern const char *dnssd_phidget_txt_ver;

#endif
