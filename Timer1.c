// Timer1.c
// Runs on LM4F120/TM4C123
// Use TIMER1 in 32-bit periodic mode to request interrupts at a periodic rate
// Daniel Valvano
// May 5, 2015

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015
  Program 7.5, example 7.6

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
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "os.h"
#include "Timer1.h"



void (*PeriodicTaskTimer0)(void);   // user function
// ***************** Timer0A_Init ****************
// Activate TIMER0 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq), 32 bits
// Outputs: none
void Timer0A_Init(void(*task)(void), uint32_t period){
  long sr;
  sr = StartCritical(); 
  SYSCTL_RCGCTIMER_R |= 0x01;   // 0) activate TIMER0
  PeriodicTaskTimer0 = task;          // user function
  TIMER0_CTL_R = 0x00000000;    // 1) disable TIMER0A during setup
  TIMER0_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER0_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER0_TAILR_R = period-1;    // 4) reload value
  TIMER0_TAPR_R = 0;            // 5) bus clock resolution
  TIMER0_ICR_R = 0x00000001;    // 6) clear TIMER0A timeout flag
  TIMER0_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0x80000000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 35, interrupt number 19
  NVIC_EN0_R = 1<<19;           // 9) enable IRQ 19 in NVIC
  TIMER0_CTL_R = 0x00000001;    // 10) enable TIMER0A
  EndCritical(sr);
}

void Timer00A_Handler(void){
  TIMER0_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer0A timeout
  (*PeriodicTaskTimer0)();                // execute user task
}




void (*PeriodicTaskTimer1)(void);   // user function

// ***************** TIMER1_Init ****************
// Activate TIMER1 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer1_Init(void(*task)(void), uint32_t period, uint32_t priority){
  SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate TIMER1
  PeriodicTaskTimer1 = task;          // user function
  TIMER1_CTL_R = 0x00000000;    // 1) disable TIMER1A during setup
  TIMER1_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER1_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER1_TAILR_R = period-1;    // 4) reload value
  TIMER1_TAPR_R = 0;            // 5) bus clock resolution
  TIMER1_ICR_R = 0x00000001;    // 6) clear TIMER1A timeout flag
  TIMER1_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)|(priority << 13); // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 37, interrupt number 21
  NVIC_EN0_R = 1<<21;           // 9) enable IRQ 21 in NVIC
  TIMER1_CTL_R = 0x00000001;    // 10) enable TIMER1A
}

void Timer1A_Handler(void){
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER1A timeout
  (*PeriodicTaskTimer1)();                // execute user task
}


void (*PeriodicSleepTimerTask)(void);   // user function

void SleepTimer3_Init(void(*task)(void), unsigned long period){
  SYSCTL_RCGCTIMER_R |= 0x08;   // 0) activate TIMER3
  PeriodicSleepTimerTask = task;          // user function
  TIMER3_CTL_R = 0x00000000;    // 1) disable TIMER3A during setup
  TIMER3_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER3_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER3_TAILR_R = period-1;    // 4) reload value
  TIMER3_TAPR_R = 0;            // 5) bus clock resolution
  TIMER3_ICR_R = 0x00000001;    // 6) clear TIMER3A timeout flag
  TIMER3_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI8_R = (NVIC_PRI8_R&0x00FFFFFF)|0x80000000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 51, interrupt number 35
  NVIC_EN1_R = 1<<(35-32);      // 9) enable IRQ 35 in NVIC
  TIMER3_CTL_R = 0x00000001;    // 10) enable TIMER3A
}

void Timer3A_Handler(void){
  TIMER3_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER3A timeout
  (*PeriodicSleepTimerTask)();                // execute user task
}

// void PeriodicSleep_Timer_Init(void(*task)(void), uint32_t freq, uint8_t priority){
//   long sr;
//   if((freq == 0) || (freq > 10000)){
//     return;                        // invalid input
//   }
//   if(priority > 6){
//     priority = 6;
//   }
//   sr = StartCritical();
//   PeriodicSleepTimerTask = task;             // user function
//   // ***************** Wide Timer4A initialization *****************
//   SYSCTL_RCGCWTIMER_R |= 0x10;     // activate clock for Wide Timer4
//   while((SYSCTL_PRWTIMER_R&0x10) == 0){};// allow time for clock to stabilize
//   WTIMER4_CTL_R &= ~TIMER_CTL_TAEN;// disable Wide Timer4A during setup
//   WTIMER4_CFG_R = TIMER_CFG_16_BIT;// configure for 32-bit timer mode
//                                    // configure for periodic mode, default down-count settings
//   WTIMER4_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
//   WTIMER4_TAILR_R = (TIME_1MS/freq - 1); // reload value
//   WTIMER4_TAPR_R = 0;              // bus clock resolution
//   WTIMER4_ICR_R = TIMER_ICR_TATOCINT;// clear WTIMER4A timeout flag
//   WTIMER4_IMR_R |= TIMER_IMR_TATOIM;// arm timeout interrupt
// //PRIn Bit   Interrupt
// //Bits 31:29 Interrupt [4n+3]
// //Bits 23:21 Interrupt [4n+2], n=25 => (4n+2)=102
// //Bits 15:13 Interrupt [4n+1]
// //Bits 7:5 Interrupt [4n]
//   NVIC_PRI25_R = (NVIC_PRI25_R&0xFF00FFFF)|(priority<<21); // priority
// // interrupts enabled in the main program after all devices initialized
// // vector number 118, interrupt number 102
// // 32 bits in each NVIC_ENx_R register, 102/32 = 3 remainder 6
//   NVIC_EN3_R = 1<<6;               // enable IRQ 102 in NVIC
//   WTIMER4_CTL_R |= TIMER_CTL_TAEN; // enable Wide Timer4A 32-b
//   EndCritical(sr);
// }

// void WideTimer4A_Handler(void){
//   WTIMER4_ICR_R = TIMER_ICR_TATOCINT;// acknowledge Wide Timer4A timeout
//   (*PeriodicSleepTimerTask)();               // execute user task
// }
