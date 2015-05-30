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
#include "phidgetinterface.h"
#include "cphidgetlist.h"
#include "pdictserver.h"
#include "pdict.h"
#include "utils.h"
#include "eventhandlers.h"
#include "PhidgetWebservice21.h"
#ifdef USE_ZEROCONF
#include "zeroconf.h"
#endif

pthread_mutex_t PhidgetsAndClientsMutex;
CClientListHandle ConnectedClients = NULL;
CNetworkPhidgetListHandle OpenPhidgets = NULL;

CPhidgetManagerHandle phidm;

int Initialized = PFALSE;
regex_t phidgetsetex;
regex_t phidgetopencloseex;

int CNetworkPhidgetInfo_areEqual(void *arg1, void *arg2)
{
	CNetworkPhidgetInfoHandle phid1 = (CNetworkPhidgetInfoHandle)arg1;
	CNetworkPhidgetInfoHandle phid2 = (CNetworkPhidgetInfoHandle)arg2;
	
	if(!phid1||!phid2||!phid1->phidget||!phid2->phidget)
		return EPHIDGET_INVALIDARG;
		
	return(CPhidget_areEqual(phid1->phidget, phid2->phidget));
}

void CNetworkPhidgetInfo_free(void *arg)
{
	CNetworkPhidgetInfoHandle phid = (CNetworkPhidgetInfoHandle)arg;
	
	CList_emptyList((CListHandle *)&phid->clients, PFALSE, NULL);
	free(phid->phidget->escapedLabel); phid->phidget->escapedLabel = NULL;
	CPhidget_free(phid->phidget); phid->phidget = NULL;
	free(phid);
	
	return;
}

void CClientInfo_free(void *arg)
{
	CClientInfoHandle client = (CClientInfoHandle)arg;
	
	CList_emptyList((CListHandle *)&client->phidgets, PFALSE, NULL);
	if(client->client->port) free(client->client->port); client->client->port=NULL;
	if(client->client->address) free(client->client->address); client->client->address=NULL;
	CPhidgetSocketClient_free(client->client); client->client = NULL;
	free(client);
	
	return;

}

int CClientInfo_areEqual(void *arg1, void *arg2)
{
	CClientInfoHandle client1 = (CClientInfoHandle)arg1;
	CClientInfoHandle client2 = (CClientInfoHandle)arg2;
	
	if(!client1||!client2||!client1->client||!client2->client)
		return EPHIDGET_INVALIDARG;
		
	return(CPhidgetSocketClient_areEqual(client1->client, client2->client));
}


int findNetworkPhidgetInfoHandleInList(CNetworkPhidgetListHandle list, long serialNumber, const char *label, int deviceID, CNetworkPhidgetInfoHandle *phid)
{
	CPhidgetHandle phidtemp;
	CNetworkPhidgetInfo netphidtemp2;
	int result;
	
	if((result = CPhidget_create(&phidtemp))) return result;
	
	netphidtemp2.phidget = phidtemp;

	phidtemp->deviceID = deviceID;
	phidtemp->serialNumber = serialNumber;
	if(label)
	{
		phidtemp->specificDevice = PHIDGETOPEN_LABEL;
		memcpy(phidtemp->label, label, (strlen(label) > (MAX_LABEL_STORAGE-1) ? (MAX_LABEL_STORAGE-1) : strlen(label)) + 1);
	}
	else
	{
		phidtemp->specificDevice = (serialNumber == -1 ? PHIDGETOPEN_ANY : PHIDGETOPEN_SERIAL);
	}
	
	result = CList_findInList((CListHandle)list, &netphidtemp2, CNetworkPhidgetInfo_areEqual, (void **)phid);
	
	CPhidget_free(phidtemp); phidtemp = NULL;
	return result;
}

