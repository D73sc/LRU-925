#pragma once

#include "DefVal.h"
extern BOOL usbOpen_flag;
extern int ItemNum;
//extern uint16_t capabilities;
extern int fineNum;

// LensConnect_Controller.c
char* OnOff(uint16_t val);

// LensInfo.c
void GeneralInfo();
void ZoomInfo();
void FocusInfo();
void IrisInfo();
void OptFilterInfo();


// LenSetup.c
void ZoomSpeedChange();
void FocusSpeedChange();
void IrisSpeedChange();
void ZoomBacklashToggleCtl();
void FocusBacklashToggleCtl();
void IrisBacklashToggleCtl();

// LensAccess.c
void MoveLens(int motorNumber);
void MoveLens1(int motor,int addr);
void UserIDRead();
void UserIDWrite();
void FineFocusMove();
void LimFocusMove(uint16_t addrData);
