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

#ifndef _EVENTHANDLERS
#define _EVENTHANDLERS
#include "stdafx.h"

int CCONV Attach(CPhidgetHandle phid, void *pdss);
int CCONV Detach(CPhidgetHandle phid, void *pdss);
int CCONV Error(CPhidgetHandle phid, void *userPtr, int errorCode, const char *errorString);
int CCONV AccelerometerAccelerationChange(CPhidgetAccelerometerHandle phid, void *pdss, int Index, double Val);
int CCONV AdvancedServoPositionChange(CPhidgetAdvancedServoHandle phid, void *pdss, int Index, double Val);
int CCONV AdvancedServoVelocityChange(CPhidgetAdvancedServoHandle phid, void *pdss, int Index, double Val);
int CCONV AdvancedServoCurrentChange(CPhidgetAdvancedServoHandle phid, void *pdss, int Index, double Val);
int CCONV BridgeBridgeData(CPhidgetBridgeHandle phid, void *userPtr, int index, double value);
int CCONV EncoderInputChange(CPhidgetEncoderHandle phid, void *pdss, int Index, int Val);
int CCONV EncoderPositionChange(CPhidgetEncoderHandle phid, void *pdss, int Index, int Time, int PositionChange);
int CCONV EncoderIndex(CPhidgetEncoderHandle phid, void *pdss, int Index, int Position);
int CCONV FrequencyCounterCount(CPhidgetFrequencyCounterHandle phid, void *userPtr, int index, int time, int counts);
int CCONV GenericPacket(CPhidgetGenericHandle phid, void *pdss, const unsigned char *packet, int length);
int CCONV GPSPositionChange(CPhidgetGPSHandle phid, void *userPtr, double latitude, double longitude, double altitude);
int CCONV GPSPositionFixStatusChange(CPhidgetGPSHandle phid, void *userPtr, int status);
int CCONV InterfaceKitInputChange(CPhidgetInterfaceKitHandle phid, void *pdss, int Index, int Val);
int CCONV InterfaceKitOutputChange(CPhidgetInterfaceKitHandle phid, void *pdss, int Index, int Val);
int CCONV InterfaceKitSensorChange(CPhidgetInterfaceKitHandle phid, void *pdss, int Index, int Val);
int CCONV IRCode(CPhidgetIRHandle phid, void *pdss, unsigned char *data, int dataLength, int bitCount, int repeat);
int CCONV IRRawData(CPhidgetIRHandle phid, void *pdss, int *data, int dataLength);
int CCONV IRLearn(CPhidgetIRHandle phid, void *pdss, unsigned char *data, int dataLength, CPhidgetIR_CodeInfoHandle codeInfo);
int CCONV MotorControlInputChange(CPhidgetMotorControlHandle phid, void *pdss, int Index, int Val);
int CCONV MotorControlVelocityChange(CPhidgetMotorControlHandle phid, void *pdss, int Index, double Val);
int CCONV MotorControlCurrentChange(CPhidgetMotorControlHandle phid, void *pdss, int Index, double val);
int CCONV MotorControlEncoderPositionChange(CPhidgetMotorControlHandle phid, void *userPtr, int index, int time, int positionChange);
int CCONV MotorControlEncoderPositionUpdate(CPhidgetMotorControlHandle phid, void *userPtr, int index, int positionChange);
int CCONV MotorControlBackEMFUpdate(CPhidgetMotorControlHandle phid, void *userPtr, int index, double voltage);
int CCONV MotorControlSensorUpdate(CPhidgetMotorControlHandle phid, void *userPtr, int index, int sensorValue);
int CCONV MotorControlCurrentUpdate(CPhidgetMotorControlHandle phid, void *userPtr, int index, double current);
int CCONV PHSensorPHChange(CPhidgetPHSensorHandle phid, void *pdss, double Val);
int CCONV RFIDTag2(CPhidgetRFIDHandle phid, void *pdss, char *Tag, CPhidgetRFID_Protocol protocol);
int CCONV RFIDTagLost2(CPhidgetRFIDHandle phid, void *pdss, char *Tag, CPhidgetRFID_Protocol protocol);
int CCONV RFIDOutputChange(CPhidgetRFIDHandle phid, void *pdss, int Index, int Val);
int CCONV ServoPositionChange(CPhidgetServoHandle phid, void *pdss, int Index, double Val);
int CCONV SpatialSpatialData(CPhidgetSpatialHandle phid, void *pdss, CPhidgetSpatial_SpatialEventDataHandle *data, int dataCount);
int CCONV StepperInputChange(CPhidgetStepperHandle phid, void *pdss, int Index, int Val);
int CCONV StepperPositionChange(CPhidgetStepperHandle phid, void *pdss, int Index, long long Val);
int CCONV StepperVelocityChange(CPhidgetStepperHandle phid, void *pdss, int Index, double Val);
int CCONV StepperCurrentChange(CPhidgetStepperHandle phid, void *pdss, int Index, double Val);
int CCONV TemperatureSensorTemperatureChange(CPhidgetTemperatureSensorHandle phid, void *pdss, int Index, double Val);
int CCONV WeightSensorWeightChange(CPhidgetWeightSensorHandle phid, void *pdss, double Val);