int findClientInfoHandleInList(CClientListHandle list, const char *ipaddr, const char *port, CClientInfoHandle *client)
{
	CPhidgetSocketClientHandle clienttemp;
	CClientInfo netclienttemp2;
	int result;

	if((result = CPhidgetSocketClient_create(&clienttemp))) return result;
	
	netclienttemp2.client = clienttemp;
	
	if(!(clienttemp->address = strdup(ipaddr)))
		return EPHIDGET_NOMEMORY;
	if(!(clienttemp->port = strdup(port)))
		return EPHIDGET_NOMEMORY;
		
	result = CList_findInList((CListHandle)list, &netclienttemp2, CClientInfo_areEqual, (void **)client);

	//have to free these here because I created them here
	//trying to free them in the CPhidgetSocketClient_free (which is in the dll) causes heap unhappiness
	free(clienttemp->address); clienttemp->address = NULL;
	free(clienttemp->port); clienttemp->port = NULL;
	
	CPhidgetSocketClient_free(clienttemp); clienttemp = NULL;
	return result;
}

int kill_event_handlers(CPhidgetHandle phid)
{
	switch(phid->deviceID)
	{
		case PHIDCLASS_ACCELEROMETER: 
			CPhidgetAccelerometer_set_OnAccelerationChange_Handler((CPhidgetAccelerometerHandle)phid, NULL, NULL);
			break; 
		case PHIDCLASS_ADVANCEDSERVO: 
			CPhidgetAdvancedServo_set_OnPositionChange_Handler((CPhidgetAdvancedServoHandle)phid, NULL, NULL);
			CPhidgetAdvancedServo_set_OnVelocityChange_Handler((CPhidgetAdvancedServoHandle)phid, NULL, NULL);
			CPhidgetAdvancedServo_set_OnCurrentChange_Handler((CPhidgetAdvancedServoHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_ANALOG:
			break;
		case PHIDCLASS_BRIDGE:
			CPhidgetBridge_set_OnBridgeData_Handler((CPhidgetBridgeHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_ENCODER: 
			CPhidgetEncoder_set_OnInputChange_Handler((CPhidgetEncoderHandle)phid, NULL, NULL);
			CPhidgetEncoder_set_OnPositionChange_Handler((CPhidgetEncoderHandle)phid, NULL, NULL);
			CPhidgetEncoder_set_OnIndex_Handler((CPhidgetEncoderHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_FREQUENCYCOUNTER:
			CPhidgetFrequencyCounter_set_OnCount_Handler((CPhidgetFrequencyCounterHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_GENERIC:
			CPhidgetGeneric_set_OnPacket_Handler((CPhidgetGenericHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_GPS: 
			CPhidgetGPS_set_OnPositionChange_Handler((CPhidgetGPSHandle)phid, NULL, NULL);
			CPhidgetGPS_set_OnPositionFixStatusChange_Handler((CPhidgetGPSHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_INTERFACEKIT:
			CPhidgetInterfaceKit_set_OnInputChange_Handler((CPhidgetInterfaceKitHandle)phid, NULL, NULL);
			CPhidgetInterfaceKit_set_OnOutputChange_Handler((CPhidgetInterfaceKitHandle)phid, NULL, NULL);
			CPhidgetInterfaceKit_set_OnSensorChange_Handler((CPhidgetInterfaceKitHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_IR:
			CPhidgetIR_set_OnCode_Handler((CPhidgetIRHandle)phid, NULL, NULL);
			CPhidgetIR_set_OnRawData_Handler((CPhidgetIRHandle)phid, NULL, NULL);
			CPhidgetIR_set_OnLearn_Handler((CPhidgetIRHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_LED: 
			break;
		case PHIDCLASS_MOTORCONTROL: 
			CPhidgetMotorControl_set_OnInputChange_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			CPhidgetMotorControl_set_OnVelocityChange_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			CPhidgetMotorControl_set_OnCurrentChange_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			CPhidgetMotorControl_set_OnEncoderPositionChange_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			CPhidgetMotorControl_set_OnEncoderPositionUpdate_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			CPhidgetMotorControl_set_OnBackEMFUpdate_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			CPhidgetMotorControl_set_OnSensorUpdate_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			CPhidgetMotorControl_set_OnCurrentUpdate_Handler((CPhidgetMotorControlHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_PHSENSOR: 
			CPhidgetPHSensor_set_OnPHChange_Handler((CPhidgetPHSensorHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_RFID: 
			CPhidgetRFID_set_OnTag2_Handler((CPhidgetRFIDHandle)phid, NULL, NULL);
			CPhidgetRFID_set_OnTagLost2_Handler((CPhidgetRFIDHandle)phid, NULL, NULL);
			CPhidgetRFID_set_OnOutputChange_Handler((CPhidgetRFIDHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_SERVO: 
			CPhidgetServo_set_OnPositionChange_Handler((CPhidgetServoHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_SPATIAL:
			CPhidgetSpatial_set_OnSpatialData_Handler((CPhidgetSpatialHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_STEPPER: 
			CPhidgetStepper_set_OnInputChange_Handler((CPhidgetStepperHandle)phid, NULL, NULL);
			CPhidgetStepper_set_OnPositionChange_Handler((CPhidgetStepperHandle)phid, NULL, NULL);
			CPhidgetStepper_set_OnVelocityChange_Handler((CPhidgetStepperHandle)phid, NULL, NULL);
			CPhidgetStepper_set_OnCurrentChange_Handler((CPhidgetStepperHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_TEMPERATURESENSOR: 
			CPhidgetTemperatureSensor_set_OnTemperatureChange_Handler((CPhidgetTemperatureSensorHandle)phid, NULL, NULL);
			break;
		case PHIDCLASS_TEXTLCD: 
			break;
		case PHIDCLASS_TEXTLED: 
			break;
		case PHIDCLASS_WEIGHTSENSOR: 
			CPhidgetWeightSensor_set_OnWeightChange_Handler((CPhidgetWeightSensorHandle)phid, NULL, NULL);
			break;
		default:
			break;
	}
	
	CPhidget_set_OnAttach_Handler(phid, NULL, NULL);
	CPhidget_set_OnDetach_Handler(phid, NULL, NULL);
	CPhidget_set_OnError_Handler(phid, NULL, NULL);
	
	return EPHIDGET_OK;
}

int close_phidget(void *pdss, int deviceID, long serialNumber, const char *label, const char *ipaddr, const char *port) {
	int result;
	CNetworkPhidgetInfoHandle newPhid;
	CClientInfoHandle newClient;

	pu_log(PUL_INFO, ((pds_session_t *)pdss)->pdss_id, "In close Phidget");

	result = findNetworkPhidgetInfoHandleInList(OpenPhidgets, serialNumber, label, deviceID, &newPhid);
		
	switch(result)
	{
		case EPHIDGET_OK: //device was found
		
			if(!findClientInfoHandleInList(newPhid->clients, ipaddr, port, &newClient))
			{
				/* Here we remove this client from this Phidget's client list */
				CList_removeFromList((CListHandle *)&newPhid->clients, newClient, CClientInfo_areEqual, PFALSE, NULL);
				/* Here we remove this phidget from this client's phidget list */
				CList_removeFromList((CListHandle *)&newClient->phidgets, newPhid, CNetworkPhidgetInfo_areEqual, PFALSE, NULL);
			}
			else
			{
				return EPHIDGET_NOTATTACHED; //this phidget is not opened by this client
			}

			/* no more clients, we can close */
			if(!newPhid->clients)
			{
				int ret;
				char key[MAX_KEY_SIZE];

				DPRINT(PUL_INFO, "Actually closing Phidget");

				snprintf(key, MAX_KEY_SIZE, "^/PCK/%s/%d", newPhid->phidget->deviceType, newPhid->phidget->serialNumber);
				if((ret = remove_key(pdss, key)))
					return ret;

				snprintf(key, MAX_KEY_SIZE, "^/PSK/%s/[a-zA-Z_0-9/.\\\\-]*/%d", newPhid->phidget->deviceType, newPhid->phidget->serialNumber);
				if((ret = remove_key(pdss, key)))
					return ret;

				kill_event_handlers(newPhid->phidget);

				//why do I unlock this?????
				//because if there is a read waiting to add a key, this will deadlock waiting for the read thread to exit
				//but this is not safe! because another close can come along and claim pd_mutex, and then deadlock waiting for 
				//PhidgetsAndClientsMutex, which is owned by this thread, which will now be waiting for pd_mutex.
				//so: we are moving this to the open_close event handler - so that pd_mutex is unlocked before PhidgetsAndClientsMutex is locked.
				//pd_unlock((void *)&pd_mutex);
				CPhidget_close(newPhid->phidget);
				CList_removeFromList((CListHandle *)&OpenPhidgets, newPhid, CNetworkPhidgetInfo_areEqual, PTRUE, CNetworkPhidgetInfo_free);
				//pd_lock((void *)&pd_mutex);
			}
			break;
		case EPHIDGET_NOTFOUND:
			return EPHIDGET_NOTFOUND;
		default:
			return result;
	}
	
	/* free this client if it doesn't have any more open phidgets */
	if(!newClient->phidgets)
		CList_removeFromList((CListHandle *)&ConnectedClients, newClient, CClientInfo_areEqual, PTRUE, CClientInfo_free);
	
	return EPHIDGET_OK;
}

int open_phidget(void *pdss, int deviceID, long serialNumber, const char *label, const char *ipaddr, const char *port) {
	int result, result2;
	CNetworkPhidgetInfoHandle newNetPhid;
	CClientInfoHandle newClient;

	result = findNetworkPhidgetInfoHandleInList(OpenPhidgets, serialNumber, label, deviceID, &newNetPhid);
	result2 = findClientInfoHandleInList(ConnectedClients, ipaddr, port, &newClient);
	
	DPRINT(PUL_INFO, "Open Phidget");

	switch(result)
	{
		case EPHIDGET_NOTFOUND:
		{
			CPhidgetHandle newPhid = NULL;
			switch(deviceID)
			{
			case PHIDCLASS_ACCELEROMETER: 
				CPhidgetAccelerometer_create((CPhidgetAccelerometerHandle *)&newPhid);
				CPhidgetAccelerometer_set_OnAccelerationChange_Handler((CPhidgetAccelerometerHandle)newPhid, AccelerometerAccelerationChange, pdss);
				break; 
			case PHIDCLASS_ADVANCEDSERVO: 
				CPhidgetAdvancedServo_create((CPhidgetAdvancedServoHandle *)&newPhid);
				CPhidgetAdvancedServo_set_OnPositionChange_Handler((CPhidgetAdvancedServoHandle)newPhid, AdvancedServoPositionChange, pdss);
				CPhidgetAdvancedServo_set_OnVelocityChange_Handler((CPhidgetAdvancedServoHandle)newPhid, AdvancedServoVelocityChange, pdss);
				CPhidgetAdvancedServo_set_OnCurrentChange_Handler((CPhidgetAdvancedServoHandle)newPhid, AdvancedServoCurrentChange, pdss);
				break;
			case PHIDCLASS_ANALOG:
				CPhidgetAnalog_create((CPhidgetAnalogHandle *)&newPhid);
				break;
			case PHIDCLASS_BRIDGE:
				CPhidgetBridge_create((CPhidgetBridgeHandle *)&newPhid);
				CPhidgetBridge_set_OnBridgeData_Handler((CPhidgetBridgeHandle)newPhid, BridgeBridgeData, pdss);
				break;
			case PHIDCLASS_ENCODER: 
				CPhidgetEncoder_create((CPhidgetEncoderHandle *)&newPhid);
				CPhidgetEncoder_set_OnInputChange_Handler((CPhidgetEncoderHandle)newPhid, EncoderInputChange, pdss);
				CPhidgetEncoder_set_OnPositionChange_Handler((CPhidgetEncoderHandle)newPhid, EncoderPositionChange, pdss);
				CPhidgetEncoder_set_OnIndex_Handler((CPhidgetEncoderHandle)newPhid, EncoderIndex, pdss);
				break;
			case PHIDCLASS_FREQUENCYCOUNTER:
				CPhidgetFrequencyCounter_create((CPhidgetFrequencyCounterHandle *)&newPhid);
				CPhidgetFrequencyCounter_set_OnCount_Handler((CPhidgetFrequencyCounterHandle)newPhid, FrequencyCounterCount, pdss);
				break;
			case PHIDCLASS_GENERIC:
				CPhidgetGeneric_create((CPhidgetGenericHandle *)&newPhid);
				CPhidgetGeneric_set_OnPacket_Handler((CPhidgetGenericHandle)newPhid, GenericPacket, pdss);
				break;
			case PHIDCLASS_GPS: 
				CPhidgetGPS_create((CPhidgetGPSHandle *)&newPhid);
				CPhidgetGPS_set_OnPositionChange_Handler((CPhidgetGPSHandle)newPhid, GPSPositionChange, pdss);
				CPhidgetGPS_set_OnPositionFixStatusChange_Handler((CPhidgetGPSHandle)newPhid, GPSPositionFixStatusChange, pdss);
				break;
			case PHIDCLASS_INTERFACEKIT:
				CPhidgetInterfaceKit_create((CPhidgetInterfaceKitHandle *)&newPhid);
				CPhidgetInterfaceKit_set_OnInputChange_Handler((CPhidgetInterfaceKitHandle)newPhid, InterfaceKitInputChange, pdss);
				CPhidgetInterfaceKit_set_OnOutputChange_Handler((CPhidgetInterfaceKitHandle)newPhid, InterfaceKitOutputChange, pdss);
				CPhidgetInterfaceKit_set_OnSensorChange_Handler((CPhidgetInterfaceKitHandle)newPhid, InterfaceKitSensorChange, pdss);
				break;			
			case PHIDCLASS_IR:
				CPhidgetIR_create((CPhidgetIRHandle *)&newPhid);
				CPhidgetIR_set_OnCode_Handler((CPhidgetIRHandle)newPhid, IRCode, pdss);
				CPhidgetIR_set_OnRawData_Handler((CPhidgetIRHandle)newPhid, IRRawData, pdss);
				CPhidgetIR_set_OnLearn_Handler((CPhidgetIRHandle)newPhid, IRLearn, pdss);
				break;
			case PHIDCLASS_LED: 
				CPhidgetLED_create((CPhidgetLEDHandle *)&newPhid);
				break;
			case PHIDCLASS_MOTORCONTROL: 
				CPhidgetMotorControl_create((CPhidgetMotorControlHandle *)&newPhid);
				CPhidgetMotorControl_set_OnInputChange_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlInputChange, pdss);
				CPhidgetMotorControl_set_OnVelocityChange_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlVelocityChange, pdss);
				CPhidgetMotorControl_set_OnCurrentChange_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlCurrentChange, pdss);
				CPhidgetMotorControl_set_OnEncoderPositionChange_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlEncoderPositionChange, pdss);
				CPhidgetMotorControl_set_OnEncoderPositionUpdate_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlEncoderPositionUpdate, pdss);
				CPhidgetMotorControl_set_OnBackEMFUpdate_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlBackEMFUpdate, pdss);
				CPhidgetMotorControl_set_OnSensorUpdate_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlSensorUpdate, pdss);
				CPhidgetMotorControl_set_OnCurrentUpdate_Handler((CPhidgetMotorControlHandle)newPhid, MotorControlCurrentUpdate, pdss);
				break;
			case PHIDCLASS_PHSENSOR: 
				CPhidgetPHSensor_create((CPhidgetPHSensorHandle *)&newPhid);
				CPhidgetPHSensor_set_OnPHChange_Handler((CPhidgetPHSensorHandle)newPhid, PHSensorPHChange, pdss);
				break;
			case PHIDCLASS_RFID: 
				CPhidgetRFID_create((CPhidgetRFIDHandle *)&newPhid);
				CPhidgetRFID_set_OnTag2_Handler((CPhidgetRFIDHandle)newPhid, RFIDTag2, pdss);
				CPhidgetRFID_set_OnTagLost2_Handler((CPhidgetRFIDHandle)newPhid, RFIDTagLost2, pdss);
				CPhidgetRFID_set_OnOutputChange_Handler((CPhidgetRFIDHandle)newPhid, RFIDOutputChange, pdss);
				break;
			case PHIDCLASS_SERVO: 
				CPhidgetServo_create((CPhidgetServoHandle *)&newPhid);
				CPhidgetServo_set_OnPositionChange_Handler((CPhidgetServoHandle)newPhid, ServoPositionChange, pdss);
				break;
			case PHIDCLASS_SPATIAL:
				CPhidgetSpatial_create((CPhidgetSpatialHandle *)&newPhid);
				CPhidgetSpatial_set_OnSpatialData_Handler((CPhidgetSpatialHandle)newPhid, SpatialSpatialData, pdss);
				break;
			case PHIDCLASS_STEPPER: 
				CPhidgetStepper_create((CPhidgetStepperHandle *)&newPhid);
				CPhidgetStepper_set_OnInputChange_Handler((CPhidgetStepperHandle)newPhid, StepperInputChange, pdss);
				CPhidgetStepper_set_OnPositionChange_Handler((CPhidgetStepperHandle)newPhid, StepperPositionChange, pdss);
				CPhidgetStepper_set_OnVelocityChange_Handler((CPhidgetStepperHandle)newPhid, StepperVelocityChange, pdss);
				CPhidgetStepper_set_OnCurrentChange_Handler((CPhidgetStepperHandle)newPhid, StepperCurrentChange, pdss);
				break;
			case PHIDCLASS_TEMPERATURESENSOR: 
				CPhidgetTemperatureSensor_create((CPhidgetTemperatureSensorHandle *)&newPhid);
				CPhidgetTemperatureSensor_set_OnTemperatureChange_Handler((CPhidgetTemperatureSensorHandle)newPhid, TemperatureSensorTemperatureChange, pdss);
				break;
			case PHIDCLASS_TEXTLCD: 
				CPhidgetTextLCD_create((CPhidgetTextLCDHandle *)&newPhid);
				break;
			case PHIDCLASS_TEXTLED: 
				CPhidgetTextLED_create((CPhidgetTextLEDHandle *)&newPhid);
				break;
			case PHIDCLASS_WEIGHTSENSOR: 
				CPhidgetWeightSensor_create((CPhidgetWeightSensorHandle *)&newPhid);
				CPhidgetWeightSensor_set_OnWeightChange_Handler((CPhidgetWeightSensorHandle)newPhid, WeightSensorWeightChange, pdss);
				break;
			default:
				break;
			}
			
			CPhidget_set_OnAttach_Handler(newPhid, Attach, pdss);
			CPhidget_set_OnDetach_Handler(newPhid, Detach, pdss);
			CPhidget_set_OnError_Handler(newPhid, Error, pdss);
			
			if(!(newNetPhid = malloc(sizeof(CNetworkPhidgetInfo))))
			{
				return EPHIDGET_NOMEMORY;
			}
			ZEROMEM(newNetPhid, sizeof(CNetworkPhidgetInfo));
			
			newNetPhid->phidget = newPhid;

			if(label)
				CPhidget_openLabel(newPhid, label);
			else
				CPhidget_open(newPhid, serialNumber);
				
			CList_addToList((CListHandle *)&OpenPhidgets, newNetPhid, CNetworkPhidgetInfo_areEqual);
		}
		case EPHIDGET_OK: //device was found
			switch(result2)
			{
				case EPHIDGET_NOTFOUND: /* if the client wasn't found in the client list, add it here */
				{
					if(!(newClient = malloc(sizeof(CClientInfo))))
					{
						return EPHIDGET_NOMEMORY;
					}
					ZEROMEM(newClient, sizeof(CClientInfo));
					if((result = CPhidgetSocketClient_create(&newClient->client)))
					{
						return result;
					}
					if(!(newClient->client->address = strdup(ipaddr)))
					{
						return EPHIDGET_NOMEMORY;
					}
					if(!(newClient->client->port = strdup(port)))
					{
						return EPHIDGET_NOMEMORY;
					}
						
					CList_addToList((CListHandle *)&ConnectedClients, newClient, CClientInfo_areEqual);
				}
				case EPHIDGET_OK:
					CList_addToList((CListHandle *)&newNetPhid->clients, newClient, CClientInfo_areEqual);
					CList_addToList((CListHandle *)&newClient->phidgets, newNetPhid, CNetworkPhidgetInfo_areEqual);
					break;
				default:
					return result2;
			}
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	return EPHIDGET_OK;
}

int CCONV manager_attach_handler(CPhidgetHandle phid, void *pdss) {
	int ret;
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];

	snprintf(key, MAX_KEY_SIZE, "/PSK/List/%s/%d", 
	phid->deviceType, phid->serialNumber);
	snprintf(val, MAX_VAL_SIZE, "Attached Version=%d ID=%d Label=%s", phid->deviceVersion, phid->deviceIDSpec, phid->label);
	if((ret = add_key(pdss, key, val))) 
		return ret;
	
#ifdef USE_ZEROCONF
	zeroconf_advertise_phidget(phid);
#endif

	return EPHIDGET_OK;
}

int CCONV manager_detach_handler(CPhidgetHandle phid, void *pdss) {
	int ret;
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];

	snprintf(key, MAX_KEY_SIZE, "/PSK/List/%s/%d", 
	phid->deviceType, phid->serialNumber);
	snprintf(val, MAX_VAL_SIZE, "Detached Version=%d ID=%d Label=%s", phid->deviceVersion, phid->deviceIDSpec, phid->label);
	if((ret = add_key(pdss, key, val))) 
		return ret;

#ifdef USE_ZEROCONF
	zeroconf_unadvertise_phidget(phid);
#endif

	return EPHIDGET_OK;
}

int start_phidget(pds_session_t *pdss)
{
	int res;
	const char *setpattern = "^/PCK/([a-zA-Z_0-9]*)/([0-9]*)/([a-zA-Z_0-9]*)/?([a-zA-Z_0-9]*)/?([a-zA-Z_0-9]*)$";
	const char *openclosepattern = "^/PCK/Client/([a-zA-Z_0-9\\.:%]*)/([0-9]*)/?([a-zA-Z_0-9]*)/?([-a-zA-Z_0-9]*)/?(.*)$";
	if(!Initialized)
	{
		if ((res = regcomp(&phidgetsetex, setpattern, REG_EXTENDED)) != 0) {
			fprintf(stderr, "set command pattern compilation error %d\n",
				res);
			abort();
		}
		if ((res = regcomp(&phidgetopencloseex, openclosepattern, REG_EXTENDED)) !=  0) {
			fprintf(stderr, "openclose command pattern compilation error %d\n",
				res);
			abort();
		}
		if(pthread_mutex_init(&PhidgetsAndClientsMutex, NULL) != 0)
			return EPHIDGET_UNEXPECTED;
		Initialized = PTRUE;
		add_listener(pdss, "^/PCK/Phidget", phidget_set, pdss);
		add_listener(pdss, "^/PCK/Client/", phidget_openclose, pdss);

#ifdef USE_ZEROCONF
		zeroconf_advertise_ws();
#endif
		CPhidgetManager_create(&phidm);
		CPhidgetManager_set_OnAttach_Handler(phidm, manager_attach_handler, pdss);
		CPhidgetManager_set_OnDetach_Handler(phidm, manager_detach_handler, pdss);
		CPhidgetManager_open(phidm);
	}
	return 0;
}

int stop_phidgets()
{
	if(Initialized)
	{
		pu_log(PUL_INFO, 0, "In stop_phidgets");
		
		CPhidgetManager_close(phidm);
		CPhidgetManager_delete(phidm);
		
#ifdef USE_ZEROCONF
		UninitializeZeroconf();
#endif
	}
	return 0;
}
