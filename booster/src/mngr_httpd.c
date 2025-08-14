/**
 * File: mngr_httpd.c
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024-2025 - GOODDATA LABS SL
 * Description: HTTPD server functions for manager httpd
 */

#include "mngr_httpd.h"

#define WIFI_PASS_BUFSIZE 64
static char *ssid = NULL;
static char *pass = NULL;
static int auth = -1;
static void *current_connection;
static void *valid_connection;
static char apps_installed_json[MAXIMUM_APP_INFO_SIZE];
static bool apps_installed_found = false;
static wifi_scan_data_t *networks = NULL;
static mngr_httpd_response_status_t response_status = MNGR_HTTPD_RESPONSE_OK;
static char httpd_response_message[128] = {0};

/**
 * @brief Check if the string starts with specified characters
 * (case-insensitive).
 *
 * @param str The string to check.
 * @param chars The characters to match (e.g., "YyTt").
 * @return 1 if the string starts with any of the specified characters;
 * otherwise, 0.
 */
static int starts_with_case_insensitive(const char *str, const char *chars) {
  if (str == NULL || chars == NULL || str[0] == '\0') {
    return 0;
  }
  char first_char = tolower((unsigned char)str[0]);
  while (*chars) {
    if (first_char == tolower((unsigned char)*chars)) {
      return 1;
    }
    chars++;
  }
  return 0;
}

static char *get_status_message(mngr_httpd_response_status_t status,
                                const char *detail) {
  // Concatenate the detail
  char *message = (char *)malloc(128);
  if (message == NULL) {
    return NULL;
  }
  switch (status) {
    case MNGR_HTTPD_RESPONSE_OK:
      snprintf(message, 128, "OK: %s", detail);
      break;
    case MNGR_HTTPD_RESPONSE_BAD_REQUEST:
      snprintf(message, 128, "Bad Request: %s", detail);
      break;
    case MNGR_HTTPD_RESPONSE_NOT_FOUND:
      snprintf(message, 128, "Not Found: %s", detail);
      break;
    case MNGR_HTTPD_RESPONSE_INTERNAL_SERVER_ERROR:
      snprintf(message, 128, "Internal Server Error: %s", detail);
      break;
    default:
      snprintf(message, 128, "Unknown status: %s", detail);
      break;
  }
  return message;
}

/**
 * @brief Array of SSI tags for the HTTP server.
 *
 * This array contains the SSI tags used by the HTTP server to dynamically
 * insert content into web pages. Each tag corresponds to a specific piece of
 * information that can be updated or retrieved from the server.
 */
static const char *ssi_tags[] = {
    // Max size of SSI tag is 8 chars
    "HOMEPAGE",  // 0 - Redirect to the homepage
    "SDCARDST",  // 1 - SD card status
    "APPSFLDR",  // 2 - Apps folder
    "SSID",      // 3 - SSID
    "IPADDR",    // 4 - IP address
    "SDCARDB",   // 5 - SD card status true or false
    "APPSFLDB",  // 6 - Apps folder status true or false
    "JSONPLD",   // 7 - JSON payload
    "DWNLDSTS",  // 8 - Download status
    "TITLEHDR",  // 9 - Title header
    "WIFILST",   // 10 - WiFi list
    "WLSTSTP",   // 11 - WiFi list stop
    "RSPSTS",    // 12 - Response status
    "RSPMSG",    // 13 - Response message
    "BTFTR",     // 14 - Boot feature
    "SFCNFGRB",  // 15 - Safe Config Reboot
    "SDBDRTKB",  // 16 - SD Card Baud Rate
    "APPSURL",   // 17 - Apps Catalog URL
    "NVERSION",  // 18 - New Version
    "NVERSTR",   // 19 - New Version String
    "PLHLDR11",  // 20 - Placeholder 11
    "PLHLDR12",  // 21 - Placeholder 12
    "PLHLDR13",  // 22 - Placeholder 13
    "PLHLDR14",  // 23 - Placeholder 14
    "PLHLDR15",  // 24 - Placeholder 15
    "PLHLDR16",  // 25 - Placeholder 16
    "PLHLDR17",  // 26 - Placeholder 17
    "PLHLDR18",  // 27 - Placeholder 18
    "PLHLDR19",  // 28 - Placeholder 19
    "PLHLDR20",  // 29 - Placeholder 20
    "PLHLDR21",  // 30 - Placeholder 21
    "PLHLDR22",  // 31 - Placeholder 22
    "PLHLDR23",  // 32 - Placeholder 23
    "PLHLDR24",  // 33 - Placeholder 24
    "PLHLDR25",  // 34 - Placeholder 25
    "PLHLDR26",  // 35 - Placeholder 26
    "PLHLDR27",  // 36 - Placeholder 27
    "PLHLDR28",  // 37 - Placeholder 28
    "PLHLDR29",  // 38 - Placeholder 29
    "PLHLDR30",  // 39 - Placeholder 30
    "WDHCP",     // 40 - WiFi DHCP
    "WIP",       // 41 - WiFi IP
    "WNTMSK",    // 42 - WiFi Netmask
    "WGTWY",     // 43 - WiFi Gateway
    "WDNS",      // 44 - WiFi DNS
    "WCNTRY",    // 45 - WiFi Country
    "WHSTNM",    // 46 - WiFi Hostname
    "WPWR",      // 47 - WiFi Power
    "WRSS",      // 48 - WiFi RSSI
};

