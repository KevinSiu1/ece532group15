/************************************************************************/
/*																		*/
/*	video_demo.c	--	ZYBO Video demonstration 						*/
/*																		*/
/************************************************************************/
/*	Author: Sam Bobrowicz												*/
/*	Copyright 2015, Digilent Inc.										*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/*		This file contains code for running a demonstration of the		*/
/*		Video input and output capabilities on the ZYBO. It is a good	*/
/*		example of how to properly use the display_ctrl and				*/
/*		video_capture drivers.											*/
/*																		*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		11/25/2015(SamB): Created										*/
/*																		*/
/************************************************************************/

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "video_demo.h"
#include "video_capture/video_capture.h"
#include "display_ctrl/display_ctrl.h"
#include "intc/intc.h"
#include <stdio.h>
#include "xuartlite_l.h"
//#include "xuartps.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xosd.h"
#include "../../hdmi_wrapper_hw_platform_2/drivers/ColorDetect2_v1_0/src/ColorDetect2.h"
#include "../../hdmi_wrapper_hw_platform_2/drivers/MotionDeIP_v1_0/src/MotionDeIP.h"

/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
#define VID_VTC_ID XPAR_VTC_1_DEVICE_ID
#define VID_GPIO_ID XPAR_AXI_GPIO_VIDEO_DEVICE_ID
#define VID_VTC_IRPT_ID XPAR_INTC_0_VTC_1_VEC_ID
#define VID_GPIO_IRPT_ID XPAR_INTC_0_GPIO_0_VEC_ID
#define SCU_TIMER_ID XPAR_AXI_TIMER_0_DEVICE_ID
#define UART_BASEADDR XPAR_UARTLITE_0_BASEADDR

/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */

