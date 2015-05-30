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

/* All event handlers for the various devices */
#include "stdafx.h"
#include "phidgetinterface.h"
#include "cphidgetlist.h"
#include "pdictserver.h"
#include "pdict.h"
#include "utils.h"
#include "eventhandlers.h"
#include "PhidgetWebservice21.h"
#include "zeroconf.h"

/* 
	initkey functions:
	We send every key that wouldn't be sent by an event handler - those are sent by initial events
	We set initKeys to every key we send, plus the initial events

	set functions:
	Events from client (Remote Phidget21)
*/

/*******************
 * Phidget General *
 *******************/
extern int(*fptrInitKeys[PHIDGET_DEVICE_CLASS_COUNT])(CPhidgetHandle phid, pds_session_t *pdss, int *initKeys);

int CCONV Attach(CPhidgetHandle phid, void *pdss)
{
	PWS_KEYVAL_STORAGE
	const char *name;
	//char *label = NULL;

	/* we send 6 keys in this function */
	int initKeys = 6;

	CPhidget_getDeviceName(phid, &name);

	//We need to store this in case the label is changed - we have to continue using the old one until a detach/close
	// so that openLabelRemote calls won't lose their Phidget.
	free(phid->escapedLabel); phid->escapedLabel = NULL;
	escape(phid->label, strlen(phid->label), &phid->escapedLabel);

	PWS_SET_KEY_GENERIC(Label, "%s", (phid->escapedLabel ? phid->escapedLabel : ""));
	PWS_SET_KEY_GENERIC(Version, "%d", phid->deviceVersion);
	PWS_SET_KEY_GENERIC(Name, "%s", name);
	PWS_SET_KEY_GENERIC(ID, "%d", phid->deviceIDSpec);
	PWS_SET_KEY_GENERIC(Status, "%s", "Attached");

	/* this also adds to initKeys */
	EXIT_ON_ERR( fptrInitKeys[phid->deviceID](phid, pdss, &initKeys) );

	PWS_SET_KEY_GENERIC(InitKeys, "%d", initKeys);
	PWS_END
}

int CCONV Detach(CPhidgetHandle phid, void *pdss)
{
	PWS_KEYVAL_STORAGE
	PWS_SET_KEY_GENERIC(Status, "%s", "Detached");

	snprintf(key, MAX_KEY_SIZE, "^/PCK/%s/%d", phid->deviceType, phid->serialNumber);
	EXIT_ON_ERR( remove_key(pdss, key) );

	snprintf(key, MAX_KEY_SIZE, "^/PSK/%s/[a-zA-Z_0-9/.\\\\-]*/%d", phid->deviceType, phid->serialNumber);
	EXIT_ON_ERR( remove_key(pdss, key) );

	PWS_END
}

int CCONV Error(CPhidgetHandle phid, void *pdss, int errorCode, const char *errorString)
{
	PWS_KEYVAL_STORAGE
	PWS_SET_KEY_GENERIC(Error, "%d/%s", errorCode, errorString);
	PWS_END
}

/*************************
 * Phidget Accelerometer *
 *************************/

PWS_INITKEYS(Accelerometer)
	int index=0;

	PWS_SET_KEY(NumberOfAxes, "%d", phid->phid.attr.accelerometer.numAxis);
	PWS_SET_KEY(AccelerationMin, "%lE", phid->accelerationMin);
	PWS_SET_KEY(AccelerationMax, "%lE", phid->accelerationMax);

	*initKeys += 3;

	for (index = 0; index<phid->phid.attr.accelerometer.numAxis; index++)
	{
		PWS_SET_KEY_INDEXED(Trigger, "%lE", phid->axisChangeTrigger[index]);

		*initKeys += 1;
	}

	/* acceleration events */
	*initKeys += phid->phid.attr.accelerometer.numAxis;

	PWS_END
}

PWS_SETKEYS(Accelerometer)
	if(KEYNAME("Trigger"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetAccelerometer_setAccelerationChangeTrigger(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Trigger);
	}
	else
	{
		PWS_BAD_SETTYPE(Accelerometer);
	}
	PWS_END
}

PWS_EVENT_INDEXED(Accelerometer, AccelerationChange, double Val)
	PWS_SET_KEY_INDEXED(Acceleration, "%lE", Val);
	PWS_END
}

/**************************
 * Phidget Advanced Servo *
 **************************/

PWS_INITKEYS(AdvancedServo)
	int index=0;

	PWS_SET_KEY(NumberOfMotors, "%d", phid->phid.attr.advancedservo.numMotors);
	PWS_SET_KEY(AccelerationMin, "%lE", phid->accelerationMin);
	PWS_SET_KEY(AccelerationMax, "%lE", phid->accelerationMax);
	PWS_SET_KEY(VelocityMin, "%lE", phid->velocityMin);
	PWS_SET_KEY(VelocityMaxLimit, "%lE", phid->velocityMaxLimit);
	PWS_SET_KEY(PositionMinLimit, "%lE", phid->motorPositionMinLimit);
	PWS_SET_KEY(PositionMaxLimit, "%lE", phid->motorPositionMaxLimit);
	
	*initKeys += 7;

	for(index=0;index<phid->phid.attr.advancedservo.numMotors;index++)
	{
		PWS_SET_KEY_INDEXED(PositionMin, "%lE", phid->motorPositionMin[index]);
		PWS_SET_KEY_INDEXED(PositionMax, "%lE", phid->motorPositionMax[index]);
		PWS_SET_KEY_INDEXED(Engaged, "%d", phid->motorEngagedStateEcho[index]);
		PWS_SET_KEY_INDEXED(SpeedRampingOn, "%d", phid->motorSpeedRampingStateEcho[index]);
		PWS_SET_KEY_INDEXED(Stopped, "%d", phid->motorStoppedState[index]);
		/* if the motor isn't engaged, there wouldn't be position events */
		PWS_SET_KEY_INDEXED(Position, "%lE", phid->motorPositionEcho[index]);
		PWS_SET_KEY_INDEXED(ServoParameters, "%d,%lE,%lE,%lE,%lE", phid->servoParams[index].servoType, phid->servoParams[index].min_us, phid->servoParams[index].max_us, phid->servoParams[index].us_per_degree, phid->servoParams[index].max_us_per_s);
		PWS_SET_KEY_INDEXED(VelocityMax, "%lE", phid->velocityMax[index]);

		*initKeys += 8;
	}

	/* Velocity, Current events */
	*initKeys += (2 * phid->phid.attr.advancedservo.numMotors);

	PWS_END
}

PWS_SETKEYS(AdvancedServo)
	if(KEYNAME("Acceleration"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetAdvancedServo_setAcceleration(phid, index, servo_us_to_degrees_vel(phid->servoParams[index], value, PTRUE)) );
		PWS_SET_KEY_INDEXED_OLDVAL(Acceleration);
	}
	else if(KEYNAME("VelocityLimit"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetAdvancedServo_setVelocityLimit(phid, index, servo_us_to_degrees_vel(phid->servoParams[index], value, PTRUE)) );
		PWS_SET_KEY_INDEXED_OLDVAL(VelocityLimit);
	}
	else if(KEYNAME("PositionMin"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetAdvancedServo_setPositionMin(phid, index, servo_us_to_degrees(phid->servoParams[index], value, PTRUE)) );
		PWS_SET_KEY_INDEXED_OLDVAL(PositionMin);
	}
	else if(KEYNAME("PositionMax"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetAdvancedServo_setPositionMax(phid, index, servo_us_to_degrees(phid->servoParams[index], value, PTRUE)) );
		PWS_SET_KEY_INDEXED_OLDVAL(PositionMax);
	}
	else if(KEYNAME("Position"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetAdvancedServo_setPosition(phid, index, servo_us_to_degrees(phid->servoParams[index], value, PTRUE)) );
	}
	else if(KEYNAME("Engaged"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetAdvancedServo_setEngaged(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Engaged);
	}
	else if(KEYNAME("SpeedRampingOn"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetAdvancedServo_setSpeedRampingOn(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(SpeedRampingOn);
	}
	else if(KEYNAME("ServoParameters"))
	{
		char val[MAX_VAL_SIZE];
		CPhidgetServoParameters params;
		char *endptr;
		params.servoType = strtol(state, &endptr, 10);
		params.min_us = strtod(endptr+1, &endptr);
		params.max_us = strtod(endptr+1, &endptr);
		params.us_per_degree = strtod(endptr+1, &endptr);
		params.max_us_per_s = strtod(endptr+1, NULL);
		params.state = PTRUE;

		EXIT_ON_ERR( setupNewAdvancedServoParams(phid, index, params) );

		PWS_SET_KEY_INDEXED_OLDVAL(ServoParameters);
		PWS_SET_KEY_INDEXED(PositionMinLimit, "%lE", phid->motorPositionMinLimit);
		PWS_SET_KEY_INDEXED(VelocityMax, "%lE", phid->velocityMax[index]);
	}
	else{
		PWS_BAD_SETTYPE(AdvancedServo);
	}
	PWS_END
}

PWS_EVENT_INDEXED(AdvancedServo, PositionChange, double Val)
	PWS_SET_KEY_INDEXED(Stopped, "%d", phid->motorStoppedState[index]);
	PWS_SET_KEY_INDEXED(Position, "%lE", servo_degrees_to_us(phid->servoParams[index], Val));
	PWS_END
}

PWS_EVENT_INDEXED(AdvancedServo, VelocityChange, double Val)
	PWS_SET_KEY_INDEXED(Stopped, "%d", phid->motorStoppedState[index]);
	PWS_SET_KEY_INDEXED(Velocity, "%lE", servo_degrees_to_us_vel(phid->servoParams[index], Val));
	PWS_END
}

