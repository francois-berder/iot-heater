#include "commissioned.h"
#include "settings.h"
#include "uncommissioned.h"
#include "version.h"

bool commissioned;

void setup()
{
    Serial.begin(115200);

    {
        char buffer[128];
        sprintf(buffer, "\nHeater controller (serial: %08X) ", ESP.getChipId());
        Serial.println(buffer);
    }
    Serial.println("Firmware version: " FW_VERSION);

    commissioned = settings_load();

    if (commissioned) {
        Serial.println("Entering commissioned mode");
        setup_commissioned();
    } else {
        Serial.println("Entering uncommissioned mode");
        setup_uncommissioned();
    }
}

void loop()
{
    if (commissioned)
        loop_commissioned();
    else
        loop_uncommissioned();
}