/*
 * Display and Video Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma vdma;
VideoCapture videoCapt;
INTC intc;
char fRefresh; //flag used to trigger a refresh of the Menu on video detect

/*
 * Framebuffers for video data
 */
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];
u8 *pFrames[DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers

/*
 * Interrupt vector table
 */
const ivt_t ivt[] = {
	videoGpioIvt(VID_GPIO_IRPT_ID, &videoCapt),
	videoVtcIvt(VID_VTC_IRPT_ID, &(videoCapt.vtc))
};


/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
/* ------------------------------------------------------------ */

int main(void)
{
	char userInput = 0;
	u32 locked;
	XGpio *GpioPtr = &videoCapt.gpio;
	int i;

	Xil_ICacheEnable();
	Xil_DCacheEnable();
	DemoInitialize();

	XOsd xosd_inst;
	XOsd_Config *xosd_cfg;

	xosd_cfg = XOsd_LookupConfig(XPAR_OSD_0_DEVICE_ID);
	if (XOsd_CfgInitialize(&xosd_inst, xosd_cfg, XPAR_OSD_0_BASEADDR) != XST_SUCCESS)
		xil_printf("XOsd_CfgInitialize FAILED\r\n");
	if (XOsd_SelfTest(&xosd_inst) != XST_SUCCESS)
		xil_printf("XOsd_SelfTest FAILED\r\n");

	XOsd_Reset(&xosd_inst);
	XOsd_SyncReset(&xosd_inst);

	XOsd_RegUpdateEnable(&xosd_inst);
	XOsd_Start(&xosd_inst);

	//XOsd_DisableLayer(&xosd_inst, 0);
	//XOsd_DisableLayer(&xosd_inst, 1);

	(&xosd_inst)->ScreenWidth = 0x500;
	(&xosd_inst)->ScreenHeight = 0X2D0;

	XOsd_SetLayerDimension(&xosd_inst, 0, 0, 0, 0x500, 0x2D0);
	XOsd_SetLayerDimension(&xosd_inst, 1, 0, 0, 0x500, 0x2D0);

	//Set the Colours
	u32 ColorData[16];

	//Format: ARBG (Ignore Alpha Value if SetLayerAlpha)
	//For Text Strings - only even colors matter
	ColorData[0] = 0x00000000;	//Black
	ColorData[1] = 0x00000000;	//Black
	ColorData[2] = 0xffff0000;	//Red
	ColorData[3] = 0x00000000;	//Black
	ColorData[4] = 0xff0000ff;	//Green
	ColorData[5] = 0x00000000;	//Black
	ColorData[6] = 0xff00ff00;	//Blue
	ColorData[7] = 0x00000000;	//Black
	ColorData[8] = 0xffffffff;	//White
	ColorData[9] = 0x80000000;	//Black
	ColorData[10] = 0x80008000;	//Dark Blue
	ColorData[11] = 0x80800000;	//Dark Red
	ColorData[12] = 0x80000000;	//Black
	ColorData[13] = 0x80808080;	//Dark Grey
	ColorData[14] = 0x80c0c0c0;	//Light Grey
	ColorData[15] = 0x00000000;	//Black
	//Load the Colours
	XOsd_LoadColorLUTBank(&xosd_inst, 1, 1, ColorData);

	//Load Strings
	u32 TextData[64];

	//Strings
	TextData[0] = 0x0d0b030a; //First String at 0 (Used for SCORE text)
	TextData[1] = 0x00000006;
	TextData[8] = 0x00000b0b; //Second String at 8 (Used for SCORE value)
	TextData[9] = 0x00000000;
	TextData[16] = 0x050b0b09; //Third String at 16 (Used for GOOD/BAD)
	TextData[17] = 0x00000000;
	TextData[24] = 0x00000000; //Fourth String used for Top Half of Arrow
	TextData[25] = 0x00000000;
	TextData[32] = 0x00000000; //Fifth String used for Bottom Half of Arrow
	TextData[33] = 0x00000000;

	//Characters
	TextData[2] = 0x663c1800; //A
	TextData[3] = 0x00667e66;
	TextData[4] = 0x7c667c00; //B
	TextData[5] = 0x007c6666;
	TextData[6] = 0x60603e00; //C
	TextData[7] = 0x003e6060;
	TextData[10] = 0x66667c00; //D
	TextData[11] = 0x007c6666;
	TextData[12] = 0x7c603e00; //E
	TextData[13] = 0x003e6060;
	TextData[14] = 0x3e067c00; //Unused!
	TextData[15] = 0x007e0606;
	TextData[18] = 0x60603c00; //G
	TextData[19] = 0x003c6666;
	TextData[20] = 0x7e603e00; //S
	TextData[21] = 0x007c0606;
	TextData[22] = 0x66663c00; //O/0
	TextData[23] = 0x003c6666;
	TextData[26] = 0x7c663c00; //R
	TextData[27] = 0x00666c78;
	TextData[28] = 0x0c0c1800; //1
	TextData[29] = 0x001e0c0c;
	TextData[30] = 0x7c067c00; //2
	TextData[31] = 0x007e6060;
	TextData[34] = 0x3c067c00; //3
	TextData[35] = 0x007c0606;
	TextData[36] = 0x66666600; //4
	TextData[37] = 0x0006063e;
	TextData[38] = 0x7c607e00; //5
	TextData[39] = 0x007c0606;
	TextData[40] = 0x7c603e00; //6
	TextData[41] = 0x007c6666;
	TextData[42] = 0x06667c00; //7
	TextData[43] = 0x00060606;
	TextData[44] = 0x3c663c00; //8
	TextData[45] = 0x003c6666;
	TextData[46] = 0x3e663c00; //9
	TextData[47] = 0x007c0606;

	TextData[48] = 0xfe0c1830; //Right Arrow
	TextData[49] = 0x30180cfe;
	TextData[50] = 0x7f30180c; //Left Arrow
	TextData[51] = 0x0c18307f;
	TextData[52] = 0xff000000; //Arrow Tail (Horizontal)
	TextData[53] = 0x000000ff;
	TextData[54] = 0x7e3c1800; //Up Arrow
	TextData[55] = 0x181818db;
	TextData[56] = 0xdb181818; //Down Arrow
	TextData[57] = 0x00183c7e;
	TextData[58] = 0x18181818; //Arrow Tail (Vertical)
	TextData[59] = 0x18181818;

	//Unused
	TextData[60] = 0x00000000;
	TextData[61] = 0x00000000;
	TextData[62] = 0x00000000;
	TextData[63] = 0x00000000;


	XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);

	//Create Instructions
	u32 InstSetPtr[20]; //Instructions are 4-words
	/*XOsd_CreateInstruction(&xosd_inst, InstSetPtr, 1, XOSD_INS_OPCODE_BOX, 0, 0x0, 0x0, 0x50, 0x50, 0, 0x0);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+4, 1, XOSD_INS_OPCODE_BOX, 0, 0x50, 0x0, 0xa0, 0x50, 0, 0x1);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_BOX, 0, 0xa0, 0x0, 0xf0, 0x50, 0, 0x2);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+12, 1, XOSD_INS_OPCODE_BOX, 0, 0xf0, 0x0, 0x140, 0x50, 0, 0x3);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+16, 1, XOSD_INS_OPCODE_BOX, 0, 0x140, 0x0, 0x190, 0x50, 0, 0x4);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+20, 1, XOSD_INS_OPCODE_BOX, 0, 0x190, 0x0, 0x1e0, 0x50, 0, 0x5);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+24, 1, XOSD_INS_OPCODE_BOX, 0, 0x1e0, 0x0, 0x230, 0x50, 0, 0x6);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+28, 1, XOSD_INS_OPCODE_BOX, 0, 0x230, 0x0, 0x280, 0x50, 0, 0x7);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+32, 1, XOSD_INS_OPCODE_BOX, 0, 0x280, 0x0, 0x2d0, 0x50, 0, 0x8);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+36, 1, XOSD_INS_OPCODE_BOX, 0, 0x2d0, 0x0, 0x320, 0x50, 0, 0x9);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+40, 1, XOSD_INS_OPCODE_BOX, 0, 0x320, 0x0, 0x370, 0x50, 0, 0xa);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+44, 1, XOSD_INS_OPCODE_BOX, 0, 0x370, 0x0, 0x3c0, 0x50, 0, 0xb);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+48, 1, XOSD_INS_OPCODE_BOX, 0, 0x3c0, 0x0, 0x410, 0x50, 0, 0xc);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+52, 1, XOSD_INS_OPCODE_BOX, 0, 0x410, 0x0, 0x460, 0x50, 0, 0xd);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+56, 1, XOSD_INS_OPCODE_BOX, 0, 0x460, 0x0, 0x4b0, 0x50, 0, 0xe);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+60, 1, XOSD_INS_OPCODE_BOX, 0, 0x4b0, 0x0, 0x4ff, 0x50, 0, 0xf);*/


	/*XOsd_CreateInstruction(&xosd_inst, InstSetPtr+4, 1, XOSD_INS_OPCODE_BOX, 0, 0x100, 0x100, 0x200, 0x200, 0, 0x1);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_BOX, 0x8, 0x10, 0x100, 0x50, 0x200, 0, 0x2);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+12, 1, XOSD_INS_OPCODE_BOX, 0x8, 0x160, 0x160, 0x180, 0x180, 0, 0x3);*/

	//Note 0x0 in text index field points to index 0
	//     0x1 in text index field points to index 8
	//	   0x2 in text index field should point to index 16, etc...

	//Score Text
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x2ff, 0x28f, 0x4ff, 0x28f, 0x0, 0x8);

	//Score Value
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+4, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x47f, 0x28f, 0x4ff, 0x28f, 0x1, 0x8);

	//Good/Bad
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x4);

	//Arrow (Upper and Lower Strings)
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+12, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x0, 0x0, 0x80, 0x0, 0x3, 0x2);
	XOsd_CreateInstruction(&xosd_inst, InstSetPtr+16, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x0, 0x40, 0x80, 0x40, 0x4, 0x2);

	//Load Instruction List
	XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);

	//Set Active Bank
	XOsd_SetActiveBank(&xosd_inst, 1, 1, 1, 1, 1);

	//XOsd_SetBackgroundColor(&xosd_inst, 128, 0, 0);
	//XOsd_SetLayerAlpha(&xosd_inst, 1, 1, 256);

	XOsd_EnableLayer(&xosd_inst, 1);

	COLORDETECT2_mWriteReg(XPAR_COLORDETECT2_0_S00_AXI_BASEADDR, COLORDETECT2_S00_AXI_SLV_REG0_OFFSET, 0x00000010);

	xil_printf("Hello World!\r\n");

	int score = 0;

	//DemoRun();
	while (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
	{
		XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
	}
	while (userInput != 'q')
		{
			fRefresh = 0;

			while (XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh)
			{}

			if (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
			{
				userInput = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
				xil_printf("%c", userInput);
			}
			else  //Refresh triggered by video detect interrupt
			{
				userInput = 'r';
			}

			switch (userInput)
			{
			case '1':
				//Increment Score
				switch (score)
				{
				case 0: TextData[8] = 0x00000e0b; score++; break;
				case 1: TextData[8] = 0x00000f0b; score++; break;
				case 2: TextData[8] = 0x0000110b; score++; break;
				case 3: TextData[8] = 0x0000120b; score++; break;
				case 4: TextData[8] = 0x0000130b; score++; break;
				case 5: TextData[8] = 0x0000140b; score++; break;
				case 6: TextData[8] = 0x0000150b; score++; break;
				case 7: TextData[8] = 0x0000160b; score++; break;
				case 8: TextData[8] = 0x0000170b; score++; break;
				case 9: TextData[8] = 0x00000b0e; score++; break;
				case 10: TextData[8] = 0x00000e0e; score++; break;
				case 11: TextData[8] = 0x00000f0e; score++; break;
				case 12: TextData[8] = 0x0000110e; score++; break;
				case 13: TextData[8] = 0x0000120e; score++; break;
				case 14: TextData[8] = 0x0000130e; score++; break;
				case 15: TextData[8] = 0x0000140e; score++; break;
				case 16: TextData[8] = 0x0000150e; score++; break;
				case 17: TextData[8] = 0x0000160e; score++; break;
				case 18: TextData[8] = 0x0000170e; score++; break;
				case 19: TextData[8] = 0x00000b0f; score++; break;
				default: break;
				}
				XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
				break;
			case '2':
				//Display Good
				TextData[16] = 0x050b0b09;
				XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);

				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x2ff, 0x28f, 0x4ff, 0x28f, 0x0, 0x8);
				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr+4, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x47f, 0x28f, 0x4ff, 0x28f, 0x1, 0x8);
				XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x4);
				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr+12, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x0, 0x0, 0x80, 0x0, 0x3, 0x6);
				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr+16, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x0, 0x40, 0x80, 0x40, 0x4, 0x6);
				XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				break;
			case '3':
				//Display Bad
				TextData[16] = 0x00050102;
				XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);

				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x2ff, 0x28f, 0x4ff, 0x28f, 0x0, 0x8);
				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr+4, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x47f, 0x28f, 0x4ff, 0x28f, 0x1, 0x8);
				XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x2);
				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr+12, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x0, 0x0, 0x80, 0x0, 0x3, 0x6);
				//XOsd_CreateInstruction(&xosd_inst, InstSetPtr+16, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x0, 0x40, 0x80, 0x40, 0x4, 0x6);
				XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				break;
			case '4':
				//Display Right Arrow
				TextData[24] = 0x0000181a;
				TextData[32] = 0x00000000;
				XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);

				xil_printf("Writing to Color 1 Move Right to MotionDetector\r\n");
				MOTIONDEIP_mWriteReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG0_OFFSET, 0x00000001);
				xil_printf("Reading from MotionDetector\r\n");
				while((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1) == 0) {
					MB_Sleep(500);
					xil_printf("Reading Coordinates: %x\r\n", COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG2_OFFSET));
				}
				xil_printf("%d", (COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1f));
				if ((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x3) == 3) {
					//Display Good
					TextData[16] = 0x050b0b09;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x4);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
					//TODO Increment Score on GOOD
				} else {
					//Display Bad
					TextData[16] = 0x00050102;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x2);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				}
				break;
			case '5':
				//Display Left Arrow
				TextData[24] = 0x00001a19;
				TextData[32] = 0x00000000;
				XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);

				xil_printf("Writing to Color 1 Move Left to MotionDetector\r\n");
				MOTIONDEIP_mWriteReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG0_OFFSET, 0x00010001);
				xil_printf("Reading from MotionDetector\r\n");
				while((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1) == 0) {
					MB_Sleep(500);
					xil_printf("Reading Coordinates: %x\r\n", COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG2_OFFSET));
				}
				xil_printf("%d", (COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1f));
				if ((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x3) == 3) {
					//Display Good
					TextData[16] = 0x050b0b09;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x4);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				} else {
					//Display Bad
					TextData[16] = 0x00050102;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x2);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				}
				break;
			case '6':
				//Display Up Arrow
				TextData[24] = 0x0000001b;
				TextData[32] = 0x0000001d;
				XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);

				xil_printf("Writing to Color 1 Move Up to MotionDetector\r\n");
				MOTIONDEIP_mWriteReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG0_OFFSET, 0x00020001);
				xil_printf("Reading from MotionDetector\r\n");
				while((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1) == 0) {
					MB_Sleep(500);
					xil_printf("Reading Coordinates: %x\r\n", COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG2_OFFSET));
				}
				xil_printf("%d", (COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1f));
				if ((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x3) == 3) {
					//Display Good
					TextData[16] = 0x050b0b09;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x4);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				} else {
					//Display Bad
					TextData[16] = 0x00050102;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x2);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				}
				break;
			case '7':
				//Display Down Arrow
				TextData[24] = 0x0000001d;
				TextData[32] = 0x0000001c;
				XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);

				xil_printf("Writing to Color 1 Move Down to MotionDetector\r\n");
				MOTIONDEIP_mWriteReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG0_OFFSET, 0x00030001);
				xil_printf("Reading from MotionDetector\r\n");
				while((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1) == 0) {
					MB_Sleep(500);
					xil_printf("Reading Coordinates: %x\r\n", COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG2_OFFSET));
				}
				xil_printf("%d", (COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1f));
				if ((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x3) == 3) {
					//Display Good
					TextData[16] = 0x050b0b09;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x4);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				} else {
					//Display Bad
					TextData[16] = 0x00050102;
					XOsd_LoadTextBank(&xosd_inst, 1, 1, TextData);
					XOsd_CreateInstruction(&xosd_inst, InstSetPtr+8, 1, XOSD_INS_OPCODE_TXT, 0x80, 0x3ff, 0x0, 0x4ff, 0x0, 0x2, 0x2);
					XOsd_LoadInstructionList(&xosd_inst, 1, 1, InstSetPtr, 5);
				}
				break;
			case 't':
				//Test
				xil_printf("Writing to MotionDetector\r\n");
				MOTIONDEIP_mWriteReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG0_OFFSET, 0x32100001);
				xil_printf("Reading from MotionDetector\r\n");
				while((COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1) == 0) {
					MB_Sleep(500);
					xil_printf("Reading Coordinates: %x\r\n", COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG2_OFFSET));
				}
				xil_printf("%d", (COLORDETECT2_mReadReg(XPAR_MOTIONDEIP_0_S00_AXI_BASEADDR, MOTIONDEIP_S00_AXI_SLV_REG1_OFFSET) & 0x1f));
				break;
			case 'x':
				//prints x value
				xil_printf("Reading X value: %d\r\n", COLORDETECT2_mReadReg(XPAR_COLORDETECT2_0_S00_AXI_BASEADDR, COLORDETECT2_S00_AXI_SLV_REG1_OFFSET) >> 16);
				break;
			case 'y':
				//prints y value
				xil_printf("Reading Y value: %d\r\n", COLORDETECT2_mReadReg(XPAR_COLORDETECT2_0_S00_AXI_BASEADDR, COLORDETECT2_S00_AXI_SLV_REG1_OFFSET) & 0x3ff);
				break;
			case 'd':
				//prints data value
				xil_printf("Reading Data value: %x\r\n", COLORDETECT2_mReadReg(XPAR_COLORDETECT2_0_S00_AXI_BASEADDR, COLORDETECT2_S00_AXI_SLV_REG2_OFFSET));
				break;
			case 'q':
				break;
			case 'r':
				locked = XGpio_DiscreteRead(GpioPtr, 2);
				xil_printf("%d", locked);
				break;
			default :
				//xil_printf("\n\rInvalid Selection");
				MB_Sleep(50);
			}
		}

	return 0;
}