PWS_EVENT_INDEXED(AdvancedServo, CurrentChange, double Val)
	PWS_SET_KEY_INDEXED(Current, "%lE", Val);
	PWS_END
}

/******************
 * Phidget Analog *
 ******************/

PWS_INITKEYS(Analog)
	int index=0;

	PWS_SET_KEY(NumberOfOutputs, "%d", phid->phid.attr.analog.numAnalogOutputs);
	PWS_SET_KEY(VoltageMin, "%lE", phid->voltageMin);
	PWS_SET_KEY(VoltageMax, "%lE", phid->voltageMax);

	*initKeys += 3;
	
	for(index=0;index<phid->phid.attr.analog.numAnalogOutputs;index++)
	{
		PWS_SET_KEY_INDEXED(Enabled, "%d", phid->enabledEcho[index]);
		PWS_SET_KEY_INDEXED(Voltage, "%lE", phid->voltageEcho[index]);

		*initKeys += 2;
	}

	PWS_END
}

PWS_SETKEYS(Analog)
	if(KEYNAME("Voltage"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetAnalog_setVoltage(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Voltage);
	}
	else if(KEYNAME("Enabled"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetAnalog_setEnabled(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Enabled);
	}
	else
	{
		PWS_BAD_SETTYPE(Analog);
	}
	PWS_END
}

/******************
 * Phidget Bridge *
 ******************/

PWS_INITKEYS(Bridge)
	int index=0;

	PWS_SET_KEY(NumberOfInputs, "%d", phid->phid.attr.bridge.numBridgeInputs);
	PWS_SET_KEY(DataRate, "%d", phid->dataRateEcho);
	PWS_SET_KEY(DataRateMin, "%d", phid->dataRateMin);
	PWS_SET_KEY(DataRateMax, "%d", phid->dataRateMax);

	*initKeys += 4;
	
	for(index=0;index<phid->phid.attr.bridge.numBridgeInputs;index++)
	{
		PWS_SET_KEY_INDEXED(Enabled, "%d", phid->enabledEcho[index]);
		PWS_SET_KEY_INDEXED(BridgeMax, "%lE", phid->bridgeMax[index]);
		PWS_SET_KEY_INDEXED(BridgeMin, "%lE", phid->bridgeMin[index]);
		PWS_SET_KEY_INDEXED(Gain, "%d", phid->gainEcho[index]);
		PWS_SET_KEY_INDEXED(BridgeValue, "%lE", phid->bridgeValue[index]);

		phid->lastBridgeMax[index] = phid->bridgeMax[index];
		phid->lastBridgeMin[index] = phid->bridgeMin[index];

		*initKeys += 5;
	}
	PWS_END
}

PWS_SETKEYS(Bridge)
	if(KEYNAME("Gain"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetBridge_setGain(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Gain);
	}
	else if(KEYNAME("Enabled"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetBridge_setEnabled(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Enabled);
	}
	else if(KEYNAME("DataRate"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetBridge_setDataRate(phid, value) );
		PWS_SET_KEY_OLDVAL(DataRate);
	}
	else
	{
		PWS_BAD_SETTYPE(Bridge);
	}
	PWS_END
}

PWS_EVENT_INDEXED(Bridge, BridgeData, double Val)
	// if we've changed gain, then max/min will change
	if(phid->lastBridgeMax[index] != phid->bridgeMax[index])
	{
		PWS_SET_KEY_INDEXED(BridgeMax, "%lE", phid->bridgeMax[index]);
		phid->lastBridgeMax[index] = phid->bridgeMax[index];
	}
	if(phid->lastBridgeMin[index] != phid->bridgeMin[index])
	{
		PWS_SET_KEY_INDEXED(BridgeMin, "%lE", phid->bridgeMin[index]);
		phid->lastBridgeMin[index] = phid->bridgeMin[index];
	}
	PWS_SET_KEY_INDEXED(BridgeValue, "%lE", Val);
	PWS_END
}

/*******************
 * Phidget Encoder *
 *******************/

PWS_INITKEYS(Encoder)
	int index=0;

	PWS_SET_KEY(NumberOfEncoders, "%d", phid->phid.attr.encoder.numEncoders);
	PWS_SET_KEY(NumberOfInputs, "%d", phid->phid.attr.encoder.numInputs);

	*initKeys += 2;
	
	for(index=0;index<phid->phid.attr.encoder.numEncoders;index++)
	{
		PWS_SET_KEY_INDEXED(Enabled, "%d", phid->enableStateEcho[index]);

		*initKeys += 1;
	}

	PWS_END
}

PWS_SETKEYS(Encoder)
	if(KEYNAME("ResetPosition"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetEncoder_setPosition(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(ResetPosition);
	}
	else if(KEYNAME("Enabled"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetEncoder_setEnabled(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Enabled);
	}
	else
	{
		PWS_BAD_SETTYPE(Encoder);
	}
	PWS_END
}

PWS_EVENT_INDEXED(Encoder, InputChange, int Val)
	PWS_SET_KEY_INDEXED(Input, "%d", Val);
	PWS_END
}

PWS_EVENT_INDEXED(Encoder, PositionChange, int Time, int PositionChange)
	PWS_SET_KEY_INDEXED(Position, "%d/%d/%d", Time, PositionChange, phid->encoderPosition[index]);
	PWS_END
}

PWS_EVENT_INDEXED(Encoder, Index, int Val)
	PWS_SET_KEY_INDEXED(IndexPosition, "%d", Val);
	PWS_END
}

/*****************************
 * Phidget Frequency Counter *
 *****************************/

PWS_INITKEYS(FrequencyCounter)
	int index=0;

	PWS_SET_KEY(NumberOfInputs, "%d", phid->phid.attr.frequencycounter.numFreqInputs);
	
	*initKeys += 1;

	for(index=0;index<phid->phid.attr.frequencycounter.numFreqInputs;index++)
	{
		PWS_SET_KEY_INDEXED(Count, "%lld/%lld/%lE", phid->totalTime[index], phid->totalCount[index], phid->frequency[index]);
		PWS_SET_KEY_INDEXED(Timeout, "%d", phid->timeout[index]);
		PWS_SET_KEY_INDEXED(Enabled, "%d", phid->enabledEcho[index]);
		PWS_SET_KEY_INDEXED(Filter, "%d", phid->filterEcho[index]);

		*initKeys += 4;
	}
	PWS_END
}

PWS_SETKEYS(FrequencyCounter)
	if(KEYNAME("Reset"))
	{
		char val[MAX_VAL_SIZE];
		EXIT_ON_ERR( CPhidgetFrequencyCounter_reset(phid, index) );
		PWS_SET_KEY_INDEXED(CountReset, "%lld/%lld/%lE", phid->totalTime[index], phid->totalCount[index], phid->frequency[index]);
	}
	else if(KEYNAME("Enabled"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetFrequencyCounter_setEnabled(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Enabled);
	}
	else if(KEYNAME("Filter"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetFrequencyCounter_setFilter(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Filter);
	}
	else if(KEYNAME("Timeout"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetFrequencyCounter_setTimeout(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Timeout);
	}
	else
	{
		PWS_BAD_SETTYPE(FrequencyCounter);
	}
	PWS_END
}

PWS_EVENT_INDEXED(FrequencyCounter, Count, int time, int counts)
	PWS_SET_KEY_INDEXED(Count, "%lld/%lld/%lE", phid->totalTime[index], phid->totalCount[index], phid->frequency[index]);
	PWS_END
}

/*******************
 * Phidget Generic *
 *******************/

PWS_INITKEYS(Generic)
	phid = 0;
	key[0] = val[0] = '\0';
	PWS_END
}

PWS_SETKEYS(Generic)
	phid = 0;
	key[0] = '\0';
	PWS_END
}

PWS_EVENT(Generic, Packet, const unsigned char *packet, int length)
	phid = 0;
	key[0] = val[0] = '\0';
	PWS_END
}

/***************
 * Phidget GPS *
 ***************/

