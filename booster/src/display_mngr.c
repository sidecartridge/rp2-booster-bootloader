/**
 * File: display_mngr.c
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Functions for display in manager mode.
 */

#include "display_mngr.h"

// Static assert to ensure buffer size fits within uint32_t
_Static_assert(DISPLAY_BUFFER_SIZE <= UINT32_MAX,
               "Buffer size exceeds allowed limits");

static uint8_t qrcode_url[DISPLAY_QR_BUFFER_LEN_MAX] = {0};
static char current_ssid[128] = {0};
static char current_url1[128] = {0};
static char current_url2[128] = {0};
static char current_mac[64] = {0};
static char current_status_details[64] = {0};
static bool current_status_has_details = false;
static uint8_t current_wifi_status = 0;
static uint8_t current_status = 0;

#define DISPLAY_MNGR_CONNECTION_INFO_Y 32
#define DISPLAY_MNGR_CONNECTION_INFO_TOP 24
#define DISPLAY_MNGR_CONNECTION_INFO_HEIGHT 12

static void display_mngr_draw_status(uint8_t status, const char *details);

static void display_mngr_clear_connection_info_line() {
  u8g2_SetDrawColor(display_get_u8g2_ref(), 0);
  u8g2_DrawBox(display_get_u8g2_ref(), 0, DISPLAY_MNGR_CONNECTION_INFO_TOP,
               DISPLAY_WIDTH, DISPLAY_MNGR_CONNECTION_INFO_HEIGHT);
  u8g2_SetDrawColor(display_get_u8g2_ref(), 1);
}

static void display_mngr_draw_centered_text(const char *text, uint8_t y) {
  if (text == NULL) {
    return;
  }

  u8g2_uint_t width = u8g2_GetStrWidth(display_get_u8g2_ref(), text);
  u8g2_uint_t x = (width < DISPLAY_WIDTH) ? (DISPLAY_WIDTH - width) / 2 : 0;
  u8g2_DrawStr(display_get_u8g2_ref(), x, y, text);
}

static void display_mngr_format_ssid_line(char *ssid_str, size_t ssid_str_size,
                                          const char *signal_suffix) {
  size_t visible_ssid_len = strlen(current_ssid);
  const char *suffix = (signal_suffix != NULL) ? signal_suffix : "";

  while (true) {
    if (visible_ssid_len < strlen(current_ssid)) {
      snprintf(ssid_str, ssid_str_size, "SSID: %.*s...%s",
               (int)visible_ssid_len, current_ssid, suffix);
    } else {
      snprintf(ssid_str, ssid_str_size, "SSID: %s%s", current_ssid, suffix);
    }

    if (u8g2_GetStrWidth(display_get_u8g2_ref(), ssid_str) <= DISPLAY_WIDTH ||
        visible_ssid_len == 0) {
      return;
    }

    visible_ssid_len--;
  }
}

static void display_mngr_draw_connection_info(uint8_t wifi_status) {
  char ssid_str[128] = {0};
  char signal_suffix[32] = {0};

  if (wifi_status == DISPLAY_MNGR_WIFI_STATUS_CONNECTED) {
    int32_t rssi = 0;
    if (network_getCurrentRssi(&rssi)) {
      const char *quality = network_getSignalQualityLabel(rssi);
      snprintf(signal_suffix, sizeof(signal_suffix), " (%" PRId32 " dBm, %s)",
               rssi, quality);
    }
  }

  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
  display_mngr_format_ssid_line(ssid_str, sizeof(ssid_str), signal_suffix);
  display_mngr_draw_centered_text(ssid_str, DISPLAY_MNGR_CONNECTION_INFO_Y);
}

static void draw_connection_scr(uint8_t wifi_status, const char *url1,
                                const char *url2, const char *status_details,
                                const char *mac_str) {
  u8g2_ClearBuffer(display_get_u8g2_ref());

  if (wifi_status != DISPLAY_MNGR_WIFI_STATUS_OFFLINE) {
    display_draw_qr(qrcode_url, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0,
                    DISPLAY_QR_BORDER, DISPLAY_MNGR_QR_SCALE);
  }
  display_mngr_clear_connection_info_line();
  display_mngr_draw_connection_info(wifi_status);
  display_mngr_draw_status(current_status, status_details);

  if (mac_str != NULL) {
    char mac_line[64] = {0};
    snprintf(mac_line, sizeof(mac_line), "MAC: %s", mac_str);
    u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
    display_mngr_draw_centered_text(mac_line, 8);
  }

  if (wifi_status == DISPLAY_MNGR_WIFI_STATUS_CONNECTED) {
    u8g2_SetDrawColor(display_get_u8g2_ref(), 0);
    u8g2_DrawBox(display_get_u8g2_ref(), 0, DISPLAY_HEIGHT - 24, DISPLAY_WIDTH,
                 8);
    u8g2_SetDrawColor(display_get_u8g2_ref(), 1);

    char url_str[72] = {0};
    snprintf(url_str, sizeof(url_str), "%s or %s", url1 ? url1 : "",
             url2 ? url2 : "");
    u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
    display_mngr_draw_centered_text(url_str, 19);
  }

  if (wifi_status == DISPLAY_MNGR_WIFI_STATUS_OFFLINE) {
    u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
    display_mngr_draw_centered_text(DISPLAY_MNGR_SELECT_RESET_MESSAGE,
                                    DISPLAY_HEIGHT - 17);
  }

  display_draw_product_info();
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
  display_mngr_draw_centered_text(DISPLAY_MANAGER_BYPASS_MESSAGE,
                                  DISPLAY_HEIGHT - 9);
}