void DemoInitialize()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;
	int i;

	/*
	 * Initialize an array of pointers to the 3 frame buffers
	 */
	for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
	{
		pFrames[i] = frameBuf[i];
	}

	/*
	 * Initialize VDMA driver
	 */
	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
		return;
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Interrupt controller and start it.
	 */
	Status = fnInitInterruptController(&intc);
	if(Status != XST_SUCCESS) {
		xil_printf("Error initializing interrupts");
		return;
	}
	fnEnableInterrupts(&intc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));

	/*
	 * Initialize the Video Capture device
	 */
	Status = VideoInitialize(&videoCapt, &intc, &vdma, VID_GPIO_ID, VID_VTC_ID, VID_VTC_IRPT_ID, pFrames, DEMO_STRIDE, DEMO_START_ON_DET);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Video Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Set the Video Detect callback to trigger the menu to reset, displaying the new detected resolution
	 */
	VideoSetCallback(&videoCapt, DemoISR, &fRefresh);

	//DemoPrintTest(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, DEMO_PATTERN_1);

	return;
}

void DemoRun()
{
	int nextFrame = 0;
	char userInput = 0;
	u32 locked;
	XGpio *GpioPtr = &videoCapt.gpio;

	/* Flush UART FIFO */
	while (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
	{
		XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
	}
	while (userInput != 'q')
	{
		fRefresh = 0;
		DemoPrintMenu();

		/* Wait for data on UART */
		while (XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh)
		{}

		/* Store the first character in the UART receive FIFO and echo it */
		if (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
		{
			userInput = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
			xil_printf("%c", userInput);
		}
		else  //Refresh triggered by video detect interrupt
		{
			userInput = 'r';
		}

		switch (userInput)
		{
		case '1':
			DemoChangeRes();
			break;
		case '2':
			nextFrame = dispCtrl.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '3':
			DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_0);
			break;
		case '4':
			DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_1);
			break;
		case '5':
			if (videoCapt.state == VIDEO_STREAMING)
				VideoStop(&videoCapt);
			else
				VideoStart(&videoCapt);
			break;
		case '6':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			VideoChangeFrame(&videoCapt, nextFrame);
			break;
		case '7':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			VideoStop(&videoCapt);
			DemoInvertFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '8':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			VideoStop(&videoCapt);
			DemoScaleFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case 'q':
			break;
		case 'r':
			locked = XGpio_DiscreteRead(GpioPtr, 2);
			xil_printf("%d", locked);
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			MB_Sleep(50);
		}
	}

	return;
}

void DemoPrintMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal
	xil_printf("**************************************************\n\r");
	xil_printf("*                ZYBO Video Demo                 *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*Display Resolution: %28s*\n\r", dispCtrl.vMode.label);
	xil_printf("*Display Pixel Clock Freq. (MHz): %15.3f*\n\r", dispCtrl.pxlFreq);
	xil_printf("*Display Frame Index: %27d*\n\r", dispCtrl.curFrame);
	if (videoCapt.state == VIDEO_DISCONNECTED) xil_printf("*Video Capture Resolution: %22s*\n\r", "!HDMI UNPLUGGED!");
	else xil_printf("*Video Capture Resolution: %17dx%-4d*\n\r", videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo);
	xil_printf("*Video Frame Index: %29d*\n\r", videoCapt.curFrame);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - Change Display Resolution\n\r");
	xil_printf("2 - Change Display Framebuffer Index\n\r");
	xil_printf("3 - Print Blended Test Pattern to Display Framebuffer\n\r");
	xil_printf("4 - Print Color Bar Test Pattern to Display Framebuffer\n\r");
	xil_printf("5 - Start/Stop Video stream into Video Framebuffer\n\r");
	xil_printf("6 - Change Video Framebuffer Index\n\r");
	xil_printf("7 - Grab Video Frame and invert colors\n\r");
	xil_printf("8 - Grab Video Frame and scale to Display resolution\n\r");
	xil_printf("q - Quit\n\r");
	xil_printf("\n\r");
	xil_printf("\n\r");
	xil_printf("Enter a selection:");
}