PWS_INITKEYS(GPS)
	phid = 0;
	key[0] = val[0] = '\0';

	/* position fix event */
	*initKeys += 1;

	PWS_END
}

PWS_SETKEYS(GPS)
	phid = 0;
	key[0] = '\0';
	PWS_END
}

PWS_EVENT(GPS, PositionChange, double latitude, double longitude, double altitude)
	double vel, heading;
	GPSDate date;
	GPSTime time;

	if(CPhidgetGPS_getVelocity(phid, &vel) == EPHIDGET_OK)
		PWS_SET_KEY(Velocity, "%lE", vel);

	if(CPhidgetGPS_getHeading(phid, &heading) == EPHIDGET_OK)
		PWS_SET_KEY(Heading, "%lE", heading);
	
	if(CPhidgetGPS_getDate(phid, &date) == EPHIDGET_OK && CPhidgetGPS_getTime(phid, &time) == EPHIDGET_OK)
		PWS_SET_KEY(DateTime, "%d/%d/%d/%d/%d/%d/%d", date.tm_year, date.tm_mon, date.tm_mday, time.tm_hour, time.tm_min, time.tm_sec, time.tm_ms);

	PWS_SET_KEY(Position, "%lE/%lE/%lE", latitude, longitude, altitude);

	PWS_END
}

PWS_EVENT(GPS, PositionFixStatusChange, int Val)
	PWS_SET_KEY(PositionFix, "%d", Val);
	PWS_END
}

/*************************
 * Phidget Interface Kit *
 *************************/

PWS_INITKEYS(InterfaceKit)
	int index = 0;

	PWS_SET_KEY(NumberOfSensors, "%d", phid->phid.attr.ifkit.numSensors);
	PWS_SET_KEY(NumberOfInputs, "%d", phid->phid.attr.ifkit.numInputs);
	PWS_SET_KEY(NumberOfOutputs, "%d", phid->phid.attr.ifkit.numOutputs);
	
	*initKeys += 3;

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_INTERFACEKIT_2_2_2:
		case PHIDID_INTERFACEKIT_8_8_8:
		case PHIDID_INTERFACEKIT_8_8_8_w_LCD:
			PWS_SET_KEY(Ratiometric, "%d", phid->ratiometric);
			PWS_SET_KEY(DataRateMin, "%d", phid->dataRateMin);
			//We can't do better then 16ms over the webservice
			phid->dataRateMax = 16;
			PWS_SET_KEY(DataRateMax, "%d", phid->dataRateMax);
			PWS_SET_KEY(InterruptRate, "%d", phid->interruptRate);

			*initKeys += 4;

			break;
		default:
			break;
	}

	for(index=0;index<phid->phid.attr.ifkit.numSensors;index++)
	{
		PWS_SET_KEY_INDEXED(Trigger, "%d", phid->sensorChangeTrigger[index]);
		PWS_SET_KEY_INDEXED(RawSensor, "%d", phid->sensorRawValue[index]);

		phid->dataRate[index] = 16;
		PWS_SET_KEY_INDEXED(DataRate, "%d", phid->dataRate[index]);

		*initKeys += 3;

		//These can have unknown sensor value, so we have to send it out manually as it wouldn't be sent out in the event
		switch(phid->phid.deviceIDSpec)
		{
			case PHIDID_ROTARY_TOUCH:
			case PHIDID_LINEAR_TOUCH:
				PWS_SET_KEY_INDEXED(Sensor, "%d", phid->sensorValue[index]);

				//initKeys is incremented below for sensors
				break;
			default:
				break;
		}
	}

	/* have to send these explicitely because events are not guaranteed (for older ifkits) */
	for(index=0;index<phid->phid.attr.ifkit.numOutputs;index++)
	{
		PWS_SET_KEY_INDEXED(Output, "%d", phid->outputEchoStates[index]);

		*initKeys += 1;
	}

	/* input, sensor events */
	*initKeys += phid->phid.attr.ifkit.numSensors;

	switch(phid->phid.deviceIDSpec)
	{
		//these ones don't do initial state for inputs because they only send data when an input changes
		case PHIDID_INTERFACEKIT_0_8_8_w_LCD:
		case PHIDID_INTERFACEKIT_0_5_7:
			for(index=0;index<phid->phid.attr.ifkit.numInputs;index++)
			{
				PWS_SET_KEY_INDEXED(Input, "%d", phid->physicalState[index]);

				*initKeys += 1;
			}
			break;
		default:
			*initKeys += phid->phid.attr.ifkit.numInputs;
			break;
	}

	PWS_END
}

PWS_SETKEYS(InterfaceKit)
	if(KEYNAME("Output"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetInterfaceKit_setOutputState(phid, index, value) );
	}
	else if(KEYNAME("Ratiometric"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetInterfaceKit_setRatiometric(phid, value) );
		PWS_SET_KEY_OLDVAL(Ratiometric);
	}
	else if(KEYNAME("Trigger"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetInterfaceKit_setSensorChangeTrigger(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Trigger);
	}
	else if(KEYNAME("DataRate"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetInterfaceKit_setDataRate(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(DataRate);
	}
	else
	{
		PWS_BAD_SETTYPE(Interfacekit);
	}
	PWS_END
}

PWS_EVENT_INDEXED(InterfaceKit, InputChange, int Val)
	//special case for touch sensors - if touch is removed, sensor will go unknown
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_ROTARY_TOUCH:
		case PHIDID_LINEAR_TOUCH:
			{
				int index = 0;
				PWS_SET_KEY_INDEXED(Sensor, "%d", phid->sensorValue[index]);
				PWS_SET_KEY_INDEXED(RawSensor, "%d", phid->sensorRawValue[index]);
			}
			break;
		default:
			break;
	}
	PWS_SET_KEY_INDEXED(Input, "%d", Val);
	PWS_END
}

PWS_EVENT_INDEXED(InterfaceKit, OutputChange, int Val)
	PWS_SET_KEY_INDEXED(Output, "%d", Val);
	PWS_END
}

PWS_EVENT_INDEXED(InterfaceKit, SensorChange, int Val)
	PWS_SET_KEY_INDEXED(RawSensor, "%d", phid->sensorRawValue[index]);
	PWS_SET_KEY_INDEXED(Sensor, "%d", Val);
	PWS_END
}

/**************
 * Phidget IR *
 **************/

PWS_INITKEYS(IR)
	phid = 0;
	key[0] = val[0] = '\0';
	PWS_END
}

PWS_SETKEYS(IR)
	key[0] = '\0';
	if(KEYNAME("Transmit"))
	{
		unsigned char data[16];
		CPhidgetIR_CodeInfo codeInfo;
		int length=16;

		stringToCodeInfo((char *)state, &codeInfo);
		stringToByteArray((char *)(state+sizeof(CPhidgetIR_CodeInfo)*2), data, &length);

		EXIT_ON_ERR( CPhidgetIR_Transmit(phid, data, &codeInfo) );
	}
	else if(KEYNAME("TransmitRaw"))
	{
		int data[250];
		int length=250;
		int carrierFrequency, dutyCycle, gap;
		char *endPtr;

		//this will stop at the first ','
		stringToWordArray((char *)state, data, &length);
		carrierFrequency = strtol(state+length*5+1, &endPtr, 10);
		dutyCycle = strtol(endPtr+1, &endPtr, 10);
		gap = strtol(endPtr+1, &endPtr, 10);

		EXIT_ON_ERR( CPhidgetIR_TransmitRaw(phid, data, length, carrierFrequency, dutyCycle, gap) );
	}
	else if(KEYNAME("RawDataAck"))
	{
		//make this key index available again
		phid->rawDataSendWSKeys[index] = 0;
	}
	else if(KEYNAME("Repeat"))
	{
		EXIT_ON_ERR( CPhidgetIR_TransmitRepeat(phid) );
	}
	else
	{
		PWS_BAD_SETTYPE(IR);
	}
	PWS_END
}

PWS_EVENT(IR, Code, unsigned char *data, int dataLength, int bitCount, int repeat)
	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/Code", phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber);
	byteArrayToString(data, dataLength, val);
	snprintf(val+dataLength*2, MAX_VAL_SIZE-dataLength*2, ",%d,%d", bitCount, repeat);
	EXIT_ON_ERR( add_key(pdss, key, val) );

	PWS_END
}

PWS_EVENT(IR, RawData, int *data, int dataLength)
	int i;

	//find first free key
	for(i=0;i<IR_RAW_DATA_WS_KEYS_MAX;i++)
		if(!phid->rawDataSendWSKeys[i])
			break;

	//this is an error - there are too many non-acked keys
	//All we can really do is stop sending data
	if(i==IR_RAW_DATA_WS_KEYS_MAX)
	{
		//still need to increment, so clients will notice the error
		phid->rawDataSendWSCounter++;
		goto exit;
	}

	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/RawData/%d", phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber, i);
	ZEROMEM(val, 1024);
	wordArrayToString(data, dataLength, val);
	snprintf(val+dataLength*5, MAX_VAL_SIZE-dataLength*5, ",%d", phid->rawDataSendWSCounter);
	//set the key to used
	phid->rawDataSendWSKeys[i]=phid->rawDataSendWSCounter++;

	if((ret = add_key(pdss, key, val)) != EPHIDGET_OK)
	{
		phid->rawDataSendWSKeys[i] = 0;
		goto exit;
	}

	PWS_END
}

