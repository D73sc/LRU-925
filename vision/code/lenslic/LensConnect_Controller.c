
#include"LensCtrl.h"
#include"LensConnect.h"

#define PROGRAM_VERSION "1.1.0"
int ItemNum;
int detailSelect;
int fineNum;
BOOL usbOpen_flag;
BOOL withZoom = FALSE;
BOOL withFocus = FALSE;
BOOL withIris = FALSE;
BOOL withOptFil = FALSE;

char* OnOff(uint16_t flag) {
	if (flag == 1)
		return "ON";
	else
		return "OFF";
}

void ScanUSBLensInfo() {
	DWORD numDevices = 0;
	char snString[260];			// SnString is 260bytes according to the instructions of the USB IC
	char model[25];

	UsbGetNumDevices(&numDevices);
	if (numDevices >= 1) {
		printf("No.: S/N\n");
		for (uint16_t i = 0; i < numDevices; i++) {
			int retval = UsbGetSnDevice(i, snString);
			if (retval == SUCCESS) {
				printf(" %d : %s, ", i, snString);
				UsbOpen(i);
				ModelName(model);
				printf("%s, ", model);
				UserIDRead();
				UsbClose();
			}
			else {
				printf(" %lu : ", i);
				printf("Device access error.                ");
				printf("The device may already be running.\n");
			}
		}
	}
	else
	{
		printf("No LensConnect is connected.\r\n");
	}
	printf("\n");
}

void UsbConnect(int deviceNumber) {
	uint16_t capabilities;
	int retval = UsbOpen(deviceNumber);
	if (retval != SUCCESS) {
		printf("%s\n", ErrorTxt(retval));
		return;
	}
	retval = UsbSetConfig();
	if (retval != SUCCESS) {
		printf("%s\n", ErrorTxt(retval));
		return;
	}
	printf("opened\r\n");
	CapabilitiesRead(&capabilities);
	Status2ReadSet();

	if (capabilities & ZOOM_MASK) {
		ZoomParameterReadSet();
		if ((status2 & ZOOM_MASK) == INIT_COMPLETED)
			ZoomCurrentAddrReadSet();
		withZoom = TRUE;
	}
	if (capabilities & FOCUS_MASK) {
		FocusParameterReadSet();
		if ((status2 & FOCUS_MASK) == INIT_COMPLETED)
			FocusCurrentAddrReadSet();
		withFocus = TRUE;
	}
	if (capabilities & IRIS_MASK) {
		IrisParameterReadSet();
		if ((status2 & IRIS_MASK) == INIT_COMPLETED)
			IrisCurrentAddrReadSet();
		withIris = TRUE;
	}
	if (capabilities & OPT_FILTER_MASK) {
		OptFilterParameterReadSet();
		if ((status2 & OPT_FILTER_MASK) == INIT_COMPLETED)
			OptFilterCurrentAddrReadSet();
		withOptFil = TRUE;
	}
	usbOpen_flag = TRUE;
}
UsbDisconnect() {
	UsbClose();
	usbOpen_flag = FALSE;
	withZoom = FALSE;
	withFocus = FALSE;
	withIris = FALSE;
	withOptFil = FALSE;
}

void CommandList() {
	printf("\n----- LensConnect Control -----\n");
	printf(" 0 : Command list\n");
	if (withZoom == TRUE) {
		printf("----- ZOOM  -----\n");
		printf(" 1 : Initialize zoom\n");
		if ((status2 & ZOOM_MASK) == INIT_COMPLETED) {
			printf("11 : Move zoom(%d - %d)\n", zoomMinAddr, zoomMaxAddr);
			printf("12 : Displays the current zoom  address\n\n");
		}
	}
	if (withFocus == TRUE) {
		printf("----- FOCUS -----\n");
		printf(" 2 : Initialize focus\n");
		if ((status2 & FOCUS_MASK) == INIT_COMPLETED) {
			printf("21 : Move focus(%d - %d)\n", focusMinAddr, focusMaxAddr);
			printf("22 : Displays the current focus address\n");
			printf("23 : Fine move focus control\n");
			printf("24 : Fine step number setting\n\n");
		}
	}
	if (withIris == TRUE) {
		printf("----- IRIS  -----\n");
		printf(" 3 : Initialize iris\n");
		if ((status2 & IRIS_MASK) == INIT_COMPLETED) {
			printf("31 : Move iris(%d - %d)\n", irisMinAddr, irisMaxAddr);
			printf("32 : Displays the current iris  address\n\n");
		}
	}
	if (withOptFil == TRUE){
		printf("----- Optical Fiter  -----\n");
		printf(" 4 : Initialize optical filter\n");
		if ((status2 & OPT_FILTER_MASK) == INIT_COMPLETED) {
			printf("41 : Move optical filter(0 - %d)\n", optFilMaxAddr);
			printf("42 : Displays the current optical filter address\n\n");
		}
	}
	printf("----- Other -----\n");
	printf(" 5 : Parameter setup\n");
	printf(" 6 : Displays the information\n");
	printf(" 7 : Displays the user name\n");
	printf("71 : Write the user area\n");
	printf(" 8 : Close USB (other lens)\n");
	printf(" 9 : Exit application\n\n");
}

