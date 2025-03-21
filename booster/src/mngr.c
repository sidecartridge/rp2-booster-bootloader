/**
 * File: mngr.c
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2023-25 - GOODDATA LABS SL
 * Description: C file with the main loop of the manager module
 */

#include "mngr.h"

static bool network_scan_enabled = false;
static bool device_reset = false;
static bool factory_reset = false;
static absolute_time_t reboot_time = {0};
static absolute_time_t factory_reset_time = {0};

void mngr_enable_network_scan() { network_scan_enabled = true; }

void mngr_disable_network_scan() { network_scan_enabled = false; }

wifi_scan_data_t *mngr_get_networks() { return network_getFoundNetworks(); }

int mngr_init() {
  // Initialize the network
  int err = network_initChipOnly();
  if (err != 0) {
    DPRINTF("Error initializing the network: %i\n", err);
    blink_error();
    return err;
  }

  // Initialize the SD card
  FATFS fs;
  SettingsConfigEntry *appsFolder =
      settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER);
  char *appsFolderName = "/apps";
  if (appsFolder == NULL) {
    DPRINTF(
        "APPS_FOLDER not found in the configuration. Using default value\n");
  } else {
    DPRINTF("APPS_FOLDER: %s\n", appsFolder->value);
    appsFolderName = appsFolder->value;
  }
  int sdcard_err = sdcard_initFilesystem(&fs, appsFolderName);
  if (sdcard_err != SDCARD_INIT_OK) {
    DPRINTF("Error initializing the SD card: %i\n", sdcard_err);
  } else {
    DPRINTF("SD card found & initialized\n");
  }
  appmngr_get_sdcard_info()->ready = (sdcard_err == SDCARD_INIT_OK);

  //  Check if the USB is connected. If so, check if the SD card is inserted
  //  and initialize the USB Mass storage device
  if (appmngr_get_sdcard_info()->ready) {
    if (USBDRIVE_MASS_STORE && cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN)) {
      DPRINTF("USB connected\n");

      // Disable the SELECT button
      select_coreWaitPushDisable();
#if TUD_OPT_HIGH_SPEED
      DPRINTF("USB High Speed enabled. Configure serial USB speed\n");
      sdcard_setSpiSpeedSettings();
#endif

      display_usb_start();
      usb_mass_init();

      // Deinit the network
      DPRINTF("Deinitializing the network\n");
      network_deInit();

      // Send the reboot command
      SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_RESET);
      sleep_ms(1000);
      reset_device();

    } else {
      DPRINTF("USB not connected\n");
      // Obtain the free space in the SD card
      uint32_t total_size = 0;
      uint32_t free_space = 0;
      sdcard_getInfo(&fs, &total_size, &free_space);
      appmngr_get_sdcard_info()->total_size = total_size;
      appmngr_get_sdcard_info()->free_space = free_space;
      DPRINTF("SD card total size: %uMB\n",
              appmngr_get_sdcard_info()->total_size);
      DPRINTF("SD card free space: %uMB\n",
              appmngr_get_sdcard_info()->free_space);
      char *apps_folder =
          settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value;
      if (apps_folder == NULL) {
        appmngr_get_sdcard_info()->apps_folder_found = false;
        DPRINTF("Apps folder param not found\n");
      } else {
        appmngr_get_sdcard_info()->apps_folder_found =
            sdcard_dirExist(apps_folder);
        DPRINTF(
            "Apps folder found: %s\n",
            appmngr_get_sdcard_info()->apps_folder_found ? "true" : "false");
      }
    }
  }

  // Deinit the network
  DPRINTF("Deinitializing the network\n");
  network_deInit();

  // Set hostname
  char *hostname =
      settings_find_entry(gconfig_getContext(), PARAM_HOSTNAME)->value;
  char url_host[128] = {0};
  if ((hostname != NULL) && (strlen(hostname) > 0)) {
    snprintf(url_host, sizeof(url_host), "http://%s", hostname);
  } else {
    snprintf(url_host, sizeof(url_host), "http://%s", "sidecart");
  }

  char url_ip[128] = {0};
  snprintf(url_ip, sizeof(url_ip), "http://%s", "127.0.0.1");

  // Set SSID
  char ssid[128] = {0};
  SettingsConfigEntry *ssid_param =
      settings_find_entry(gconfig_getContext(), PARAM_WIFI_SSID);
  if (strlen(ssid_param->value) == 0) {
    DPRINTF("No SSID found in config.\n");
    snprintf(ssid, sizeof(ssid), "%s", "No SSID found");
  } else {
    snprintf(ssid, sizeof(ssid), "%s", ssid_param->value);
  }

  display_mngr_start(ssid, url_host, url_ip);

  wifi_mode_t wifi_mode_value = WIFI_MODE_STA;
  err = network_wifiInit(wifi_mode_value);
  if (err != 0) {
    DPRINTF("Error initializing the network: %i\n", err);
    blink_error();
    return err;
  }

  // Connect to the WiFi network
  err = NETWORK_WIFI_STA_CONN_ERR_TIMEOUT;
  while (err == NETWORK_WIFI_STA_CONN_ERR_TIMEOUT) {
    err = network_wifiStaConnect();
    if ((err > 0) && (err < NETWORK_WIFI_STA_CONN_ERR_TIMEOUT)) {
      DPRINTF("Error connecting to the WiFi network: %i\n", err);
      blink_error();
      display_mngr_wifi_change_status(2, NULL, NULL);  // Error
      return err;
    }
  }
  DPRINTF("WiFi connected\n");

  // Disable the SELECT button
  // select_coreWaitPushDisable();

  // Enable the SELECT button again, but only to reset the BOOSTER
  // select_coreWaitPush(reset_device);

  ip4_addr_t ip = network_getCurrentIp();
  DPRINTF("IP address: %s\n", ip4addr_ntoa(&ip));

  snprintf(url_ip, sizeof(url_ip), "http://%s", ip4addr_ntoa(&ip));

  // appmngr load table
  uint8_t *table = malloc(FLASH_SECTOR_SIZE);
  uint16_t table_length = 0;
  appmngr_load_apps_lookup_table(table, &table_length);
  appmngr_print_apps_lookup_table(table, table_length);
  free(table);

  display_mngr_wifi_change_status(1, url_host, url_ip);
  display_refresh();

  // Init the download apps
  appmngr_init();

  // Configure the sd card, if possible
  bool sdcard_ready = false;
  if (!appmngr_get_sdcard_info()->ready) {
    // Try to initialize the SD card
    SettingsConfigEntry *appsFolder =
        settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER);
    char *appsFolderName = "/apps";
    if (appsFolder == NULL) {
      DPRINTF(
          "APPS_FOLDER not found in the configuration. Using default "
          "value\n");
    } else {
      DPRINTF("APPS_FOLDER: %s\n", appsFolder->value);
      appsFolderName = appsFolder->value;
    }
    int sdcard_err = sdcard_initFilesystem(&fs, appsFolderName);
    if (sdcard_err == SDCARD_INIT_OK) {
      DPRINTF("SD card found & initialized\n");
      appmngr_get_sdcard_info()->ready = true;
      // Obtain the free space in the SD card
      uint32_t total_size = 0;
      uint32_t free_space = 0;
      sdcard_getInfo(&fs, &total_size, &free_space);
      appmngr_get_sdcard_info()->total_size = total_size;
      appmngr_get_sdcard_info()->free_space = free_space;
      DPRINTF("SD card total size: %uMB\n",
              appmngr_get_sdcard_info()->total_size);
      DPRINTF("SD card free space: %uMB\n",
              appmngr_get_sdcard_info()->free_space);
      char *apps_folder =
          settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value;
      if (apps_folder != NULL) {
        appmngr_get_sdcard_info()->apps_folder_found =
            sdcard_dirExist(apps_folder);
      }
      sdcard_ready = true;
    } else {
      DPRINTF("Error initializing the SD card: %i\n", sdcard_err);
    }
  }

  // Start the HTTP server
  mngr_httpd_start();
  sleep_ms(500);

  int wifi_scan_polling_interval = 5;  // 5 seconds
  absolute_time_t wifi_scan_time =
      make_timeout_time_ms(5 * 1000);  // 3 seconds minimum for network scanning
  absolute_time_t show_screen_timeout =
      make_timeout_time_ms(10 * 1000);  // 10 seconds to show the screen
  absolute_time_t start_download_time =
      make_timeout_time_ms(86400 * 1000);  // 3 seconds to start the download
  reboot_time = make_timeout_time_ms(0);
  factory_reset_time = make_timeout_time_ms(0);
  absolute_time_t launch_time = make_timeout_time_ms(0);
  bool bypass = true;
  while (1) {
#if PICO_CYW43_ARCH_POLL
    network_safe_poll();
    cyw43_arch_wait_for_work_until(wifi_scan_time);
#else
    sleep_ms(100);
#endif
    if (network_scan_enabled) {
      network_scan(&wifi_scan_time, wifi_scan_polling_interval);
    }
    if (bypass &&
        (absolute_time_diff_us(get_absolute_time(), show_screen_timeout) < 0)) {
      // Send continue to desktop command
      // SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_CONTINUE);
      bypass = false;
    }

    if (appmngr_get_download_status() == DOWNLOAD_STATUS_IN_PROGRESS) {
      appmngr_poll_download_app();
    }

    switch (appmngr_get_download_status()) {
      case DOWNLOAD_STATUS_REQUESTED: {
        start_download_time =
            make_timeout_time_ms(3 * 1000);  // 3 seconds to start the download
        appmngr_download_status(DOWNLOAD_STATUS_NOT_STARTED);
        break;
      }
      case DOWNLOAD_STATUS_NOT_STARTED: {
        if ((absolute_time_diff_us(get_absolute_time(), start_download_time) <
             0)) {
          err = appmngr_start_download_app();
          if (err != DOWNLOAD_OK) {
            DPRINTF("Error downloading app. Drive to error page.\n");
          }
        }
        break;
      }
      case DOWNLOAD_STATUS_STARTED: {
        if (appmngr_get_download_error() != DOWNLOAD_OK) {
          DPRINTF("Error downloading app. Drive to error page.\n");
          appmngr_confirm_failed_download_app();
          appmngr_download_status(DOWNLOAD_STATUS_FAILED);
        }
        break;
      }
      case DOWNLOAD_STATUS_COMPLETED: {
        // Save the app info to the SD card
        download_err_t err = appmngr_finish_download_app();
        appmngr_download_error(err);
        if (err != DOWNLOAD_OK) {
          DPRINTF("Error finishing download app\n");
          appmngr_confirm_failed_download_app();
          appmngr_download_status(DOWNLOAD_STATUS_FAILED);
          break;
        }
        appmngr_confirm_download_app();
        appmngr_download_status(DOWNLOAD_STATUS_IDLE);
        break;
      }
    }

    if (appmngr_get_launch_status() == DOWNLOAD_LAUNCHAPP_INPROGRESS) {
      if ((absolute_time_diff_us(get_absolute_time(), launch_time) < 0)) {
        download_launch_err_t err = appmngr_launch_app();
        if (err == DOWNLOAD_LAUNCHAPP_OK) {
          device_reset = true;
          reboot_time = make_timeout_time_ms(0);
        }
      }
    } else if (appmngr_get_launch_status() == DOWNLOAD_LAUNCHAPP_SCHEDULED) {
      appmngr_set_launch_status(DOWNLOAD_LAUNCHAPP_INPROGRESS);
      display_mngr_change_status(4);  // Launching app message
      display_refresh();
      launch_time = make_timeout_time_ms(5 * 1000);  // 5 seconds to launch
      DPRINTF("Launching app in 5 seconds\n");
    }

    // Check if the device needs to be reset
    if (device_reset) {
      if (absolute_time_diff_us(get_absolute_time(), reboot_time) < 0) {
        DPRINTF("Rebooting\n");
        device_reset = false;
        // Send the reboot command
        SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_RESET);
        sleep_ms(1000);
        reset_device();
      }
    }

    // Check if the device needs to be factory reset
    if (factory_reset) {
      if (absolute_time_diff_us(get_absolute_time(), factory_reset_time) < 0) {
        DPRINTF("Factory reset\n");
        factory_reset = false;
        // Send the reboot command
        SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_RESET);
        sleep_ms(1000);
        reset_deviceAndEraseFlash();
      }
    }

    // Check remote commands
    term_loop();
  }
}

void mngr_schedule_reset(int seconds) {
  DPRINTF("Rebooting in %d seconds\n", seconds);
  device_reset = true;
  reboot_time = make_timeout_time_ms(seconds * 1000);
}

void mngr_schedule_factory_reset(int seconds) {
  DPRINTF("Factory reset in %d seconds\n", seconds);
  factory_reset = true;
  factory_reset_time = make_timeout_time_ms(seconds * 1000);
}

void mngr_loop() {}