PWS_EVENT(IR, Learn, unsigned char *data, int dataLength, CPhidgetIR_CodeInfoHandle codeInfo)
	ZEROMEM(val, MAX_VAL_SIZE);

	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/Learn", phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber);
	codeInfoToString(codeInfo, val);
	byteArrayToString(data, dataLength, val+sizeof(CPhidgetIR_CodeInfo)*2);
	EXIT_ON_ERR( add_key(pdss, key, val) );

	PWS_END
}

/***************
 * Phidget LED *
 ***************/

PWS_INITKEYS(LED)
	int index = 0;

	PWS_SET_KEY(NumberOfLEDs, "%d", phid->phid.attr.led.numLEDs);

	*initKeys += 1;

	switch(phid->phid.deviceUID)
	{
		case PHIDUID_LED_64_ADV:
			PWS_SET_KEY(Voltage, "%d", phid->voltage);
			PWS_SET_KEY(CurrentLimit, "%d", phid->currentLimit);
			*initKeys += 2;
			break;
		case PHIDUID_LED_64_ADV_M3:
			PWS_SET_KEY(Voltage, "%d", phid->voltage);
			//though not used for get, this is still a valid key.
			PWS_SET_KEY(CurrentLimit, "%d", phid->currentLimit);
			*initKeys += 2;
			for(index=0;index<phid->phid.attr.led.numLEDs;index++)
			{
				PWS_SET_KEY_INDEXED(CurrentLimitIndexed, "%lf", phid->LED_CurrentLimit[index]);
				*initKeys += 1;
			}
			break;
		default:
			break;
	}

	for(index=0;index<phid->phid.attr.led.numLEDs;index++)
	{
		PWS_SET_KEY_INDEXED(Brightness, "%lf", phid->LED_Power[index]);

		*initKeys += 1;
	}

	PWS_END
}

PWS_SETKEYS(LED)
	if(KEYNAME("Brightness"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetLED_setBrightness(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Brightness);
	}
	else if(KEYNAME("Voltage"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetLED_setVoltage(phid, value) );
		PWS_SET_KEY_OLDVAL(Voltage);
	}
	else if(KEYNAME("CurrentLimit"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetLED_setCurrentLimit(phid, value) );
		PWS_SET_KEY_OLDVAL(CurrentLimit);
	}
	else if(KEYNAME("CurrentLimitIndexed"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetLED_setCurrentLimitIndexed(phid, index, value) );
		PWS_SET_KEY_OLDVAL(CurrentLimitIndexed);
	}
	else
	{
		PWS_BAD_SETTYPE(LED);
	}
	PWS_END
}

/*************************
 * Phidget Motor Control *
 *************************/

PWS_INITKEYS(MotorControl)
	int index=0;

	PWS_SET_KEY(NumberOfMotors, "%d", phid->phid.attr.motorcontrol.numMotors);
	PWS_SET_KEY(NumberOfInputs, "%d", phid->phid.attr.motorcontrol.numInputs);
	PWS_SET_KEY(AccelerationMin, "%lE", phid->accelerationMin);
	PWS_SET_KEY(AccelerationMax, "%lE", phid->accelerationMax);
	PWS_SET_KEY(NumberOfEncoders, "%d", phid->phid.attr.motorcontrol.numEncoders);
	PWS_SET_KEY(NumberOfSensors, "%d", phid->phid.attr.motorcontrol.numSensors);

	*initKeys += 6;
	
	/* velocity events */
	*initKeys += phid->phid.attr.motorcontrol.numMotors;
	/* input events */
	*initKeys += phid->phid.attr.motorcontrol.numInputs;
	/* sensors/rawSensors events */
	*initKeys += (2 * phid->phid.attr.motorcontrol.numSensors);
	
	for(index=0;index<phid->phid.attr.servo.numMotors;index++)
	{
		PWS_SET_KEY_INDEXED(Acceleration, "%lE", phid->motorAccelerationEcho[index]);
		PWS_SET_KEY_INDEXED(Braking, "%lE", phid->motorBrakingEcho[index]);
		
		*initKeys += 2;
	}

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_MOTORCONTROL_HC_2MOTOR:
			/* current events */
			*initKeys += phid->phid.attr.motorcontrol.numMotors;
			break;
		case PHIDID_MOTORCONTROL_1MOTOR:
			/* supply voltage from event */
			(*initKeys)++;
			/* current events */
			*initKeys += phid->phid.attr.motorcontrol.numMotors;

			PWS_SET_KEY(Ratiometric, "%d", phid->ratiometric);

			*initKeys += 1;

			for(index=0;index<phid->phid.attr.servo.numMotors;index++)
			{
				PWS_SET_KEY_INDEXED(BackEMFState, "%d", phid->backEMFSensingStateEcho[index]);
				PWS_SET_KEY_INDEXED(BackEMF, "%lE", phid->motorSensedBackEMF[index]);
			
				*initKeys += 2;
			}

			break;
		default:
			break;
	}

	PWS_END
}

PWS_SETKEYS(MotorControl)
	if(KEYNAME("Acceleration"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetMotorControl_setAcceleration(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Acceleration);
	}
	else if(KEYNAME("Velocity"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetMotorControl_setVelocity(phid, index, value) );
	}
	else if(KEYNAME("ResetEncoderPosition"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetMotorControl_setEncoderPosition(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(EncoderPosition);
	}
	else if(KEYNAME("BackEMFState"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetMotorControl_setBackEMFSensingState(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(BackEMFState);
	}
	else if(KEYNAME("Braking"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetMotorControl_setBraking(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Braking);
	}
	else if(KEYNAME("Ratiometric"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetMotorControl_setRatiometric(phid, value) );
		PWS_SET_KEY_OLDVAL(Ratiometric);
	}
	else
	{
		PWS_BAD_SETTYPE(MotorControl);
	}
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, InputChange, int Val)
	PWS_SET_KEY_INDEXED(Input, "%d", Val);
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, VelocityChange, double Val)
	PWS_SET_KEY_INDEXED(Velocity, "%lE", Val);
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, CurrentChange, double Val)
	PWS_SET_KEY_INDEXED(Current, "%lE", Val);
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, EncoderPositionChange, int Time, int PositionChange)
	int posn;
	//because of positionDelta
	CPhidgetMotorControl_getEncoderPosition(phid, index, &posn);
	PWS_SET_KEY_INDEXED(EncoderPosition, "%d/%d", phid->encoderTimeStamp[index], posn);
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, EncoderPositionUpdate, int Val)
	int posn;
	//because of positionDelta
	CPhidgetMotorControl_getEncoderPosition(phid, index, &posn);
	PWS_SET_KEY_INDEXED(EncoderPositionUpdate, "%d", posn);

	//EncoderPositionUpdate is fired every time the board sends data - we'll hijack it to send supply voltage
	if(phid->supplyVoltage != PUNK_DBL && phid->supplyVoltage != phid->lastVoltage)
	{
		PWS_SET_KEY(SupplyVoltage, "%lE", phid->supplyVoltage);
		phid->lastVoltage = phid->supplyVoltage;
	}
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, BackEMFUpdate, double Val)
	PWS_SET_KEY_INDEXED(BackEMF, "%lE", Val);
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, SensorUpdate, int Val)
	PWS_SET_KEY_INDEXED(RawSensor, "%d", phid->sensorRawValue[index]);
	PWS_SET_KEY_INDEXED(Sensor, "%d", Val);
	PWS_END
}

PWS_EVENT_INDEXED(MotorControl, CurrentUpdate, double Val)
	PWS_SET_KEY_INDEXED(CurrentUpdate, "%lE", Val);
	PWS_END
}

/*********************
 * Phidget PH Sensor *
 *********************/

PWS_INITKEYS(PHSensor)

	PWS_SET_KEY(PHMin, "%lE", phid->phMin);
	PWS_SET_KEY(PHMax, "%lE", phid->phMax);
	PWS_SET_KEY(PotentialMin, "%lE", phid->potentialMin);
	PWS_SET_KEY(PotentialMax, "%lE", phid->potentialMax);
	PWS_SET_KEY(PH, "%lE", phid->PH);
	PWS_SET_KEY(Potential, "%lE", phid->Potential);
	PWS_SET_KEY(Trigger, "%lE", phid->PHChangeTrigger);

	*initKeys += 7;

	PWS_END
}