void CommandSetupList() {
	printf("\n");
	if (withZoom == TRUE) {
		printf(" 1: Zoom \n");
	}
	if (withFocus == TRUE) {
		printf(" 2: Focus \n");
	}
	if (withIris == TRUE) {
		printf(" 3: Iris  \n");
	}
	printf(" 5: Return to main\n\n");
}

void DetailSetupList(int motorNumber) {
	uint16_t flag;
	printf("\n 1: Speed change. ");
	switch(motorNumber) {
	case ZOOM:
		printf("Current zoom speed is %d\n", zoomSpeedPPS);
		break;
	case FOCUS:
		printf("Current focus speed is %d\n", focusSpeedPPS);
		break;
	case IRIS:
		printf("Current iris speed is %d\n", irisSpeedPPS);
		break;
	}
	printf(" 2: Backlash correction change. ");
	switch (motorNumber) {
	case ZOOM:
		ZoomBacklashRead(&flag);
		printf("Currently %s\n", OnOff(flag));
		break;
	case FOCUS:
		FocusBacklashRead(&flag);
		printf("Currently %s\n", OnOff(flag));
		break;
	case IRIS:
		IrisBacklashRead(&flag);
		printf("Currently %s\n", OnOff(flag));
		break;
	}
	printf(" 5: Back (Cancel)\n\n");
}

void CommandLensInfoList() {
	printf("\n----- Information -----\n");
	printf(" 0: Displays General\n");
	if (withZoom == TRUE){
		printf(" 1: Displays Zoom\n");
	}
	if (withFocus == TRUE){
		printf(" 2: Displays Focus\n");
	}
	if (withIris == TRUE){
		printf(" 3: Displays Iris\n");
	}
	if (withOptFil == TRUE){
		printf(" 4: Displays Opticalfilter\n");
	}
	printf(" 5: Return to main\n\n");
}

void MainExe(int number) {
	int num;
	if (number == 0) {

	}
	else if ((number == 1) && (withZoom == TRUE)) {
		printf("Initializing\n");
		ZoomInit();			// +commandList();
	}
	else if ((number == 11) && (withZoom == TRUE)) {
		MoveLens(ZOOM);
	}
	else if ((number == 12) && (withZoom == TRUE)) {
		printf("\nZoom current address = %u\n\n", zoomCurrentAddr);
	}
	else if ((number == 2) && (withFocus == TRUE)) {
		printf("Initializing\n");
		FocusInit();		// +commandList();
	}
	else if ((number == 21) && (withFocus == TRUE)) {
		MoveLens(FOCUS);
	}
	else if ((number == 22) && (withFocus == TRUE)) {
		printf("\nFocus current address %u\n\n", focusCurrentAddr);
	}
	else if ((number == 23) && (withFocus == TRUE)) {
		printf("\nFine move focus:\n z -> Near / x-> Far / enter -> End, fine step = %u\n", fineNum);
		FineFocusMove();
	}
	else if ((number == 24) && (withFocus == TRUE)) {
		printf("Fine move step number = ");
		scanf_s("%u", &num);
		fineNum = num;
	}
	else if ((number == 3) && (withIris == TRUE)) {
		printf("Initializing\n");
		IrisInit();			// +commandList();
	}
	else if ((number == 31) && (withIris == TRUE)) {
		MoveLens(IRIS);
	}
	else if ((number == 32) && (withIris == TRUE)) {
		printf("\nIris current address %u\n\n", irisCurrentAddr);
	}
	else if ((number == 4) && (withOptFil == TRUE)) {
		printf("Initializing\n");
		OptFilterInit();	// +commandList();
	}
	else if ((number == 41) && (withOptFil == TRUE)) {
		MoveLens(OPT);
	}
	else if ((number == 42) && (withOptFil == TRUE)) {
		printf("\nOptical filter current address %u\n\n", optCurrentAddr);
	}
	else if (number == 5) {
		CommandSetupList();
		ItemNum = SETUP;
	}
	else if (number == 6) {
		CommandLensInfoList();
		ItemNum = INFO;
	}
	else if (number == 7) {
		printf("\n");
		UserIDRead();
		printf("\n");
	}
	else if (number == 71) {
		UserIDWrite();
	}
	else if (number == 8) {
		UsbDisconnect();
	}
	else {
		printf("\nWrong number entered.\n\n");
	}
	if (number <= 4)
		CommandList();
}