/**
 * @brief
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to after the floppy disk image is
 * selected.
 */
static const char *cgi_test(int iIndex, int iNumParams, char *pcParam[],
                            char *pcValue[]) {
  DPRINTF("TEST CGI handler called with index %d\n", iIndex);
  return "/test.shtml";
}

/**
 * @brief Download the selected app fron the data passed as json in the GET
 * request
 *
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to wait for the download process
 */
const char *cgi_download(int iIndex, int iNumParams, char *pcParam[],
                         char *pcValue[]) {
  DPRINTF("cgi_download called with index %d\n", iIndex);
  bool update = false;
  for (size_t i = 0; i < iNumParams; i++) {
    /* check if parameter is "update" */
    if (strcmp(pcParam[i], "update") == 0) {
      DPRINTF("Update flag found, value: %s\n", pcValue[i]);
      if (starts_with_case_insensitive(pcValue[i], "YyTt")) {
        update = true;
      } else {
        update = false;
      }
      DPRINTF("Update flag set to: %d\n", update);
    }
  }
  appmngr_set_download_update(update);
  for (size_t i = 0; i < iNumParams; i++) {
    /* check if parameter is "json" */
    if (strcmp(pcParam[i], "json") == 0) {
      DPRINTF("JSON encoded value: %s\n", pcValue[i]);
      int ret;
      size_t len = 0;
      char output_buffer[4096];
      ret = mbedtls_base64_decode(output_buffer,          // destination
                                  sizeof(output_buffer),  // dest size
                                  &len,  // number of bytes decoded
                                  (const unsigned char *)pcValue[i],
                                  strlen(pcValue[i]));
      // Place a null terminator at the end of the string
      output_buffer[len] = '\0';
      if (ret != 0) {
        DPRINTF("Error decoding base64: %d\n", ret);
        static char error_url[64];  // Static buffer for the error URL
        snprintf(error_url, sizeof(error_url),
                 "/error.shtml?error=%d&error_msg=Decoding%20error", ret);
        return error_url;
      }
      DPRINTF("Decoded value: %s\n", output_buffer);
      download_err_t err = appmngr_save_app_info(output_buffer);
      if (err != DOWNLOAD_OK) {
        DPRINTF("Error saving app info: %d\n", err);
        static char error_url[64];  // Static buffer for the error URL
        snprintf(error_url, sizeof(error_url),
                 "/error.shtml?error=%d&error_msg=Downloading%20error", err);
        return error_url;
      }
      DPRINTF("App info saved\n");
      appmngr_download_status(DOWNLOAD_STATUS_REQUESTED);
      return "/downloading.shtml";
    }
  }
}

