// os.c
// Runs on LM4F120/TM4C123/MSP432
// A very simple real time operating system with minimal features.
// Daniel Valvano
// February 8, 2016

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2016
   "Embedded Systems: Real-Time Operating Systems for ARM Cortex-M Microcontrollers",
   ISBN: 978-1466468863, , Jonathan Valvano, copyright (c) 2016
   Programs 4.4 through 4.12, section 4.2
 Copyright 2016 by Jonathan W. Valvano, valvano@mail.utexas.edu
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
#include "Timer1.h"
#include "os.h"
#include "PLL.h"
#include "../inc/tm4c123gh6pm.h"
#include "FIFO.h"
#include "ST7735Ext.h"
// #include "inc/UART.h"
// #include "inc/LED.h"



void DisableInterrupts(void);  // low power mode

#define PE3  (*((volatile unsigned long *)0x40024020))
#define priorityScheduler 0
#define roundScheduler 1
#define NULL 0

unsigned long PutI;      // index of where to put next
unsigned long GetI;      // index of where to get next
unsigned long Fifo[FSIZE];

long MailboxLetter;
unsigned long LostMailBoxLetter;
Sema4Type MailSent;


Sema4Type FifoElement;
Sema4Type CurrentSize;
unsigned long LostFifoElement;

unsigned long TimeSlice;

// function definitions in os.s
void StartOS(void);
void SysTick_Handler(void);
void PrioritySwitch(void);
__svc(3) void SVC_Handler();


//Declaration
void SetInitialStack(int i);
void Tcbs_Init();
// void (*BackgroundTask)(void);   // Background task for switch
void (*SW1Task)(void);   // SW1 Task
void (*SW2Task)(void);   // SW2 Task
void static runsleep(void);



tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
tcbType *NewRunPt;
int trackPriorityThreadIndex = 0;
int32_t Stacks[NUMTHREADS][STACKSIZE];


int IsSwitchInit = 0;
int NumPeriodicTaskCreated = 0;
int NumBlockThread = 0; //To track the number of blocked thread


// int CurrentThreadNum = 0;

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: systick, 80 MHz PLL
// input:  none
// output: none
void OS_Init(void){
  DisableInterrupts();
  PLL_Init(Bus80MHz);
  // LED_Init();
  Tcbs_Init();
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
  SleepTimer3_Init(runsleep,TIME_1MS);   //set timer to priority 3
	EnableInterrupts();
}

void Switch_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x20;      // (a) activate clock for Port F 
  GPIO_PORTF_LOCK_R = 0x4C4F434B; // unlock GPIO Port F
  GPIO_PORTF_CR_R = 0x1F;         // allow changes to PF4-0
  //GPIO_PORTF_DIR_R = 0x02;        // (c) make PF4,PF0 in and PF1 is out 

  GPIO_PORTF_DIR_R |= 0x0E;  //0b01110  input is 0 while output is 1

  // GPIO_PORTF_DEN_R |= 0x13;       //  enable digital I/O on PF4,PF0, PF1
  GPIO_PORTF_DEN_R |= 0x0000001F;
  GPIO_PORTF_PUR_R |= 0x11;       // pullups on PF4,PF0
  GPIO_PORTF_IS_R &= ~0x11;       // (d) PF4,PF0 are edge-sensitive 
  GPIO_PORTF_IBE_R &= ~0x11;      //     PF4,PF0 are not both edges 
  GPIO_PORTF_IEV_R &= ~0x11;      //     PF4,PF0 falling edge event 
  GPIO_PORTF_ICR_R = 0x11;        // (e) clear flags
  GPIO_PORTF_IM_R |= 0x11;        // (f) arm interrupt on PF4,PF0
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // (g) priority 5
  NVIC_EN0_R = 0x40000000;        // (h) enable interrupt 30 in NVIC       // (h) enable interrupt 30 in NVIC
  EnableInterrupts();
}




