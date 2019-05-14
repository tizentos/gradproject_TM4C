#include "Ultrasonic.h"
#include "../inc/tm4c123gh6pm.h"
#define NVIC_EN0_INT19          0x00080000  // Interrupt 19 enable
#define NVIC_EN0_INT22          0x00400000  // Interrupt 19 enable
#define TIMER_TAILR_TAILRL_M    0x0000FFFF  // GPTM TimerA Interval Load
                                            // Register Low
#define PF2                     (*((volatile uint32_t *)0x40025010))
#define PB3  (*((volatile unsigned long *)0x40005020)) //PB3 
#define MAX_DATA_POINT      10 

uint32_t Period;              // (1/clock) units
uint32_t First;               // Timer0A first edge
extern int CanMeasure;
int captureCount = 0;
extern float distance;


uint32_t FirstTop;               // Timer1A first edge
uint32_t PeriodTop;              // (1/clock) units
int captureCountTop;
extern float distanceTop;
extern int CanMeasureTop;


int countSide;
int periodDataSide[5];
uint32_t FirstSide;               // Timer1A first edge
uint32_t PeriodSide;              // (1/clock) units
int captureCountSide;
extern float distanceSide;
extern int CanMeasureSide;


// Initialize GPIOB for Ultrasonic Sensor
// void GPIO_PortBInit(){
//     SYSCTL_RCGCGPIO_R |= 0x02;       // activate port B and port F
//     GPIO_PORTB_DIR_R &= ~(0x40|0x10);       // make PB6  & PB4 in
//     GPIO_PORTB_AFSEL_R |= (0x40|0x10);      // enable alt funct on PB6/T0CCP0
//     GPIO_PORTB_DEN_R |= (0x40|0x10);        // enable digital I/O on PB6
//                                    // configure PB6 as T0CCP0
//     GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xF0FFFFFF)+0x07000000;
//                                        // configure PB6 as T1CCP0
//     GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFF0FFFF)+0x00070000;
//     GPIO_PORTB_AMSEL_R &= ~(0x40|0x10);     // disable analog functionality on PB6
// }

void GPIO_PortBInit(){
    SYSCTL_RCGCGPIO_R |= 0x02;       // activate port B a
    GPIO_PORTB_DIR_R &= ~(0x40|0x10|0x04);       // make PB6,PB4 and PB2  in
    GPIO_PORTB_DIR_R |= 0x08;    //make PB3 output
    GPIO_PORTB_AFSEL_R |= (0x40|0x10|0x04);      // enable alt funct on PB6/T0CCP0
    GPIO_PORTB_DEN_R |= (0x40|0x10|0x08|0x04);        // enable digital I/O on PB2,3,4,6
                                   // configure PB6 as T0CCP0
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xF0FFFFFF)+0x07000000;
                                       // configure PB4 as T1CCP0
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFF0FFFF)+0x00070000;
                                        // configure PB2 as T3CCP0
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFFF0FF)+0x00000700;
    GPIO_PORTB_AMSEL_R &= ~(0x40|0x10|0x08|0x04);     // disable analog functionality on PB6
}

void GPIOC_Init(){
  SYSCTL_RCGCGPIO_R |= 0x04;       // activate port C
  GPIO_PORTC_DIR_R &= ~(0x40|0x10|0x04);       // make PB6,PB4 and PB2  in
  GPIO_PORTB_DIR_R |= 0x08;    //make PB3 output
  GPIO_PORTB_AFSEL_R |= (0x40|0x10|0x04);      // enable alt funct on PB6/T0CCP0
  GPIO_PORTB_DEN_R |= (0x40|0x10|0x08|0x04);        // enable digital I/O on PB2,3,4,6
                                  // configure PB6 as T0CCP0
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xF0FFFFFF)+0x07000000;
                                      // configure PB4 as T1CCP0
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFF0FFFF)+0x00070000;
                                      // configure PB2 as T3CCP0
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFFF0FF)+0x00000700;
  GPIO_PORTB_AMSEL_R &= ~(0x40|0x10|0x08|0x04);     // disable analog functionality on PB6
}
// max period is (2^24-1)*12.5ns = 209.7151ms
// min period determined by time to run ISR, which is about 1us
void PeriodMeasure_Init(void){
  SYSCTL_RCGCTIMER_R |= 0x01;// activate timer0    
  TIMER0_CTL_R &= ~TIMER_CTL_TAEN; // disable timer0A during setup
  TIMER0_CFG_R = TIMER_CFG_16_BIT; // configure for 16-bit timer mode
                                   // configure for 24-bit capture mode
  TIMER0_TAMR_R = (TIMER_TAMR_TACMR|TIMER_TAMR_TAMR_CAP);
                                   // configure for both edge event
  TIMER0_CTL_R |= TIMER_CTL_TAEVENT_BOTH;
  TIMER0_TAILR_R = TIMER_TAILR_TAILRL_M;// start value
  TIMER0_TAPR_R = 0xFF;            // activate prescale, creating 24-bit
  TIMER0_IMR_R |= TIMER_IMR_CAEIM; // enable capture match interrupt
  TIMER0_ICR_R = TIMER_ICR_CAECINT;// clear timer0A capture match flag
  TIMER0_CTL_R |= TIMER_CTL_TAEN;  // enable timer0A 16-b, +edge timing, interrupts
                                   // Timer0A=priority 2
  NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0x40000000; // top 3 bits
  NVIC_EN0_R = NVIC_EN0_INT19;     // enable interrupt 19 in NVIC
}

