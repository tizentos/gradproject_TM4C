
#include "stdint.h"
#include <string.h> 
#include "REPLTerminal.h"
#include "UART.h"
#include "UART2.h"
#include  "ST7735Ext.h"
#include "os.h"
// #include "../Lab4.c"
// #include <stdio.h>

#define MAX_NUM_CHAR 20

//global variables
char UART0_inputString[20]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
char ESP_inputString[20]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int length = 0;
char CLEARSTRING[20]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};


//internal private function declaration
void OutCRLF(void);
void assembleChar(char, int); 
void getSubstring(char * ,char * ,int ,int );

//threads to call
extern void ControlRedLed(int);
extern void ControlGreenLed(int);
extern void SendLEDStatus();

//initializes UART0
void REPLTerminal_Init(void){
    UART_Init();
    OutCRLF();
    // LED_Init();
    REPLEvaluate("-d Welcome");
}


//Helper function to align terminal cursor correctly
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}

void uart0_assembleChar(char character){
        if(character != CR){
        if(character == BS){
            if(length){
                UART0_inputString[length]=0;
                length--;
                UART_OutChar('BS');
            }
        }
        else if(length < MAX_NUM_CHAR){
            UART0_inputString[length] = character;
            length++;
            UART_OutChar(character);
        }
    }else{
        OutCRLF();
        length=0;
        REPLEvaluate(UART0_inputString);
    }
}

void esp_assembleChar(char character){
    if(character != CR){
        if (length < MAX_NUM_CHAR){
            ESP_inputString[length] = character;
            length++;
            // UART_OutChar(character);
        }
    }else{
        OutCRLF();
        length=0;
        REPLEvaluate(ESP_inputString);
    }
}
void assembleChar(char character, int terminal) {

    //switch case to know which terminal
    switch (terminal)
    {
        case ESP_TERMINAL:
            esp_assembleChar(character);
            break;
        case UART0_TERMINAL:
            //code
            uart0_assembleChar(character);
            break;
        default:
            break;
    }
}

void getSubstring(char *dest,char *src,int length,int position){
    int c=0;
    while (c < length) {
        dest[c] = src[position+c-1];
        c++;
    }
    dest[c] = '\0';
}


//Main interpreter function
void REPLEvaluate(char *inputString){
    char  command[3];
    getSubstring(command,inputString,3,1);
    char output[20];   //check the size of this buffer
    getSubstring(output,inputString,16,4);

    //clear string
    //strcpy(inputString,CLEARSTRING);
	for (int i=0; i<20;i++){
		inputString[i] = 0x00;
	}

    if (strcmp(DEBUG_MESSAGE,command) == 0){
        //Display directory
        // eFile_Directory(&REPLPrint);
        REPLPrint(output);
    }
    if (strcmp(FIRST_LED,command) == 0){
        //turn on/off LED
			  char  c = output[0];
        if (isdigit(c)){
					int state = c - '0';    //we only need first element of output
					ControlRedLed(state);
				}
    }
    if (strcmp(SECOND_LED,command) == 0){
        //control green LED 
			  char  c = output[0];
        if (isdigit(c)){
					int state = c - '0';    //we only need first element of output
					ControlGreenLed(state);
				}
    }
    if (strcmp(SEND_DATA,command) == 0){
        //might not implement this command
        SendLEDStatus();

    }
    if (strcmp(RESET_ESP,command) == 0){
        //set RST to high
    }
    if (strcmp(CLEARSCREEN,command)==0){
        //print to terminal
        ST7735Ext_Clear();
    }
}


// prints to the terminal
void REPLPrint(char *output){
    UART_OutString(output);
    OutCRLF();
}


//REPL Interrupt service routine
void REPL_Handler(){
    char character;
    UART0_Handler();
    while ( character=UART_InCharNonBlock()){
        //REPLEvaluate(inputString);
        assembleChar(character, UART0_TERMINAL);
    }
}

void ESP_Handler(){
    char character;
    UART2_Handler();
    while ( character=ESP8266_InChar()){
        //REPLEvaluate(inputString);
        assembleChar(character,ESP_TERMINAL);
    }
}

// // this is used for printf to output to the usb uart
// int fputc(int ch, FILE *f){
//   UART_OutChar(ch);
//   return 1;
// }