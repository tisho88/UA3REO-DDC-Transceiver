#include "settings.h"
#if (defined(LCD_RA8875))

//Header files
#include "lcd_driver.h"
#include "main.h"
#include "fonts.h"
#include "functions.h"
#include "trx_manager.h"

static bool activeWindowIsFullscreen = true;
ITCM static void LCDDriver_waitBusy(void);
ITCM static void LCDDriver_waitBTE(void);

//***** Functions prototypes *****//
ITCM static void LCDDriver_setActiveWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

//Write command to LCD
ITCM inline void LCDDriver_SendCommand(uint16_t com)
{
  *(__IO uint16_t *)((uint32_t)(LCD_FSMC_COMM_ADDR)) = com;
}

//Write data to LCD
ITCM inline void LCDDriver_SendData(uint16_t data)
{
  *(__IO uint16_t *)((uint32_t)(LCD_FSMC_DATA_ADDR)) = data;
}

//Write pair command-data
ITCM inline void LCDDriver_writeReg(uint16_t reg, uint16_t val)
{
  LCDDriver_SendCommand(reg);
  LCDDriver_SendData(val);
}

//Read command from LCD
ITCM inline uint16_t LCDDriver_ReadStatus(void)
{
  return *(__IO uint16_t *)((uint32_t)(LCD_FSMC_COMM_ADDR));
}
//Read data from LCD
ITCM inline uint16_t LCDDriver_ReadData(void)
{
  return *(__IO uint16_t *)((uint32_t)(LCD_FSMC_DATA_ADDR));
}

//Read Register
ITCM inline uint16_t LCDDriver_readReg(uint16_t reg)
{
  LCDDriver_SendCommand(reg);
  return LCDDriver_ReadData();
}

//Initialise function
void LCDDriver_Init(void)
{
  //PLL Init
  LCDDriver_writeReg(LCD_RA8875_PLLC1, LCD_RA8875_PLLC1_PLLDIV1 + 11);
  HAL_Delay(1);
  LCDDriver_writeReg(LCD_RA8875_PLLC2, LCD_RA8875_PLLC2_DIV4);
  HAL_Delay(1);

  //Software reset
  LCDDriver_writeReg(LCD_RA8875_PWRR, LCD_RA8875_PWRR_SOFTRESET);
  HAL_Delay(1);
  LCDDriver_writeReg(LCD_RA8875_PWRR, LCD_RA8875_PWRR_NORMAL);
  HAL_Delay(1);

  //select color and bus mode
  LCDDriver_writeReg(LCD_RA8875_SYSR, LCD_RA8875_SYSR_16BPP | LCD_RA8875_SYSR_MCU16);

  // Timing values
  uint8_t pixclk = LCD_RA8875_PCSR_PDATL | LCD_RA8875_PCSR_2CLK;
  uint8_t hsync_start = 32;
  uint8_t hsync_pw = 96;
  uint8_t hsync_finetune = 0;
  uint8_t hsync_nondisp = 26;
  uint8_t vsync_pw = 2;
  uint16_t vsync_nondisp = 32;
  uint16_t vsync_start = 23;

  LCDDriver_writeReg(LCD_RA8875_PCSR, pixclk);
  HAL_Delay(1);

  // Horizontal settings registers
  LCDDriver_writeReg(LCD_RA8875_HDWR, (LCD_WIDTH / 8) - 1); // H width: (HDWR + 1) * 8 = 480
  LCDDriver_writeReg(LCD_RA8875_HNDFTR, LCD_RA8875_HNDFTR_DE_HIGH + hsync_finetune);
  LCDDriver_writeReg(LCD_RA8875_HNDR, (hsync_nondisp - hsync_finetune - 2) / 8); // H non-display: HNDR * 8 + HNDFTR + 2 = 10
  LCDDriver_writeReg(LCD_RA8875_HSTR, hsync_start / 8 - 1);                      // Hsync start: (HSTR + 1)*8
  LCDDriver_writeReg(LCD_RA8875_HPWR, LCD_RA8875_HPWR_LOW + (hsync_pw / 8 - 1)); // HSync pulse width = (HPWR+1) * 8

  // Vertical settings registers
  LCDDriver_writeReg(LCD_RA8875_VDHR0, (uint16_t)(LCD_HEIGHT - 1) & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_VDHR1, (uint16_t)(LCD_HEIGHT - 1) >> 8);
  LCDDriver_writeReg(LCD_RA8875_VNDR0, vsync_nondisp - 1); // V non-display period = VNDR + 1
  LCDDriver_writeReg(LCD_RA8875_VNDR1, vsync_nondisp >> 8);
  LCDDriver_writeReg(LCD_RA8875_VSTR0, vsync_start - 1); // Vsync start position = VSTR + 1
  LCDDriver_writeReg(LCD_RA8875_VSTR1, vsync_start >> 8);
  LCDDriver_writeReg(LCD_RA8875_VPWR, LCD_RA8875_VPWR_LOW + vsync_pw - 1); // Vsync pulse width = VPWR + 1

  // Set active window
  LCDDriver_setActiveWindow(0, 0, (LCD_WIDTH - 1), (LCD_HEIGHT - 1));

  //PWM setting
  LCDDriver_writeReg(LCD_RA8875_P1CR, LCD_RA8875_P1CR_ENABLE | (LCD_RA8875_PWM_CLK_DIV1024 & 0xF));
  LCDDriver_writeReg(LCD_RA8875_P1DCR, 0x05);

  //clear screen
  LCDDriver_Fill(COLOR_WHITE);

  //display ON
  LCDDriver_writeReg(LCD_RA8875_PWRR, LCD_RA8875_PWRR_NORMAL | LCD_RA8875_PWRR_DISPON);
  HAL_Delay(10);
}

