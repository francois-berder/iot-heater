#ifndef SETTINGS_H
#define SETTINGS_H

#include "Arduino.h"
#include <stdint.h>

struct __attribute__((packed)) settings_t {
    uint32_t magic0;
    char name[32];
    char ssid[64];
    char password[64];
    char basestation[32];
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
 * @param[in] basestation
 */
void settings_create(String &name, String &ssid, String &password, String &basestation);

/**
 * @brief Get name
 *
 * @param[out] name 32 char array
 */
void settings_get_name(char *name);

/**
 * @brief Get ssid
 *
 * @param[out] ssid 64 char array
 */
void settings_get_ssid(char *ssid);

/**
 * @brief Get name
 *
 * @param[out] password 64 char array
 */
void settings_get_password(char *password);

/**
 * @brief Get basestation hostname or IP address
 *
 * @param[out] basestation 32 char array
 */
void settings_get_basestation(char *basestation);

/**
 * @brief Quick settings check
 *
 * @return True if settings are valid, false otherwise
 */
bool settings_check();

#endif
