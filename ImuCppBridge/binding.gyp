{
    'targets' : [
      {
        'target_name' : 'ImuCppBridge',
		'sources' : ['ImuCppBridge.cpp', '../../CTDInterface/CIMUInterface.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTMath.cpp',			
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTIMUHal.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTFusion.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTFusionKalman4.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTFusionRTQF.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTIMUSettings.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTIMUAccelCal.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/RTIMUMagCal.cpp',	
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMU.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUNull.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUMPU9150.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUMPU9250.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUGD20HM303D.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUGD20M303DLHC.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUGD20HM303DLHC.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMULSM9DS0.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUBMX055.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTIMUBNO055.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTPressure.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTPressureBMP180.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTPressureLPS25H.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTPressureMS5611.cpp',
					'../../../OpenROVIMU/RTIMULib/RTIMULib/IMUDrivers/RTPressureMS5637.cpp' ],
		'include_dirs': ['../../../OpenROVIMU/RTIMULib/RTIMULib']
      }
    ]
}