PWS_SETKEYS(PHSensor)
	if(KEYNAME("Trigger"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetPHSensor_setPHChangeTrigger(phid, value) );
		PWS_SET_KEY_OLDVAL(Trigger);
	}
	else if(KEYNAME("Temperature"))
	{
		char val[MAX_VAL_SIZE];
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetPHSensor_setTemperature(phid, value) );
		PWS_SET_KEY(PHMin, "%lE", phid->phMin);
		PWS_SET_KEY(PHMax, "%lE", phid->phMax);
	}
	else{
		PWS_BAD_SETTYPE(PHSensor);
	}
	PWS_END
}

PWS_EVENT(PHSensor, PHChange, double Val)
	PWS_SET_KEY(Potential, "%lE", phid->Potential);
	PWS_SET_KEY(PH, "%lE", Val);
	PWS_END
}

/****************
 * Phidget RFID *
 ****************/

PWS_INITKEYS(RFID)
	int index = 0;

	PWS_SET_KEY(NumberOfOutputs, "%d", phid->phid.attr.rfid.numOutputs);
	PWS_SET_KEY(TagState, "%d", phid->tagPresent);
	PWS_SET_KEY(LastTag, "%02x%02x%02x%02x%02x/%d/%s",
		phid->lastTag.tagData[0],phid->lastTag.tagData[1],phid->lastTag.tagData[2],phid->lastTag.tagData[3],phid->lastTag.tagData[4],
		phid->lastTag.protocol, phid->lastTag.tagString);
	
	*initKeys += 3;

	/* have to send these explicitely because events are not guaranteed (for older readers) */
	for(index=0;index<phid->phid.attr.rfid.numOutputs;index++)
	{
		PWS_SET_KEY_INDEXED(Output, "%d", phid->outputEchoState[index]);

		*initKeys += 1;
	}

	if(phid->phid.attr.rfid.numOutputs > 0)
	{
		PWS_SET_KEY(AntennaOn, "%d", phid->antennaEchoState);
		PWS_SET_KEY(LEDOn, "%d", phid->ledEchoState);

		*initKeys += 2;
	}

	PWS_END
}

PWS_SETKEYS(RFID)
	if(KEYNAME("Output"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetRFID_setOutputState(phid, index, value) );
	}
	else if(KEYNAME("AntennaOn"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetRFID_setAntennaOn(phid, value) );
		PWS_SET_KEY_OLDVAL(AntennaOn);
	}
	else if(KEYNAME("LEDOn"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetRFID_setLEDOn(phid, value) );
		PWS_SET_KEY_OLDVAL(LEDOn);
	}
	else if(KEYNAME("WriteTag"))
	{
		CPhidgetRFID_Protocol proto;
		int lock;
		char *tagString;
		char *endPtr;
		
		proto = (CPhidgetRFID_Protocol)strtol(state, &endPtr, 10);
		lock = strtol(endPtr+1, &endPtr, 10);
		tagString = endPtr+1;

		EXIT_ON_ERR( CPhidgetRFID_write(phid, tagString, proto, lock) );
	}
	else
	{
		PWS_BAD_SETTYPE(RFID);
	}
	PWS_END
}

PWS_EVENT(RFID, Tag2, char *Tag, CPhidgetRFID_Protocol protocol)
	PWS_SET_KEY(TagState, "%s", "1");
	PWS_SET_KEY(Tag2, "%d/%s",protocol, Tag);
	PWS_END
}

PWS_EVENT(RFID, TagLost2, char *Tag, CPhidgetRFID_Protocol protocol)
	PWS_SET_KEY(TagState, "%s", "0");
	PWS_SET_KEY(TagLoss2, "%d/%s",protocol, Tag);
	PWS_END
}

PWS_EVENT_INDEXED(RFID, OutputChange, int Val)
	PWS_SET_KEY_INDEXED(Output, "%d", Val);
	PWS_END
}

/*****************
 * Phidget Servo *
 *****************/

PWS_INITKEYS(Servo)
	int index = 0;

	PWS_SET_KEY(NumberOfMotors, "%d", phid->phid.attr.servo.numMotors);
	PWS_SET_KEY(PositionMinLimit, "%lE", phid->motorPositionMinLimit);
	PWS_SET_KEY(PositionMaxLimit, "%lE", phid->motorPositionMaxLimit);

	*initKeys += 3;

	/* have to send these explicitely because events are not guaranteed (for older servo boards) */
	for(index=0;index<phid->phid.attr.servo.numMotors;index++)
	{
		PWS_SET_KEY_INDEXED(Position, "%lE", phid->motorPositionEcho[index]);
		PWS_SET_KEY_INDEXED(Engaged, "%d", phid->motorEngagedStateEcho[index]);
		PWS_SET_KEY_INDEXED(ServoParameters, "%d,%lE,%lE,%lE", phid->servoParams[index].servoType, phid->servoParams[index].min_us, phid->servoParams[index].max_us, phid->servoParams[index].us_per_degree);

		*initKeys += 3;
	}

	PWS_END
}

PWS_SETKEYS(Servo)
	if(KEYNAME("Position"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetServo_setPosition(phid, index, servo_us_to_degrees(phid->servoParams[index], value, PTRUE)) );
	}
	else if(KEYNAME("Engaged"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetServo_setEngaged(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Engaged);
	}
	else if(KEYNAME("ServoParameters"))
	{
		char val[MAX_VAL_SIZE];
		CPhidgetServoParameters params;
		char *endptr;
		params.servoType = strtol(state, &endptr, 10);
		params.min_us = strtod(endptr+1, &endptr);
		params.max_us = strtod(endptr+1, &endptr);
		params.us_per_degree = strtod(endptr+1, NULL);
		params.max_us_per_s = 0; //not used for PhidgetServo because no Acceleration/Velocity control
		params.state = PTRUE;

		EXIT_ON_ERR( setupNewServoParams(phid, index, params) );
		PWS_SET_KEY_INDEXED_OLDVAL(ServoParameters);
		PWS_SET_KEY(PositionMinLimit, "%lE", phid->motorPositionMinLimit);
	}
	else
	{
		PWS_BAD_SETTYPE(Servo);
	}
	PWS_END
}

PWS_EVENT_INDEXED(Servo, PositionChange, double Val)
	PWS_SET_KEY_INDEXED(Position, "%lE", servo_degrees_to_us(phid->servoParams[index], Val));
	PWS_END
}

/*******************
 * Phidget Spatial *
 *******************/

PWS_INITKEYS(Spatial)
	//We can't do better then 10ms over the webservice - so round up
	switch(phid->interruptRate)
	{
		case 4:
			phid->dataRate = 12;
			phid->dataRateMax = 12;
			break;
		case 8:
		default:
			phid->dataRate = 16;
			phid->dataRateMax = 16;
			break;
	}

	PWS_SET_KEY(AccelerationAxisCount, "%d", phid->phid.attr.spatial.numAccelAxes);
	PWS_SET_KEY(GyroAxisCount, "%d", phid->phid.attr.spatial.numGyroAxes);
	PWS_SET_KEY(CompassAxisCount, "%d", phid->phid.attr.spatial.numCompassAxes);
	PWS_SET_KEY(DataRateMin, "%d", phid->dataRateMin);
	PWS_SET_KEY(DataRateMax, "%d", phid->dataRateMax);
	PWS_SET_KEY(DataRate, "%d", phid->dataRate);
	PWS_SET_KEY(InterruptRate, "%d", phid->interruptRate);

	*initKeys += 7;

	if(phid->phid.attr.spatial.numAccelAxes)
	{
		PWS_SET_KEY(AccelerationMin, "%lE", phid->accelerationMin);
		PWS_SET_KEY(AccelerationMax, "%lE", phid->accelerationMax);

		*initKeys += 2;
	}

	if(phid->phid.attr.spatial.numGyroAxes)
	{
		PWS_SET_KEY(AngularRateMin, "%lE", phid->angularRateMin);
		PWS_SET_KEY(AngularRateMax, "%lE", phid->angularRateMax);

		*initKeys += 2;
	}

	if(phid->phid.attr.spatial.numCompassAxes)
	{
		PWS_SET_KEY(MagneticFieldMin, "%lE", phid->magneticFieldMin);
		PWS_SET_KEY(MagneticFieldMax, "%lE", phid->magneticFieldMax);

		*initKeys += 2;
	}

	/* data event - all data is consodidated into one event */
	*initKeys += 1;

	PWS_END
}

