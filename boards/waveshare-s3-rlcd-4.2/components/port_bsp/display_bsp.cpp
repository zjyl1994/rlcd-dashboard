#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "display_bsp.h"

DisplayPort::DisplayPort(int mosi, int scl, int dc, int cs, int rst, int width, int height, spi_host_device_t spihost) : 
mosi_(mosi), 
scl_(scl), 
dc_(dc), 
cs_(cs), 
rst_(rst), 
width_(width), 
height_(height),
spihost_(spihost)
{
}

bool DisplayPort::Init() {
    if (initialized_) {
        return true;
    }

    esp_err_t        ret;
    spi_bus_config_t buscfg   = {};
    int              transfer = width_ * height_;
    buscfg.miso_io_num                   = -1;
    buscfg.mosi_io_num                   = mosi_;
    buscfg.sclk_io_num                   = scl_;
    buscfg.quadwp_io_num                 = -1;
    buscfg.quadhd_io_num                 = -1;
    buscfg.max_transfer_sz               = transfer;
    ret                                  = spi_bus_initialize(spihost_, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("Display", "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num = dc_;
    io_config.cs_gpio_num = cs_;
    io_config.pclk_hz = 10 * 1000 * 1000;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spihost_, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("Display", "LCD panel IO init failed: %s", esp_err_to_name(ret));
        spi_bus_free(spihost_);
        return false;
    }

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << rst_);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        ESP_LOGE("Display", "Reset GPIO init failed: %s", esp_err_to_name(ret));
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
        spi_bus_free(spihost_);
        return false;
    }

    Set_ResetIOLevel(1);

    DisplayLen                = transfer >> 3; //(1byte 8ipex)
    DispBuffer                = (uint8_t *) heap_caps_malloc(DisplayLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (DispBuffer == NULL) {
        ESP_LOGE("Display", "Display buffer allocation failed: %d bytes", DisplayLen);
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
        spi_bus_free(spihost_);
        return false;
    }

#if (AlgorithmOptimization == 3)
	PixelIndexLUT = (uint16_t (*)[300])heap_caps_malloc(transfer * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	PixelBitLUT   = (uint8_t (*)[300])heap_caps_malloc(transfer * sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (PixelIndexLUT == NULL || PixelBitLUT == NULL) {
        ESP_LOGE("Display", "Pixel LUT allocation failed");
        free(PixelIndexLUT);
        free(PixelBitLUT);
        PixelIndexLUT = NULL;
        PixelBitLUT = NULL;
        free(DispBuffer);
        DispBuffer = NULL;
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
        spi_bus_free(spihost_);
        return false;
    }
    if(width_ == 400) {
        InitLandscapeLUT();
    } else {
        InitPortraitLUT();
    }
#endif
    initialized_ = true;
    return true;
}

DisplayPort::~DisplayPort() {
}

static inline uint8_t get_i1_bit(const uint8_t *row, int x)
{
    return (row[x >> 3] >> (7 - (x & 7))) & 0x01;
}

void DisplayPort::RLCD_Init() {
    RLCD_Reset();

    RLCD_SendCommand(0xD6);  // NVM Load Control
	RLCD_SendData(0x17);
	RLCD_SendData(0x02);

	RLCD_SendCommand(0xD1); //Booster Enable
	RLCD_SendData(0x01);

	RLCD_SendCommand(0xC0); //Gate Voltage Control
	RLCD_SendData(0x11);   
	RLCD_SendData(0x04);   

	RLCD_SendCommand(0xC1); //VSHP Setting
	RLCD_SendData(0x69);
	RLCD_SendData(0x69);
	RLCD_SendData(0x69);
	RLCD_SendData(0x69);

	RLCD_SendCommand(0xC2);
	RLCD_SendData(0x19);
	RLCD_SendData(0x19);
	RLCD_SendData(0x19);
	RLCD_SendData(0x19);

	RLCD_SendCommand(0xC4);
	RLCD_SendData(0x4B);
	RLCD_SendData(0x4B);
	RLCD_SendData(0x4B);
	RLCD_SendData(0x4B);

	RLCD_SendCommand(0xC5);
	RLCD_SendData(0x19);
	RLCD_SendData(0x19);
	RLCD_SendData(0x19);
	RLCD_SendData(0x19);

	RLCD_SendCommand(0xD8);
	RLCD_SendData(0x80);
	RLCD_SendData(0xE9);

	RLCD_SendCommand(0xB2);
	RLCD_SendData(0x02);

	RLCD_SendCommand(0xB3);
	RLCD_SendData(0xE5);
	RLCD_SendData(0xF6);
	RLCD_SendData(0x05);
	RLCD_SendData(0x46);
	RLCD_SendData(0x77);
	RLCD_SendData(0x77);
	RLCD_SendData(0x77);
	RLCD_SendData(0x77);
	RLCD_SendData(0x76);
	RLCD_SendData(0x45);

	RLCD_SendCommand(0xB4);
	RLCD_SendData(0x05);
	RLCD_SendData(0x46);
	RLCD_SendData(0x77);
	RLCD_SendData(0x77);
	RLCD_SendData(0x77);
	RLCD_SendData(0x77);
	RLCD_SendData(0x76);
	RLCD_SendData(0x45);

	RLCD_SendCommand(0x62);
	RLCD_SendData(0x32);
	RLCD_SendData(0x03);
	RLCD_SendData(0x1F);

	RLCD_SendCommand(0xB7);
	RLCD_SendData(0x13);

	RLCD_SendCommand(0xB0);
	RLCD_SendData(0x64);

	RLCD_SendCommand(0x11); 
	vTaskDelay(pdMS_TO_TICKS(200));     
	RLCD_SendCommand(0xC9);
	RLCD_SendData(0x00);

	RLCD_SendCommand(0x36);
	RLCD_SendData(0x48); 

	RLCD_SendCommand(0x3A);
	RLCD_SendData(0x11); 

	RLCD_SendCommand(0xB9);
	RLCD_SendData(0x20);

	RLCD_SendCommand(0xB8);
	RLCD_SendData(0x29);

	RLCD_SendCommand(0x21);

	RLCD_SendCommand(0x2A); 
	RLCD_SendData(0x12);
	RLCD_SendData(0x2A);

	RLCD_SendCommand(0x2B); 
	RLCD_SendData(0x00);
	RLCD_SendData(0xC7);

	RLCD_SendCommand(0x35);
	RLCD_SendData(0x00);

	RLCD_SendCommand(0xD0);
	RLCD_SendData(0xFF);

	RLCD_SendCommand(0x38);
	RLCD_SendCommand(0x29);

    RLCD_ColorClear(ColorWhite);
}

void DisplayPort::RLCD_ColorClear(uint8_t color) {
    memset(DispBuffer, color, DisplayLen);
}

void DisplayPort::RLCD_Display() {
    RLCD_SendCommand(0x2A);     // Column Address Set
    	RLCD_SendData(0x12);
    	RLCD_SendData(0x2A);

  	RLCD_SendCommand(0x2B);     // Page Address Set
  	RLCD_SendData(0x00);
  	RLCD_SendData(0xC7);

  	RLCD_SendCommand(0x2c);     // Page Address Set

	RLCD_Sendbuffera(DispBuffer,DisplayLen);
}

void DisplayPort::RLCD_BlitLVGLI1Full(const uint8_t *src, uint32_t stride) {
    if (src == NULL) {
        return;
    }

    if (width_ != 400 || height_ != 300) {
        RLCD_BlitLVGLI1Area(0, 0, width_ - 1, height_ - 1, src, stride);
        return;
    }

    const int grouped_rows = height_ >> 2;

    for (int byte_x = 0; byte_x < (width_ >> 1); byte_x++) {
        const int src_x0 = byte_x << 1;
        const int src_x1 = src_x0 + 1;
        const int dest_base = byte_x * grouped_rows;

        for (int block_y = 0; block_y < grouped_rows; block_y++) {
            const int inv_base = block_y << 2;
            const uint8_t *row0 = src + (size_t)(height_ - 1 - inv_base) * stride;
            const uint8_t *row1 = src + (size_t)(height_ - 2 - inv_base) * stride;
            const uint8_t *row2 = src + (size_t)(height_ - 3 - inv_base) * stride;
            const uint8_t *row3 = src + (size_t)(height_ - 4 - inv_base) * stride;

            DispBuffer[dest_base + block_y] =
                (get_i1_bit(row0, src_x0) << 7) |
                (get_i1_bit(row0, src_x1) << 6) |
                (get_i1_bit(row1, src_x0) << 5) |
                (get_i1_bit(row1, src_x1) << 4) |
                (get_i1_bit(row2, src_x0) << 3) |
                (get_i1_bit(row2, src_x1) << 2) |
                (get_i1_bit(row3, src_x0) << 1) |
                (get_i1_bit(row3, src_x1) << 0);
        }
    }
}

void DisplayPort::RLCD_BlitLVGLI1Area(int x1, int y1, int x2, int y2, const uint8_t *src, uint32_t stride) {
    if (src == NULL) {
        return;
    }

    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x2 >= width_) {
        x2 = width_ - 1;
    }
    if (y2 >= height_) {
        y2 = height_ - 1;
    }

    for (int y = y1; y <= y2; y++) {
        const uint8_t *row = src + (size_t)(y - y1) * stride;

        for (int x = x1; x <= x2; x++) {
            const int rx = x - x1;
            const uint8_t src_bit = get_i1_bit(row, rx);
            const uint32_t idx = PixelIndexLUT[x][y];
            const uint8_t mask = PixelBitLUT[x][y];
            uint8_t *dest = &DispBuffer[idx];

            if (src_bit) {
                *dest |= mask;
            } else {
                *dest &= (uint8_t)~mask;
            }
        }
    }
}

void DisplayPort::RLCD_Reset(void) {
    Set_ResetIOLevel(1);
    vTaskDelay(pdMS_TO_TICKS(50));
    Set_ResetIOLevel(0);
    vTaskDelay(pdMS_TO_TICKS(20));
    Set_ResetIOLevel(1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void DisplayPort::RLCD_SendCommand(uint8_t Reg) {
    esp_err_t ret = esp_lcd_panel_io_tx_param(io_handle, Reg, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE("Display", "LCD command 0x%02x failed: %s", Reg, esp_err_to_name(ret));
    }
}

void DisplayPort::RLCD_SendData(uint8_t Data) {
    esp_err_t ret = esp_lcd_panel_io_tx_param(io_handle, -1, &Data, 1);
    if (ret != ESP_OK) {
        ESP_LOGE("Display", "LCD data 0x%02x failed: %s", Data, esp_err_to_name(ret));
    }
}

void DisplayPort::RLCD_Sendbuffera(uint8_t *Data, int len) {
    esp_err_t ret = esp_lcd_panel_io_tx_color(io_handle, -1, Data, len);
    if (ret != ESP_OK) {
        ESP_LOGE("Display", "LCD buffer transfer failed: %s", esp_err_to_name(ret));
    }
}

void DisplayPort::Set_ResetIOLevel(uint8_t level) {
    gpio_set_level((gpio_num_t) rst_, level ? 1 : 0);
}
#if (AlgorithmOptimization != 3)

void DisplayPort::RLCD_SetPortraitPixel(uint16_t x, uint16_t y, uint8_t color) {
    if((x >= width_) || (y >= height_)) {
  	  	ESP_LOGE("Pixel","Beyond the limit : (%d,%d)",x ,y);
        return;
  	}
#if (AlgorithmOptimization == 2)
	const uint16_t W4 = width_ >> 2;  

    uint16_t byte_x = x >> 2;        
    uint16_t byte_y = y >> 1;        

    uint32_t index = byte_y * W4 + byte_x;

    uint8_t local_x = x & 0x03; 
    uint8_t local_y = y & 0x01; 

    uint8_t bit = 7 - ((local_x << 1) | local_y);

    uint8_t mask = 1 << bit;

    if (color)
        DispBuffer[index] |= mask;
    else
        DispBuffer[index] &= ~mask;
#else
    uint16_t byte_x = x / 4;
    uint16_t byte_y = y / 2;

    uint32_t index = byte_y * (width_ / 4) + byte_x;

    uint8_t local_x = x % 4;  
    uint8_t local_y = y % 2;  
    uint8_t bit = 7 - (local_x * 2 + local_y);
    if (color)
        DispBuffer[index] |=  (1 << bit);
    else
        DispBuffer[index] &= ~(1 << bit);
#endif
}

void DisplayPort::RLCD_SetLandscapePixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= width_ || y >= height_)
        return;
#if (AlgorithmOptimization == 2)

	uint16_t inv_y = (height_ - 1 - y);
    const uint16_t H4 = height_ >> 2;  
    uint16_t byte_x = x >> 1;          
    uint16_t block_y = inv_y >> 2;     
    uint32_t index = byte_x * H4 + block_y;
    uint8_t local_x = x & 0x01;        
    uint8_t local_y = inv_y & 0x03;    
    uint8_t bit = 7 - ((local_y << 1) | local_x);
    uint8_t mask = 1 << bit;
    if (color)
        DispBuffer[index] |= mask;
    else
        DispBuffer[index] &= ~mask;
#else
    uint16_t inv_y = height_ - 1 - y;

    uint16_t byte_x  = x / 2;           // 0..199
    uint16_t block_y = inv_y / 4;       // 0..74

    uint32_t index = byte_x * (height_ / 4) + block_y; 

    uint8_t local_x = x % 2;            // 0 or 1
    uint8_t local_y = inv_y % 4;        // 0..3

    uint8_t bit = 7 - (local_y * 2 + local_x);

    if (color)
        DispBuffer[index] |= (1 << bit);
    else
        DispBuffer[index] &= ~(1 << bit);
#endif
}

#endif


#if (AlgorithmOptimization == 3)

void DisplayPort::InitPortraitLUT() {
    uint16_t W4 = width_ >> 2;
    for (uint16_t y = 0; y < height_; y++)
    {
        uint16_t byte_y = y >> 1;
        uint8_t  local_y = y & 1;

        for (uint16_t x = 0; x < width_; x++)
        {
            uint16_t byte_x = x >> 2;
            uint8_t  local_x = x & 3;

            uint32_t index = byte_y * W4 + byte_x;
            uint8_t bit = 7 - ((local_x << 1) | local_y);

            PixelIndexLUT[x][y] = index;
            PixelBitLUT  [x][y] = (1 << bit);
        }
    }
}

void DisplayPort::InitLandscapeLUT() {
    uint16_t H4 = height_ >> 2;

    for (uint16_t y = 0; y < height_; y++)
    {
        uint16_t inv_y = height_ - 1 - y;
        uint16_t block_y = inv_y >> 2;
        uint8_t  local_y  = inv_y & 3;

        for (uint16_t x = 0; x < width_; x++)
        {
            uint16_t byte_x = x >> 1;
            uint8_t  local_x = x & 1;

            uint32_t index = byte_x * H4 + block_y;
            uint8_t bit = 7 - ((local_y << 1) | local_x);

            PixelIndexLUT[x][y] = index;
            PixelBitLUT  [x][y] = (1 << bit);
        }
    }
}

void DisplayPort::RLCD_SetPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= width_ || y >= height_) {
        return;
    }

    uint32_t idx = PixelIndexLUT[x][y];
    uint8_t  mask = PixelBitLUT[x][y];

    uint8_t *p = &DispBuffer[idx];

    if (color)
        *p |= mask;
    else
        *p &= ~mask;
}

#endif
