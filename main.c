//***********************  main.c  ***********************
// Program written by:
// - Steven Prickett  steven.prickett@gmail.com
//
// Brief desicription of program:
// - Initializes an ESP8266 module to act as a WiFi client
//   and fetch weather data from openweathermap.org
//
//*********************************************************
/* Modified by Jonathan Valvano
 March 28, 2017
 Out of the box: to make this work you must
 Step 1) Set parameters of your AP in lines 59-60 of esp8266.c
 Step 2) Change line 39 with directions in lines 40-42
 Step 3) Run a terminal emulator like Putty or TExasDisplay at
         115200 bits/sec, 8 bit, 1 stop, no flow control
 Step 4) Set line 50 to match baud rate of your ESP8266 (9600 or 115200)
 Step 5) Some ESP8266 respond with "ok", others with "ready"
         esp8266.c ESP8266_Reset function, try different strings like "ready" and "ok"
 Step 6) Some ESP8266 respond with "ok", others with "no change"
         esp8266.c ESP8266_SetWifiMode function, try different strings like "no change" and "ok"
 Example
 AT+GMR version 0018000902 uses "ready" and "no change"
 AT+GMR version:0.60.0.0 uses "ready" and "ok"*/
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "pll.h"
#include "UART.h"
#include "UART2.h"
#include "os.h"
#include "REPLTerminal.h"
#include "Ultrasonic.h"
#include "ST7735Ext.h"
// #include "esp8266.h"
#include "LED.h"
// prototypes for functions defined in startup.s
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
void WaitForInterrupt(void);  // low power mode


bool GPIO_STATE[2] = {false,false};     //GPIO_STATE[0] = RedLed
									 // GPIO_STATE[1] = GreenLed

Sema4Type ESP_Terminal;
int CanMeasure = 1;
float distance;

float distanceTop;
int CanMeasureTop = 1;
int CanMeasureSide = 1;
float distanceSide;

void interpreter(void){
	while(1){
		WaitForInterrupt();
	}
}

void  ControlRedLed(int state){
	if (state == 1){
		LED_RedOn();
		GPIO_STATE[0] = true;
	 } else  {
		LED_RedOff();
		GPIO_STATE[0] = false;
	 }
}

void  ControlGreenLed(int state){
	if (state == 1){
		LED_GreenOn();
		GPIO_STATE[1] = true;
	}else {
		LED_GreenOff();
		GPIO_STATE[1] = false;
	}
}

void SendLEDStatus(){
	// char * buf[20];
    char buf[32];
	//GPIO status builder
    sprintf(buf,"-g { \"29\":%s, \"31\":%s}\r\n", 
		GPIO_STATE[0]?"true": "false",
		GPIO_STATE[1]? "true": "false");
	// {"29":false,"31":true}

	OS_bWait(&ESP_Terminal);
	ESP8266_OutString(buf);    //send status to ESP_8266;
	OS_bSignal(&ESP_Terminal);
}
void SendData(void){
    // ESP8266_OutString("Tosin ESP");
	int data = distance; //dummy data 100cm
	char buf[32];
	//GPIO status builder
    sprintf(buf,"-s { \"distance\": %d}\r\n",data); 
	OS_bWait(&ESP_Terminal);
	ESP8266_OutString(buf);    //send status to ESP_8266;
	OS_bSignal(&ESP_Terminal);
}

void UltrasonicThread(){
	while(1){
    if (CanMeasure){
    SendPulse(4*PULSE_1US);
	  CanMeasure = 0; 
 }
	}
}

void Display(){
  while(1){

		ST7735Ext_Message(FIRSTSCREEN, 1,0,"IP Address:",ST7735_Color565(0,255,0));

    char distanceBuffer[30];
    sprintf(distanceBuffer,"distance:  %3dcm", (int)distance);
    ST7735Ext_Message(SECONDSCREEN, 2,0,distanceBuffer,ST7735_Color565(0,255,0));
  }
}

int main(void){ 
  DisableInterrupts(); 
//   PLL_Init(Bus80MHz);   //initialized in OS already
  OS_Init();
  ESP8266_Init(115200);
  LED_Init();
//   UART_Init();
  REPLTerminal_Init();
  ST7735Ext_Init();
	Ultrasonic_Init();
  OS_InitSemaphore(&ESP_Terminal,1);
  OS_AddThread(&interpreter,STACKSIZE,3);
	OS_AddThread(&UltrasonicThread,STACKSIZE,3);
	OS_AddThread(&Display,STACKSIZE,3);
  OS_AddPeriodicThread(&SendData,500*TIME_2MS,2);
//   OS_Fifo_Init();
//   char character;
//   while(1){
//     ESP8266_OutString("Tosin ESP");
//     LED_BlueToggle();
//     delay(100);
//     character=ESP8266_InChar();
//     while(character){
// 	character=ESP8266_InChar();
//         UART_OutChar(character);
//     }
//   }
  OS_Launch(TIME_1MS);
}

// transparent mode for testing
// void ESP8266SendCommand(char *);
// int main2(void){  char data;
//   DisableInterrupts();
//   PLL_Init(Bus80MHz);
//   LED_Init();  
//   Output_Init();       // UART0 as a terminal
//   printf("\n\r-----------\n\rSystem starting at 9600 baud...\n\r");
// //  ESP8266_Init(38400);
//   ESP8266_InitUART(9600,true);
//   ESP8266_EnableRXInterrupt();
//   EnableInterrupts();
//   ESP8266SendCommand("AT+RST\r\n");
//   data = UART_InChar();
// //  ESP8266SendCommand("AT+UART=115200,8,1,0,3\r\n");
// //  data = UART_InChar();
// //  ESP8266_InitUART(115200,true);
// //  data = UART_InChar();
  
//   while(1){
// // echo data back and forth
//     data = UART_InCharNonBlock();
//     if(data){
//       ESP8266_PrintChar(data);
//     }
//   }
// }




