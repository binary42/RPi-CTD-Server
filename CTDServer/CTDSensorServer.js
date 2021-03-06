/**
 * author:		J. Neilan
 * email:		jimbolysses@gmail.com
 * version:		v0.8
 * notes:
 * 	CTD server for http interfacing to the CTD payload. Uses socket-io, http, fs, phidgets, and
 * 	node-gyp C++ bridge for reading analog sensor data from the phidgets interface and IMU/Depth data
 * 	from the OpenROV IMU. Serves a generic table html page for data display. You will need to modify
 * 	the host ip and port for your own specific systems.
 * 
 * License info: Listen, I borrowed code and modified it. Thanks to the following sources: 
 * 	Martin Christen - www.martinchristen.ch/node/tutorial11
 * 	Various nodejs tutorial sites, esspecially brew - www.bonebrews.com/ds18b20-temperatures-in-your-
 * 	browser/
 * 
 * The MIT License (MIT)

	Copyright (c) 2015 All of Us :)

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
 * 	
 * 
 */

// Includes
var server 				= require( 'http' ).createServer( handler );
var io 					= require( '/opt/node/lib/node_modules/socket.io' )( server );

// File server
var fs 					= require( 'fs' );

// Phidgets 8/8/8 interface
var phidgets 			= require( '/opt/node/lib/node_modules/phidgets' );
var pik 				= new phidgets.PhidgetInterfaceKit();

// IMU C++ bridge - change as you need
var imu 				= require( '../ImuCppBridge/build/Release/ImuCppBridge' );

// Listen to port - change as you need
server.listen(8080);

// Ensure that incomming connections see index.html
function handler( req, res )
{
	fs.readFile(__dirname + '/index.html',
	function( err, data )
	{
		if( err )
		{
			res.writehead( 500 );
			return res.end( 'Error loading index.html' );
		}
		
		res.writeHead( 200 );
		res.end( data );
	});
}

// Connection handling - Emit phidgets sensor data to client
io.sockets.on('connection', function ( socket ) 
{
	// Interval for IMU reads - value in ms
	var imuInterval = 250;
	var imuData;
	
	console.log('\nServer listening on localhost:8080\n\n');

	pik.open({host: "localhost", port: 8888});
		
	socket.emit( 'sensors', {'test': 0 } );

	// Send temperature reading out to connected clients
    pik.on( 'sensor', function( emitter, data )	
    {	
		
		// Calculate Temperature - index 4
		if( data.index === 4 )
		{
			// Voltage and then scaled with / 10
			data.value = ( ( 5.22 * data.value ) * ( 100 / 1024 ) ) / 10;
			
			// Conversion to degrees F
			data.value *= ( 9.0 / 5.0 ) + 32.0;
		}
		// Calculate Pressure - index 5
		if( data.index === 5 )
		{
			data.value = ( 250 * ( ( data.value / 1024 ) +0.04 );
		} 
		// Calculate Salinity - index 6
		if( data.index === 6 )
		{
			data.value = ( 12.0 * data.value ) / 1024;
		}
		// Calculate Light - index 7
		if( data.index === 7 )
		{
			
		}
		
		socket.emit( 'values', {'id': data.index, 'value': data.value} );
	});
	
	setInterval( function()
	{
		// Imu Fused Pose Information	
		imuData = imu.GetIMUPoseData();
		socket.emit( 'values', {'id': 8, 'value': imuData} );
		
		imuData = imu.GetIMUDepthData();
		socket.emit( 'values', {'id': 9, 'value': imuData} );
		
	}, imuInterval );

});