void Tcbs_Init(){
  long status;
  status = StartCritical();
  int i = 0;
  while(i < NUMTHREADS){
    // SetInitialStack(i);
    tcbs[i].id=i;
    tcbs[i].active = 0;
    tcbs[i].sleep = 0;
    tcbs[i].killed = 0;
    tcbs[i].priority = 8;   //intentionally out of range
    tcbs[i].running = 0 ; //thread not running yet
    if ((i+1) >= NUMTHREADS){
      tcbs[i].next = &tcbs[0];
      break;
    }
    tcbs[i].next=&tcbs[i+1];
    i++;
  } 
  RunPt = &tcbs[0]; 
  RunPt -> running = 1; //now running
  EndCritical(status);
}

void SetInitialStack(int i){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000;   // thumb bit
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12
  Stacks[i][STACKSIZE-5] = 0x03030303;   // R3
  Stacks[i][STACKSIZE-6] = 0x02020202;   // R2
  Stacks[i][STACKSIZE-7] = 0x01010101;   // R1
  Stacks[i][STACKSIZE-8] = 0x00000000;   // R0
  Stacks[i][STACKSIZE-9] = 0x11111111;   // R11
  Stacks[i][STACKSIZE-10] = 0x10101010;  // R10
  Stacks[i][STACKSIZE-11] = 0x09090909;  // R9
  Stacks[i][STACKSIZE-12] = 0x08080808;  // R8
  Stacks[i][STACKSIZE-13] = 0x07070707;  // R7
  Stacks[i][STACKSIZE-14] = 0x06060606;  // R6
  Stacks[i][STACKSIZE-15] = 0x05050505;  // R5
  Stacks[i][STACKSIZE-16] = 0x04040404;  // R4
}

//******** OS_AddThread ***************
// add three foregound threads to the scheduler
// Inputs: three pointers to a void/void foreground tasks
// Outputs: 1 if successful, 0 if this thread can not be added
int OS_AddThreads(void(*task0)(void),void(*task1)(void),void(*task2)(void)){
  OS_AddThread(task0,STACKSIZE,7);
  OS_AddThread(task1,STACKSIZE,7);
  OS_AddThread(task2,STACKSIZE,7);
  // RunPt = &tcbs[0];       // thread 0 will run first
  return 1;               // successful
}


//Function to add new thread to the TCB
int OS_AddThread(void(*task)(void),unsigned long stackSize, unsigned long priority){
  int32_t status;
  status = StartCritical();
  for (int i=0 ; i < NUMTHREADS; i++){
    if (tcbs[i].active == 0){
      //add thread
      Stacks[i][stackSize-2]=(int32_t)task; // PC
      tcbs[i].active = 1;
      tcbs[i].priority = priority;
      tcbs[i].blocked = 0;
      SetInitialStack(i);
      EndCritical(status);
      return 1;
    }
  }
  EndCritical(status);
  //return fail message
  return 0;
}

int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority){

  switch (NumPeriodicTaskCreated)
  {
    case 0:  
      Timer0A_Init(task,period);
      NumPeriodicTaskCreated++;
      return 1;
    case 1:
      Timer1_Init(task,period,priority);
      NumPeriodicTaskCreated++;
      return 1;
    default:
      //Throw Error message
      return 0;
  }
   return 0;   //code shouldn't reach here 
}

int OS_AddSW1Task(void(*task)(void), unsigned long priority){
  SW1Task = task;
  if(!IsSwitchInit){
    Switch_Init();
    IsSwitchInit = 1;
    return 1;
  } else {
     return 1;
  }
}
int OS_AddSW2Task(void(*task)(void), unsigned long priority){
  SW2Task = task;
  if(!IsSwitchInit){
    Switch_Init();
    IsSwitchInit = 1;
    return 1;
  } else {
     return 1;
  }
}

unsigned long OS_Id(void){
  return RunPt -> id;
}

