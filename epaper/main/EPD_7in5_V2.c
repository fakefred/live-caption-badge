/*****************************************************************************
* | File      	:	EPD_7in5.c
* | Author      :   Waveshare team
* | Function    :   Electronic paper driver
* | Info        :
*----------------
* |	This version:   V3.0
* | Date        :   2023-12-18
* | Info        :
*****************************************************************************
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "EPD_7in5_V2.h"
#include "DEV_Config.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

static const char *TAG = "EPD";

QueueHandle_t gpio_evt_queue;
void IRAM_ATTR busy_isr_handler(void *arg) {
	int dummy = 1;
	xQueueSendFromISR(gpio_evt_queue, &dummy, NULL);
}


/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_Reset(void) {
	ESP_LOGI(EPD_TAG, "EPD_Reset");
	DEV_Digital_Write(EPD_RST_PIN, 1);
	DEV_Delay_ms(5);
	DEV_Digital_Write(EPD_RST_PIN, 0);
	DEV_Delay_ms(10);
	DEV_Digital_Write(EPD_RST_PIN, 1);
	DEV_Delay_ms(5);
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
static void EPD_SendCommand(UBYTE Reg) {
	DEV_Digital_Write(EPD_DC_PIN, 0);
	spi_write_byte(Reg);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
static void EPD_SendData(UBYTE Data) {
	DEV_Digital_Write(EPD_DC_PIN, 1);
	spi_write_byte(Data);
}

static void EPD_SendData2(UBYTE *pData, UDOUBLE len) {
	DEV_Digital_Write(EPD_DC_PIN, 1);
	spi_write_bytes(pData, len);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
static void EPD_WaitUntilIdle(void) {
	int dummy = 0;
	while (xQueueReceive(gpio_evt_queue, &dummy, pdMS_TO_TICKS(5000)) != pdTRUE) {
		ESP_LOGW(TAG, "EPD_WaitUntilIdle xQueueReceive timeout");
	}
}
/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
static void EPD_7IN5_V2_TurnOnDisplay(void) {
	xQueueReset(gpio_evt_queue);
	EPD_SendCommand(0x12); // DISPLAY REFRESH
	DEV_Delay_ms(10); //!!!The delay here is necessary, 200uS at least!!!
	EPD_WaitUntilIdle();
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
UBYTE EPD_7IN5_V2_Init(void) {	
	static bool first_time = true;

	ESP_LOGI(EPD_TAG, "EPD_Init");
	if (first_time) {
		gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
		gpio_install_isr_service(0);
		gpio_isr_handler_add(EPD_BUSY_PIN, busy_isr_handler, NULL);
	}
	first_time = false;
	
	EPD_Reset();
	EPD_SendCommand(0x01); // POWER SETTING
	EPD_SendData(0x07);
	EPD_SendData(0x07); // VGH=20V,VGL=-20V
	EPD_SendData(0x3f); // VDH=15V
	EPD_SendData(0x3f); // VDL=-15V

	// Enhanced display drive(Add 0x06 command)
	EPD_SendCommand(0x06); // Booster Soft Start
	EPD_SendData(0x17);
	EPD_SendData(0x17);
	EPD_SendData(0x28);
	EPD_SendData(0x17);

	xQueueReset(gpio_evt_queue);
	EPD_SendCommand(0x04); // POWER ON
	DEV_Delay_ms(10);
	EPD_WaitUntilIdle(); // waiting for the electronic paper IC to release
	                     // the idle signal

	EPD_SendCommand(0X00); // PANNEL SETTING
	EPD_SendData(0x1F);    // KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

	EPD_SendCommand(0x61); // tres
	EPD_SendData(0x03);    // source 800
	EPD_SendData(0x20);
	EPD_SendData(0x01); // gate 480
	EPD_SendData(0xE0);

	EPD_SendCommand(0X15);
	EPD_SendData(0x00);

	EPD_SendCommand(0X50); // VCOM AND DATA INTERVAL SETTING
	EPD_SendData(0x10);
	EPD_SendData(0x07);

	EPD_SendCommand(0X60); // TCON SETTING
	EPD_SendData(0x22);

	return 0;
}

UBYTE EPD_7IN5_V2_Init_Fast(void) {
	ESP_LOGI(EPD_TAG, "EPD_Init_Fast");

	EPD_Reset();
	EPD_SendCommand(0X00); // PANNEL SETTING
	EPD_SendData(0x1F);    // KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

	EPD_SendCommand(0X50); // VCOM AND DATA INTERVAL SETTING
	EPD_SendData(0x10);
	EPD_SendData(0x07);

	xQueueReset(gpio_evt_queue);
	EPD_SendCommand(0x04); // POWER ON
	DEV_Delay_ms(10);
	EPD_WaitUntilIdle(); // waiting for the electronic paper IC to release
	                     // the idle signal

	// Enhanced display drive(Add 0x06 command)
	EPD_SendCommand(0x06); // Booster Soft Start
	EPD_SendData(0x27);
	EPD_SendData(0x27);
	EPD_SendData(0x18);
	EPD_SendData(0x17);

	EPD_SendCommand(0xE0);
	EPD_SendData(0x02);
	EPD_SendCommand(0xE5);
	EPD_SendData(0x5A);

	return 0;
}

UBYTE EPD_7IN5_V2_Init_Part(void) {
	ESP_LOGI(EPD_TAG, "EPD_Init_Part");
	
	EPD_Reset();

	EPD_SendCommand(0X00); // PANNEL SETTING
	EPD_SendData(0x1F);    // KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

	xQueueReset(gpio_evt_queue);
	EPD_SendCommand(0x04); // POWER ON
	DEV_Delay_ms(100);
	EPD_WaitUntilIdle(); // waiting for the electronic paper IC to release
	                     // the idle signal

	EPD_SendCommand(0xE0);
	EPD_SendData(0x02);
	EPD_SendCommand(0xE5);
	EPD_SendData(0x6E);

	return 0;
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_7IN5_V2_Clear(void) {
	ESP_LOGI(EPD_TAG, "EPD_Clear");

	UWORD Width, Height;
	Width = (EPD_7IN5_V2_WIDTH % 8 == 0) ? (EPD_7IN5_V2_WIDTH / 8)
	                                     : (EPD_7IN5_V2_WIDTH / 8 + 1);
	Height = EPD_7IN5_V2_HEIGHT;
	UBYTE image[EPD_7IN5_V2_WIDTH / 8] = {0x00};

	UWORD i;
	EPD_SendCommand(0x10);
	for (i = 0; i < Width; i++) {
		image[i] = 0xFF;
	}
	for (i = 0; i < Height; i++) {
		EPD_SendData2(image, Width);
	}

	EPD_SendCommand(0x13);
	for (i = 0; i < Width; i++) {
		image[i] = 0x00;
	}
	for (i = 0; i < Height; i++) {
		EPD_SendData2(image, Width);
	}

	EPD_7IN5_V2_TurnOnDisplay();
}

void EPD_7IN5_V2_ClearBlack(void) {
	ESP_LOGI(EPD_TAG, "EPD_ClearBlack");

	UWORD Width, Height;
	Width = (EPD_7IN5_V2_WIDTH % 8 == 0) ? (EPD_7IN5_V2_WIDTH / 8)
	                                     : (EPD_7IN5_V2_WIDTH / 8 + 1);
	Height = EPD_7IN5_V2_HEIGHT;
	UBYTE image[EPD_7IN5_V2_WIDTH / 8] = {0x00};

	UWORD i;
	EPD_SendCommand(0x10);
	for (i = 0; i < Width; i++) {
		image[i] = 0x00;
	}
	for (i = 0; i < Height; i++) {
		EPD_SendData2(image, Width);
	}

	EPD_SendCommand(0x13);
	for (i = 0; i < Width; i++) {
		image[i] = 0xFF;
	}
	for (i = 0; i < Height; i++) {
		EPD_SendData2(image, Width);
	}

	EPD_7IN5_V2_TurnOnDisplay();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:	blackimage: full-screen image buffer
******************************************************************************/
void EPD_7IN5_V2_Display(UBYTE *blackimage) {
	ESP_LOGI(EPD_TAG, "EPD_Display");

	UDOUBLE Width, Height;
	Width = (EPD_7IN5_V2_WIDTH % 8 == 0) ? (EPD_7IN5_V2_WIDTH / 8)
	                                     : (EPD_7IN5_V2_WIDTH / 8 + 1);
	Height = EPD_7IN5_V2_HEIGHT;

	EPD_SendCommand(0x10);
	for (UDOUBLE j = 0; j < Height; j++) {
		EPD_SendData2((UBYTE *)(blackimage + j * Width), Width);
	}

	EPD_SendCommand(0x13);
	for (UDOUBLE j = 0; j < Height; j++) {
		for (UDOUBLE i = 0; i < Width; i++) {
			blackimage[i + j * Width] = ~blackimage[i + j * Width];
		}
	}
	for (UDOUBLE j = 0; j < Height; j++) {
		EPD_SendData2((UBYTE *)(blackimage + j * Width), Width);
	}
	EPD_7IN5_V2_TurnOnDisplay();
}