PWS_SETKEYS(Spatial)
	if(KEYNAME("DataRate"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetSpatial_setDataRate(phid, value) );
		PWS_SET_KEY_OLDVAL(DataRate);
	}
	else if(KEYNAME("ZeroGyro"))
	{
		EXIT_ON_ERR( CPhidgetSpatial_zeroGyro(phid) );
	}
	else if(KEYNAME("CompassCorrectionParams"))
	{
		double magField, off0, off1, off2, gain0, gain1, gain2, T0, T1, T2, T3, T4, T5;
		char *endptr;

		magField = strtod(state, &endptr);
		off0 = strtod(endptr+1, &endptr);
		off1 = strtod(endptr+1, &endptr);
		off2 = strtod(endptr+1, &endptr);
		gain0 = strtod(endptr+1, &endptr);
		gain1 = strtod(endptr+1, &endptr);
		gain2 = strtod(endptr+1, &endptr);
		T0 = strtod(endptr+1, &endptr);
		T1 = strtod(endptr+1, &endptr);
		T2 = strtod(endptr+1, &endptr);
		T3 = strtod(endptr+1, &endptr);
		T4 = strtod(endptr+1, &endptr);
		T5 = strtod(endptr+1, NULL);

		EXIT_ON_ERR( CPhidgetSpatial_setCompassCorrectionParameters(phid, magField, off0, off1, off2, gain0, gain1, gain2, T0, T1, T2, T3, T4, T5) );
	}
	else
	{
		PWS_BAD_SETTYPE(Spatial);
	}
	PWS_END
}

PWS_EVENT(Spatial, SpatialData, CPhidgetSpatial_SpatialEventDataHandle *data, int dataCount)
	int i, len;

	//Always send all data - even for Phidgets that don't have all axes
	snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/SpatialData", phid->phid.deviceType, (phid->phid.escapedLabel ? phid->phid.escapedLabel : ""), phid->phid.serialNumber);
	val[0] = '\0';
	for(i=0;i<3;i++)
	{
		len = strlen(val);
		snprintf(val+len, MAX_VAL_SIZE-len, "%lE,", data[0]->acceleration[i]);
	}
	for(i=0;i<3;i++)
	{
		len = strlen(val);
		snprintf(val+len, MAX_VAL_SIZE-len, "%lE,", data[0]->angularRate[i]);
	}
	for(i=0;i<3;i++)
	{
		len = strlen(val);
		snprintf(val+len, MAX_VAL_SIZE-len, "%lE,", data[0]->magneticField[i]);
	}
	len = strlen(val);
	snprintf(val+len, MAX_VAL_SIZE-len, "%d, %d", data[0]->timestamp.seconds, data[0]->timestamp.microseconds);
	EXIT_ON_ERR( add_key(pdss, key, val) );

	PWS_END
}

/*******************
 * Phidget Stepper *
 *******************/

PWS_INITKEYS(Stepper)
	int index = 0;

	PWS_SET_KEY(NumberOfMotors, "%d", phid->phid.attr.stepper.numMotors);
	PWS_SET_KEY(NumberOfInputs, "%d", phid->phid.attr.stepper.numInputs);
	PWS_SET_KEY(PositionMin, "%lld", phid->motorPositionMin);
	PWS_SET_KEY(PositionMax, "%lld", phid->motorPositionMax);
	PWS_SET_KEY(AccelerationMin, "%lE", phid->accelerationMin);
	PWS_SET_KEY(AccelerationMax, "%lE", phid->accelerationMax);
	PWS_SET_KEY(VelocityMin, "%lE", phid->motorSpeedMin);
	PWS_SET_KEY(VelocityMax, "%lE", phid->motorSpeedMax);
	PWS_SET_KEY(CurrentMin, "%lE", phid->currentMin);
	PWS_SET_KEY(CurrentMax, "%lE", phid->currentMax);

	*initKeys += 10;

	for(index=0;index<phid->phid.attr.stepper.numMotors;index++)
	{
		PWS_SET_KEY_INDEXED(Engaged, "%d", phid->motorEngagedStateEcho[index]);
		PWS_SET_KEY_INDEXED(Stopped, "%d", phid->motorStoppedState[index]);
		/* if the motor isn't engaged, there wouldn't be position events */
		PWS_SET_KEY_INDEXED(CurrentPosition, "%lld", phid->motorPositionEcho[index]);
		PWS_SET_KEY_INDEXED(TargetPosition, "%lld", phid->motorPosition[index]);

		*initKeys += 4;
	}

	/* inputs events */
	*initKeys += phid->phid.attr.stepper.numInputs;
	/* velocity events */
	*initKeys += phid->phid.attr.stepper.numMotors;
	/* current events */
	if(phid->phid.deviceIDSpec == PHIDID_BIPOLAR_STEPPER_1MOTOR)
		*initKeys += phid->phid.attr.stepper.numMotors;

	PWS_END
}

PWS_SETKEYS(Stepper)
	if(KEYNAME("Acceleration"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetStepper_setAcceleration(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Acceleration);
	}
	else if(KEYNAME("VelocityLimit"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetStepper_setVelocityLimit(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(VelocityLimit);
	}
	else if(KEYNAME("TargetPosition"))
	{
		GET_INT64_VAL;
		EXIT_ON_ERR( CPhidgetStepper_setTargetPosition(phid, index, value) );
	}
	else if(KEYNAME("CurrentPosition"))
	{
		GET_INT64_VAL;
		EXIT_ON_ERR( CPhidgetStepper_setCurrentPosition(phid, index, value) );
	}
	else if(KEYNAME("CurrentLimit"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetStepper_setCurrentLimit(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(CurrentLimit);
	}
	else if(KEYNAME("Engaged"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetStepper_setEngaged(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Engaged);
	}
	else
	{
		PWS_BAD_SETTYPE(Stepper);
	}
	PWS_END
}

PWS_EVENT_INDEXED(Stepper, InputChange, int Val)
	PWS_SET_KEY_INDEXED(Input, "%d", Val);
	PWS_END
}

PWS_EVENT_INDEXED(Stepper, PositionChange, long long Val)
	PWS_SET_KEY_INDEXED(Stopped, "%d", phid->motorStoppedState[index]);
	PWS_SET_KEY_INDEXED(CurrentPosition, "%lld", Val);
	PWS_END
}

PWS_EVENT_INDEXED(Stepper, VelocityChange, double Val)
	PWS_SET_KEY_INDEXED(Stopped, "%d", phid->motorStoppedState[index]);
	PWS_SET_KEY_INDEXED(Velocity, "%lE", Val);
	PWS_END
}

PWS_EVENT_INDEXED(Stepper, CurrentChange, double Val)
	PWS_SET_KEY_INDEXED(Current, "%lE", Val);
	PWS_END
}

/******************************
 * Phidget Temperature Sensor *
 ******************************/

PWS_INITKEYS(TemperatureSensor)
	int index=0;

	PWS_SET_KEY(NumberOfSensors, "%d", phid->phid.attr.temperaturesensor.numTempInputs);
	PWS_SET_KEY(PotentialMin, "%lE", phid->potentialMin);
	PWS_SET_KEY(PotentialMax, "%lE", phid->potentialMax);
	PWS_SET_KEY(AmbientTemperatureMin, "%lE", phid->ambientTemperatureMin);
	PWS_SET_KEY(AmbientTemperatureMax, "%lE", phid->ambientTemperatureMax);
	PWS_SET_KEY(AmbientTemperature, "%lE", phid->AmbientTemperature);

	*initKeys += 6;

	for(index=0;index<phid->phid.attr.temperaturesensor.numTempInputs;index++)
	{
		PWS_SET_KEY_INDEXED(TemperatureMin, "%lE", phid->temperatureMin[index]);
		PWS_SET_KEY_INDEXED(TemperatureMax, "%lE", phid->temperatureMax[index]);
		PWS_SET_KEY_INDEXED(Potential, "%lE", phid->Potential[index]);
		/* can't trust the event because it could be out of range */
		PWS_SET_KEY_INDEXED(Temperature, "%lE", phid->Temperature[index]);
		PWS_SET_KEY_INDEXED(Trigger, "%lE", phid->TempChangeTrigger[index]);
		PWS_SET_KEY_INDEXED(ThermocoupleType, "%d", phid->ThermocoupleType[index]);

		*initKeys += 6;
	}

	PWS_END
}

