

#ifndef _ULTRASONIC_H_
#define  _ULTRASONIC_H_

#include "stdint.h"
#include  "../inc/tm4c123gh6pm.h"

#define PULSE_1US   40  
#define PULSE_4US   4*PULSE_1US   
#define PULSE_1MS   40000


void GPIO_PortBInit();

//Initialize ultrasonic sensor 
void Down_Ultrasonic_Init(uint16_t period, uint16_t duty);
void Top_Ultrasonic_Init(uint16_t period, uint16_t duty);
//start sending trigger signal
void SendPulse(uint16_t duty);
void SendPulseTop(uint16_t duty);


//stop the ultrasonic from sending trigger signal
void StopPulse(void);
void StopPulseTop(void);


// max period is (2^24-1)*12.5ns = 209.7151ms
// min period determined by time to run ISR, which is about 1us
//listen to echo signal with this timer
void PeriodMeasure_Init(void);
void PeriodMeasure_InitTop(void);

void Ultrasonic_Init();

static void delay(uint32_t N);



#endif