/******************************************************************************
function :	Sends rectangular area from image buffer in RAM to e-Paper and
		displays. Coordinates are start-inclusive, end-exclusive.
parameter:	blackimage: full-screen image buffer
******************************************************************************/
void EPD_7IN5_V2_Display_Part(UBYTE *blackimage, UDOUBLE x_start,
                              UDOUBLE y_start, UDOUBLE x_end, UDOUBLE y_end) {
	ESP_LOGI(EPD_TAG, "EPD_Display_Part");

	if (x_start >= x_end) {
		ESP_LOGE(EPD_TAG, "EPD_Display_Part: x_start (%u) must be less than x_end (%u)",
		         (unsigned int)x_start, (unsigned int)x_end);
		return;
	}

	if (y_start >= y_end) {
		ESP_LOGE(EPD_TAG, "EPD_Display_Part: y_start (%u) must be less than y_end (%u)",
		         (unsigned int)y_start, (unsigned int)y_end);
		return;
	}

	if (x_end > EPD_7IN5_V2_WIDTH || y_end > EPD_7IN5_V2_HEIGHT) {
		ESP_LOGE(EPD_TAG, "EPD_Display_Part: (x_end=%u, y_end=%u) exceeds display range",
		         (unsigned int)x_end, (unsigned int)y_end);
		return;
	}

	// Because SPI transmissions are in bytes, the screen only accepts x coordinates that are
	// multiples of 8.
	x_start = x_start / 8 * 8;
	x_end = (x_end + 7) / 8 * 8;

	ESP_LOGI(EPD_TAG, "EPD_Display_Part: Drawing (%u, %u) -- (%u, %u)", (unsigned int)x_start,
	         (unsigned int)y_start, (unsigned int)x_end, (unsigned int)y_end);

	EPD_SendCommand(0x50);
	EPD_SendData(0xA9);
	EPD_SendData(0x07);

	EPD_SendCommand(0x91); // enter partial mode
	EPD_SendCommand(0x90); // resolution setting

	EPD_SendData(x_start / 256);
	EPD_SendData(x_start % 256);

	// Unlike in software, the range that the hardware accepts is inclusive on
	// both ends. Therefore, the end coordinate must be a multiple of 8 minus 1.
	EPD_SendData((x_end - 1) / 256);
	EPD_SendData((x_end - 1) % 256);

	EPD_SendData(y_start / 256);
	EPD_SendData(y_start % 256);

	EPD_SendData((y_end - 1) / 256);
	EPD_SendData((y_end - 1) % 256);

	EPD_SendData(0x01);

	EPD_SendCommand(0x13);

	UDOUBLE rect_width_bytes = (x_end - x_start + 1) / 8;
	for (UDOUBLE y = y_start; y < y_end; y++) {
		EPD_SendData2((UBYTE *)(blackimage + y * EPD_7IN5_V2_WIDTH_BYTES + x_start / 8),
		              rect_width_bytes);
	}
	EPD_7IN5_V2_TurnOnDisplay();
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_7IN5_V2_Sleep(void) {
	ESP_LOGI(EPD_TAG, "EPD_Sleep");

	EPD_SendCommand(0X02); // power off
	EPD_WaitUntilIdle();
	EPD_SendCommand(0X07); // deep sleep
	EPD_SendData(0xA5);
}
