#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void dashboard_ui_init(void);

void dashboard_ui_update_time(int hour, int minute, int second);
void dashboard_ui_update_date(int year, int month, int day, int week);
void dashboard_ui_show_message(const char *title, const char *content);
void dashboard_ui_update_kv(const char *kv_text);
void dashboard_ui_update_temp_humi(float temp, float humi);
void dashboard_ui_update_wifi_status(bool connected, const char *ssid, int rssi);
void dashboard_ui_update_mqtt_status(bool connected);
void dashboard_ui_update_battery(int level);
void dashboard_ui_update_traffic_light(int group, int state);
void dashboard_ui_set_provisioning(bool active, const char *ap_ssid, const char *ap_ip);

#ifdef __cplusplus
}
#endif

#endif
