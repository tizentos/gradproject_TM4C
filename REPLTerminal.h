

#ifndef _REPL_H
#define _REPL_H



#define  ESP_TERMINAL     1
#define  UART0_TERMINAL   0

#define  DEBUG_MESSAGE     "-d "
#define  FIRST_LED         "-r "
#define  SECOND_LED        "-g "
#define  SEND_DATA         "-s "
#define  RESET_ESP         "-o "
#define CLEARSCREEN        "-c "






// #define LOADELF  "-l "
// #define PRINTSECOND  "-s "
// #define ECHO "-e "
// #define CLEARSCREEN "-c "


// #define TEST  "-t "
// #define FORMAT  "-f "
// #define DISPLAYDIRECTORY "-d "
// #define OPENFILE "-o "
// #define READFILE "-r "
// #define WRITEFILE "-w "
// #define DELETEFILE "-m "
// #define NEWFILE "-n "


void REPLTerminal_Init(void);
void REPLEvaluate(char *command);
void REPLPrint(char *output);

#endif