//Set screen rotation
void LCDDriver_setRotation(uint8_t rotate){

}

//Set cursor position
ITCM inline void LCDDriver_SetCursorAreaPosition(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  activeWindowIsFullscreen = false;
  LCDDriver_waitBusy();
  LCDDriver_setActiveWindow(x1, y1, x2, y2);
  LCDDriver_writeReg(LCD_RA8875_CURH0, x1);
  LCDDriver_writeReg(LCD_RA8875_CURH1, x1 >> 8);
  LCDDriver_writeReg(LCD_RA8875_CURV0, y1);
  LCDDriver_writeReg(LCD_RA8875_CURV1, y1 >> 8);
  LCDDriver_SendCommand(LCD_RA8875_MRWC);
}

ITCM static inline void LCDDriver_SetCursorPosition(uint16_t x, uint16_t y)
{
  if (!activeWindowIsFullscreen)
  {
    activeWindowIsFullscreen = true;
    LCDDriver_setActiveWindow(0, 0, (LCD_WIDTH - 1), (LCD_HEIGHT - 1));
  }
  LCDDriver_waitBusy();
  LCDDriver_writeReg(LCD_RA8875_CURH0, x);
  LCDDriver_writeReg(LCD_RA8875_CURH1, x >> 8);
  LCDDriver_writeReg(LCD_RA8875_CURV0, y);
  LCDDriver_writeReg(LCD_RA8875_CURV1, y >> 8);
  LCDDriver_SendCommand(LCD_RA8875_MRWC);
}

ITCM static void LCDDriver_setActiveWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  LCDDriver_waitBusy();
  // Set active window X
  LCDDriver_writeReg(LCD_RA8875_HSAW0, x1); // horizontal start point
  LCDDriver_writeReg(LCD_RA8875_HSAW1, x1 >> 8);
  LCDDriver_writeReg(LCD_RA8875_HEAW0, x2); // horizontal end point
  LCDDriver_writeReg(LCD_RA8875_HEAW1, x2 >> 8);

  // Set active window Y
  LCDDriver_writeReg(LCD_RA8875_VSAW0, y1); // vertical start point
  LCDDriver_writeReg(LCD_RA8875_VSAW1, y1 >> 8);
  LCDDriver_writeReg(LCD_RA8875_VEAW0, y2); // vertical end point
  LCDDriver_writeReg(LCD_RA8875_VEAW1, y2 >> 8);
}

ITCM static void LCDDriver_waitBusy(void)
{
  while ((LCDDriver_ReadStatus() & 0x80) == 0x80)
    __asm("nop");
}

ITCM static void LCDDriver_waitBTE(void)
{
  while ((LCDDriver_ReadStatus() & 0x40) == 0x40)
    if (NVIC_GetPriority(((int32_t)__get_IPSR()) - 16) == 8)
      CPULOAD_GoToSleepMode();
    else
      __asm("nop");
}

ITCM static bool LCDDriver_waitPoll(uint16_t regname, uint8_t waitflag)
{
  uint32_t t = 300000;
  while (t > 0)
  {
    t--;
    uint16_t temp = LCDDriver_readReg(regname);
    if (!(temp & waitflag))
      return true;
  }
  return false;
}

//Write data to a single pixel
ITCM void LCDDriver_drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  LCDDriver_SetCursorPosition(x, y);
  LCDDriver_SendData(color);
}

