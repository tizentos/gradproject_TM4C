

#ifndef _ST7735EXT_H_
#define _ST7735EXT_H_


#define FIRSTSCREEN    0 
#define SECONDSCREEN   8
#define LINESPACE      1

void ST7735Ext_Init(void);


/**
 * Message method that creates a two screen abstraction
 * device - FIRSTSCREEN or SECONDSCREEN
 * line - Line number in the device
 * offset - Starting column of the device
 *  string - String to spring
 * color - get color from ST3375 Color declarations
 * */
void ST7735Ext_Message (int device, int line,int offset, char *string, int16_t color);


/**
 * This function clears the screen
 * */
void ST7735Ext_Clear (void);

#endif