PWS_SETKEYS(TextLCD)
	int row = (index >> 8) & 0xff;
	int column = (index >> 16) & 0xff;
	index = index & 0xff; //screen

	//Note: we are protected by a mutex so this is safe
	EXIT_ON_ERR( CPhidgetTextLCD_setScreen(phid, index) );
	if(KEYNAME("Backlight"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTextLCD_setBacklight(phid, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Backlight);
	}
	else if(KEYNAME("Brightness"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTextLCD_setBrightness(phid, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Brightness);
	}
	else if(KEYNAME("Contrast"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTextLCD_setContrast(phid, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Contrast);
	}
	else if(KEYNAME("CursorOn"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTextLCD_setCursorOn(phid, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(CursorOn);
	}
	else if(KEYNAME("CursorBlink"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTextLCD_setCursorBlink(phid, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(CursorBlink);
	}
	else if(KEYNAME("ScreenSize"))
	{
		char val[MAX_VAL_SIZE];
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTextLCD_setScreenSize(phid, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(ScreenSize);
		PWS_SET_KEY_INDEXED(NumberOfRows, "%d", phid->rowCount[index]);
		PWS_SET_KEY_INDEXED(NumberOfColumns, "%d", phid->columnCount[index]);
	}
	else if(KEYNAME("DisplayString"))
	{
		EXIT_ON_ERR( CPhidgetTextLCD_setDisplayString(phid, row, (char *)state) );
	}
	else if(KEYNAME("DisplayCharacter"))
	{
		EXIT_ON_ERR( CPhidgetTextLCD_setDisplayCharacter(phid, row, column, state[0]) );
	}
	else if(KEYNAME("CustomCharacter"))
	{
		int val1, val2;
		char *endptr;
		val1 = strtol(state, &endptr, 10);
		val2 = strtol(endptr+1, NULL, 10);
		EXIT_ON_ERR( CPhidgetTextLCD_setCustomCharacter(phid, row, val1, val2) );
	}
	else if(KEYNAME("Init"))
	{
		EXIT_ON_ERR( CPhidgetTextLCD_initialize(phid) );
	}
	else
	{
		PWS_BAD_SETTYPE(TextLCD);
	}
	PWS_END
}

PWS_SETKEYS(TemperatureSensor)
	if(KEYNAME("Trigger"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetTemperatureSensor_setTemperatureChangeTrigger(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(Trigger);
	}
	else if(KEYNAME("ThermocoupleType"))
	{
		char val[MAX_VAL_SIZE];
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTemperatureSensor_setThermocoupleType(phid, index, value) );
		PWS_SET_KEY_INDEXED_OLDVAL(ThermocoupleType);
		PWS_SET_KEY_INDEXED(TemperatureMin, "%lE", phid->temperatureMin[index]);
		PWS_SET_KEY_INDEXED(TemperatureMax, "%lE", phid->temperatureMax[index]);
	}
	else
	{
		PWS_BAD_SETTYPE(TemperatureSensor);
	}
	PWS_END
}

PWS_EVENT_INDEXED(TemperatureSensor, TemperatureChange, double Val)
	PWS_SET_KEY_INDEXED(Potential, "%lE", phid->Potential[index]);
	PWS_SET_KEY(AmbientTemperature, "%lE", phid->AmbientTemperature);
	PWS_SET_KEY_INDEXED(Temperature, "%lE", Val);
	PWS_END
}

/********************
 * Phidget Text LCD *
 ********************/

PWS_INITKEYS(TextLCD)
	int index=0;

	PWS_SET_KEY(NumberOfScreens, "%d", phid->phid.attr.textlcd.numScreens);

	*initKeys += 1;

	for (index = 0; index<phid->phid.attr.textlcd.numScreens; index++)
	{
		PWS_SET_KEY_INDEXED(NumberOfRows, "%d", phid->rowCount[index]);
		PWS_SET_KEY_INDEXED(NumberOfColumns, "%d", phid->columnCount[index]);
		PWS_SET_KEY_INDEXED(ScreenSize, "%d", phid->screenSize[index]);
		PWS_SET_KEY_INDEXED(Contrast, "%d", phid->contrastEcho[index]);
		PWS_SET_KEY_INDEXED(Backlight, "%d", phid->backlightEcho[index]);
		PWS_SET_KEY_INDEXED(Brightness, "%d", phid->brightnessEcho[index]);

		*initKeys += 6;
	}

	PWS_END
}

/********************
 * Phidget Text LED *
 ********************/

PWS_INITKEYS(TextLED)
	PWS_SET_KEY(NumberOfRows, "%d", phid->phid.attr.textled.numRows);
	PWS_SET_KEY(NumberOfColumns, "%d", phid->phid.attr.textled.numColumns);
	
	*initKeys += 2;

	PWS_END
}

PWS_SETKEYS(TextLED)
	if(KEYNAME("Brightness"))
	{
		GET_INT_VAL;
		EXIT_ON_ERR( CPhidgetTextLED_setBrightness(phid, value) );
		PWS_SET_KEY_OLDVAL(Brightness);
	}
	else if(KEYNAME("DisplayString"))
	{
		EXIT_ON_ERR( CPhidgetTextLED_setDisplayString(phid, index, (char *)state) );
	}
	else
	{
		PWS_BAD_SETTYPE(TextLED);
	}
	PWS_END
}

/*************************
 * Phidget Weight Sensor *
 *************************/

PWS_INITKEYS(WeightSensor)
	PWS_SET_KEY(Weight, "%lE", phid->Weight);
	PWS_SET_KEY(Trigger, "%lE", phid->WeightChangeTrigger);
	
	*initKeys += 2;

	PWS_END
}

PWS_EVENT(WeightSensor, WeightChange, double Val)
	PWS_SET_KEY(Weight, "%lE", Val);
	PWS_END
}

PWS_SETKEYS(WeightSensor)
	if(KEYNAME("Trigger"))
	{
		GET_DOUBLE_VAL;
		EXIT_ON_ERR( CPhidgetWeightSensor_setWeightChangeTrigger(phid, value) );
		PWS_SET_KEY_OLDVAL(Trigger);
	}
	else
	{
		PWS_BAD_SETTYPE(WeightSensor);
	}
	PWS_END
}

/***************************
 * Function pointer arrays *
 ***************************/

int(*fptrInitKeys[PHIDGET_DEVICE_CLASS_COUNT])(CPhidgetHandle phid, pds_session_t *pdss, int *initKeys) = {
NULL,
NULL,
phidgetAccelerometer_initkeys,
phidgetAdvancedServo_initkeys,
phidgetEncoder_initkeys,
phidgetGPS_initkeys,
NULL,//old gyro,
phidgetInterfaceKit_initkeys,
phidgetLED_initkeys,
phidgetMotorControl_initkeys,
phidgetPHSensor_initkeys,
phidgetRFID_initkeys,
phidgetServo_initkeys,
phidgetStepper_initkeys,
phidgetTemperatureSensor_initkeys,
phidgetTextLCD_initkeys,
phidgetTextLED_initkeys,
phidgetWeightSensor_initkeys,
phidgetGeneric_initkeys,
phidgetIR_initkeys,
phidgetSpatial_initkeys,
phidgetFrequencyCounter_initkeys,
phidgetAnalog_initkeys,
phidgetBridge_initkeys};

int(*fptrSet[PHIDGET_DEVICE_CLASS_COUNT])(CPhidgetHandle phid, const char *setThing, int index, const char *state, void *pdss) = {
NULL,
NULL,
phidgetAccelerometer_set,
phidgetAdvancedServo_set,
phidgetEncoder_set,
phidgetGPS_set,
NULL,//old gyro,
phidgetInterfaceKit_set,
phidgetLED_set,
phidgetMotorControl_set,
phidgetPHSensor_set,
phidgetRFID_set,
phidgetServo_set,
phidgetStepper_set,
phidgetTemperatureSensor_set,
phidgetTextLCD_set,
phidgetTextLED_set,
phidgetWeightSensor_set,
phidgetGeneric_set,
phidgetIR_set,
phidgetSpatial_set,
phidgetFrequencyCounter_set,
phidgetAnalog_set,
phidgetBridge_set};

/*****************************************
 * Events from client (remote phidget21) *
 *****************************************/
