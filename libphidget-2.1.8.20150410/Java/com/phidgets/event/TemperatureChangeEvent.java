/*
 * This file is part of libphidget21
 *
 * Copyright © 2006-2015 Phidgets Inc <patrick@phidgets.com>
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

package com.phidgets.event;

import com.phidgets.Phidget;

/**
 * This class represents the data for a TemperatureChangeEvent.
 * 
 * @author Phidgets Inc.
 */
public class TemperatureChangeEvent
{
	Phidget source;
	int index;
	double value;

	/**
	 * Class constructor. This is called internally by the phidget library when creating this event.
	 * 
	 * @param source the Phidget object from which this event originated
	 */
	public TemperatureChangeEvent(Phidget source, int index, double value) {
		this.source = source;
		this.index = index;
		this.value = value;
	}

	/**
	 * Returns the source Phidget of this event. This is a reference to the Phidget object from which this
	 * event was called. This object can be cast into a specific type of Phidget object to call specific
	 * device calls on it.
	 * 
	 * @return the event caller
	 */
	public Phidget getSource() {
		return source;
	}

	/**
	 * Returns the index of the temperature sensor. An index of 0 indicated the Cold Juction Temperature sensing IC.
	 * An index of 1 indicated the thermocouple.
	 * 
	 * @return the index of the sensor
	 */
	public int getIndex() {
		return index;
	}

	/**
	 * Returns the temperature of the sensor is degrees celcius.
	 * 
	 * @return the temperature of the sensor
	 */
	public double getValue() {
		return value;
	}

	/**
	 * Returns a string containing information about the event.
	 * 
	 * @return an informative event string
	 */
	public String toString() {
		return source.toString() + " Temperature " + index + " changed to "
		  + value;
	}
}