void DemoChangeRes()
{
	int fResSet = 0;
	int status;
	char userInput = 0;

	/* Flush UART FIFO */
	while (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
		{
			XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
		}

	while (!fResSet)
	{
		DemoCRMenu();

		/* Wait for data on UART */
		while (XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh)
		{}

		/* Store the first character in the UART recieve FIFO and echo it */

		userInput = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
		xil_printf("%c", userInput);
		status = XST_SUCCESS;
		switch (userInput)
		{
		case '1':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_640x480);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '2':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_800x600);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '3':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1280x720);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '4':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1280x1024);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '5':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1920x1080);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case 'q':
			fResSet = 1;
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			MB_Sleep(50);
		}
		if (status == XST_DMA_ERROR)
		{
			xil_printf("\n\rWARNING: AXI VDMA Error detected and cleared\n\r");
		}
	}
}

void DemoCRMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal
	xil_printf("**************************************************\n\r");
	xil_printf("*                ZYBO Video Demo                 *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*Current Resolution: %28s*\n\r", dispCtrl.vMode.label);
	printf("*Pixel Clock Freq. (MHz): %23.3f*\n\r", dispCtrl.pxlFreq);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - %s\n\r", VMODE_640x480.label);
	xil_printf("2 - %s\n\r", VMODE_800x600.label);
	xil_printf("3 - %s\n\r", VMODE_1280x720.label);
	xil_printf("4 - %s\n\r", VMODE_1280x1024.label);
	xil_printf("5 - %s\n\r", VMODE_1920x1080.label);
	xil_printf("q - Quit (don't change resolution)\n\r");
	xil_printf("\n\r");
	xil_printf("Select a new resolution:");
}

