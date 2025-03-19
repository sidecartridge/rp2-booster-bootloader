/**
 * File: display_fabric.c
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Functions for display in fabric mode.
 */

#include "display_fabric.h"

// Static assert to ensure buffer size fits within uint32_t
_Static_assert(DISPLAY_BUFFER_SIZE <= UINT32_MAX,
               "Buffer size exceeds allowed limits");

static char url_ip[128] = {0};
static char url_host[128] = {0};

// Draw graphics into the buffer
void draw_connection_step1_scr(const uint8_t qrcode_wifi[], const char *ssid,
                               const char *password, const char *auth,
                               uint8_t wifi_status) {
  // // Clear the buffer first
  u8g2_ClearBuffer(display_get_u8g2_ref());

  display_draw_qr(qrcode_wifi, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT, 0, 0,
                  DISPLAY_QR_BORDER, DISPLAY_QR_SCALE);

  // Connection info
  char ssid_str[40] = {0};
  char auth_str[40] = {0};
  char pass_str[40] = {0};
  snprintf(ssid_str, sizeof(ssid_str), "SSID: %s", ssid);
  snprintf(pass_str, sizeof(pass_str), "Pass: %s", password);

  // Use Amstrad CPC font!
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_amstrad_cpc_extended_8f);
  u8g2_DrawStr(display_get_u8g2_ref(),
               LEFT_PADDING_FOR_CENTER(ssid_str, DISPLAY_TILES_WIDTH / 2) * 8,
               32, ssid_str);
  if (strcmp(auth, "OPEN") != 0) {
    u8g2_DrawStr(display_get_u8g2_ref(),
                 LEFT_PADDING_FOR_CENTER(pass_str, DISPLAY_TILES_WIDTH / 2) * 8,
                 40, pass_str);
  }

  // Wifi status
  display_fabric_wifi_change_status(wifi_status);

  // Steps info
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
  u8g2_DrawStr(display_get_u8g2_ref(),
               LEFT_PADDING_FOR_CENTER(DISPLAY_CONNECTION_STEP1_MSG, 34) * 5, 8,
               DISPLAY_CONNECTION_STEP1_MSG);

  // Product info
  display_draw_product_info();

  // ByPass message
  u8g2_DrawStr(display_get_u8g2_ref(),
               LEFT_PADDING_FOR_CENTER(DISPLAY_BYPASS_MESSAGE, 68) * 5,
               DISPLAY_HEIGHT - 8, DISPLAY_BYPASS_MESSAGE);

  // Frames
  u8g2_DrawRFrame(display_get_u8g2_ref(), 0, 16, 156, 168, 8);
}

// Draw graphics into the buffer
void draw_connection_step2_scr(const uint8_t qrcode_url[], const char *url1,
                               const char *url2) {
  display_draw_qr(qrcode_url, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT, 160, 0,
                  DISPLAY_QR_BORDER, DISPLAY_QR_SCALE);

  // URL
  u8g2_DrawStr(display_get_u8g2_ref(),
               160 + LEFT_PADDING_FOR_CENTER(url1, DISPLAY_TILES_WIDTH / 2) * 8,
               32, url1);
  u8g2_DrawStr(display_get_u8g2_ref(),
               160 + LEFT_PADDING_FOR_CENTER(url2, DISPLAY_TILES_WIDTH / 2) * 8,
               40, url2);

  // Steps info
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
  u8g2_DrawStr(
      display_get_u8g2_ref(),
      160 + LEFT_PADDING_FOR_CENTER(DISPLAY_CONNECTION_STEP2_MSG, 34) * 5, 8,
      DISPLAY_CONNECTION_STEP2_MSG);

  // Frames
  u8g2_DrawRFrame(display_get_u8g2_ref(), 164, 16, 156, 168, 8);
}

