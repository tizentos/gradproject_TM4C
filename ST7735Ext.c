
/** This is an extension of the LCD driver   
 * 
 * */
#include "stdio.h"
#include "stdint.h"
#include "ST7735.h"

#include "ST7735Ext.h"



/**
 * Function to initialize the screen
 * */
void ST7735Ext_Init(){
    ST7735_InitR(INITR_REDTAB);
    ST7735Ext_Clear ();
}

/**
 * Message method that creates a two screen abstraction
 * device - FIRSTSCREEN or SECONDSCREEN
 * line - Line number in the device
 * offset - Starting column of the device
 *  string - String to spring
 * color - get color from ST3375 Color declarations
 * */
void ST7735Ext_Message (int device, int line,int offset, char *string, int16_t color){
    line*=LINESPACE;
    switch (device)
    {
        case FIRSTSCREEN:
            ST7735_DrawString(offset,line,string, color);
            break;
        case SECONDSCREEN:
            ST7735_DrawString(offset,(SECONDSCREEN + line),string, color);
            break;
        default:
            ST7735_DrawString(0,offset,"Error Input", ST7735_Color565(0,255,0));
            break;
    }
    return;
}


//Clears screen
void ST7735Ext_Clear (void){
    ST7735_FillScreen(ST7735_BLACK);
    for (int i=0; i< ST7735_TFTWIDTH; i++){
        ST7735_DrawFastHLine(i,79,2,ST7735_BLUE);
    }
}