//******** OS_Launch ***************
// start the scheduler, enable interrupts
// Inputs: number of clock cycles for each time slice
//         (maximum of 24 bits)
// Outputs: none (does not return)
void OS_Launch(uint32_t theTimeSlice){
  NVIC_ST_RELOAD_R = theTimeSlice - 1; // reload value
  TimeSlice = theTimeSlice;
  NVIC_ST_CTRL_R |= 0x00000007; // enable, core clock and interrupt arm     // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}


//Get TCB with highest priority
tcbType *getHighestPriorityThread(tcbType tcbList[]){
  int i = trackPriorityThreadIndex;
  tcbType tempTcb=tcbList[trackPriorityThreadIndex];
  for (i; i < NUMTHREADS ; i++){
    if (tempTcb.priority > tcbList[i].priority){
      tempTcb = tcbList[i];
      trackPriorityThreadIndex=i;
    }
  }
  (++trackPriorityThreadIndex)%NUMTHREADS;
  return &tempTcb;
}


void Scheduler(void){
    do{
      if (roundScheduler){
        RunPt = RunPt->next;
      }else if ( priorityScheduler){
        int i=0;
        int j=0;
        for (j ; j< NUMTHREADS;j++){
          if (tcbs[j].active && !tcbs[j].sleep && !tcbs[j].blocked && !tcbs[j].running){
            RunPt = &tcbs[j];
          }
        }
        for (i; i < NUMTHREADS ; i++){
          if (tcbs[i].active && !tcbs[i].sleep && !tcbs[i].blocked  && !tcbs[i].running){
						if (RunPt->priority >= tcbs[i].priority){
							RunPt = &tcbs[i];
					  }
          } 
          tcbs[i].running = 0;
			  }
      }
    } while ((!RunPt -> active) || RunPt ->sleep || RunPt->blocked);
    RunPt->running = 1;
}

//******** OS_Suspend ***************
// temporarily suspend the running thread and launch another
// Inputs: none
// Outputs: none
void OS_Suspend(void){
    // STCURRENT = 0;
  NVIC_ST_CURRENT_R = NVIC_ST_CURRENT_S;  //reset timer to zero for equl sharing
  // NVIC_INT_CTRL_R |= 0x04000000; // trigger SysTick
  // NVIC_INT_CTRL_R |= 0x10000000;
	SVC_Handler();
  // SysTick_Handler();
}


//High level check for available space in block thread array
int IsBlockedThreadFifoAvailable(Sema4Type *semaPt){
  for (int i=0 ; i < MAX_NUM_BLOCKED_THREAD; i++){
    if (semaPt->blockedTcb[i] == NULL){
      return 1;
    }
  }
  return 0;
}

//Get TCB pointer with highest priority
tcbType *highestPriorityThread(tcbType *tcbList[]){
  int i=0;
  int j=0;
  int blockThreadIndexToClear = 0; //to clean up unblocked thread footprint
  tcbType *tempTcb = NULL;
  tempTcb -> priority = 8; 
  for (j; j < NUMTHREADS ; j++){
    if (tcbList[j] != NULL){
      if (tempTcb->priority < tcbList[j]->priority){
        continue;
      } else if (tempTcb->priority > tcbList[j]-> priority){
        tempTcb = tcbList[j];
        blockThreadIndexToClear = j;
      }
    }
  }
  tcbList[blockThreadIndexToClear] = NULL; //clearing the actual position of the thread
  return tempTcb;
}
void OS_InitSemaphore(Sema4Type *semaPt, long value){
  semaPt ->Value = value;
  semaPt -> PutI = 0;
  semaPt -> GetI = 0;
  for (int i = 0 ; i<MAX_NUM_BLOCKED_THREAD; i++){
    semaPt->blockedTcb[i] = NULL;
  }
}
//Counting semaphore wait
void OS_Wait(Sema4Type *semaPt){
  DisableInterrupts();
  if((semaPt->Value) == 0){

    if (IsBlockedThreadFifoAvailable(semaPt)){
      RunPt -> blocked = 1;   //to block the thread
      semaPt -> blockedTcb[semaPt->PutI] = RunPt;
      semaPt->PutI = (semaPt->PutI + 1)%MAX_NUM_BLOCKED_THREAD;

			EnableInterrupts();
      OS_Suspend();
       return ;
    }
  }
  (semaPt->Value) = (semaPt->Value) - 1;
  EnableInterrupts();
}


//Function to implement priority switch //Still testing this function
//The function is deffirent from the Scheduler()
void HandOver(){
	   NewRunPt->running = 1;
     RunPt->running = 0;
     RunPt = &tcbs[NewRunPt->id];
}

//Counting semaphore signal
void OS_Signal(Sema4Type *semaPt){
  DisableInterrupts();
  (semaPt->Value) = (semaPt->Value) + 1;
  if (priorityScheduler){
    //wake up thread with highest priority
    NewRunPt = highestPriorityThread(semaPt->blockedTcb); 
    
    NewRunPt->blocked = 0;  //wake up thread

    //Check which priority is higher
    //if ((NewRunPt != NULL)&&(NewRunPt->priority < RunPt->priority)){
      // NewRunPt->blocked = 0;
        //NewRunPt->running = 1;
     // RunPt->running = 0;
     // RunPt = NULL;
     // RunPt = NewRunPt;
      //  NVIC_ST_CURRENT_R = NVIC_ST_CURRENT_S; 
		   //	EnableInterrupts();
      //  PrioritySwitch();
			//return;
  // }
    //reset Systick timer
    //switch to this thread
   // EnableInterrupts();
   // return;
  }
  if (roundScheduler){
    if (semaPt ->blockedTcb[semaPt->GetI] != NULL){
      semaPt ->blockedTcb[semaPt->GetI]->blocked = 0;
      semaPt ->blockedTcb[semaPt->GetI] = NULL;
      semaPt->GetI = (semaPt->GetI + 1)%MAX_NUM_BLOCKED_THREAD;
    }
  }
  EnableInterrupts();
}


//Function planned to be used alongside HandOver()
void PrioritySwitch(){
	NVIC_ST_CURRENT_R = NVIC_ST_CURRENT_S;  //reset timer to zero for equl sharing
  NVIC_INT_CTRL_R |= 0x10000000;
	PendSV_Handler();
}

void OS_bWait(Sema4Type *semaPt){
  DisableInterrupts();
  while((semaPt->Value) == 0){
    EnableInterrupts(); // <- interrupts can occur here
    // OS_Suspend();
    DisableInterrupts();
  }
  (semaPt->Value) = (semaPt->Value) - 1;
  EnableInterrupts();
}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
  DisableInterrupts();

  //Bound to binary (0 and 1)
  // (semaPt->Value >= 1) ? (semaPt->Value = 1) : 
  // (semaPt ->Value= semaPt->Value + 1);
  semaPt->Value = 1;
  EnableInterrupts();
}