// max period is (2^24-1)*12.5ns = 209.7151ms
// min period determined by time to run ISR, which is about 1us
void PeriodMeasure_InitTop(void){
  SYSCTL_RCGCTIMER_R |= 0x02;    // activate timer0    
  TIMER1_CTL_R &= ~TIMER_CTL_TAEN; // disable timer0A during setup
  TIMER1_CFG_R = TIMER_CFG_16_BIT; // configure for 16-bit timer mode
                                   // configure for 24-bit capture mode
  TIMER1_TAMR_R = (TIMER_TAMR_TACMR|TIMER_TAMR_TAMR_CAP);
                                   // configure for both edge event
  TIMER1_CTL_R |= TIMER_CTL_TAEVENT_BOTH;
  TIMER1_TAILR_R = TIMER_TAILR_TAILRL_M;// start value
  TIMER1_TAPR_R = 0xFF;            // activate prescale, creating 24-bit
  TIMER1_IMR_R |= TIMER_IMR_CAEIM; // enable capture match interrupt
  TIMER1_ICR_R = TIMER_ICR_CAECINT;// clear timer0A capture match flag
  TIMER1_CTL_R |= TIMER_CTL_TAEN;  // enable timer0A 16-b, +edge timing, interrupts
                                   // Timer1B=priority 3

  NVIC_EN0_R |= 1<<21;           // 9) enable IRQ 21 in NVIC
  TIMER1_CTL_R |= 0x00000001;    // 10) enable TIMER1A
  // NVIC_PRI5_R = (NVIC_PRI5_R&0xFF00FFFF)|0x00600000; // top 3 bits
  // NVIC_EN0_R = NVIC_EN0_INT22;     // enable interrupt 22 in NVIC
}

void PeriodMeasure_InitSide(void){
  SYSCTL_RCGCTIMER_R |= 0x08;    // activate timer3 
  TIMER3_CTL_R &= ~TIMER_CTL_TAEN; // disable timer0A during setup
  TIMER3_CFG_R = TIMER_CFG_16_BIT; // configure for 16-bit timer mode
                                   // configure for 24-bit capture mode
  TIMER3_TAMR_R = (TIMER_TAMR_TACMR|TIMER_TAMR_TAMR_CAP);
                                   // configure for both edge event
  TIMER3_CTL_R |= TIMER_CTL_TAEVENT_BOTH;
  TIMER3_TAILR_R = TIMER_TAILR_TAILRL_M;// start value
  TIMER3_TAPR_R = 0xFF;            // activate prescale, creating 24-bit
  TIMER3_IMR_R |= TIMER_IMR_CAEIM; // enable capture match interrupt
  TIMER3_ICR_R = TIMER_ICR_CAECINT;// clear timer0A capture match flag
  TIMER3_CTL_R |= TIMER_CTL_TAEN;  // enable timer0A 16-b, +edge timing, interrupts
                                   // Timer1B=priority 3

  NVIC_EN1_R |= 1<<(35-32);           // 9) enable IRQ 21 in NVIC
  // TIMER3_CTL_R |= 0x00000001;    // 10) enable TIMER1A
  // NVIC_PRI5_R = (NVIC_PRI5_R&0xFF00FFFF)|0x00600000; // top 3 bits
  // NVIC_EN1_R = NVIC_EN0_INT22;     // enable interrupt 22 in NVIC
}

