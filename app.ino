#define ARDUINO 101

#include <FastLED.h>
#include <LowPower.h>

// Uncomment for debugging functions.
//#define DEBUG

#include "defines.hpp"
#include "globals.hpp"
#include "types.hpp"
#include "led_funcs.hpp"

void onWakeup()
{
}

void setup()
{
    // Setup pins.
    pinMode(LED_BUILTIN,                   OUTPUT);
    pinMode(LED_STRIP_PIN,                 OUTPUT);

    digitalWrite(LED_STRIP_PIN, 0);

    // Setup serial.
#ifdef DEBUG
    Serial.begin(9600);
    while (!Serial) {;}
#endif

    // Setup FastLED.
    FastLED.addLeds<WS2812B, LED_STRIP_PIN>(Globals::leds, LED_STRIP_LEDS);

    // Setup globals.
    Globals::deltaTimeMS              = 0;
    Globals::lightState.state         = LightStateState::Resting;
    Globals::lightState.stateLastTick = LightStateState::Off;
    Globals::lightState.timer         = 0;
    Globals::lightingFuncs[0]         = &lightingRainbow;
    Globals::lightingFuncs[1]         = &lightingStepping;

#ifdef DEBUG
    Serial.println("Finished Setup");
#endif

    // Make the random seed a bit more random, by reading from a floating analog input.
    pinMode(A0, INPUT);
    for(int i = 0; i < 1000 / 50; i++)
    {
        delay(50);
        Globals::randomSeed += analogRead(A0);
    }

    randomSeed(Globals::randomSeed);

    // Just to make sure all the wiring is ok, and to get a visualisation of the seed.
    LED::setAll(CRGB(Globals::randomSeed % 255, 128, 128));
}

void handleDeltaTime()
{
    static unsigned int prevTime = 0;
    unsigned int currTime = millis();

    // Handle when it overflows, though it'll probably need a battery change before then.
    if(currTime < prevTime)
        prevTime = 0;

    Globals::deltaTimeMS = currTime - prevTime;
    prevTime = currTime;
}

void doStateMachine()
{
    static LightingFunc selectedFunc = nullptr;

    switch(Globals::lightState.state)
    {
        case LightStateState::Resting:
            Globals::lightState.timer -= Globals::deltaTimeMS;

            if(Globals::deltaTimeMS <= 0)
            {
                Globals::lightState.state = LightStateState::Starting;
                Globals::lightState.timer = LED_STRIP_TIME_ON_MS;
            }
            break;

        case LightStateState::Off:
            digitalWrite(LED_BUILTIN, LOW);
            LED::setAll(CRGB(0));

            // Sleep (to conserve power) before we check again.
#ifndef DEBUG
            LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
#endif
            break;

        case LightStateState::Starting:
            if(selectedFunc == nullptr)
                selectedFunc = Globals::lightingFuncs[random(0, LIGHTING_FUNCS_COUNT)];

            if(selectedFunc(LightingFuncState::Start)) // True = starting animation is done.
                Globals::lightState.state = LightStateState::On;
            break;

        case LightStateState::On:
            digitalWrite(LED_BUILTIN, HIGH);

            Globals::lightState.timer -= Globals::deltaTimeMS;
            selectedFunc(LightingFuncState::Step);

            if(Globals::lightState.timer <= 0)
                Globals::lightState.state = LightStateState::Ending;
            break;

        case LightStateState::Ending:
            if(selectedFunc(LightingFuncState::End)) // True = starting animation is done.
            {
                selectedFunc = nullptr;
                Globals::lightState.state = LightStateState::Off;
                LED::setAll(CRGB(0));
            }

            break;

        default:
            break;
    }
}

void loop()
{
    handleDeltaTime();
    doStateMachine();

    delay(CYCLE_DELAY_MS);
}

/* LIGHTING FUNCTIONS */
bool lightingRainbow(LightingFuncState state)
{
    if(state == LightingFuncState::Start)
        return true;

    if(state == LightingFuncState::Step)
    {
        LED::doRainbow();
        return false; // Doesn't matter for this state.
    }

    if(state == LightingFuncState::End)
    {
        LED::doRainbow();
        return LED::doFadeout();
    }

    return false;
}

bool lightingStepping(LightingFuncState state)
{
    if(state == LightingFuncState::Start)
    {
        FastLED.setBrightness(128);
        return LED::doStepdownAnimation(CRGB(128, 128, 128));
    }

    if(state == LightingFuncState::Step)
        return false;

    if(state == LightingFuncState::End)
    {
        bool done = LED::doStepupAnimation(CRGB(0, 0, 0));
        if(done)
            FastLED.setBrightness(255);

        return done;
    }

    return false;
}