//Fill the entire screen with a background color
ITCM void LCDDriver_Fill(uint16_t color)
{
  LCDDriver_Fill_RectXY(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

//Rectangle drawing functions
ITCM void LCDDriver_Fill_RectXY(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  LCDDriver_waitBusy();
  if (!activeWindowIsFullscreen)
  {
    activeWindowIsFullscreen = true;
    LCDDriver_setActiveWindow(0, 0, (LCD_WIDTH - 1), (LCD_HEIGHT - 1));
  }

  /* Set X */
  LCDDriver_SendCommand(LCD_RA8875_DLHSR0);
  LCDDriver_SendData(x0);
  LCDDriver_SendCommand(LCD_RA8875_DLHSR1);
  LCDDriver_SendData(x0 >> 8);

  /* Set Y */
  LCDDriver_SendCommand(LCD_RA8875_DLVSR0);
  LCDDriver_SendData(y0);
  LCDDriver_SendCommand(LCD_RA8875_DLVSR1);
  LCDDriver_SendData(y0 >> 8);

  /* Set X1 */
  LCDDriver_SendCommand(LCD_RA8875_DLHER0);
  LCDDriver_SendData(x1);
  LCDDriver_SendCommand(LCD_RA8875_DLHER1);
  LCDDriver_SendData(x1 >> 8);

  /* Set Y1 */
  LCDDriver_SendCommand(LCD_RA8875_DLVER0);
  LCDDriver_SendData(y1);
  LCDDriver_SendCommand(LCD_RA8875_DLVER1);
  LCDDriver_SendData(y1 >> 8);

  /* Set Color */
  LCDDriver_SendCommand(LCD_RA8875_FGCR0);
  LCDDriver_SendData((color & 0xf800) >> 11);
  LCDDriver_SendCommand(LCD_RA8875_FGCR1);
  LCDDriver_SendData((color & 0x07e0) >> 5);
  LCDDriver_SendCommand(LCD_RA8875_FGCR2);
  LCDDriver_SendData((color & 0x001f));

  /* Draw! */
  LCDDriver_SendCommand(LCD_RA8875_DCR);
  LCDDriver_SendData(0xB0); //filled rect

  /* Wait for the command to finish */
  LCDDriver_waitPoll(LCD_RA8875_DCR, LCD_RA8875_DCR_LINESQUTRI_STATUS);
}

ITCM void LCDDriver_Fill_RectWH(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  LCDDriver_Fill_RectXY(x, y, x + w, y + h, color);
}

//Line drawing functions
ITCM void LCDDriver_drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  LCDDriver_waitBusy();
  if (!activeWindowIsFullscreen)
  {
    activeWindowIsFullscreen = true;
    LCDDriver_setActiveWindow(0, 0, (LCD_WIDTH - 1), (LCD_HEIGHT - 1));
  }

  /* Set X */
  LCDDriver_SendCommand(LCD_RA8875_DLHSR0);
  LCDDriver_SendData(x0);
  LCDDriver_SendCommand(LCD_RA8875_DLHSR1);
  LCDDriver_SendData(x0 >> 8);

  /* Set Y */
  LCDDriver_SendCommand(LCD_RA8875_DLVSR0);
  LCDDriver_SendData(y0);
  LCDDriver_SendCommand(LCD_RA8875_DLVSR1);
  LCDDriver_SendData(y0 >> 8);

  /* Set X1 */
  LCDDriver_SendCommand(LCD_RA8875_DLHER0);
  LCDDriver_SendData(x1);
  LCDDriver_SendCommand(LCD_RA8875_DLHER1);
  LCDDriver_SendData(x1 >> 8);

  /* Set Y1 */
  LCDDriver_SendCommand(LCD_RA8875_DLVER0);
  LCDDriver_SendData(y1);
  LCDDriver_SendCommand(LCD_RA8875_DLVER1);
  LCDDriver_SendData(y1 >> 8);

  /* Set Color */
  LCDDriver_SendCommand(LCD_RA8875_FGCR0);
  LCDDriver_SendData((color & 0xf800) >> 11);
  LCDDriver_SendCommand(LCD_RA8875_FGCR1);
  LCDDriver_SendData((color & 0x07e0) >> 5);
  LCDDriver_SendCommand(LCD_RA8875_FGCR2);
  LCDDriver_SendData((color & 0x001f));

  /* Draw! */
  LCDDriver_SendCommand(LCD_RA8875_DCR);
  LCDDriver_SendData(0x80); //line

  /* Wait for the command to finish */
  LCDDriver_waitPoll(LCD_RA8875_DCR, LCD_RA8875_DCR_LINESQUTRI_STATUS);
}

ITCM void LCDDriver_drawFastHLine(uint16_t x, uint16_t y, int16_t w, uint16_t color)
{
  LCDDriver_drawLine(x, y, x + w, y, color);
}

ITCM void LCDDriver_drawFastVLine(uint16_t x, uint16_t y, int16_t h, uint16_t color)
{
  LCDDriver_drawLine(x, y, x, y + h, color);
}

ITCM void LCDDriver_drawRectXY(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  LCDDriver_waitBusy();
  if (!activeWindowIsFullscreen)
  {
    activeWindowIsFullscreen = true;
    LCDDriver_setActiveWindow(0, 0, (LCD_WIDTH - 1), (LCD_HEIGHT - 1));
  }

  LCDDriver_SendCommand(LCD_RA8875_DLHSR0);
  LCDDriver_SendData(x0);
  LCDDriver_SendCommand(LCD_RA8875_DLHSR1);
  LCDDriver_SendData(x0 >> 8);

  /* Set Y */
  LCDDriver_SendCommand(LCD_RA8875_DLVSR0);
  LCDDriver_SendData(y0);
  LCDDriver_SendCommand(LCD_RA8875_DLVSR1);
  LCDDriver_SendData(y0 >> 8);

  /* Set X1 */
  LCDDriver_SendCommand(LCD_RA8875_DLHER0);
  LCDDriver_SendData(x1);
  LCDDriver_SendCommand(LCD_RA8875_DLHER1);
  LCDDriver_SendData(x1 >> 8);

  /* Set Y1 */
  LCDDriver_SendCommand(LCD_RA8875_DLVER0);
  LCDDriver_SendData(y1);
  LCDDriver_SendCommand(LCD_RA8875_DLVER1);
  LCDDriver_SendData(y1 >> 8);

  /* Set Color */
  LCDDriver_SendCommand(LCD_RA8875_FGCR0);
  LCDDriver_SendData((color & 0xf800) >> 11);
  LCDDriver_SendCommand(LCD_RA8875_FGCR1);
  LCDDriver_SendData((color & 0x07e0) >> 5);
  LCDDriver_SendCommand(LCD_RA8875_FGCR2);
  LCDDriver_SendData((color & 0x001f));

  /* Draw! */
  LCDDriver_SendCommand(LCD_RA8875_DCR);
  LCDDriver_SendData(0x90); //not filled rect

  /* Wait for the command to finish */
  LCDDriver_waitPoll(LCD_RA8875_DCR, LCD_RA8875_DCR_LINESQUTRI_STATUS);
}

ITCM void LCDDriver_BTE_copyArea(uint16_t sx, uint16_t sy, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, bool fromEnd)
{
  if (fromEnd)
  {
    sx = sx + w;
    sy = sy + h - 1;
    dx = dx + w;
    dy = dy + h - 1;
  }

  LCDDriver_waitBusy();

  //1. Setting source layer and address REG[54h], [55h], [56h], [57h]
  LCDDriver_writeReg(LCD_RA8875_BTE_HSBE0, sx & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_HSBE1, (sx >> 8) & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_VSBE0, sy & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_VSBE1, (sy >> 8) & 0xFF);

  //2. Setting destination layer and address REG[58h], [59h], [5Ah], [5Bh]
  LCDDriver_writeReg(LCD_RA8875_BTE_HDBE0, dx & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_HDBE1, (dx >> 8) & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_VDBE0, dy & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_VDBE1, (dy >> 8) & 0xFF);

  //3. Setting BTE width and height REG[5Ch], [5Dh], [5Eh], [5Fh]
  LCDDriver_writeReg(LCD_RA8875_BTE_BEWR0, w & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_BEWR1, (w >> 8) & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_BEHR0, h & 0xFF);
  LCDDriver_writeReg(LCD_RA8875_BTE_BEHR1, (h >> 8) & 0xFF);

  //4. Setting BTE operation and ROP function REG[51h] Bit[3:0] = 2h
  if (fromEnd)
    LCDDriver_writeReg(LCD_RA8875_BTE_BECR1, LCD_RA8875_BTE_BECR1_MVN | LCD_RA8875_BTE_BECR1_DS);
  else
    LCDDriver_writeReg(LCD_RA8875_BTE_BECR1, LCD_RA8875_BTE_BECR1_MVP | LCD_RA8875_BTE_BECR1_DS);

  //5. Enable BTE function REG[50h] Bit7 = 1
  LCDDriver_writeReg(LCD_RA8875_BTE_BECR0, LCDDriver_readReg(LCD_RA8875_BTE_BECR0) | LCD_RA8875_BTE_BECR0_BTEON);

  //6. Check STSR REG Bit6 check 2D final
  LCDDriver_waitBTE();
}

#endif
