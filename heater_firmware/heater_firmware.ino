#include "commissioned.h"
#include "settings.h"
#include "uncommissioned.h"

bool commissioned;

void setup()
{
    Serial.begin(115200);

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
