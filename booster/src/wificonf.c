/**
 * File: wificonf.c
 * Author: Diego Parrilla Santamaría
 * Date: September 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The .wificonf file on the microSD card (EPIC-10, D-09)
 */

#include "wificonf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ff.h"
#include "gconfig.h"

// Trim leading and trailing blanks in place; returns the new start.
static char *wificonf_trimBlanks(char *text) {
  while (*text == ' ' || *text == '\t') {
    text++;
  }
  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
    text[--len] = '\0';
  }
  return text;
}

// Boolean grammar of TCPIP_DHCP (D-09): 1/True/Yes/T/Y and 0/False/No/F/N,
// case-insensitive. Anything else is WIFICONF_DHCP_UNSET, which the caller
// treats as if the line were absent. Deliberately stricter than the Manager
// form, where anything not starting with y or t counts as false: a typo in
// this file must not silently disable DHCP.
static wificonf_dhcp_t wificonf_parseBool(const char *value) {
  static const char *const onWords[] = {"1", "true", "yes", "t", "y"};
  static const char *const offWords[] = {"0", "false", "no", "f", "n"};
  for (size_t i = 0; i < sizeof(onWords) / sizeof(onWords[0]); i++) {
    if (strcasecmp(value, onWords[i]) == 0) {
      return WIFICONF_DHCP_ON;
    }
  }
  for (size_t i = 0; i < sizeof(offWords) / sizeof(offWords[0]); i++) {
    if (strcasecmp(value, offWords[i]) == 0) {
      return WIFICONF_DHCP_OFF;
    }
  }
  return WIFICONF_DHCP_UNSET;
}

// Store one validated TCPIP_* value, or record the failure. An invalid line
// empties the field and flags the whole block, so the operator gets the
// stored setting and a log line naming the key rather than half of what they
// wrote.
static void wificonf_parseTcpipValue(wificonf_t *cfg, const char *key,
                                     const char *value,
                                     bool (*parse)(const char *, char *),
                                     char *out) {
  if (parse(value, out)) {
    return;
  }
  out[0] = '\0';
  cfg->tcpipInvalid = true;
  DPRINTF("Invalid %s in %s: '%s'\n", key, WIFICONF_FILE, value);
}

