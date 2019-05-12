// UART.c
// Runs on LM4F120/TM4C123
// Use UART2 to implement bidirectional data transfer to and from a
// computer running HyperTerminal.  This time, interrupts and FIFOs
// are used.
// Daniel Valvano
// September 11, 2013
// Modified by EE345L students Charlie Gough && Matt Hawk
// Modified by EE345M students Agustinus Darmawan && Mingjie Qiu

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015
   Program 5.11 Section 5.6, Program 3.10

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"


#include "FIFO.h"
#include "UART2.h"

#define NVIC_EN0_INT5           0x00000020  // Interrupt 5 enable

#define UART_FR_RXFF            0x00000040  // UART Receive FIFO Full
#define UART_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART_CTL_UARTEN         0x00000001  // UART Enable
#define UART_IFLS_RX1_8         0x00000000  // RX FIFO >= 1/8 full
#define UART_IFLS_TX1_8         0x00000000  // TX FIFO <= 1/8 full
#define UART_IM_RTIM            0x00000040  // UART Receive Time-Out Interrupt
                                            // Mask
#define UART_IM_TXIM            0x00000020  // UART Transmit Interrupt Mask
#define UART_IM_RXIM            0x00000010  // UART Receive Interrupt Mask
#define UART_RIS_RTRIS          0x00000040  // UART Receive Time-Out Raw
                                            // Interrupt Status
#define UART_RIS_TXRIS          0x00000020  // UART Transmit Raw Interrupt
                                            // Status
#define UART_RIS_RXRIS          0x00000010  // UART Receive Raw Interrupt
                                            // Status
#define UART_ICR_RTIC           0x00000040  // Receive Time-Out Interrupt Clear
#define UART_ICR_TXIC           0x00000020  // Transmit Interrupt Clear
#define UART_ICR_RXIC           0x00000010  // Receive Interrupt Clear



void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
#define FIFOSIZE   512         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure
                              // create index implementation FIFO (see FIFO.h)
AddIndexFifo(Rx2, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)
AddIndexFifo(Tx2, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)

