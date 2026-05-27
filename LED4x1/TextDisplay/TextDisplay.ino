/*
GND -> G
CLK -> D5 
DIN -> D7
CS  -> D8 (can change)

LED function:
  - text : for text display
  - light: for adjust bright, 0-15
*/

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN D8 // LED Pic

MD_Parola P(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void LED(const char* text, int light) {
  P.begin();              // start LED
  P.displayClear();       // clear LED
  P.setIntensity(light);  // 0-15
  P.displayText(text, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayAnimate();
}

void setup() {
  
    // 0-15
  LED("Hi", 1);
}

void loop() {
  delay(2000);
  LED("My", 1);
  delay(2000);
  LED("Name", 1);
  delay(2000);
  LED("Is", 1);
  delay(2000);
  LED("VISAL", 1);
}
