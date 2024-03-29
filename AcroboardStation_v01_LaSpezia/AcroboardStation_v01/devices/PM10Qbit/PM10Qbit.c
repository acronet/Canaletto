/*
 * ACRONET Project
 * http://www.acronet.cc
 *
 * Copyright ( C ) 2014 Acrotec srl
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the EUPL v.1.1 license.  See http://ec.europa.eu/idabc/eupl.html for details.
 *
 * Notes: PM10Qbit provides measurements of PM10 concentrations every 4s. This driver provides
 *        an # of samples, average, max and min values.
 */

#include <asf.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "sysclk.h"

#include "conf_board.h"
#include "delay.h"

#include "drivers/usart_interrupt/buffer_usart.h"
#include "config/conf_usart_serial.h"

#include "devices/PM10Qbit/PM10Qbit.h"

#define PM10_BUFSIZE	32 // (60/4)=15 samples/min * 2 min = 30
#define BUFSIZE			8
#define MAXPM10VAL		20

uint32_t g_partialSum;
//uint16_t g_maxVal = 0, g_minVal = UINT16_MAX;
uint16_t g_measureCounter;
uint8_t g_valDistr[MAXPM10VAL] = {0};
char g_szBuf[BUFSIZE],g_PM10string[BUFSIZE];
volatile bool g_recordingData = false;

void PM10Qbit_init(void){
	//USART options.
	static usart_rs232_options_t RS232_SERIAL_OPTIONS = {
		.baudrate = USART_RS232_1_BAUDRATE,
		.charlength = USART_CHAR_LENGTH,
		.paritytype = USART_PARITY,
		.stopbits = USART_STOP_BIT
	};
	sysclk_enable_module(SYSCLK_PORT_C,PR_USART0_bm);
	usart_serial_init(USART_RS232_1, &RS232_SERIAL_OPTIONS);
	usart_set_rx_interrupt_level(USART_RS232_1,USART_INT_LVL_MED);	usart_rx_enable(USART_RS232_1);
	g_measureCounter = 0;
	g_partialSum=0;
	sei();
}


static void PM10Qbit_save_sample(void)
{
	uint16_t bufVal = 0;
	g_recordingData = true;
	bufVal += (uint16_t)(g_PM10string[3] - 48);
	bufVal += (uint16_t)(g_PM10string[2]-48) * 10 ;
	bufVal += (uint16_t)(g_PM10string[1]-48) * 100 ;
	bufVal += (uint16_t)(g_PM10string[0]-48) * 1000 ;
	
	g_partialSum += (uint32_t)bufVal;
	g_measureCounter++;
	//if(bufVal>g_maxVal)
		//g_maxVal = bufVal;
	//if(bufVal<g_minVal)
		//g_minVal = bufVal;
	if (bufVal<MAXPM10VAL-1)
		g_valDistr[bufVal]++;
	else
		g_valDistr[MAXPM10VAL-1]++;
	g_PM10string[0]=0;
	g_recordingData = false;
	
	//debug_string(NORMAL,g_PM10string,false);
	//char szBuf[256];
	//sprintf_P(szBuf,PSTR("\tsample %u = %u\r\n"),g_measureCounter,bufVal);
	//debug_string(NORMAL,szBuf,false);
	//sprintf_P(szBuf,PSTR("partialSum = %u\r\n"),g_partialSum);
	//debug_string(NORMAL,szBuf,false);
	//for (uint8_t i = 0; i<MAXPM10VAL; i++)
	//{
		//sprintf_P(szBuf,PSTR("V%u = %u\t"),i,g_valDistr[i]);
		//debug_string(NORMAL,szBuf,false);
	//}
	//debug_string(NORMAL,PSTR("\r\n"),true);
}

void PM10Qbit_get_value (PM10QBIT_STATS * const temp){
	while (g_recordingData);
	temp->mean = (float)(g_partialSum*100) / (float)(g_measureCounter-g_valDistr[0]);
	temp->samples=g_measureCounter;
	//temp->maxVal=g_maxVal;
	//temp->minVal=g_minVal;
	memcpy_ram2ram(temp->valDistr,&g_valDistr,sizeof(g_valDistr));
	//****************************//
	//g_maxVal = 0;
	//g_minVal = UINT16_MAX;
	g_measureCounter = 0;
	g_partialSum = 0;
	memset(g_valDistr, 0, sizeof(g_valDistr));
}

static bool usartc_USARTRS232_1_RX_CBuffer_Complete(void)
{
	static const char headerString[] = "m: ";
	USART_t * const ad = USART_RS232_1;
	static uint8_t idx=0;	const uint8_t dt = ad->DATA;
	/* Advance buffer head. */	if (idx<3)		if (dt == headerString[idx])			g_szBuf[idx++]=dt;		else			idx=0; 	else		g_szBuf[idx++]=dt;	if ( idx == 7)	{		idx=0;		g_PM10string[0]= g_szBuf[3];		g_PM10string[1]= g_szBuf[4];		g_PM10string[2]= g_szBuf[5];		g_PM10string[3]= g_szBuf[6];		g_PM10string[4]= 0;		PM10Qbit_save_sample();	}	return true;
}

ISR(USARTC0_RXC_vect)
{
	usartc_USARTRS232_1_RX_CBuffer_Complete();
}