// Initialize UART2
// Baud rate is 115200 bits/sec
void ESP8266_Init(uint32_t baud){
  // ESP8266_EchoResponse = echo;
  SYSCTL_RCGCUART_R |= 0x04; // Enable UART2
  while((SYSCTL_PRUART_R&0x04)==0){};
  SYSCTL_RCGCGPIO_R |= 0x0A; // Enable PORT B,D clock gating
  while((SYSCTL_PRGPIO_R&0x08)==0){}; 
  Rx2Fifo_Init();                        // initialize empty FIFOs
  Tx2Fifo_Init();
  GPIO_PORTD_LOCK_R = 0x4C4F434B;   // 2) unlock GPIO Port D
  GPIO_PORTD_CR_R = 0xFF;           // allow changes to PD7
  GPIO_PORTD_AFSEL_R |= 0xC0; // PD7,PD6 UART2
  GPIO_PORTD_PCTL_R =(GPIO_PORTD_PCTL_R&0x00FFFFFF)|0x11000000;
  GPIO_PORTD_DEN_R   |= 0xC0; //PD6,PD7 UART2 
  UART2_CTL_R &= ~UART_CTL_UARTEN;                  // Clear UART2 enable bit during config
  UART2_IBRD_R = (80000000/16)/baud;   
  UART2_FBRD_R = ((64*((80000000/16)%baud))+baud/2)/baud;      
//  UART2_IBRD_R = 43;   // IBRD = int(80,000,000 / (16 * 115,200)) = int(43.403)
// UART2_FBRD_R = 26;   // FBRD = round(0.4028 * 64 ) = 26
  UART2_LCRH_R = (UART_LCRH_WLEN_8|UART_LCRH_FEN);  // 8 bit word length, 1 stop, no parity, FIFOs enabled
  UART2_IFLS_R &= ~0x3F;                            // Clear TX and RX interrupt FIFO level fields
  UART2_IFLS_R += UART_IFLS_RX1_8 ;                 // RX FIFO interrupt threshold >= 1/8th full
  UART2_IM_R  |= UART_IM_RXIM | UART_IM_RTIM;       // Enable interupt on RX and RX transmission end
  UART2_CTL_R |= UART_CTL_UARTEN;                   // Set UART2 enable bit    
  GPIO_PORTB_DIR_R |= 0x02; // PB1 output to reset
  GPIO_PORTB_PCTL_R =GPIO_PORTB_PCTL_R&0xFFFFFF0F;
  GPIO_PORTB_DEN_R   |= 0x02; //PB1 output 
  GPIO_PORTB_DATA_R |= 0x02;  // reset high

  NVIC_EN1_R = 1<<1; // interrupt 33
  EnableInterrupts();
}
// copy from hardware RX FIFO to software RX FIFO
// stop when hardware RX FIFO is empty or software RX FIFO is full
void static copyHardwareToSoftware(void){
  char letter;
  while(((UART2_FR_R&UART_FR_RXFE) == 0) && (Rx2Fifo_Size() < (FIFOSIZE - 1))){
    letter = UART2_DR_R;
    Rx2Fifo_Put(letter);
  }
}
// copy from software TX FIFO to hardware TX FIFO
// stop when software TX FIFO is empty or hardware TX FIFO is full
void static copySoftwareToHardware(void){
  char letter;
  while(((UART2_FR_R&UART_FR_TXFF) == 0) && (Tx2Fifo_Size() > 0)){
    Tx2Fifo_Get(&letter);
    UART2_DR_R = letter;
  }
}
// input ASCII character from UART
// spin if RxFifo is empty
char ESP8266_InChar(void){
  char letter;
  if(Rx2Fifo_Get(&letter) == FIFOFAIL){
    return 0;  // empty
  };
  return(letter);
}
// output ASCII character to UART
// spin if TxFifo is full
void ESP8266_OutChar(char data){
  while(Tx2Fifo_Put(data) == FIFOFAIL){};
  UART2_IM_R &= ~UART_IM_TXIM;          // disable TX FIFO interrupt
  copySoftwareToHardware();
  UART2_IM_R |= UART_IM_TXIM;           // enable TX FIFO interrupt
}
// at least one of three things has happened:
// hardware TX FIFO goes from 3 to 2 or less items
// hardware RX FIFO goes from 1 to 2 or more items
// UART receiver has timed out
void UART2_Handler(void){
  if(UART2_RIS_R&UART_RIS_TXRIS){       // hardware TX FIFO <= 2 items
    UART2_ICR_R = UART_ICR_TXIC;        // acknowledge TX FIFO
    // copy from software TX FIFO to hardware TX FIFO
    copySoftwareToHardware();
    if(Tx2Fifo_Size() == 0){             // software TX FIFO is empty
      UART2_IM_R &= ~UART_IM_TXIM;      // disable TX FIFO interrupt
    }
  }
  if(UART2_RIS_R&UART_RIS_RXRIS){       // hardware RX FIFO >= 2 items
    UART2_ICR_R = UART_ICR_RXIC;        // acknowledge RX FIFO
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
  if(UART2_RIS_R&UART_RIS_RTRIS){       // receiver timed out
    UART2_ICR_R = UART_ICR_RTIC;        // acknowledge receiver time out
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
}

//------------UART_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void ESP8266_OutString(char *pt){
  while(*pt){
    ESP8266_OutChar(*pt);
    pt++;
  }
}
//------------UART_InUDec------------
// InUDec accepts ASCII input in unsigned decimal format
//     and converts to a 32-bit unsigned number
//     valid range is 0 to 4294967295 (2^32-1)
// Input: none
// Output: 32-bit unsigned number
// If you enter a number above 4294967295, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t ESP8266_InUDec(void){
uint32_t number=0, length=0;
char character;
  character = ESP8266_InChar();
  while(character != CR){ // accepts until <enter> is typed
// The next line checks that the input is a digit, 0-9.
// If the character is not 0-9, it is ignored and not echoed
    if((character>='0') && (character<='9')) {
      number = 10*number+(character-'0');   // this line overflows if above 4294967295
      length++;
      ESP8266_OutChar(character);
    }
// If the input is a backspace, then the return number is
// changed and a backspace is outputted to the screen
    else if((character==BS) && length){
      number /= 10;
      length--;
      ESP8266_OutChar(character);
    }
    character = ESP8266_InChar();
  }
  return number;
}

