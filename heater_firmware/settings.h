#ifndef SETTINGS_H
#define SETTINGS_H

#include "Arduino.h"
#include <stdint.h>

struct __attribute__((packed)) settings_t {
    uint32_t magic0;
    char name[32];
    char ssid[64];
    char password[64];
    uint32_t magic1;
};

/**
 * @brief Load settings from EEPROM
 *
 * Note that EEPROM must be initialized before
 * calling this function.
 *
 * @return True if it contains a valid, false otherwise
 */
bool settings_load(void);

/**
 * @brief Erase settings
 */
void settings_erase(void);

/**
 * @brief Create and save it to EEPROM
 *
 * @param[in] name
 * @param[in] ssid
 * @param[in] password
 */
void settings_create(String &name, String &ssid, String &password);

/**
 * @brief Get name
 *
 * @param[out] name 32 char array
 */
void settings_get_name(char *name);

/**
 * @brief Get name
 *
 * @param[out] name 64 char array
 */
void settings_get_ssid(char *ssid);

/**
 * @brief Get name
 *
 * @param[out] name 64 char array
 */
void settings_get_password(char *password);

#endif
