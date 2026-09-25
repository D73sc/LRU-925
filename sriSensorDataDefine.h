
#ifndef SRI_SENSOR_DATA_DEFINE_H
#define SRI_SENSOR_DATA_DEFINE_H
#include <basetsd.h>

typedef unsigned char       BYTE;

struct ForceSensorData
{
	BYTE sensorId;
	float channels[6];//力数据，分别是FX-MZ,单位N/Nm
	UINT16 packetNum;//包编号
	float momentArm_X;//X方向的力臂，单位mm
	float momentArm_Y;
};


#endif