/**
 * @brief Update the given params with the new values
 *
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to wait for the download process
 */
const char *cgi_saveparams(int iIndex, int iNumParams, char *pcParam[],
                           char *pcValue[]) {
  DPRINTF("cgi_saveparams called with index %d\n", iIndex);
  for (size_t i = 0; i < iNumParams; i++) {
    /* check if parameter is "json" */
    if (strcmp(pcParam[i], "json") == 0) {
      DPRINTF("JSON encoded value: %s\n", pcValue[i]);
      int ret;
      size_t len = 0;
      char output_buffer[4096];
      ret = mbedtls_base64_decode(
          (unsigned char *)output_buffer,  // destination
          sizeof(output_buffer),           // dest size
          &len,                            // number of bytes decoded
          (const unsigned char *)pcValue[i], strlen(pcValue[i]));
      // Place a null terminator at the end of the string
      output_buffer[len] = '\0';
      if (ret != 0) {
        DPRINTF("Error decoding base64: %d\n", ret);
        response_status = MNGR_HTTPD_RESPONSE_BAD_REQUEST;
        char detail[64] = {0};
        snprintf(detail, sizeof(detail), "Error decoding base64: %d", ret);
        snprintf(httpd_response_message, sizeof(httpd_response_message), "%s",
                 get_status_message(response_status, detail));
      } else {
        DPRINTF("Decoded value: %s\n", output_buffer);
        // Parse the JSON object
        bool valid_json = true;
        cJSON *root = cJSON_Parse(output_buffer);
        if (root == NULL) {
          DPRINTF("Error parsing JSON\n");
          response_status = MNGR_HTTPD_RESPONSE_BAD_REQUEST;
          snprintf(httpd_response_message, sizeof(httpd_response_message),
                   "Error parsing JSON");
          valid_json = false;
        } else {
          // Iterate over the JSON array
          cJSON *item = NULL;
          cJSON_ArrayForEach(item, root) {
            cJSON *name = cJSON_GetObjectItem(item, "name");
            cJSON *type = cJSON_GetObjectItem(item, "type");
            cJSON *value = cJSON_GetObjectItem(item, "value");

            // Print each parameter
            if (cJSON_IsString(name) && cJSON_IsString(type) &&
                cJSON_IsString(value)) {
              DPRINTF("Param Name: %s, Type: %s, Value: %s\n",
                      name->valuestring, type->valuestring, value->valuestring);
              if (strcasecmp(type->valuestring, "STRING") == 0) {
                settings_put_string(gconfig_getContext(), name->valuestring,
                                    value->valuestring);
                DPRINTF("Setting %s to %s saved.\n", name->valuestring,
                        value->valuestring);
              } else if (strcasecmp(type->valuestring, "INT") == 0) {
                settings_put_integer(gconfig_getContext(), name->valuestring,
                                     atoi(value->valuestring));
                DPRINTF("Setting %s to %d saved.\n", name->valuestring,
                        atoi(value->valuestring));
              } else if (strcasecmp(type->valuestring, "BOOL") == 0) {
                // Check if the value starts with "Y", "y", "T", or "t"
                int bool_value =
                    starts_with_case_insensitive(value->valuestring, "YyTt");
                settings_put_bool(gconfig_getContext(), name->valuestring,
                                  bool_value);
                DPRINTF("Setting %s to %s saved.\n", name->valuestring,
                        bool_value ? "true" : "false");
              } else {
                DPRINTF("Invalid parameter type in JSON\n");
                response_status = MNGR_HTTPD_RESPONSE_BAD_REQUEST;
                snprintf(httpd_response_message, sizeof(httpd_response_message),
                         "Invalid parameter type in JSON");
                valid_json = false;
              }
            } else {
              DPRINTF("Invalid parameter structure in JSON\n");
              response_status = MNGR_HTTPD_RESPONSE_BAD_REQUEST;
              snprintf(httpd_response_message, sizeof(httpd_response_message),
                       "Invalid parameter structure in JSON");
              valid_json = false;
            }
          }

          // Free the parsed JSON object
          cJSON_Delete(root);

          if (valid_json) {
            settings_save(gconfig_getContext(), true);
            DPRINTF("Settings saved\n");
            response_status = MNGR_HTTPD_RESPONSE_OK;
            snprintf(httpd_response_message, sizeof(httpd_response_message),
                     "");
          }
        }
      }
      return "/response.shtml";
    }
  }

  // If no "json" parameter is found
  response_status = MNGR_HTTPD_RESPONSE_BAD_REQUEST;
  snprintf(httpd_response_message, sizeof(httpd_response_message),
           "Missing 'json' parameter");
  return "/response.shtml";
}

