#include "commissioned.h"
#include "settings.h"
#include "uncommissioned.h"

bool commissioned;

void setup()
{
    commissioned = settings_load();

    if (commissioned)
        setup_commissioned();
    else
        setup_uncommissioned();
}

void loop()
{
    if (commissioned)
        loop_commissioned();
    else
        loop_uncommissioned();
}
