#include "SPI/sd_card_spi.h"
#include "hw_config.h"
#include "my_debug.h"
#include "pico/mutex.h"
#include "sd_card.h"

static bool driver_initialized;

static void set_drive_prefix(sd_card_t *sd_card_p, size_t drive_num) {
#if FF_STR_VOLUME_ID == 0
  myASSERT(drive_num < 10);
  sd_card_p->state.drive_prefix[0] = (char)('0' + drive_num);
  sd_card_p->state.drive_prefix[1] = ':';
  sd_card_p->state.drive_prefix[2] = '\0';
#elif FF_STR_VOLUME_ID == 1
  size_t idx = 0;
  while ((idx + 1 < sizeof(sd_card_p->state.drive_prefix)) &&
         (VolumeStr[drive_num][idx] != '\0')) {
    sd_card_p->state.drive_prefix[idx] = VolumeStr[drive_num][idx];
    idx++;
  }
  myASSERT(idx + 1 < sizeof(sd_card_p->state.drive_prefix));
  sd_card_p->state.drive_prefix[idx++] = ':';
  sd_card_p->state.drive_prefix[idx] = '\0';
#elif FF_STR_VOLUME_ID == 2
  size_t idx = 0;
  myASSERT(sizeof(sd_card_p->state.drive_prefix) > 2);
  sd_card_p->state.drive_prefix[idx++] = '/';
  while ((idx + 1 < sizeof(sd_card_p->state.drive_prefix)) &&
         (VolumeStr[drive_num][idx - 1] != '\0')) {
    sd_card_p->state.drive_prefix[idx] = VolumeStr[drive_num][idx - 1];
    idx++;
  }
  myASSERT(idx < sizeof(sd_card_p->state.drive_prefix));
  sd_card_p->state.drive_prefix[idx] = '\0';
#else
#error "Unknown FF_STR_VOLUME_ID"
#endif
}

void sd_lock(sd_card_t *sd_card_p) {
  myASSERT(mutex_is_initialized(&sd_card_p->state.mutex));
  mutex_enter_blocking(&sd_card_p->state.mutex);
}

void sd_unlock(sd_card_t *sd_card_p) {
  myASSERT(mutex_is_initialized(&sd_card_p->state.mutex));
  mutex_exit(&sd_card_p->state.mutex);
}

bool sd_is_locked(sd_card_t *sd_card_p) {
  uint32_t owner_out;

  myASSERT(mutex_is_initialized(&sd_card_p->state.mutex));
  return !mutex_try_enter(&sd_card_p->state.mutex, &owner_out);
}

bool sd_card_detect(sd_card_t *sd_card_p) {
  if (!sd_card_p->use_card_detect) {
    sd_card_p->state.m_Status &= (DSTATUS)~STA_NODISK;
    return true;
  }

  if (gpio_get(sd_card_p->card_detect_gpio) == sd_card_p->card_detected_true) {
    sd_card_p->state.m_Status &= (DSTATUS)~STA_NODISK;
    return true;
  }

  sd_card_p->state.m_Status |= (STA_NODISK | STA_NOINIT);
  sd_card_p->state.card_type = SDCARD_NONE;
  return false;
}

char const *sd_get_drive_prefix(sd_card_t *sd_card_p) {
  myASSERT(driver_initialized);
  if (!sd_card_p) {
    return "";
  }
  return sd_card_p->state.drive_prefix;
}

bool sd_init_driver(void) {
  auto_init_mutex(initialized_mutex);
  mutex_enter_blocking(&initialized_mutex);

  bool ok = true;

  if (!driver_initialized) {
    myASSERT(sd_get_num());

    for (size_t i = 0; i < sd_get_num(); ++i) {
      sd_card_t *sd_card_p = sd_get_by_num(i);
      if (!sd_card_p) {
        continue;
      }

      myASSERT(sd_card_p->type);

      if (!mutex_is_initialized(&sd_card_p->state.mutex)) {
        mutex_init(&sd_card_p->state.mutex);
      }
      sd_lock(sd_card_p);

      sd_card_p->state.m_Status = STA_NOINIT;
      set_drive_prefix(sd_card_p, i);

      if (sd_card_p->use_card_detect) {
        if (sd_card_p->card_detect_use_pull) {
          if (sd_card_p->card_detect_pull_hi) {
            gpio_pull_up(sd_card_p->card_detect_gpio);
          } else {
            gpio_pull_down(sd_card_p->card_detect_gpio);
          }
        }
        gpio_init(sd_card_p->card_detect_gpio);
      }

      switch (sd_card_p->type) {
        case SD_IF_SPI:
          myASSERT(sd_card_p->spi_if_p);
          myASSERT(sd_card_p->spi_if_p->spi);
          sd_spi_ctor(sd_card_p);
          if (!my_spi_init(sd_card_p->spi_if_p->spi)) {
            ok = false;
          }
          sd_go_idle_state(sd_card_p);
          break;
        case SD_IF_NONE:
        case SD_IF_SDIO:
        default:
          myASSERT(false);
          ok = false;
          break;
      }

      sd_unlock(sd_card_p);
    }

    driver_initialized = true;
  }

  mutex_exit(&initialized_mutex);
  return ok;
}