void draw_reset_scr() {
  // Clear the buffer first
  u8g2_ClearBuffer(display_get_u8g2_ref());

  // Use Amstrad CPC font!
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_amstrad_cpc_extended_8f);
  u8g2_DrawStr(
      display_get_u8g2_ref(),
      LEFT_PADDING_FOR_CENTER(DISPLAY_RESET_WAIT_MESSAGE, DISPLAY_TILES_WIDTH) *
          8,
      100, DISPLAY_RESET_WAIT_MESSAGE);
  u8g2_DrawStr(display_get_u8g2_ref(),
               LEFT_PADDING_FOR_CENTER(DISPLAY_RESET_FORCE_MESSAGE,
                                       DISPLAY_TILES_WIDTH) *
                   8,
               108, DISPLAY_RESET_FORCE_MESSAGE);

  // Product info
  display_draw_product_info();
}

// Change the wifi status in the buffer
void display_fabric_wifi_change_status(uint8_t wifi_status) {
  // Use 8x8 font
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_amstrad_cpc_extended_8f);
  // Wifi status
  char wifi_status_str[20] = {0};
  switch (wifi_status) {
    case 0:
      snprintf(wifi_status_str, sizeof(wifi_status_str), "Starting WIFI...");
      break;
    case 1:
      snprintf(wifi_status_str, sizeof(wifi_status_str), "   WIFI Ready   ");
      break;
    case 2:
      snprintf(wifi_status_str, sizeof(wifi_status_str), "   Connected!   ");
      break;
  }
  u8g2_DrawStr(display_get_u8g2_ref(),
               LEFT_PADDING_FOR_CENTER(wifi_status_str, 20) * 8,
               DISPLAY_HEIGHT - 24, wifi_status_str);
}

// Change the portal status in the buffer
void display_fabric_portal_change_status(uint8_t portal_status) {
  // Use 8x8 font
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_amstrad_cpc_extended_8f);
  // Wifi status
  char portal_status_str[20] = {0};
  switch (portal_status) {
    case 0:
      snprintf(portal_status_str, sizeof(portal_status_str),
               "Starting Portal...");
      break;
    case 1:
      snprintf(portal_status_str, sizeof(portal_status_str),
               "   Portal Ready   ");
      break;
    case 2:
      snprintf(portal_status_str, sizeof(portal_status_str),
               "   Connected!   ");
      break;
  }
  u8g2_DrawStr(display_get_u8g2_ref(),
               160 + LEFT_PADDING_FOR_CENTER(portal_status_str, 20) * 8,
               DISPLAY_HEIGHT - 24, portal_status_str);
}

// The main function should be as follows:
void display_fabric_start(const char *ssid, const char *password,
                          const char *auth, const char *url1,
                          const char *url2) {
  // Safely copy url1 and url2 into static buffers
  snprintf(url_ip, sizeof(url_ip), "%s", url1 ? url1 : "");
  snprintf(url_host, sizeof(url_host), "%s", url2 ? url2 : "");

  size_t buffer_size = DISPLAY_BUFFER_SIZE;  // Safe usage
  (void)buffer_size;  // To avoid unused variable warning if not used elsewhere

  // Initialize the u8g2 library for a custom buffer
  display_setup_u8g2();

  // Create the QR codes
  uint8_t qrcode_wifi[DISPLAY_QR_BUFFER_LEN_MAX];
  char qr_text[256];
  snprintf(qr_text, sizeof(qr_text), "WIFI:T:%s;S:%s;P:%s;;", auth, ssid,
           password);  // WiFi information
  DPRINTF("QR WIFI text: %s\n", qr_text);
  display_create_qr(qrcode_wifi, qr_text);

  // Set the flag to NOT-RESET the computer
  SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_NOP);

  draw_connection_step1_scr(qrcode_wifi, ssid, password, auth, 0);
  display_refresh();

  DPRINTF("Exiting fabric display\n");
}

void display_fabric_portal_connection() {
  // Create the QR codes
  uint8_t qrcode_url[DISPLAY_QR_BUFFER_LEN_MAX];
  char qr_text[256];
  snprintf(qr_text, sizeof(qr_text), url_ip);  // URL portal
  DPRINTF("QR URL text: %s\n", qr_text);
  display_create_qr(qrcode_url, qr_text);

  draw_connection_step2_scr(qrcode_url, url_ip, url_host);
  display_refresh();
}

void display_fabric_reset() {
  // Show reset and force message
  draw_reset_scr();
  display_refresh();
}
