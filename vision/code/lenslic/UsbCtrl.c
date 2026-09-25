
#include "LensCtrl.h"

HID_SMBUS_DEVICE connectedDevice;
BYTE i2cAddr = I2CSLAVEADDR * 2;
BYTE receivedData[80];

int UsbGetNumDevices(DWORD *numDevices) {
	int retval = HidSmbus_GetNumDevices(numDevices, VID, PID);
	return retval;
}
int UsbGetSnDevice(uint16_t index, char* SnString) {
	int retval = HidSmbus_GetString(index, VID, PID, SnString, 
		HID_SMBUS_GET_SERIAL_STR);
	return retval;
}
int UsbOpen(DWORD deviceNumber) {
	int retval = HidSmbus_Open(&connectedDevice, deviceNumber, VID, PID);
	return retval;
}
void UsbClose() {
	HidSmbus_Close(connectedDevice);
}
int UsbSetConfig() {
	int retval = HidSmbus_SetSmbusConfig(connectedDevice,
		BITRATE, i2cAddr, AUTOREADRESPOND,
		WRITETIMEOUT, READTIMEOUT,
		SCLLOWTIMEOUT, TRANSFARRETRIES);
	if (retval != HID_SMBUS_SUCCESS)
		return retval;

	retval = HidSmbus_SetGpioConfig(connectedDevice, 
		DIRECTION, MODE, SPECIAL, CLKDIV);
	if (retval != HID_SMBUS_SUCCESS)
		return retval;

	retval = HidSmbus_SetTimeouts(connectedDevice, RESPONSETIMEOUT);
	if (retval != HID_SMBUS_SUCCESS)
		return retval;

	return retval;
}


int UsbRead(uint16_t segmentOffset, uint16_t receiveSize) {	

	uint8_t sendData[SEGMENTOFFSET_LENGTH];
	sendData[0] = segmentOffset >> 8;
	sendData[1] = (uint8_t)segmentOffset;
	BYTE sendSize = sizeof(sendData);

	int retval = HidSmbus_WriteRequest(connectedDevice, i2cAddr, 
		sendData, sendSize);
	if (retval != HID_SMBUS_SUCCESS)
		return retval;

	retval = HidSmbus_ReadRequest(connectedDevice, i2cAddr, receiveSize);
	if (retval != HID_SMBUS_SUCCESS)
		return retval;

	retval = HidSmbus_ForceReadResponse(connectedDevice, receiveSize);
	if (retval != HID_SMBUS_SUCCESS)
		return retval;
		
	uint8_t receiveData[128];
	HID_SMBUS_S0 status;
	BYTE totalBytesRead = 0;
	BYTE bufferSize = 62;
	BYTE bytesRead = 0;

	do {
		retval = HidSmbus_GetReadResponse(connectedDevice, &status, 
			receiveData + totalBytesRead, bufferSize, &bytesRead);
		if (retval != HID_SMBUS_SUCCESS)
			return retval;

		totalBytesRead += bytesRead;
	} while (totalBytesRead < receiveSize);

	for (int i = 0; i < totalBytesRead; i++) {
		receivedData[i] = receiveData[i];
	}
	return retval;
}


uint8_t* UsbRead1(uint16_t segmentOffset, uint16_t receiveSize) {
    uint8_t sendData[SEGMENTOFFSET_LENGTH];
    sendData[0] = segmentOffset >> 8;
    sendData[1] = (uint8_t)segmentOffset;
    BYTE sendSize = sizeof(sendData);

    // 定义静态数组存储最后一次接收的数据
    static uint8_t lastReceivedData[128];
    uint8_t receiveData[128];
    HID_SMBUS_S0 status;
    BYTE totalBytesRead = 0;
    BYTE bufferSize = 62;
    BYTE bytesRead = 0;
    int retval;

    // 发送请求
    retval = HidSmbus_WriteRequest(connectedDevice, i2cAddr, sendData, sendSize);
    if (retval != HID_SMBUS_SUCCESS) {
        return NULL; // 发生错误时返回NULL
    }

    retval = HidSmbus_ReadRequest(connectedDevice, i2cAddr, receiveSize);
    if (retval != HID_SMBUS_SUCCESS) {
        return NULL; // 发生错误时返回NULL
    }

    retval = HidSmbus_ForceReadResponse(connectedDevice, receiveSize);
    if (retval != HID_SMBUS_SUCCESS) {
        return NULL; // 发生错误时返回NULL
    }

    // 接收数据，但只保留最后一次的数据
    do {
        retval = HidSmbus_GetReadResponse(connectedDevice, &status, receiveData, bufferSize, &bytesRead);
        if (retval != HID_SMBUS_SUCCESS || bytesRead == 0) {
            return NULL; // 发生错误或无数据时返回NULL
        }

        // 复制当前接收到的数据到lastReceivedData
        memcpy(lastReceivedData, receiveData, bytesRead);
        totalBytesRead += bytesRead;
    } while (totalBytesRead < receiveSize);

    // 返回指向最后一次接收数据的指针
    return lastReceivedData;
}


uint16_t UsbRead2Bytes() {
	return ((receivedData[0] << 8) | receivedData[1]);
}
uint32_t CountRead() {
	uint32_t count;
	count = ((receivedData[0] << 24) | (receivedData[1] << 16) |
		(receivedData[2] << 8) | (receivedData[3]));
	return count;
}
int UsbWrite(uint16_t segmentOffset, uint16_t writeData) {
	uint8_t sendData[SEGMENTOFFSET_LENGTH + DATA_LENGTH];
	sendData[0] = segmentOffset >> 8;
	sendData[1] = (uint8_t)segmentOffset;
	sendData[2] = writeData >> 8;
	sendData[3] = (uint8_t)writeData;
	BYTE sendSize = sizeof(sendData);
	int retval = HidSmbus_WriteRequest(connectedDevice, i2cAddr, 
		sendData, sendSize);

	return retval;
}