void OS_Kill(void){
    //For code to be here, RunPt is on the active thread
    //Set active to zero
    DisableInterrupts();
    RunPt ->active = 0;   //To kill a thread for now
    RunPt -> killed = 1;   //To denote a killed thread
    EnableInterrupts();
    OS_Suspend();
}

void OS_Sleep(unsigned long sleepTime){
  RunPt -> sleep = sleepTime;
  OS_Suspend();
}

void static runsleep(void){
// **DECREMENT SLEEP COUNTERS
	int32_t i;
	for (i=0;i<NUMTHREADS;i++){ 
      if(tcbs[i].sleep != 0) {	//search for sleeping main threads
			tcbs[i].sleep --;	//decrement sleep period by 1ms
		}
	}
}

// ******** OS_FIFO_Init ************
// Initialize FIFO.  
// One event thread producer, one main thread consumer
// Inputs:  none
// Outputs: none
void OS_Fifo_Init(unsigned long fifosize){ 
  //Init the FIFO with indexes and CurrentSize and LostData set to 0
	PutI = 0;
	GetI = 0;
	OS_InitSemaphore(&CurrentSize,0);
  // OS_InitSemaphore(&LostFifoElement,0);
}

long OS_Fifo_Size(void){
  return CurrentSize.Value;
  // return OSFifo_Size();
}