// format is: "/PCK/(devicetype)/(serial number)/(thing to set)[/(index)[/(extra info)]]"
void phidget_set(const char *k, const char *escapedv, pdict_reason_t r, const char *pde_oldval, void *pdss)
{
	regmatch_t pmatch[6];
	char *deviceType = NULL;
	char *serialNumber = NULL;
	char *setThing = NULL; //ie output
	char *index = NULL;
	char *setThingSpecific = NULL;
	char *v;
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE]; 
	
	int deviceID;

	long serial;
	CNetworkPhidgetInfoHandle device = NULL;

	int res, ind=-1, ret = EPHIDGET_OK;

	if (!unescape(escapedv, &v, NULL))
	{
		pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Error in unescape");
		return;
	}

	if(!strncmp(v, "\001", 1) && strlen(v) == 1)
	{
		memset(v,0,1);
	}

	if(r!=PDR_ENTRY_REMOVING)
	{
		if ((res = regexec(&phidgetsetex, k, 6, pmatch, 0)) != 0) {
			pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Error in phidget_set - pattern not met (%s:%s)",k,v);
			return;
		}
		getmatchsub(k, &deviceType, pmatch, 1);
		getmatchsub(k, &serialNumber, pmatch, 2);
		getmatchsub(k, &setThing, pmatch, 3);
		getmatchsub(k, &index, pmatch, 4);
		getmatchsub(k, &setThingSpecific, pmatch, 5);// - not needed?


		//look for a device with that serial number / device type
		pd_unlock((void *)&pd_mutex);
		pthread_mutex_lock(&PhidgetsAndClientsMutex);
		if(deviceType && serialNumber)
		{
			CNetworkPhidgetListHandle trav = 0;
			deviceID=phidget_type_to_id(deviceType);
			serial = strtol(serialNumber, NULL, 10);
			device = NULL;
			
			for(trav = OpenPhidgets; trav; trav = trav->next)
			{
				if(trav->phidgetInfo)
				{
					if(trav->phidgetInfo->phidget)
					{
						if(trav->phidgetInfo->phidget->deviceID == deviceID
						   && trav->phidgetInfo->phidget->serialNumber == serial)
							device = trav->phidgetInfo;
					}
				}
			}
		}
			
		if(!device)
		{
			pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Couldn't find device");
			goto done;
		}

		if(KEYNAME("Label"))
		{
			if((ret = CPhidget_setDeviceLabel(device->phidget, v)) != EPHIDGET_OK)
				pu_log(PUL_WARN, ((pds_session_t *)pdss)->pdss_id, "Couldn't set label: %d", ret);
			else
			{
				char *label = NULL;
				escape(device->phidget->label, strlen(device->phidget->label), &label);
				snprintf(key, MAX_KEY_SIZE, "/PSK/%s/%s/%d/Label", device->phidget->deviceType, (device->phidget->escapedLabel ? device->phidget->escapedLabel : ""), device->phidget->serialNumber);
				snprintf(val, MAX_VAL_SIZE, "%s", label); \
				if((ret = add_key(pdss, key, val)) != EPHIDGET_OK)
					pu_log(PUL_WARN, ((pds_session_t *)pdss)->pdss_id, "Couldn't retransmit label: %d", ret);
				free(label);
			}

		}
		else if(fptrSet[device->phidget->deviceID] && setThing)
		{
			if(index)
				ind = strtol(index, NULL, 10);
			if((ret = fptrSet[device->phidget->deviceID](device->phidget, setThing, ind, v, pdss)) != EPHIDGET_OK)
				pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Error in set function: %d (key:\"%s\", Val:\"%s\")", ret,k,v);
		}
		else
		{
			pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Error in phidget_set (key:\"%s\", Val:\"%s\")", ret,k,v);
		}
	done:
		pthread_mutex_unlock(&PhidgetsAndClientsMutex);
		pd_lock((void *)&pd_mutex);
		free(deviceType); deviceType = NULL;
		free(serialNumber); serialNumber = NULL;
		free(setThing); setThing = NULL;
		free(index); index = NULL;
		free(setThingSpecific); setThingSpecific = NULL;
	}
	free(v); v = NULL;
}

// format is: "/PCK/Client/(ip address)/(client port)/(device type)/(serial number)/(label)"
void phidget_openclose(const char *k, const char *escapedv, pdict_reason_t r, const char *pde_oldval, void *pdss)
{
	regmatch_t pmatch[6];
	char *ipaddr = NULL;
	char *port = NULL;
	char *deviceType = NULL; //ie output
	char *serialNumber = NULL;
	char *label = NULL, *l = NULL;
	char *v;

	long serial;
	int deviceID;

	int res;

	if (!unescape(escapedv, &v, NULL))
	{
		pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Error in unescape");
		return;
	}

	if(!strncmp(v, "\001", 1) && strlen(v) == 1)
	{
		memset(v,0,1);
	}

	if ((res = regexec(&phidgetopencloseex, k, 6, pmatch, 0)) != 0) {
		pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Error in phidget_openclose - pattern not met (%s:%s)",k,v);
		return;
	}
	getmatchsub(k, &ipaddr, pmatch, 1);
	getmatchsub(k, &port, pmatch, 2);
	getmatchsub(k, &deviceType, pmatch, 3);
	getmatchsub(k, &serialNumber, pmatch, 4);
	getmatchsub(k, &l, pmatch, 5);

	if(l)
	{
		if (!unescape(l, &label, NULL))
		{
			pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Error in unescape");
			return;
		}
	}

	switch(r)
	{
		case PDR_ENTRY_REMOVING:
			//This is called when the "Open" key is removed - the client closed the connection without explicitely calling close
			if(!strcmp(v, "Open")) //An open device was detached
			{
				//close devices referenced by this client
				CNetworkPhidgetListHandle trav = 0;
				CPhidgetHandle devices_to_close[128];
				int num_to_close = 0, i;
				CClientInfoHandle newClient;
				
				pd_unlock((void *)&pd_mutex);
				pthread_mutex_lock(&PhidgetsAndClientsMutex);
				if(!findClientInfoHandleInList(ConnectedClients, ipaddr, port, &newClient))
				{
					for (trav=newClient->phidgets; trav; trav = trav->next) {
						devices_to_close[num_to_close++] = trav->phidgetInfo->phidget;
					}
				}

				for(i=0;i<num_to_close;i++)
				{
					if(devices_to_close[i]->specificDevice == PHIDGETOPEN_LABEL)
						close_phidget(pdss, devices_to_close[i]->deviceID, -1, devices_to_close[i]->label, ipaddr, port);
					else
						close_phidget(pdss, devices_to_close[i]->deviceID, devices_to_close[i]->serialNumber, NULL, ipaddr, port);
				}
				pthread_mutex_unlock(&PhidgetsAndClientsMutex);
				pd_lock((void *)&pd_mutex);
			}
			break;
		default:
			if(!strcmp(ipaddr, "0.0.0.0"))
			{
				pu_log(PUL_INFO, ((pds_session_t *)pdss)->pdss_id, "Phidget %s: (AS3 Client, ID: %s), (Device: %s, %s%s, %s%s)", v, port, deviceType, 
					   serialNumber?"Serial Number: ":"Any Serial", serialNumber?serialNumber:"",
					   label?"Label: ":"Any Label", label?label:"");
			}
			else
			{
				pu_log(PUL_INFO, ((pds_session_t *)pdss)->pdss_id, "Phidget %s: (Client: %s:%s), (Device: %s, %s%s, %s%s)", v, ipaddr, port, deviceType, 
					   serialNumber?"Serial Number: ":"Any Serial", serialNumber?serialNumber:"",
					   label?"Label: ":"Any Label", label?label:"");
			}
			if(deviceType && v)
			{
				if(!strcmp(v, "Open"))
				{
					if(serialNumber && strlen(serialNumber) > 0)
						serial = strtol(serialNumber, NULL, 10);
					else
						serial = -1;
					deviceID = phidget_type_to_id(deviceType);
					
					pd_unlock((void *)&pd_mutex);
					pthread_mutex_lock(&PhidgetsAndClientsMutex);
					if(open_phidget(pdss, deviceID, serial, label, ipaddr, port))
					{
						pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Couldn't open device");
						pthread_mutex_unlock(&PhidgetsAndClientsMutex);
						pd_lock((void *)&pd_mutex);
						free(ipaddr); ipaddr = NULL;
						free(port); port = NULL;
						free(deviceType); deviceType = NULL;
						free(serialNumber); serialNumber = NULL;
						free(label); label = NULL;
						free(l); l = NULL;
						free(v); label = NULL;
						return;
					}
					pthread_mutex_unlock(&PhidgetsAndClientsMutex);
					pd_lock((void *)&pd_mutex);
				}
				//client called close
				//for some reason, this used to not run for "open any" phidgets - why??
				if(!strcmp(v, "Close"))// && (serialNumber || label))
				{
					if(serialNumber)
						serial = strtol(serialNumber, NULL, 10);
					else
						serial = -1;
					deviceID = phidget_type_to_id(deviceType);
					
					pd_unlock((void *)&pd_mutex);
					pthread_mutex_lock(&PhidgetsAndClientsMutex);
					if((res = close_phidget(pdss, deviceID, serial, label, ipaddr, port)))
					{
						if(res == EPHIDGET_NOTFOUND)
							pu_log(PUL_INFO, ((pds_session_t *)pdss)->pdss_id, "Couldn't find device that close was called on");
						else
							pu_log(PUL_ERR, ((pds_session_t *)pdss)->pdss_id, "Couldn't close device");
						pthread_mutex_unlock(&PhidgetsAndClientsMutex);
						pd_lock((void *)&pd_mutex);
						free(ipaddr); ipaddr = NULL;
						free(port); port = NULL;
						free(deviceType); deviceType = NULL;
						free(serialNumber); serialNumber = NULL;
						free(label); label = NULL;
						free(l); l = NULL;
						free(v); v = NULL;
						return;
					}
					pthread_mutex_unlock(&PhidgetsAndClientsMutex);
					pd_lock((void *)&pd_mutex);
				}
			}
	}

	free(ipaddr); ipaddr = NULL;
	free(port); port = NULL;
	free(deviceType); deviceType = NULL;
	free(serialNumber); serialNumber = NULL;
	free(label); label = NULL;
	free(l); l = NULL;
	free(v); v = NULL;
}
