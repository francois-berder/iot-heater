#include "board.h"
#include "commissioned.h"
#include "Arduino.h"

static void button_isr()
{
    /* @todo Not yet implemented */
}

void setup_commissioned()
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);

    digitalWrite(LED1_PIN, 1);
    digitalWrite(LED2_PIN, 0);

    pinMode(BUTTON_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr, CHANGE);
}

void loop_commissioned()
{
    /* @todo Not yet implemented */
}