// period is 16-bit number of PWM clock cycles in one period (3<=period)
// period for PB6 and PB7 must be the same
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)
// PWM clock rate = processor clock rate/SYSCTL_RCC_PWMDIV
//                = BusClock/2
//                = 80 MHz/2 = 40 MHz (in this example)
// Output on PB7/M0PWM1
void Down_Ultrasonic_Init(uint16_t period, uint16_t duty){
  volatile unsigned long delay;
  SYSCTL_RCGCPWM_R |= 0x01;             // 1) activate PWM0
  SYSCTL_RCGCGPIO_R |= 0x02;            // 2) activate port B
  delay = SYSCTL_RCGCGPIO_R;            // allow time to finish activating
  GPIO_PORTB_AFSEL_R |= 0x80;           // enable alt funct on PB7
  GPIO_PORTB_PCTL_R &= ~0xF0000000;     // configure PB7 as M0PWM1
  GPIO_PORTB_PCTL_R |= 0x40000000;
  GPIO_PORTB_AMSEL_R &= ~0x80;          // disable analog functionality on PB7
  GPIO_PORTB_DEN_R |= 0x80;             // enable digital I/O on PB7
  SYSCTL_RCC_R |= SYSCTL_RCC_USEPWMDIV; // 3) use PWM divider
  SYSCTL_RCC_R &= ~SYSCTL_RCC_PWMDIV_M; //    clear PWM divider field
  SYSCTL_RCC_R += SYSCTL_RCC_PWMDIV_2;  //    configure for /2 divider
  PWM0_0_CTL_R = 0;                     // 4) re-loading down-counting mode
  PWM0_0_GENB_R = (PWM_0_GENB_ACTCMPBD_ONE|PWM_0_GENB_ACTLOAD_ZERO);
  // PB7 goes low on LOAD
  // PB7 goes high on CMPB down
  PWM0_0_LOAD_R = period - 1;           // 5) cycles needed to count down to 0
  PWM0_0_CMPB_R = duty - 1;             // 6) count value when output rises
  PWM0_0_CTL_R |= 0x00000001;           // 7) start PWM0
  PWM0_ENABLE_R &= ~(0x00000002);          // Disable output to PB7/M0PWM1
}

void Top_Ultrasonic_Init(uint16_t period, uint16_t duty){
   volatile unsigned long delay;
  SYSCTL_RCGCPWM_R |= 0x01;             // 1) activate PWM0
  SYSCTL_RCGCGPIO_R |= 0x02;            // 2) activate port B
  delay = SYSCTL_RCGCGPIO_R;            // allow time to finish activating
  GPIO_PORTB_AFSEL_R |= 0x20;           // enable alt funct on PB5
  GPIO_PORTB_PCTL_R &= ~0x00F00000;     // configure PB5 as M0PWM1
  GPIO_PORTB_PCTL_R |= 0x00400000;
  GPIO_PORTB_AMSEL_R &= ~0x20;          // disable analog functionality on PB7
  GPIO_PORTB_DEN_R |= 0x20;             // enable digital I/O on PB7
  SYSCTL_RCC_R |= SYSCTL_RCC_USEPWMDIV; // 3) use PWM divider
  SYSCTL_RCC_R &= ~SYSCTL_RCC_PWMDIV_M; //    clear PWM divider field
  SYSCTL_RCC_R += SYSCTL_RCC_PWMDIV_2;  //    configure for /2 divider
  PWM0_1_CTL_R = 0;                     // 4) re-loading down-counting mode
  PWM0_1_GENB_R = (PWM_0_GENB_ACTCMPBD_ONE|PWM_0_GENB_ACTLOAD_ZERO);
  // PWM0_0_GENB_R = (PWM_0_GENB_ACTCMPBD_ONE|PWM_0_GENB_ACTLOAD_ZERO);
  PWM0_1_LOAD_R = period - 1;           // 5) cycles needed to count down to 0
  PWM0_1_CMPB_R = duty - 1;             // 6) count value when output rises
  PWM0_1_CTL_R |= 0x00000001;           // 7) start PWM0
  PWM0_ENABLE_R &= ~(0x00000008);          // Disable output to PB7/M0PWM1
}

// change duty cycle of PB7
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)
void SendPulse(uint16_t duty){
  PWM0_0_CMPB_R = duty - 1;             // 6) count value when output rises
  PWM0_ENABLE_R |= (0x00000002);          // Start output to PB7/M0PWM1
}

void SendPulseTop(uint16_t duty){
  PWM0_1_CMPB_R = duty - 1;             // 6) count value when output rises
  PWM0_ENABLE_R |= (0x00000008);          // Start output to PB7/M0PWM1
}

void SendPulseSide(){
  PB3 ^= 0x08;
  delay(1);
  PB3 ^= 0x08;
}

void StopPulse(){
    PWM0_ENABLE_R &= ~(0x00000002);          // Disable output to PB7/M0PWM1
	  // PWM0_0_CTL_R &= ~(0x00000001);           // 7) start PWM0
}