// ******** OS_FIFO_Put ************
// Put an entry in the FIFO.  
// Exactly one event thread puts,
// do not block or spin if full
// Inputs:  data to be stored
// Outputs: 0 if successful, -1 if the FIFO is full
int OS_Fifo_Put(unsigned long data){
	if(CurrentSize.Value == FSIZE) { //FIFO is full
		LostFifoElement++;
		return 0; //Error
	}
	else {
		Fifo[PutI] = data;	//store data in FIFO at put index
		PutI = (PutI + 1)%FSIZE; //Increment Put index and wrap around if necessary
		OS_Signal(&CurrentSize);
		return 1;	//Success
	}
}

// ******** OS_FIFO_Get ************
// Get an entry from the FIFO.   
// Exactly one main thread get,
// do block if empty
// Inputs:  none
// Outputs: data retrieved
unsigned long OS_Fifo_Get(void){
  unsigned long data;
	OS_Wait(&CurrentSize);	//Wait till there is data in FIFO, block if empty
	data = Fifo[GetI];	//Get stored data from Fifo
	GetI = (GetI + 1) % FSIZE;	//Incremet Get index and wrap around
  return data;
}

void OS_MailBox_Init(void){
  // include data field and semaphore
	MailboxLetter = 0;
	MailSent.Value = 0;
	LostMailBoxLetter = 0;
}


void OS_MailBox_Send(unsigned long data){
	if(MailSent.Value)
	{
		LostMailBoxLetter++;
	}
	else
	{
		MailboxLetter = data;
		OS_Signal(&MailSent);
	}
}

unsigned long OS_MailBox_Recv(void){ 
  unsigned long data;
	OS_Wait(&MailSent);
	data = MailboxLetter;
	return data;
}

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void){
  unsigned long time = NVIC_ST_CURRENT_R;
  return time;
}

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop){
  long elapse = start-stop;  //Because the timer is counting down from the reload value
  (elapse < 0) ? ( elapse+=TimeSlice) : ( elapse = elapse);
  return elapse;
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
  int status;
	status=StartCritical();
	NVIC_ST_CURRENT_R = 0;
	EndCritical(status);
}

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void){
  unsigned long time = NVIC_ST_CURRENT_R;
  return ( time / 80000); //every 80000 count is 1ms
}

unsigned long OS_UsTime(void){
  unsigned long time = NVIC_ST_CURRENT_R;
  return ( time / 8000); //every 80000 count is 1ms
}


void GPIOPortF_Handler(void){
   PE3 = 0x08;
  if(GPIO_PORTF_RIS_R&0x10){  // poll PF4
  //  UART_OutString("SW1 GPIO_F Triggered"); OutCRLF();
   GPIO_PORTF_ICR_R = 0x10;  // acknowledge flag4
    // OS_Signal(&SW1);          // signal SW1 occurred
    (*SW1Task)();
		// FallingEdges = FallingEdges + 1;
  }
  if(GPIO_PORTF_RIS_R&0x01){  // poll PF0
  //  UART_OutString("SW2 GPIO_F Triggered"); OutCRLF();
   GPIO_PORTF_ICR_R = 0x01;  // acknowledge flag0
   (*SW2Task)();
    // OS_Signal(&SW2);          // signal SW2 occurred
		// FallingEdges = FallingEdges - 1;
  }
   PE3 = 0x00;

}

