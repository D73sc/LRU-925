#include"LensCtrl.h"
#include"LensConnect.h"
#include<conio.h>

void MoveLens(int motor) {
	int addrData = 0;
	printf("\nEnter the address you want to go ");
	scanf_s("%u", &addrData);
	switch (motor) {
	case ZOOM:
		ZoomMove((uint16_t)addrData);
		break;
	case FOCUS :
		FocusMove((uint16_t)addrData);
		break;
	case IRIS:
		IrisMove((uint16_t)addrData);
		break;
	case OPT:
		OptFilterMove((uint16_t)addrData);
		break;
	}
	printf("\n");
}

void MoveLens1(int motor,int addr) {
    int addrData = addr;
    switch (motor) {
    case ZOOM:
        ZoomMove((uint16_t)addrData);
        break;
    case FOCUS :
        FocusMove((uint16_t)addrData);
        break;
    case IRIS:
        IrisMove((uint16_t)addrData);
        break;
    case OPT:
        OptFilterMove((uint16_t)addrData);
        break;
    }
}


void UserIDRead() {
	char userName[33];
	UserAreaRead(userName);
	printf("%s\n", userName);

}
void UserIDWrite() {
	char userName[34];
	printf("\nPlease fill in the user area(32byte full)\n");
	printf("12345678901234567890123456789012\n");
	scanf_s(" %32[^\n]%*[^\n]", userName, 34);
	UserAreaWrite(userName);
	printf("\n");
}

void FineFocusMove() {
#define QUIT_CHAR 0x0d	// Enter key
	int c;
	uint16_t addrData;
	while ((c = _getch()) != QUIT_CHAR) {
		switch (c) {
		case 0x7A:
			addrData = focusCurrentAddr + fineNum;
			LimFocusMove(addrData);
			//FocusMove(addrData);
			break;
		case 0x78:
			addrData = focusCurrentAddr - fineNum;
			LimFocusMove(addrData);
			//FocusMove(addrData);
			break;
		default:
			break;
		}
	}
	printf("\nEnd fine move focus mode\n");
	putchar('\n');
}

void LimFocusMove(uint16_t addrData){
	if ((focusMaxAddr > addrData) & (addrData > focusMinAddr)) {
		int retval = FocusMove(addrData);
		if (retval == SUCCESS) {
			printf("\rAddr= %d", focusCurrentAddr);
		}
	}
	else {
		printf("\rLimit      ");
	}
}