/**
 * @brief Show the first app found in the apps folder
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to after the floppy disk image is
 * selected.
 */
static const char *cgi_fsfirst(int iIndex, int iNumParams, char *pcParam[],
                               char *pcValue[]) {
  DPRINTF("FSFIRST CGI handler called with index %d\n", iIndex);
  apps_installed_found = appmngr_ffirst(apps_installed_json);
  if (!apps_installed_found) {
    DPRINTF("No apps found\n");
    return "/jsonempty.shtml";
  } else {
    DPRINTF("Apps found\n");
  }
  return "/json.shtml";
}

/**
 * @brief Show the next app found in the apps folder
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to after the floppy disk image is
 * selected.
 */
static const char *cgi_fsnext(int iIndex, int iNumParams, char *pcParam[],
                              char *pcValue[]) {
  DPRINTF("FSNEXT CGI handler called with index %d\n", iIndex);
  apps_installed_found = appmngr_fnext(apps_installed_json);
  if (!apps_installed_found) {
    DPRINTF("No more apps found\n");
    return "/jsonempty.shtml";
  } else {
    DPRINTF("Apps found\n");
  }
  return "/json.shtml";
}

/**
 * @brief Delete the app info file from the SD card
 *
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to wait for the download process
 */
const char *cgi_deleteapp(int iIndex, int iNumParams, char *pcParam[],
                          char *pcValue[]) {
  DPRINTF("cgi_deleteapp called with index %d\n", iIndex);
  download_delete_err_t err = DOWNLOAD_DELETEAPP_OK;
  for (size_t i = 0; i < iNumParams; i++) {
    /* check if parameter is "json" */
    if (strcmp(pcParam[i], "uuid") == 0) {
      DPRINTF("APP to delete with UUID: %s\n", pcValue[i]);
      err = appmngr_delete_app(pcValue[i]);
      if (err == DOWNLOAD_DELETEAPP_OK) {
        DPRINTF("App deleted\n");
        return "/deleting.shtml";
      }
    }
  }
  // Use err code 0 for generic error
  static char error_url[128];  // Static buffer for the error URL
  snprintf(error_url, sizeof(error_url),
           "/error.shtml?error=%d&error_msg=Deleting%20error", err);
  return error_url;
}

/**
 * @brief Launch the app with the uuid given
 *
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to wait for the download process
 */
const char *cgi_launchapp(int iIndex, int iNumParams, char *pcParam[],
                          char *pcValue[]) {
  DPRINTF("cgi_launchapp called with index %d\n", iIndex);
  download_launch_err_t err = DOWNLOAD_LAUNCHAPP_OK;
  for (size_t i = 0; i < iNumParams; i++) {
    /* check if parameter is "json" */
    if (strcmp(pcParam[i], "uuid") == 0) {
      DPRINTF("APP to launch with UUID: %s\n", pcValue[i]);
      appmngr_schedule_launch_app(pcValue[i]);
      return "/launching.html";
    }
  }
  // Use err code 0 for generic error
  static char error_url[128];  // Static buffer for the error URL
  snprintf(error_url, sizeof(error_url),
           "/error.shtml?error=%d&error_msg=Deleting%20error", err);
  return error_url;
}

/**
 * @brief Reboot the device
 *
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to wait for the download process
 */