// One line, "KEY=value". Keys are matched case-insensitively; blank lines and
// lines starting with ';' or '#' are comments; unknown keys are ignored. The
// three WiFi keys receive the text after '=' untouched, exactly as the
// factory-mode parser did before EPIC-10; the TCPIP_* values are trimmed and
// validated.
void wificonf_parseLine(wificonf_t *cfg, char *line) {
  // Trim trailing newline/carriage return
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    line[--len] = '\0';
  }

  char *start = line;
  while (*start == ' ' || *start == '\t') {
    start++;
  }
  if (*start == '\0' || *start == ';' || *start == '#') {
    return;
  }

  char *separator = strchr(start, '=');
  if (separator == NULL) {
    DPRINTF("Ignoring line without '=' in %s: %s\n", WIFICONF_FILE, start);
    return;
  }
  *separator = '\0';
  const char *key = wificonf_trimBlanks(start);
  char *value = separator + 1;

  if (strcasecmp(key, WIFICONF_KEY_SSID) == 0) {
    char raw[MAX_SSID_LENGTH * 2];
    strncpy(raw, value, sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';
    if (!network_parseSSID(raw, cfg->ssid)) {
      DPRINTF("Invalid SSID in %s\n", WIFICONF_FILE);
    }
    cfg->ssid[MAX_SSID_LENGTH - 1] = '\0';
    cfg->hasSsid = true;
  } else if (strcasecmp(key, WIFICONF_KEY_PASS) == 0) {
    char raw[MAX_PASSWORD_LENGTH * 2];
    strncpy(raw, value, sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';
    if (!network_parsePassword(raw, cfg->pass)) {
      DPRINTF("Invalid password in %s\n", WIFICONF_FILE);
    }
    cfg->pass[MAX_PASSWORD_LENGTH - 1] = '\0';
    cfg->hasPass = true;
  } else if (strcasecmp(key, WIFICONF_KEY_AUTH) == 0) {
    cfg->auth = (uint16_t)atoi(value);
    cfg->hasAuth = true;
  } else if (strcasecmp(key, WIFICONF_KEY_DHCP) == 0) {
    const char *word = wificonf_trimBlanks(value);
    cfg->dhcp = wificonf_parseBool(word);
    if (cfg->dhcp == WIFICONF_DHCP_UNSET) {
      DPRINTF("Invalid %s value '%s' in %s; treating the line as absent\n", key,
              word, WIFICONF_FILE);
    }
  } else if (strcasecmp(key, WIFICONF_KEY_ADDRESS) == 0) {
    wificonf_parseTcpipValue(cfg, key, wificonf_trimBlanks(value),
                             network_parseIPv4Address, cfg->address);
  } else if (strcasecmp(key, WIFICONF_KEY_NETMASK) == 0) {
    wificonf_parseTcpipValue(cfg, key, wificonf_trimBlanks(value),
                             network_parseIPv4Netmask, cfg->netmask);
  } else if (strcasecmp(key, WIFICONF_KEY_GATEWAY) == 0) {
    wificonf_parseTcpipValue(cfg, key, wificonf_trimBlanks(value),
                             network_parseIPv4Address, cfg->gateway);
  } else if (strcasecmp(key, WIFICONF_KEY_DNS) == 0) {
    wificonf_parseTcpipValue(cfg, key, wificonf_trimBlanks(value),
                             network_parseDnsList, cfg->dns);
  } else {
    DPRINTF("Ignoring unknown key in %s: %s\n", WIFICONF_FILE, key);
  }
}

bool wificonf_isPresent(void) {
  FILINFO info = {0};
  return f_stat(WIFICONF_FILE, &info) == FR_OK;
}

bool wificonf_readFile(wificonf_t *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  FIL file;
  FRESULT res = f_open(&file, WIFICONF_FILE, FA_READ);
  if (res != FR_OK) {
    DPRINTF("No %s on the SD card (%d)\n", WIFICONF_FILE, (int)res);
    return false;
  }
  char line[WIFICONF_LINE_MAX];
  while (f_gets(line, sizeof(line), &file) != NULL) {
    wificonf_parseLine(cfg, line);
  }
  f_close(&file);
  DPRINTF(
      "%s: SSID=%s (%s) AUTH=%u (%s) %s=%s address=%s netmask=%s "
      "gateway=%s dns=%s%s\n",
      WIFICONF_FILE, cfg->ssid, cfg->hasSsid ? "set" : "absent",
      (unsigned)cfg->auth, cfg->hasAuth ? "set" : "absent", WIFICONF_KEY_DHCP,
      cfg->dhcp == WIFICONF_DHCP_UNSET ? "absent"
      : cfg->dhcp == WIFICONF_DHCP_ON  ? "on"
                                       : "off",
      cfg->address, cfg->netmask, cfg->gateway, cfg->dns,
      cfg->tcpipInvalid ? " (a TCPIP_* line was invalid)" : "");
  return true;
}

// The three writers below only touch the settings when the stored text
// differs, and say so. That is what lets a card that keeps the file boot
// without a flash write every time.
static bool wificonf_putString(const char *key, const char *value) {
  SettingsConfigEntry *entry = settings_find_entry(gconfig_getContext(), key);
  if (entry != NULL && strcmp(entry->value, value) == 0) {
    return false;
  }
  settings_put_string(gconfig_getContext(), key, value);
  return true;
}

static bool wificonf_putBool(const char *key, bool value) {
  const char *text = "false";
  if (value) {
    text = "true";
  }
  SettingsConfigEntry *entry = settings_find_entry(gconfig_getContext(), key);
  if (entry != NULL && strcmp(entry->value, text) == 0) {
    return false;
  }
  settings_put_bool(gconfig_getContext(), key, value);
  return true;
}

static bool wificonf_putInteger(const char *key, int value) {
  char text[SETTINGS_MAX_VALUE_LENGTH] = {0};
  snprintf(text, sizeof(text), "%d", value);
  SettingsConfigEntry *entry = settings_find_entry(gconfig_getContext(), key);
  if (entry != NULL && strcmp(entry->value, text) == 0) {
    return false;
  }
  settings_put_integer(gconfig_getContext(), key, value);
  return true;
}

// Accumulate "did anything change" without short-circuiting: every writer
// must run, and a plain `changed = f() || changed` is an int in C.
static void wificonf_noteChange(bool *changed, bool didChange) {
  if (didChange) {
    *changed = true;
  }
}

// Apply the TCPIP_* block (D-09). No TCPIP_DHCP line: the stored setting
// stands. TCPIP_DHCP on: stored as such, address lines ignored. TCPIP_DHCP
// off: all or nothing. Address, netmask and gateway must all have parsed and
// no TCPIP_* line may have failed, or the block is dropped and the stored
// setting stands; DNS is optional and the stored resolver stands without it.
// Nothing partial ever reaches the settings, so a wrong static configuration
// leaves the device as it was rather than dark.
static bool wificonf_applyTcpip(const wificonf_t *cfg) {
  switch (cfg->dhcp) {
    case WIFICONF_DHCP_UNSET:
      DPRINTF("No %s in %s. Stored TCP/IP settings stand\n", WIFICONF_KEY_DHCP,
              WIFICONF_FILE);
      return false;
    case WIFICONF_DHCP_ON:
      DPRINTF("DHCP enabled by %s\n", WIFICONF_FILE);
      return wificonf_putBool(PARAM_WIFI_DHCP, true);
    case WIFICONF_DHCP_OFF:
      break;
  }

  const char *missing = NULL;
  if (cfg->address[0] == '\0') {
    missing = WIFICONF_KEY_ADDRESS;
  } else if (cfg->netmask[0] == '\0') {
    missing = WIFICONF_KEY_NETMASK;
  } else if (cfg->gateway[0] == '\0') {
    missing = WIFICONF_KEY_GATEWAY;
  }
  if (missing != NULL) {
    DPRINTF(
        "Static TCP/IP dropped: %s missing or invalid. Stored settings "
        "stand\n",
        missing);
    return false;
  }
  if (cfg->tcpipInvalid) {
    DPRINTF(
        "Static TCP/IP dropped: a TCPIP_* line was invalid. Stored "
        "settings stand\n");
    return false;
  }

  bool changed = false;
  wificonf_noteChange(&changed, wificonf_putBool(PARAM_WIFI_DHCP, false));
  wificonf_noteChange(&changed,
                      wificonf_putString(PARAM_WIFI_IP, cfg->address));
  wificonf_noteChange(&changed,
                      wificonf_putString(PARAM_WIFI_NETMASK, cfg->netmask));
  wificonf_noteChange(&changed,
                      wificonf_putString(PARAM_WIFI_GATEWAY, cfg->gateway));
  if (cfg->dns[0] != '\0') {
    wificonf_noteChange(&changed, wificonf_putString(PARAM_WIFI_DNS, cfg->dns));
  }
  DPRINTF("Static TCP/IP from %s: %s mask %s gateway %s dns %s\n",
          WIFICONF_FILE, cfg->address, cfg->netmask, cfg->gateway,
          cfg->dns[0] != '\0' ? cfg->dns : "(stored)");
  return changed;
}

bool wificonf_applySettings(const wificonf_t *cfg) {
  bool changed = false;
  if (cfg->hasSsid) {
    wificonf_noteChange(&changed,
                        wificonf_putString(PARAM_WIFI_SSID, cfg->ssid));
  }
  if (cfg->hasPass) {
    wificonf_noteChange(&changed,
                        wificonf_putString(PARAM_WIFI_PASSWORD, cfg->pass));
  }
  if (cfg->hasAuth) {
    wificonf_noteChange(&changed,
                        wificonf_putInteger(PARAM_WIFI_AUTH, cfg->auth));
  }
  wificonf_noteChange(&changed, wificonf_applyTcpip(cfg));
  DPRINTF("%s applied; settings %s\n", WIFICONF_FILE,
          changed ? "changed" : "unchanged");
  return changed;
}
