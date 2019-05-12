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
#include "ST7735Ext.h"
// #include "esp8266.h"
#include "LED.h"
// prototypes for functions defined in startup.s
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
void WaitForInterrupt(void);  // low power mode
// char Fetch[] = "POST /api/v1/jN80oNCqadNp1tvMQJcf/attributes HTTP/1.1\r\nHost:demo.thingsboard.io\r\nContent-Type: application/json\r\nCache-Control: no-cache\r\n\r\n{distance : 5}\r\n\r\n";
char Fetch[] = "GET /api/data HTTP/1.1\r\nHost: bio-remote.herokuapp.com\r\nAccept: application/json\r\nKeep-Alive: timeout=10, max=1000\r\n\r\n";
// 1) go to http://openweathermap.org/appid#use 
// 2) Register on the Sign up page
// 3) get an API key (APPID) replace the 1234567890abcdef1234567890abcdef with your APPID


// char* server = "demo.thingsboard.io";
char* server = "bio-remote.herokuapp.com";

// int main1(void){  
//   DisableInterrupts();
//   PLL_Init(Bus80MHz);
//   LED_Init();  
//   Output_Init();       // UART0 only used for debugging
//   printf("\n\r-----------\n\rSystem starting...\n\r");
//   ESP8266_Init(115200);  // connect to access point, set up as client
//   ESP8266_GetVersionNumber();
//   while(1){
//     ESP8266_GetStatus();
//     if(ESP8266_MakeTCPConnection(server)){ // open socket in server
//       LED_GreenOn();
//       ESP8266_SendTCP(Fetch);
//     }
//     ESP8266_CloseTCPConnection();
//     while(Board_Input()==0){// wait for touch
//     }; 
//     LED_GreenOff();
//     LED_RedToggle();
//   }
// }

bool GPIO_STATE[2] = {false,false};     //GPIO_STATE[0] = RedLed
									 // GPIO_STATE[1] = GreenLed


Sema4Type ESP_Terminal;
static void delay(uint32_t N) {
    for(int n = 0; n < N; n++)                         // N time unitss
        for(int msec = 10000; msec > 0; msec--);       // 1 time unit
}

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
	int data = 170; //dummy data 100cm
	char buf[32];
	//GPIO status builder
    sprintf(buf,"-s { \"distance\": %d}\r\n",data); 
	OS_bWait(&ESP_Terminal);
	ESP8266_OutString(buf);    //send status to ESP_8266;
	OS_bSignal(&ESP_Terminal);
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
  OS_InitSemaphore(&ESP_Terminal,1);
  OS_AddThread(&interpreter,STACKSIZE,3);
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