void phidget_set(const char *k, const char *v, pdict_reason_t r, const char *pde_oldval, void *arg);
void phidget_openclose(const char *k, const char *v, pdict_reason_t r, const char *pde_oldval, void *arg);

//MACROS

#define PWS_KEYVAL_STORAGE \
	int ret=EPHIDGET_OK; \
	char key[MAX_KEY_SIZE]; \
	char val[MAX_VAL_SIZE]; 

//Function starts

#define PWS_INITKEYS(pname) \
int phidget##pname##_initkeys(CPhidgetHandle arg, pds_session_t *pdss, int *initKeys) \
{ \
	PWS_KEYVAL_STORAGE \
	CPhidget##pname##Handle phid = (CPhidget##pname##Handle)arg;

#define PWS_EVENT(pname,param,...) \
int CCONV pname##param(CPhidget##pname##Handle phid, void *pdss, __VA_ARGS__) \
{ \
	PWS_KEYVAL_STORAGE

#define PWS_EVENT_INDEXED(pname,param,...) \
int CCONV pname##param(CPhidget##pname##Handle phid, void *pdss, int index, __VA_ARGS__) \
{ \
	PWS_KEYVAL_STORAGE

#define PWS_SETKEYS(pname) \
int phidget##pname##_set(CPhidgetHandle arg, const char *setThing, int index, const char *state, void *pdss) \
{ \
	int ret=EPHIDGET_OK; \
	char key[MAX_KEY_SIZE]; \
	CPhidget##pname##Handle phid = (CPhidget##pname##Handle)arg;

//Inline functions

#define PWS_SET_KEY(kname, kvaltype, ...) \
	do { \
	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/" #kname , phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber); \
	snprintf(val, MAX_VAL_SIZE, kvaltype, __VA_ARGS__); \
	if((ret = add_key(pdss, key, val)) != EPHIDGET_OK) \
		goto exit; \
	}while(0)

#define PWS_SET_KEY_GENERIC(kname, kvaltype, ...) \
	do { \
	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/" #kname , phid->deviceType, (phid->escapedLabel ? phid->escapedLabel : ""), phid->serialNumber); \
	snprintf(val, MAX_VAL_SIZE, kvaltype, __VA_ARGS__); \
	if((ret = add_key(pdss, key, val)) != EPHIDGET_OK) \
		goto exit; \
	}while(0)

#define PWS_SET_KEY_OLDVAL(kname) \
	do { \
	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/" #kname , phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber); \
	if((ret = add_key(pdss, key, state)) != EPHIDGET_OK) \
		goto exit; \
	}while(0)

#define PWS_SET_KEY_INDEXED(kname, kvaltype, ...) \
	do { \
	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/" #kname "/%d", phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber, index); \
	snprintf(val, MAX_VAL_SIZE, kvaltype, __VA_ARGS__); \
	if((ret = add_key(pdss, key, val)) != EPHIDGET_OK) \
		goto exit; \
	}while(0)

#define PWS_SET_KEY_INDEXED_OLDVAL(kname) \
	do { \
	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/" #kname "/%d", phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber, index); \
	if((ret = add_key(pdss, key, state)) != EPHIDGET_OK) \
		goto exit; \
	}while(0)

//These are defined in cphidgetmacros.h - don't redefine here
//#define GET_DOUBLE_VAL double value = strtod(state, NULL)
//#define GET_INT_VAL int value = strtol(state, NULL, 10)
//#define GET_INT64_VAL __int64 value = strtoll(state, NULL, 10)
//#define KEYNAME(name) !strncmp(setThing, name, sizeof(name))

#define EXIT_ON_ERR(func) if( ( ret = func ) != EPHIDGET_OK ) goto exit

#define PWS_BAD_SETTYPE(pname) \
	do { \
		pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Bad setType for " #pname ": %s\n", setThing); \
			return EPHIDGET_INVALIDARG; \
	}while(0)

//Function ends

#define PWS_END \
	goto exit; \
exit: \
	return ret;

#endif
