#include "type.h"
#include "LCDHw.h"
#include "LCDGui.h"
#include "LCDTest.h"
#include "LCDTouch.h"
#include "pic.h"
#include "lcd_app.h"

#include <stdint.h>
#include <stdio.h>

extern u8 CMD_RDX;
extern u8 CMD_RDY;

//========================variable==========================//
u16 ColorTab[5]={RED,GREEN,BLUE,YELLOW,BRED};//
//=====================end of variable======================//

/*****************************************************************************
 * @name       :void DrawTestPage(u8 *str)
 * @date       :2018-08-09 
 * @function   :Drawing test interface
 * @parameters :str:the start address of the Chinese and English strings
 * @retvalue   :None
******************************************************************************/ 
void DrawTestPage(u8 *str)
{
//up
LCD_Clear(WHITE);
LCD_Fill(0,0,lcddev.width,20,BLUE);
//down
LCD_Fill(0,lcddev.height-20,lcddev.width,lcddev.height,BLUE);
POINT_COLOR=WHITE;
Gui_StrCenter(0,2,WHITE,BLUE,str,16,1);//
Gui_StrCenter(0,lcddev.height-18,WHITE,BLUE,"http://www.lcdwiki.com",16,1);//
//
//LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE);
}

/*****************************************************************************
 * @name       :void main_test(void)
 * @date       :2018-08-09 
 * @function   :Drawing the main Interface of the Comprehensive Test Program
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void main_test(void)
{
	DrawTestPage("");	
	Gui_StrCenter(0,30,RED,BLUE,"",16,1);//
	Gui_StrCenter(0,60,RED,BLUE,"",16,1);//	
	Gui_StrCenter(0,90,GREEN,BLUE,"2.4\" ILI9341 240X320",16,1);//
	Gui_StrCenter(0,120,BLUE,BLUE,"xiaoFeng@QDtech 2018-08-20",16,1);//
	delay_ms(1500);		
	delay_ms(1500);
}

/*****************************************************************************
 * @name       :void Test_Color(void)
 * @date       :2018-08-09 
 * @function   :Color fill test(white,black,red,green,blue)
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Test_Color(void)
{
	//DrawTestPage("1:");
	LCD_Fill(0,0,lcddev.width,lcddev.height,WHITE);
	Show_Str(20,30,BLUE,YELLOW,"BL Test",16,1);delay_ms(800);
	LCD_Fill(0,0,lcddev.width,lcddev.height,RED);
	Show_Str(20,30,BLUE,YELLOW,"RED ",16,1);delay_ms(800);
	LCD_Fill(0,0,lcddev.width,lcddev.height,GREEN);
	Show_Str(20,30,BLUE,YELLOW,"GREEN ",16,1);delay_ms(800);
	LCD_Fill(0,0,lcddev.width,lcddev.height,BLUE);
	Show_Str(20,30,RED,YELLOW,"BLUE ",16,1);delay_ms(800);
}

/*****************************************************************************
 * @name       :void Test_FillRec(void)
 * @date       :2018-08-09 
 * @function   :Rectangular display and fill test
								Display red,green,blue,yellow,pink rectangular boxes in turn,
								1500 milliseconds later,
								Fill the rectangle in red,green,blue,yellow and pink in turn
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Test_FillRec(void)
{
	u8 i=0;
	DrawTestPage("2:GUI");
	LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE);
	for (i=0; i<5; i++) 
	{
		POINT_COLOR=ColorTab[i];
		LCD_DrawRectangle(lcddev.width/2-80+(i*15),lcddev.height/2-80+(i*15),lcddev.width/2-80+(i*15)+60,lcddev.height/2-80+(i*15)+60); 
	}
	delay_ms(1500);	
	LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE); 
	for (i=0; i<5; i++) 
	{
		POINT_COLOR=ColorTab[i];
		LCD_DrawFillRectangle(lcddev.width/2-80+(i*15),lcddev.height/2-80+(i*15),lcddev.width/2-80+(i*15)+60,lcddev.height/2-80+(i*15)+60); 
	}
	delay_ms(1500);
}

/*****************************************************************************
 * @name       :void Test_Circle(void)
 * @date       :2018-08-09 
 * @function   :circular display and fill test
								Display red,green,blue,yellow,pink circular boxes in turn,
								1500 milliseconds later,
								Fill the circular in red,green,blue,yellow and pink in turn
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Test_Circle(void)
{
	u8 i=0;
	DrawTestPage("3:GUI");
	LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE);
	for (i=0; i<5; i++)  
		gui_circle(lcddev.width/2-80+(i*25),lcddev.height/2-50+(i*25),ColorTab[i],30,0);
	delay_ms(1500);	
	LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE); 
	for (i=0; i<5; i++) 
	  	gui_circle(lcddev.width/2-80+(i*25),lcddev.height/2-50+(i*25),ColorTab[i],30,1);
	delay_ms(1500);
}

/*****************************************************************************
 * @name       :void English_Font_test(void)
 * @date       :2018-08-09 
 * @function   :English display test
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void English_Font_test(void)
{
	DrawTestPage("5:");
	Show_Str(10,30,BLUE,YELLOW,"6X12:abcdefghijklmnopqrstuvwxyz0123456789",12,0);
	Show_Str(10,45,BLUE,YELLOW,"6X12:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",12,1);
	Show_Str(10,60,BLUE,YELLOW,"6X12:~!@#$%^&*()_+{}:<>?/|-+.",12,0);
	Show_Str(10,80,BLUE,YELLOW,"8X16:abcdefghijklmnopqrstuvwxyz0123456789",16,0);
	Show_Str(10,100,BLUE,YELLOW,"8X16:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",16,1);
	Show_Str(10,120,BLUE,YELLOW,"8X16:~!@#$%^&*()_+{}:<>?/|-+.",16,0); 
	delay_ms(1200);
}

/*****************************************************************************
 * @name       :void Test_Triangle(void)
 * @date       :2018-08-09 
 * @function   :triangle display and fill test
								Display red,green,blue,yellow,pink triangle boxes in turn,
								1500 milliseconds later,
								Fill the triangle in red,green,blue,yellow and pink in turn
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Test_Triangle(void)
{
	u8 i=0;
	DrawTestPage("4:GUI Triangle");
	LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE);
	for(i=0;i<5;i++)
	{
		POINT_COLOR=ColorTab[i];
		Draw_Triangel(lcddev.width/2-80+(i*20),lcddev.height/2-20+(i*15),lcddev.width/2-50-1+(i*20),lcddev.height/2-20-52-1+(i*15),lcddev.width/2-20-1+(i*20),lcddev.height/2-20+(i*15));
	}
	delay_ms(1500);	
	LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE); 
	for(i=0;i<5;i++)
	{
		POINT_COLOR=ColorTab[i];
		Fill_Triangel(lcddev.width/2-80+(i*20),lcddev.height/2-20+(i*15),lcddev.width/2-50-1+(i*20),lcddev.height/2-20-52-1+(i*15),lcddev.width/2-20-1+(i*20),lcddev.height/2-20+(i*15));
	}
	delay_ms(1500);
}

/*****************************************************************************
 * @name       :void Chinese_Font_test(void)
 * @date       :2018-08-09 
 * @function   :chinese display test
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Chinese_Font_test(void)
{	
	DrawTestPage("6:");
	Show_Str(10,30,BLUE,YELLOW,"16X16:",16,0);
	Show_Str(10,50,BLUE,YELLOW,"16X16:Welcome",16,0);
	Show_Str(10,70,BLUE,YELLOW,"24X24:",24,1);
	Show_Str(10,100,BLUE,YELLOW,"32X32:",32,1);
	delay_ms(1200);
}

/*****************************************************************************
 * @name       :void Pic_test(void)
 * @date       :2018-08-09 
 * @function   :picture display test
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Pic_test(void)
{
	DrawTestPage("7:");
	//LCD_Fill(0,20,lcddev.width,lcddev.height-20,WHITE);
	Gui_Drawbmp16(30,30,gImage_qq);
	Show_Str(30+12,75,BLUE,YELLOW,"QQ",16,1);
	Gui_Drawbmp16(90,30,gImage_qq);
	Show_Str(90+12,75,BLUE,YELLOW,"QQ",16,1);
	Gui_Drawbmp16(150,30,gImage_qq);
	Show_Str(150+12,75,BLUE,YELLOW,"QQ",16,1);
	delay_ms(1200);
}

/*****************************************************************************
 * @name       :void Rotate_Test(void)
 * @date       :2018-08-09 
 * @function   :rotate test
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Rotate_Test(void)
{
	u8 i=0;
	u8 *Direction[4]={"Rotation:0","Rotation:90","Rotation:180","Rotation:270"};
	
	for(i=0;i<4;i++)
	{
	LCD_direction(i);
	DrawTestPage("8:");
	Show_Str(20,30,BLUE,YELLOW,Direction[i],16,1);
	Gui_Drawbmp16(30,50,gImage_qq);
	delay_ms(1000);delay_ms(1000);
	}
	LCD_direction(USE_HORIZONTAL);
}

static void print_touch_factor(const char *name, float value)
{
    int32_t scaled;
    uint32_t magnitude;

    scaled = (int32_t)(value * 100000000.0f +
                       (value >= 0.0f ? 0.5f : -0.5f));
    magnitude = scaled < 0
                    ? (uint32_t)(-(int64_t)scaled)
                    : (uint32_t)scaled;
    printf("#define %s (%s%lu.%08luf)\r\n", name,
           scaled < 0 ? "-" : "",
           (unsigned long)(magnitude / 100000000UL),
           (unsigned long)(magnitude % 100000000UL));
}

/*****************************************************************************
 * @name       :void Touch_Test(void)
 * @function   :Run four-point touch calibration and print fixed parameters
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void Touch_Test(void)
{
    uint16_t release_wait_count = 0;
    float saved_xfac = tp_dev.xfac;
    float saved_yfac = tp_dev.yfac;
    short saved_xoff = tp_dev.xoff;
    short saved_yoff = tp_dev.yoff;
    u8 saved_touchtype = tp_dev.touchtype;
    u8 saved_cmd_rdx = CMD_RDX;
    u8 saved_cmd_rdy = CMD_RDY;

    TP_Init();
    while (tp_dev.scan(1) != 0U && release_wait_count < 200U) {
        delay_ms(10);
        release_wait_count++;
    }
    tp_dev.sta = 0;

    TP_Adjust();
    if (tp_dev.xfac != 0.0f && tp_dev.yfac != 0.0f) {
        printf("\r\nTouch calibration result (paste into code):\r\n");
        print_touch_factor("TOUCH_CAL_XFAC", tp_dev.xfac);
        print_touch_factor("TOUCH_CAL_YFAC", tp_dev.yfac);
        printf("#define TOUCH_CAL_XOFF (%d)\r\n", tp_dev.xoff);
        printf("#define TOUCH_CAL_YOFF (%d)\r\n", tp_dev.yoff);
        printf("#define TOUCH_CAL_TOUCHTYPE (%uU)\r\n",
               tp_dev.touchtype);
        printf("#define TOUCH_CAL_CMD_RDX (0x%02XU)\r\n", CMD_RDX);
        printf("#define TOUCH_CAL_CMD_RDY (0x%02XU)\r\n\r\n", CMD_RDY);
    } else {
        printf("\r\nTouch calibration timed out; values were not changed.\r\n");
    }

    tp_dev.xfac = saved_xfac;
    tp_dev.yfac = saved_yfac;
    tp_dev.xoff = saved_xoff;
    tp_dev.yoff = saved_yoff;
    tp_dev.touchtype = saved_touchtype;
    CMD_RDX = saved_cmd_rdx;
    CMD_RDY = saved_cmd_rdy;
    tp_dev.sta = 0;
}