void SetupExe(int number) {
	static int motorNumber ;
	if (detailSelect == OFF) {
		if ((number == ZOOM) && (withZoom == TRUE)) {
			motorNumber = ZOOM;
			detailSelect = ON;
		}
		else if ((number == FOCUS) && (withFocus == TRUE)) {
			motorNumber = FOCUS;
			detailSelect = ON;
		}
		else if ((number == IRIS) && (withIris == TRUE)) {
			motorNumber = IRIS;
			detailSelect = ON;
		}
		else if (number == BACK_MAIN) {
			ItemNum = MAIN;
			CommandList();
		}
		else {
			printf("\nWrong number entered.\n\n");
			CommandSetupList();
		}
		if (detailSelect == ON) {
			DetailSetupList(motorNumber);
		}
	}
	else {					// detailSelect == ON
		switch (number) {
		case 1:
			switch (motorNumber) {
			case ZOOM:
				ZoomSpeedChange();
				break;
			case FOCUS:
				FocusSpeedChange();
				break;
			case IRIS:
				IrisSpeedChange();
				break;
			}
			break;
		case 2:
			switch (motorNumber) {
			case ZOOM:
				ZoomBacklashToggleCtl();
				break;
			case FOCUS:
				FocusBacklashToggleCtl();
				break;
			case IRIS:
				IrisBacklashToggleCtl();
				break;
			}
			break;
		case 5:
			break;
		default:
			printf("\nWrong number entered.\n\n");
			break;
		}
		CommandSetupList();
		detailSelect = OFF;
	}
}
void InfoExe(int number) {
	if (number == GENE) {
		GeneralInfo();
	}
	else if ((number == ZOOM) && (withZoom == TRUE)) {
		ZoomInfo();
	}
	else if ((number == FOCUS) && (withFocus == TRUE)) {
		FocusInfo();
	}
	else if ((number == IRIS) && (withIris == TRUE)) {
		IrisInfo();
	}
	else if ((number == OPT) && (withOptFil == TRUE)) {
		OptFilterInfo();
	}
	else if ((number == BACK_MAIN)) {
		ItemNum = MAIN;
		CommandList();
	}
	else {
		printf("\nWrong number entered.\n\n");
	}
	if (number!= BACK_MAIN) 
		CommandLensInfoList();
}

// int main()
// {
//     int number;
//     ItemNum = MAIN;
//     usbOpen_flag = FALSE;
//     fineNum = 2;
//     printf("LensConnect Control %s\n", PROGRAM_VERSION);
//     while (TRUE)
//     {
//         if (usbOpen_flag == FALSE) {	// When not Open
//             ScanUSBLensInfo();					// Up to 8lenses
//             printf(" 8 : Scan lens\n");
//             printf(" 9 : Exit application\n");
//             printf("\nPlease select connect lens No.");
//             scanf_s("%u", &number);
//             if (number == 9) {
//                 UsbDisconnect();
//                 break;
//             }
//             if (number != 8) {
//                 printf(" No.%u lens ", number);
//                 UsbConnect(number);
//                 CommandList();
//             }
//         }
//         else {							// When Open
//             printf("Select number:");
//             scanf_s("%u", &number);
//             switch (ItemNum) {
//             case MAIN:
//                 if (number == 9) {
//                     UsbDisconnect();
//                     return 1;
//                 }
//                 if (number == 8) {
//                     UsbDisconnect();
//                 }
//                 MainExe(number);
//                 break;
//             case SETUP:
//                 SetupExe(number);
//                 break;
//             case INFO:
//                 InfoExe(number);
//                 break;
//             }
//         }
//     }
// }

