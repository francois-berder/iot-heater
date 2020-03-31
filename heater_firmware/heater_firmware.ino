#include "commissioned.h"
#include "settings.h"
#include "uncommissioned.h"

#define xstr(s) str(s)
#define str(s) #s

#ifndef GIT_HASH
#define GIT_HASH    "unknown"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME  "unknown"
#endif

bool commissioned;

void setup()
{
    Serial.begin(115200);
    Serial.println("\nHeater controller - " xstr(GIT_HASH) " - " xstr(BUILD_TIME));

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