void DemoInvertFrame(u8 *srcFrame, u8 *destFrame, u32 width, u32 height, u32 stride)
{
	u32 xcoi, ycoi;
	u32 lineStart = 0;
	for(ycoi = 0; ycoi < height; ycoi++)
	{
		for(xcoi = 0; xcoi < (width * 3); xcoi+=3)
		{
			destFrame[xcoi + lineStart] = ~srcFrame[xcoi + lineStart];         //Red
			destFrame[xcoi + lineStart + 1] = ~srcFrame[xcoi + lineStart + 1]; //Blue
			destFrame[xcoi + lineStart + 2] = ~srcFrame[xcoi + lineStart + 2]; //Green
		}
		lineStart += stride;
	}
	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);
}


/*
 * Bilinear interpolation algorithm. Assumes both frames have the same stride.
 */
void DemoScaleFrame(u8 *srcFrame, u8 *destFrame, u32 srcWidth, u32 srcHeight, u32 destWidth, u32 destHeight, u32 stride)
{
	float xInc, yInc; // Width/height of a destination frame pixel in the source frame coordinate system
	float xcoSrc, ycoSrc; // Location of the destination pixel being operated on in the source frame coordinate system
	float x1y1, x2y1, x1y2, x2y2; //Used to store the color data of the four nearest source pixels to the destination pixel
	int ix1y1, ix2y1, ix1y2, ix2y2; //indexes into the source frame for the four nearest source pixels to the destination pixel
	float xDist, yDist; //distances between destination pixel and x1y1 source pixels in source frame coordinate system

	int xcoDest, ycoDest; // Location of the destination pixel being operated on in the destination coordinate system
	int iy1; //Used to store the index of the first source pixel in the line with y1
	int iDest; //index of the pixel data in the destination frame being operated on

	int i;

	xInc = ((float) srcWidth - 1.0) / ((float) destWidth);
	yInc = ((float) srcHeight - 1.0) / ((float) destHeight);

	ycoSrc = 0.0;
	for (ycoDest = 0; ycoDest < destHeight; ycoDest++)
	{
		iy1 = ((int) ycoSrc) * stride;
		yDist = ycoSrc - ((float) ((int) ycoSrc));

		/*
		 * Save some cycles in the loop below by presetting the destination
		 * index to the first pixel in the current line
		 */
		iDest = ycoDest * stride;

		xcoSrc = 0.0;
		for (xcoDest = 0; xcoDest < destWidth; xcoDest++)
		{
			ix1y1 = iy1 + ((int) xcoSrc) * 3;
			ix2y1 = ix1y1 + 3;
			ix1y2 = ix1y1 + stride;
			ix2y2 = ix1y1 + stride + 3;

			xDist = xcoSrc - ((float) ((int) xcoSrc));

			/*
			 * For loop handles all three colors
			 */
			for (i = 0; i < 3; i++)
			{
				x1y1 = (float) srcFrame[ix1y1 + i];
				x2y1 = (float) srcFrame[ix2y1 + i];
				x1y2 = (float) srcFrame[ix1y2 + i];
				x2y2 = (float) srcFrame[ix2y2 + i];

				/*
				 * Bilinear interpolation function
				 */
				destFrame[iDest] = (u8) ((1.0-yDist)*((1.0-xDist)*x1y1+xDist*x2y1) + yDist*((1.0-xDist)*x1y2+xDist*x2y2));
				iDest++;
			}
			xcoSrc += xInc;
		}
		ycoSrc += yInc;
	}

	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);

	return;
}

