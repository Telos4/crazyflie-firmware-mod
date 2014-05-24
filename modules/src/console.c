/**
 *    ||          ____  _ __                           
 * +------+      / __ )(_) /_______________ _____  ___ 
 * | 0xBC |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * +------+    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *  ||  ||    /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Crazyflie control firmware
 *
 * Copyright (C) 2011-2012 Bitcraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * console.c - Used to send console data to client
 */

#include <stdbool.h>

/*FreeRtos includes*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "crtp.h"

#include "mpu6050.h"
#include "imu.h"
#include "system.h"
#include <stdio.h>

CRTPPacket messageToPrint;
xSemaphoreHandle synch = NULL;

static void telemetryTask(void* param);

static bool isInit;

/**
 * Send the data to the client
 */
static void consoleSendMessage(void)
{
  crtpSendPacketBlock(&messageToPrint);
  messageToPrint.size = 0;
}

void consoleInit()
{
  if (isInit)
    return;

  messageToPrint.size = 0;
  messageToPrint.header = CRTP_HEADER(CRTP_PORT_CONSOLE, 0);
  vSemaphoreCreateBinary(synch);

  xTaskCreate(telemetryTask, (const signed char * const)"TELEMETRY", configMINIMAL_STACK_SIZE, NULL, /*Priority*/1, NULL);
  
  isInit = true;
}

bool consoleTest(void)
{
  return isInit;
}

int16_t consolePutint16_t(int16_t value)
{
  if (xSemaphoreTake(synch, portMAX_DELAY) == pdTRUE)
  {
    messageToPrint.data[messageToPrint.size] = (int8_t) (value >> 8);
    messageToPrint.size++;
    messageToPrint.data[messageToPrint.size] = (int8_t) (value & 0xF);
    messageToPrint.size++;

    if (messageToPrint.size == CRTP_MAX_DATA_SIZE)
    {
      consoleSendMessage();
    }
    xSemaphoreGive(synch);
  }
  
  return value;
}

int consolePutchar(int ch)
{
  if (xSemaphoreTake(synch, portMAX_DELAY) == pdTRUE)
  {
    messageToPrint.data[messageToPrint.size] = (unsigned char)ch;
    messageToPrint.size++;
    if (ch == '\n' || messageToPrint.size == CRTP_MAX_DATA_SIZE)
    {
      consoleSendMessage();
    }
    xSemaphoreGive(synch);
  }
  
  return (unsigned char)ch;
}

int consolePuts(char *str)
{
  int ret = 0;
  
  while(*str)
    ret |= consolePutchar(*str++);
  
  return ret;
}

void consoleFlush(void)
{
  if (xSemaphoreTake(synch, portMAX_DELAY) == pdTRUE)
  {
    consoleSendMessage();
    xSemaphoreGive(synch);
  }
}

void i16tostr(int16_t value, char* str)
{
	int digits=1;
	int i;
	int dummy = 10;

	while(value/dummy !=0)
	{
		digits++;
		dummy *= 10;
	}
	
	if(value<0)
	{
		str[0] = '-';
		value = -value;
	}
	else
		str[0] = '+';

	dummy = 1;
	for(i=1; i<digits; i++)
		dummy *= 10;
	
	for(i=1; i<=digits; i++)
	{
		str[i] = (value%(dummy*10))/dummy + 48;
		dummy /= 10;
	}

	for(i=digits+1; i<7; i++)
		str[i] = 32;
}

static void telemetryTask(void* param)
{
	int16_t axi16, ayi16, azi16;
	int16_t gxi16, gyi16, gzi16;

	Axis3f gyroOut, accOut;

	char str[8];
	str[7] = 0;

	//Wait for the system to be fully started
	systemWaitStart();

	float fullScaleAccelGPL = mpu6050GetFullScaleAccelGPL();
	float fullScaleGyroDPL = mpu6050GetFullScaleGyroDPL();

	consolePuts("ax      | ay      | az      | gx      | gy      | gz\n");
	consolePuts("mm/s/s  | mm/s/s  | mm/s/s  | D/s     | D/s     | D/s\n");

	uint32_t lastWakeTime = xTaskGetTickCount();

	while(1)
	{
		//mit M2T kannst du die Zeit in ms angeben
		//alternativ kannst du anstatt M2T auch F2T benutzen, F2T bekommt als Argument die Frequenz
		vTaskDelayUntil(&lastWakeTime, M2T(5000)); //5000ms

		imu6Read(&gyroOut, &accOut);

		axi16 = 1000*accOut.x;
		ayi16 = 1000*accOut.y;
		azi16 = 1000*accOut.z;
		gxi16 = gyroOut.x;
		gyi16 = gyroOut.y;
		gzi16 = gyroOut.z;

		/*
		//Werte bezogen auf das Body-frame (hiess doch so, oder!?)
		mpu6050GetMotion6(&axi16, &ayi16, &azi16, &gxi16, &gyi16, &gzi16);

		axi16 = 1000*fullScaleAccelGPL*axi16;
		ayi16 = 1000*fullScaleAccelGPL*ayi16;
		azi16 = 1000*fullScaleAccelGPL*azi16;
		gxi16 = fullScaleGyroDPL*gxi16;
		gyi16 = fullScaleGyroDPL*gyi16;
		gzi16 = fullScaleGyroDPL*gzi16;
		*/

		i16tostr(axi16, str);
		consolePuts(str);
		consolePuts(" | ");
		i16tostr(ayi16, str);
		consolePuts(str);
		consolePuts(" | ");
		i16tostr(azi16, str);
		consolePuts(str);
		consolePuts(" | ");
		i16tostr(gxi16, str);
		consolePuts(str);
		consolePuts(" | ");
		i16tostr(gyi16, str);
		consolePuts(str);
		consolePuts(" | ");
		i16tostr(gzi16, str);
		consolePuts(str);
		consolePutchar('\n');
	}
}