static void display_mngr_draw_status(uint8_t status, const char *details) {
  // Use 8x8 font
  u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_amstrad_cpc_extended_8f);

  // Status
  char status_str[40] = {0};
  switch (status) {
    case 0:
      snprintf(status_str, sizeof(status_str), "Connecting to WIFI...");
      break;
    case 1:
      snprintf(status_str, sizeof(status_str), "      Connected!     ");
      break;
    case DISPLAY_MNGR_WIFI_STATUS_RETRY_ERROR:
      snprintf(status_str, sizeof(status_str),
               details != NULL ? details : "   Connection Error! ");
      break;
    case DISPLAY_MNGR_WIFI_STATUS_OFFLINE:
      snprintf(status_str, sizeof(status_str),
               details != NULL ? details : "Offline mode. Network disabled.");
      break;
    case 5:
      snprintf(status_str, sizeof(status_str),
               details != NULL ? details : "Downloading firmware ");
      break;
    case 6:
      snprintf(status_str, sizeof(status_str),
               details != NULL ? details : "Firmware ready for upgrade");
      break;
    case 7:
      snprintf(status_str, sizeof(status_str),
               details != NULL ? details : " Flash erased, no reboot  ");
      break;
    default:
      snprintf(status_str, sizeof(status_str), "   Launching App...  ");
      break;
  }

  u8g2_DrawStr(display_get_u8g2_ref(),
               LEFT_PADDING_FOR_CENTER(status_str, DISPLAY_TILES_WIDTH) * 8,
               DISPLAY_HEIGHT - 24, status_str);
}

// Change the status in the buffer
void display_mngr_change_status(uint8_t status, const char *details) {
  current_status = status;
  current_status_has_details = (details != NULL);
  if (current_status_has_details) {
    snprintf(current_status_details, sizeof(current_status_details), "%s",
             details);
  } else {
    current_status_details[0] = '\0';
  }

  display_mngr_draw_status(status, details);
}

// Change the wifi status in the buffer
void display_mngr_wifi_change_status(uint8_t wifi_status, const char *url1,
                                     const char *url2,
                                     const char *status_details,
                                     const char *mac_str) {
  current_wifi_status = wifi_status;
  current_status = wifi_status;
  current_status_has_details = (status_details != NULL);
  if (current_status_has_details) {
    snprintf(current_status_details, sizeof(current_status_details), "%s",
             status_details);
  } else {
    current_status_details[0] = '\0';
  }
  snprintf(current_url1, sizeof(current_url1), "%s", url1 ? url1 : "");
  snprintf(current_url2, sizeof(current_url2), "%s", url2 ? url2 : "");
  snprintf(current_mac, sizeof(current_mac), "%s", mac_str ? mac_str : "");

  draw_connection_scr(
      wifi_status, current_url1, current_url2,
      current_status_has_details ? current_status_details : NULL,
      current_mac[0] != '\0' ? current_mac : NULL);
}

void display_mngr_refresh_connection_info() {
  if (current_wifi_status != DISPLAY_MNGR_WIFI_STATUS_CONNECTED) {
    return;
  }

  display_mngr_clear_connection_info_line();
  display_mngr_draw_connection_info(current_wifi_status);
  display_refresh();
}

void display_mngr_redraw_current(void) {
  draw_connection_scr(
      current_wifi_status, current_url1, current_url2,
      current_status_has_details ? current_status_details : NULL,
      current_mac[0] != '\0' ? current_mac : NULL);
  display_refresh();
}

// The main function should be as follows:
void display_mngr_start(const char *ssid, const char *url1, const char *url2) {
  size_t buffer_size = DISPLAY_BUFFER_SIZE;  // Safe usage
  (void)buffer_size;  // To avoid unused variable warning if not used elsewhere

  // Initialize the u8g2 library for a custom buffer
  display_setup_u8g2();

  snprintf(current_ssid, sizeof(current_ssid), "%s", ssid ? ssid : "");
  snprintf(current_url1, sizeof(current_url1), "%s", url1 ? url1 : "");
  snprintf(current_url2, sizeof(current_url2), "%s", url2 ? url2 : "");
  current_mac[0] = '\0';
  current_status_details[0] = '\0';
  current_status_has_details = false;
  current_wifi_status = 0;
  current_status = 0;
  display_create_qr(qrcode_url, url1);

  // Set the flag to NOT-RESET the computer
  SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_NOP);

  draw_connection_scr(0, current_url1, current_url2, NULL, NULL);
  display_refresh();

  DPRINTF("Exiting fabric display\n");
}