void StopPulseTop(){
    PWM0_ENABLE_R &= ~(0x00000008);          // Disable output to PB7/M0PWM1
	  // PWM0_0_CTL_R &= ~(0x00000001);           // 7) start PWM0
}



void Timer11A_Handler(void){
  StopPulseTop();
  PF2 = PF2^0x04;  // toggle PF2
  captureCountTop++;
  captureCountTop = captureCountTop%2; 

	if (!captureCountTop){
		PeriodTop = (FirstTop - TIMER1_TAR_R)&0xFFFFFF;// 24 bits, 12.5ns resolution
    distanceTop = ((PeriodTop * 0.0000000125) * 340)/2;
    distanceTop*=100;  //in centimetres
    TIMER1_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
    return;

	}
  FirstTop = TIMER1_TAR_R;            // setup for next
  PF2 = PF2^0x04;  // toggle PF2
	TIMER1_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
}


void Timer0A_Handler(void){
  // StopPulse();   
  // PF2 = PF2^0x04;  // toggle PF2
  captureCount++;
  captureCount = captureCount%2; 
  CanMeasure = 0;
	if (captureCount == 1){
    First = TIMER0_TAR_R;            // setup for next
    TIMER0_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
		return;
  } else if (captureCount == 0){
    Period = (First - TIMER0_TAR_R)&0xFFFFFF;// 24 bits, 12.5ns resolution
    distance = ((Period * 0.0000000125) * 340)/2;
    distance*=100;  //in centimetres
    CanMeasure = 1;
    // PF2 = PF2^0x04;  // toggle PF2
		TIMER0_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
    return;
  }
}

void Timer33A_Handler(void){
  // StopPulse();   
  captureCountSide++;
  captureCountSide = captureCountSide%2;    //counter to ascertain rising and falling edge are measured sequentially
  CanMeasureSide = 0;
  if (captureCountSide == 1){  //first edge
    FirstSide = TIMER3_TAR_R;
	//	SendPulseSide();
		TIMER3_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
		return;
  } else if(captureCountSide == 0){  //second edge
    PeriodSide = (FirstSide - TIMER3_TAR_R)&0xFFFFFF;// 24 bits, 12.5ns resolution
    distanceSide = ((PeriodSide * 0.0000000125) * 340)/2;
    distanceSide*=100;  //in centimetres
    if (countSide < MAX_DATA_POINT) {
			periodDataSide[countSide] = distanceSide;
			countSide++;
			//SendPulseSide();
		  TIMER3_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
			return;
    } 
    if (countSide == MAX_DATA_POINT) {
      // SendDownUltraSonicToCan(periodDataDown);
      countSide = 0;
      CanMeasureSide = 1;
     // SendPulseSide();
      distanceSide = periodDataSide[3];
			TIMER3_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
      return;
    }
  }
}



// void Timer1A_Handler(void){
//   PF2 = PF2^0x04;  // toggle PF2
//   StopPulseTop();   
//   captureCountTop++;
//   captureCountTop = captureCountTop%2; 
//   CanMeasureTop = 0;
//   // TIMER1_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
// 	if (captureCountTop == 1){
//     FirstTop = TIMER1_TAR_R;
// 		TIMER1_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
// 		return;
// 	} else if( captureCountTop == 0){
//     PeriodTop = (FirstTop - TIMER1_TAR_R)&0xFFFFFF;// 24 bits, 12.5ns resolution
//     distanceTop = ((PeriodTop * 0.0000000125) * 340)/2;
//     distanceTop*=100;  //in centimetres
//     CanMeasureTop = 1;
//     PF2 = PF2^0x04;  // toggle PF2
//     TIMER1_ICR_R = TIMER_ICR_CAECINT;// acknowledge timer0A capture match
//     //ST7735_Message(0,0, "Distance(Top) : ", distanceTop);
// 		return;
//   }
// }




void Ultrasonic_Init(){
  Down_Ultrasonic_Init(40000,(PULSE_4US));        //initialize ultrasonic driver for 4us output signal
  // Top_Ultrasonic_Init(40000,(PULSE_4US));        //initialize ultrasonic driver for 4us output signal
  GPIO_PortBInit();
  PeriodMeasure_Init();            // initialize 24-bit timer0A in capture mode
  // PeriodMeasure_InitTop();            // initialize 24-bit timer0A in capture mode
    // PeriodMeasure_InitSide();
  CanMeasure = 1;
  // CanMeasureTop = 1;
}

static void delay(uint32_t N) {
    for(int n = 0; n < N; n++)                         // N time unitss
        for(int msec = 10000; msec > 0; msec--);       // 1 time unit
}