const char *cgi_reboot(int iIndex, int iNumParams, char *pcParam[],
                       char *pcValue[]) {
  DPRINTF("cgi_reboot called with index %d\n", iIndex);
  mngr_schedule_reset(5);  // Reboot in 5 seconds
  return "/ap_step3.shtml";
}

/**
 * @brief Factory reset the device
 *
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to wait for the download process
 */
const char *cgi_factoryreset(int iIndex, int iNumParams, char *pcParam[],
                             char *pcValue[]) {
  DPRINTF("cgi_factoryreset called with index %d\n", iIndex);
  mngr_schedule_factory_reset(5);  // Factory reset in 5 seconds
  return "/ap_step3.shtml";
}

const char *cgi_firmware_upgrade_start(int iIndex, int iNumParams,
                                       char *pcParam[], char *pcValue[]) {
  DPRINTF("cgi_firmware_upgrade_start called with index %d\n", iIndex);
  mngr_firmwareUpgradeStart();
  response_status = MNGR_HTTPD_RESPONSE_OK;
  snprintf(httpd_response_message, sizeof(httpd_response_message), "");
  return "/response.shtml";
}

const char *cgi_firmware_upgrade_downloaded(int iIndex, int iNumParams,
                                            char *pcParam[], char *pcValue[]) {
  DPRINTF("cgi_firmware_upgrade_downloaded called with index %d\n", iIndex);
  DPRINTF("Firmware upgrade state: %d\n", mngr_get_firmwareUpgradeState());
  if (mngr_get_firmwareUpgradeState() == FIRMWARE_UPGRADE_DOWNLOADED) {
    response_status = MNGR_HTTPD_RESPONSE_OK;
    snprintf(httpd_response_message, sizeof(httpd_response_message), "");
  } else {
    response_status = MNGR_HTTPD_RESPONSE_BAD_REQUEST;
    snprintf(httpd_response_message, sizeof(httpd_response_message),
             "Download firmware failed");
  }
  return "/response.shtml";
}

const char *cgi_firmware_upgrade_confirm(int iIndex, int iNumParams,
                                         char *pcParam[], char *pcValue[]) {
  DPRINTF("cgi_firmware_upgrade_confirm called with index %d\n", iIndex);
  if (mngr_get_firmwareUpgradeState() == FIRMWARE_UPGRADE_DOWNLOADED) {
    mngr_firmwareUpgradeInstall();
    response_status = MNGR_HTTPD_RESPONSE_OK;
    snprintf(httpd_response_message, sizeof(httpd_response_message), "");
  } else {
    response_status = MNGR_HTTPD_RESPONSE_BAD_REQUEST;
    snprintf(httpd_response_message, sizeof(httpd_response_message),
             "Firmware upgrade failed");
  }
  return "/response.shtml";
}

/**
 * @brief Array of CGI handlers for floppy select and eject operations.
 *
 * This array contains the mappings between the CGI paths and the corresponding
 * handler functions for selecting and ejecting floppy disk images for drive A
 * and drive B.
 */
static const tCGI cgi_handlers[] = {
    {"/test.cgi", cgi_test},
    {"/download.cgi", cgi_download},
    {"/saveparams.cgi", cgi_saveparams},
    {"/reboot.cgi", cgi_reboot},
    {"/factoryreset.cgi", cgi_factoryreset},
    {"/mngr_fsfirst.cgi", cgi_fsfirst},
    {"/mngr_fsnext.cgi", cgi_fsnext},
    {"/mngr_deleteapp.cgi", cgi_deleteapp},
    {"/mngr_launchapp.cgi", cgi_launchapp},
    {"/firmware_upgrade_start.cgi", cgi_firmware_upgrade_start},
    {"/firmware_upgrade_downloaded.cgi", cgi_firmware_upgrade_downloaded},
    {"/firmware_upgrade_confirm.cgi", cgi_firmware_upgrade_confirm},
};

