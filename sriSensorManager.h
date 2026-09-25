#ifndef SRI_SRISENSORIDASEXTEND_H
#define SRI_SRISENSORIDASEXTEND_H

//#include "sriCommDefine.h"
//#include "sriCommManager.h"
#include "sriSensorDataDefine.h"
#include <unordered_map>
typedef unsigned char       BYTE;

class CSRICommManager;

class CSRISensorManager
{
public:
	CSRISensorManager();
	~CSRISensorManager();
	bool sriRunSensorManager( std::string LocalServerIP, int LocalServerPort);


	bool sriStartSample(BYTE sensorId, int sampRate);
	bool sriStopSample(BYTE sensorId);
	bool GetSensorData(BYTE sensorId,std::vector<ForceSensorData>& forceSensorDataVector);
	bool GetRealtimeData(BYTE sensorId,ForceSensorData& forceSensorDataVector);

	bool GetAllSensorRealtimeData(std::unordered_map<BYTE, ForceSensorData>& forceSensorDataMap);
	bool SetSensorDataZero(BYTE sensorId);
	bool SetAllSensorDataZero();
	bool GetOnlineSensorsId(std::vector<BYTE>& sensorIds);

	bool GetAllSensorData(std::unordered_map<BYTE, std::vector<ForceSensorData>>& forceSensorDataMap);

	int mLocalPort = 4008;
private:
	CSRICommManager* mSensorManager;

	//多个传感器，通过unordered_map数据类型返回，这两个接口暂时不使用
	bool GetAllSensorRealtimeData(std::vector<ForceSensorData>& forceSensorDataVector);
	bool GetAllSensorData(std::vector < std::vector<ForceSensorData>>& forceSensorDataVector);

};//
#endif
