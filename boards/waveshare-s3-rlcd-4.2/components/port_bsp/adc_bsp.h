#pragma once

#include <esp_adc/adc_oneshot.h>

bool Adc_PortInit(void);
float Adc_GetBatteryVoltage(int *data);
uint8_t Adc_GetBatteryLevel(void);