/**
 * @brief Initializes the HTTP server with optional SSI tags, CGI handlers, and
 * an SSI handler function.
 *
 * This function initializes the HTTP server and sets up the provided Server
 * Side Include (SSI) tags, Common Gateway Interface (CGI) handlers, and SSI
 * handler function. It first calls the httpd_init() function to initialize the
 * HTTP server.
 *
 * The filesystem for the HTTP server is in the 'fs' directory in the project
 * root.
 *
 * @param ssi_tags An array of strings representing the SSI tags to be used in
 * the server-side includes.
 * @param num_tags The number of SSI tags in the ssi_tags array.
 * @param ssi_handler_func A pointer to the function that handles SSI tags.
 * @param cgi_handlers An array of tCGI structures representing the CGI handlers
 * to be used.
 * @param num_cgi_handlers The number of CGI handlers in the cgi_handlers array.
 */
static void httpd_server_init(const char *ssi_tags[], size_t num_tags,
                              tSSIHandler ssi_handler_func,
                              const tCGI *cgi_handlers,
                              size_t num_cgi_handlers) {
  httpd_init();

  // SSI Initialization
  if (num_tags > 0) {
    for (size_t i = 0; i < num_tags; i++) {
      LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN",
                  strlen(ssi_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
    }
    http_set_ssi_handler(ssi_handler_func, ssi_tags, num_tags);
  } else {
    DPRINTF("No SSI tags defined.\n");
  }

  // CGI Initialization
  if (num_cgi_handlers > 0) {
    http_set_cgi_handlers(cgi_handlers, num_cgi_handlers);
  } else {
    DPRINTF("No CGI handlers defined.\n");
  }

  DPRINTF("HTTP server initialized.\n");
}

/**
 * @brief Server Side Include (SSI) handler for the HTTPD server.
 *
 * This function is called when the server needs to dynamically insert content
 * into web pages using SSI tags. It handles different SSI tags and generates
 * the corresponding content to be inserted into the web page.
 *
 * @param iIndex The index of the SSI handler.
 * @param pcInsert A pointer to the buffer where the generated content should be
 * inserted.
 * @param iInsertLen The length of the buffer.
 * @param current_tag_part The current part of the SSI tag being processed (used
 * for multipart SSI tags).
 * @param next_tag_part A pointer to the next part of the SSI tag to be
 * processed (used for multipart SSI tags).
 * @return The length of the generated content.
 */
static u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
                         ,
                         u16_t current_tag_part, u16_t *next_tag_part
#endif /* LWIP_HTTPD_SSI_MULTIPART */
) {
  // DPRINTF("SSI handler called with index %d\n", iIndex);
  size_t printed;
  switch (iIndex) {
    case 0: /* "HOMEPAGE" */
      // Always to the first step of the configuration
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          "<meta http-equiv='refresh' content='0;url=/mngr_home.shtml'>");
      break;
    case 1: /* "SDCARDST" */
    {
      if (!appmngr_get_sdcard_info()->ready) {
        printed =
            snprintf(pcInsert, iInsertLen,
                     "<span class=\"text-warning\">No SD card found</span>");
      } else {
        printed = snprintf(pcInsert, iInsertLen, "%d MB",
                           appmngr_get_sdcard_info()->free_space);
      }
      break;
    }
    case 2: /* "APPSFLDR" */
    {
      if (!appmngr_get_sdcard_info()->ready) {
        printed =
            snprintf(pcInsert, iInsertLen,
                     "<span class=\"text-warning\">No SD card found</span>");
      } else {
        char *apps_folder =
            settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value;
        if (!appmngr_get_sdcard_info()->apps_folder_found) {
          printed = snprintf(pcInsert, iInsertLen,
                             "<span class=\"text-warning\">%s not found</span>",
                             apps_folder);
        } else {
          printed = snprintf(pcInsert, iInsertLen, "%s", apps_folder);
        }
      }
      break;
    }
    case 3: /* "SSID" */
    {
      char *ssid =
          settings_find_entry(gconfig_getContext(), PARAM_WIFI_SSID)->value;
      if (ssid != NULL) {
        printed = snprintf(pcInsert, iInsertLen, "%s", ssid);
      } else {
        printed =
            snprintf(pcInsert, iInsertLen,
                     "<span class=\"text-error\">No network selected</span>");
      }
      break;
    }
    case 4: /* IPADDR */
    {
      ip_addr_t ipaddr = network_getCurrentIp();
      if (&ipaddr != NULL) {
        printed = snprintf(pcInsert, iInsertLen, "%s", ip4addr_ntoa(&ipaddr));
      } else {
        printed = snprintf(pcInsert, iInsertLen,
                           "<span class=\"text-error\">No IP address</span>");
      }
      break;
    }
    case 5: /* SDCARDB */
    {
      if (!appmngr_get_sdcard_info()->ready) {
        printed = snprintf(pcInsert, iInsertLen, "false");
      } else {
        printed = snprintf(pcInsert, iInsertLen, "true");
      }
      break;
    }
    case 6: /* APPSFLDB */
    {
      if (!appmngr_get_sdcard_info()->ready) {
        printed = snprintf(pcInsert, iInsertLen, "false");
      } else {
        if (!appmngr_get_sdcard_info()->apps_folder_found) {
          printed = snprintf(pcInsert, iInsertLen, "false");
        } else {
          printed = snprintf(pcInsert, iInsertLen, "true");
        }
      }
      break;
    }
    case 7: /* JSONPLD */
    {
      // DPRINTF("SSI JSONPLD handler called with index %d\n", iIndex);
      int chunk_size = 128;
      /* The offset into json based on current tag part */
      size_t offset = current_tag_part * chunk_size;
      size_t json_len = strlen(apps_installed_json);

      /* If offset is beyond the end, we have no more data */
      if (offset >= json_len) {
        /* No more data, so no next part */
        printed = 0;
        break;
      }

      /* Calculate how many bytes remain from offset */
      size_t remain = json_len - offset;
      /* We want to send up to chunk_size bytes per part, or what's left if
       * <chunk_size */
      size_t chunk_len = (remain < chunk_size) ? remain : chunk_size;

      /* Also ensure we don't exceed iInsertLen - 1, to leave room for '\0' */
      if (chunk_len > (size_t)(iInsertLen - 1)) {
        chunk_len = iInsertLen - 1;
      }

      /* Copy that chunk into pcInsert */
      memcpy(pcInsert, &apps_installed_json[offset], chunk_len);
      pcInsert[chunk_len] = '\0'; /* null-terminate */

      printed = (u16_t)chunk_len;

      /* If there's more data after this chunk, increment next_tag_part */
      if ((offset + chunk_len) < json_len) {
        *next_tag_part = current_tag_part + 1;
      }
      break;
    }
    case 8: /* DWNLDSTS */
    {
      download_status_t status = appmngr_get_download_status();
      download_err_t err = appmngr_get_download_error();

      switch (status) {
        case DOWNLOAD_STATUS_IDLE:
        case DOWNLOAD_STATUS_COMPLETED:
          printed = snprintf(
              pcInsert, iInsertLen, "%s",
              "<meta http-equiv='refresh' content='0;url=/mngr_home.shtml'>");
          break;
          break;
        case DOWNLOAD_STATUS_FAILED:
          printed = snprintf(
              pcInsert, iInsertLen,
              "<meta http-equiv='refresh' "
              "content='0;url=/"
              "error.shtml?error=%d&error_msg=Download%%20error:%%20%s'>",
              err, appmngr_get_download_error_str());
          break;
          break;
        default:
          printed = snprintf(
              pcInsert, iInsertLen, "%s",
              "<meta http-equiv='refresh' content='5;url=/downloading.shtml'>");
          break;
      }
      break;
    }
    case 9: /* TITLEHDR */
    {
#if _DEBUG == 0
      printed = snprintf(pcInsert, iInsertLen, "%s (%s)", BOOSTER_TITLE,
                         RELEASE_VERSION);
#else
      printed = snprintf(pcInsert, iInsertLen, "%s (%s-%s)", BOOSTER_TITLE,
                         RELEASE_VERSION, RELEASE_DATE);
#endif
      break;
    }
    case 10: /* WIFILST */
    {
      if (current_tag_part == 0) {
        mngr_enable_network_scan();
        networks = mngr_get_networks();
      }
      // Loop through networks->count;
      if (networks != NULL) {
        if (current_tag_part < networks->count) {
          wifi_network_info_t *network = &networks->networks[current_tag_part];
          char last_char =
              (current_tag_part == networks->count - 1) ? ' ' : ',';
          printed = snprintf(pcInsert, iInsertLen,
                             "{\"SSID\": \"%s\",\"BSSID\": \"%s\",\"RSSI\": "
                             "\"%d\",\"AUTH\": \"%d\"}%c",
                             network->ssid, network->bssid, network->rssi,
                             network->auth_mode, last_char);
          *next_tag_part = current_tag_part + 1;
        } else {
          printed = 0;
          networks = NULL;
        }
      } else {
        printed = 0;
      }
      break;
    }
    case 11: /* WLSTSTP */
    {
      // Stop the network scan and print an empty string
      mngr_disable_network_scan();
      printed = snprintf(pcInsert, iInsertLen, "");
      break;
    }
    case 12: /* RSPSTS */
    {
      printed = snprintf(pcInsert, iInsertLen, "%d", response_status);
      break;
    }
    case 13: /* RSPMSG */
    {
      printed = snprintf(pcInsert, iInsertLen, "%s", httpd_response_message);
      break;
    }
    case 14: /* BTFTR */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_BOOT_FEATURE)->value);
      break;
    }
    case 15: /* SFCNFGRB */
    {
      printed =
          snprintf(pcInsert, iInsertLen, "%s",
                   strcasecmp(settings_find_entry(gconfig_getContext(),
                                                  PARAM_SAFE_CONFIG_REBOOT)
                                  ->value,
                              "true") == 0
                       ? "Yes"
                       : "No");
      break;
    }
    case 16: /* SDBDRTKB */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_SD_BAUD_RATE_KB)
              ->value);
      break;
    }
    case 17: /* APPSURL */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_APPS_CATALOG_URL)
              ->value);
      break;
    }
    case 18: /* NVERSION */
    {
      printed = snprintf(pcInsert, iInsertLen, "%s",
                         version_isNewer() ? "Yes" : "No");
      break;
    }
    case 19: /* NVERSTR */
    {
      printed = snprintf(pcInsert, iInsertLen, "%s", version_get_string());
      break;
    }
    case 40: /* WDHCP */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          strcasecmp(
              settings_find_entry(gconfig_getContext(), PARAM_WIFI_DHCP)->value,
              "true") == 0
              ? "Yes"
              : "No");
      break;
    }
    case 41: /* WIP */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_WIFI_IP)->value);
      break;
    }
    case 42: /* WNTMSK */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_WIFI_NETMASK)->value);
      break;
    }
    case 43: /* WGTWY */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_WIFI_GATEWAY)->value);
      break;
    }
    case 44: /* WDNS */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_WIFI_DNS)->value);
      break;
    }
    case 45: /* WCNTRY */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_WIFI_COUNTRY)->value);
      break;
    }
    case 46: /* WHSTNM */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_HOSTNAME)->value);
      break;
    }
    case 47: /* WPWR */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          settings_find_entry(gconfig_getContext(), PARAM_WIFI_POWER)->value);
      break;
    }
    case 48: /* WRSS */
    {
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          strcasecmp(
              settings_find_entry(gconfig_getContext(), PARAM_WIFI_RSSI)->value,
              "true") == 0
              ? "Yes"
              : "No");
      break;
    }
    default: /* unknown tag */
      printed = 0;
      break;
  }
  LWIP_ASSERT("sane length", printed <= 0xFFFF);
  return (u16_t)printed;
}

// The main function should be as follows:
void mngr_httpd_start() {
  // Initialize the HTTP server with SSI tags and CGI handlers
  httpd_server_init(ssi_tags, LWIP_ARRAYSIZE(ssi_tags), ssi_handler,
                    cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
}