void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int pattern)
{
	u32 xcoi, ycoi;
	u32 iPixelAddr;
	u8 wRed, wBlue, wGreen;
	u32 wCurrentInt;
	double fRed, fBlue, fGreen, fColor;
	u32 xLeft, xMid, xRight, xInt;
	u32 yMid, yInt;
	double xInc, yInc;


	switch (pattern)
	{
	case DEMO_PATTERN_0:

		xInt = width / 4; //Four intervals, each with width/4 pixels
		xLeft = xInt * 3;
		xMid = xInt * 2 * 3;
		xRight = xInt * 3 * 3;
		xInc = 256.0 / ((double) xInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		yInt = height / 2; //Two intervals, each with width/2 lines
		yMid = yInt;
		yInc = 256.0 / ((double) yInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		fBlue = 0.0;
		fRed = 256.0;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3)
		{
			/*
			 * Convert color intensities to integers < 256, and trim values >=256
			 */
			wRed = (fRed >= 256.0) ? 255 : ((u8) fRed);
			wBlue = (fBlue >= 256.0) ? 255 : ((u8) fBlue);
			iPixelAddr = xcoi;
			fGreen = 0.0;
			for(ycoi = 0; ycoi < height; ycoi++)
			{

				wGreen = (fGreen >= 256.0) ? 255 : ((u8) fGreen);
				frame[iPixelAddr] = wRed;
				frame[iPixelAddr + 1] = wBlue;
				frame[iPixelAddr + 2] = wGreen;
				if (ycoi < yMid)
				{
					fGreen += yInc;
				}
				else
				{
					fGreen -= yInc;
				}

				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			if (xcoi < xLeft)
			{
				fBlue = 0.0;
				fRed -= xInc;
			}
			else if (xcoi < xMid)
			{
				fBlue += xInc;
				fRed += xInc;
			}
			else if (xcoi < xRight)
			{
				fBlue -= xInc;
				fRed -= xInc;
			}
			else
			{
				fBlue += xInc;
				fRed = 0;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	case DEMO_PATTERN_1:

		xInt = width / 7; //Seven intervals, each with width/7 pixels
		xInc = 256.0 / ((double) xInt); //256 color intensities per interval. Notice that overflow is handled for this pattern.

		fColor = 0.0;
		wCurrentInt = 1;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3)
		{

			/*
			 * Just draw white in the last partial interval (when width is not divisible by 7)
			 */
			if (wCurrentInt > 7)
			{
				wRed = 255;
				wBlue = 255;
				wGreen = 255;
			}
			else
			{
				if (wCurrentInt & 0b001)
					wRed = (u8) fColor;
				else
					wRed = 0;

				if (wCurrentInt & 0b010)
					wBlue = (u8) fColor;
				else
					wBlue = 0;

				if (wCurrentInt & 0b100)
					wGreen = (u8) fColor;
				else
					wGreen = 0;
			}

			iPixelAddr = xcoi;

			for(ycoi = 0; ycoi < height; ycoi++)
			{
				frame[iPixelAddr] = wRed;
				frame[iPixelAddr + 1] = wBlue;
				frame[iPixelAddr + 2] = wGreen;
				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			fColor += xInc;
			if (fColor >= 256.0)
			{
				fColor = 0.0;
				wCurrentInt++;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	default :
		xil_printf("Error: invalid pattern passed to DemoPrintTest");
	}
}

void DemoISR(void *callBackRef, void *pVideo)
{
	char *data = (char *) callBackRef;
	*data = 1; //set fRefresh to 1
}