//-----------------------UART_OutUDec-----------------------
// Output a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1-10 digits with no space before or after
void ESP8266_OutUDec(uint32_t n){
// This function uses recursion to convert decimal number
//   of unspecified length as an ASCII string
  if(n >= 10){
    ESP8266_OutUDec(n/10);
    n = n%10;
  }
  ESP8266_OutChar(n+'0'); /* n is between 0 and 9 */
}

//---------------------UART_InUHex----------------------------------------
// Accepts ASCII input in unsigned hexadecimal (base 16) format
// Input: none
// Output: 32-bit unsigned number
// No '$' or '0x' need be entered, just the 1 to 8 hex digits
// It will convert lower case a-f to uppercase A-F
//     and converts to a 16 bit unsigned number
//     value range is 0 to FFFFFFFF
// If you enter a number above FFFFFFFF, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t ESP8266_InUHex(void){
uint32_t number=0, digit, length=0;
char character;
  character = ESP8266_InChar();
  while(character != CR){
    digit = 0x10; // assume bad
    if((character>='0') && (character<='9')){
      digit = character-'0';
    }
    else if((character>='A') && (character<='F')){
      digit = (character-'A')+0xA;
    }
    else if((character>='a') && (character<='f')){
      digit = (character-'a')+0xA;
    }
// If the character is not 0-9 or A-F, it is ignored and not echoed
    if(digit <= 0xF){
      number = number*0x10+digit;
      length++;
      ESP8266_OutChar(character);
    }
// Backspace outputted and return value changed if a backspace is inputted
    else if((character==BS) && length){
      number /= 0x10;
      length--;
      ESP8266_OutChar(character);
    }
    character = ESP8266_InChar();
  }
  return number;
}

//--------------------------UART_OutUHex----------------------------
// Output a 32-bit number in unsigned hexadecimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1 to 8 digits with no space before or after
void ESP8266_OutUHex(uint32_t number){
// This function uses recursion to convert the number of
//   unspecified length as an ASCII string
  if(number >= 0x10){
    ESP8266_OutUHex(number/0x10);
    ESP8266_OutUHex(number%0x10);
  }
  else{
    if(number < 0xA){
      ESP8266_OutChar(number+'0');
     }
    else{
      ESP8266_OutChar((number-0x0A)+'A');
    }
  }
}

//------------UART_InString------------
// Accepts ASCII characters from the serial port
//    and adds them to a string until <enter> is typed
//    or until max length of the string is reached.
// It echoes each character as it is inputted.
// If a backspace is inputted, the string is modified
//    and the backspace is echoed
// terminates the string with a null character
// uses busy-waiting synchronization on RDRF
// Input: pointer to empty buffer, size of buffer
// Output: Null terminated string
// -- Modified by Agustinus Darmawan + Mingjie Qiu --
void ESP8266_InString(char *bufPt, uint16_t max) {
int length=0;
char character;
  character = ESP8266_InChar();
  while(character != CR){
    if(character == BS){
      if(length){
        bufPt--;
        length--;
        ESP8266_OutChar(BS);
      }
    }
    else if(length < max){
      *bufPt = character;
      bufPt++;
      length++;
      ESP8266_OutChar(character);
    }
    character = ESP8266_InChar();
  }
  *bufPt = 0;
}
