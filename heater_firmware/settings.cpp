#include "settings.h"
#include <EEPROM.h>
#include <string.h>

#define EEPROM_LEN  (512)

#define MAGIC0      (0xf1772e9a)
#define MAGIC1      (0x86b81fcb)

static struct settings_t settings;

static bool is_valid(void)
{
    return settings.magic0 == MAGIC0 && settings.magic1 == MAGIC1;
}

bool settings_load(void)
{
    uint8_t *dst = (uint8_t *)&settings;
    int addr = 0;
    bool valid;

    EEPROM.begin(EEPROM_LEN);

    /* Read settings from EEPROM */
    while (addr < sizeof(settings)) {
      *dst++ = EEPROM.read(addr);
      ++addr;
    }

    valid = is_valid();

    if (!valid)
        memset(&settings, 0, sizeof(settings));

    return valid;
}

void settings_erase(void)
{
    /* Erase EEPROM */
    for (int i = 0; i < EEPROM_LEN; ++i)
        EEPROM.write(i, 0xFF);
    EEPROM.commit();

    memset(&settings, 0, sizeof(settings));
}

void settings_create(String &name, String &ssid, String &password, String &basestation)
{
    /* Erase EEPROM */
    for (int i = 0; i < EEPROM_LEN; ++i)
        EEPROM.write(i, 0xFF);
    EEPROM.commit();

    /* Init settings */
    memset(&settings, 0, sizeof(settings));
    settings.magic0 = MAGIC0;
    settings.magic1 = MAGIC1;

    name.toCharArray(settings.name, sizeof(settings.name));
    ssid.toCharArray(settings.ssid, sizeof(settings.ssid));
    password.toCharArray(settings.password, sizeof(settings.password));
    basestation.toCharArray(settings.basestation, sizeof(settings.basestation));

    /* Write settings */
    uint8_t *src = (uint8_t *)&settings;
    int addr = 0;
    while (addr < sizeof(settings)) {
      EEPROM.write(addr, *src++);
      addr++;
    }
    EEPROM.commit();
}

void settings_get_name(char *name)
{
    if (is_valid())
        memcpy(name, settings.name, sizeof(settings.name));
    else
        memset(name, 0, sizeof(settings.name));
}

void settings_get_ssid(char *ssid)
{
    if (is_valid())
        memcpy(ssid, settings.ssid, sizeof(settings.ssid));
    else
        memset(ssid, 0, sizeof(settings.ssid));
}

void settings_get_password(char *password)
{
    if (is_valid())
        memcpy(password, settings.password, sizeof(settings.password));
    else
        memset(password, 0, sizeof(settings.password));
}

void settings_get_basestation(char *basestation)
{
    if (is_valid())
        memcpy(basestation, settings.basestation, sizeof(settings.basestation));
    else
        memset(basestation, 0, sizeof(settings.basestation));
}


bool settings_check()
{
    if (strlen(settings.name) == 0 || strlen(settings.ssid) == 0 || strlen(settings.basestation) == 0)
        return false;

    for (int i = 0; i < strlen(settings.name); ++i) {
        bool is_char_valid = ('a' <= settings.name[i] && settings.name[i] <= 'z')
                          || ('A' <= settings.name[i] && settings.name[i] <= 'Z')
                          || ('0' <= settings.name[i] && settings.name[i] <= '9');
        if (!is_char_valid)
            return false;
    }

    /* Note that empty password is allowed to support open wifi */

    return is_valid();
}
