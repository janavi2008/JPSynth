#include <ESP_I2S.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "samples.h"
const int16_t *Data[8] = {kick_data, hihat_closed_data, crash_data, clap_data, snare_data, tom1_data, tom2_data, tom3_data};
const int DrumPin[8] = {1, 2, 3, 4, 5, 6, 7, 8}; //the drums are -kick, hi hat, crash, clap, snare, tom1, tom2, tom3
const int DataLength[8] = {kick_len, hihat_closed_len, crash_len, clap_len, snare_len, tom1_len, tom2_len, tom3_len};

#define SDA_PIN 9
#define SCL_PIN 10
#define rotA 15
#define rotB 16
#define DIN 11
#define BCLK 12
#define LRC 13
#define rotButton 14
#define Push1 43
#define Push2 44

Adafruit_VL53L0X sensor;
ESP32Encoder encoder;
I2SClass i2s;
enum drumstate {Reading, HitDetect, Cooldown};
const int sampleRate = 32000;
const int audioBuf = 128;
const int Thresh = 100;
volatile bool hihat = false;
volatile float pitch = 1.0;
const int scanLen = 3;
const int Cool = 30;
volatile float maxVol = 0.2;
volatile float SmoothDist = 200;

struct piezo{
  drumstate state;
  float base;
  int peak;
  unsigned long hitStart;
  unsigned long coolEnd;
  const int16_t *data;
  int length;
  float position;
  float vol;
  bool on;
};
piezo drums[8];

void Calibration(){
  for (byte i = 0; i < 8; i++){
    unsigned long total = 0;
    for(byte j = 0; j < 200; j++){
      total += analogRead(DrumPin[i]);
      delayMicroseconds(200);    }
    drums[i].base = total/200;
    drums[i].state = Reading;
    drums[i].peak = 0;
    drums[i].hitStart = 0;
    drums[i].coolEnd = 0;
  }
}

int detectDrum(piezo &p, int reading){
  unsigned long now = millis();
  int level = abs(reading - (int)p.base);
  switch (p.state){
    case Reading:
      if (level > Thresh){
        p.state = HitDetect;
        p.peak = level;
        p.hitStart = now;
      }  break;
    case HitDetect:
      if (level > p.peak){
        p.peak = level;
      }
      if (now - p.hitStart > scanLen){
        p.state = Cooldown;
        p.coolEnd = now + Cool;
        return p.peak;
      }     break;
    case Cooldown:
      if ((long)(now - p.coolEnd) >= 0){
        p.state = Reading;
      }   break;
  }
  return 0;
}

float netVelocity(int peak){
  float vel = peak - Thresh;
  vel = vel/1000;
  vel = constrain(vel, 0.0, 1.0);
  return vel;
}

void startSound(int pad, float vel){
  drums[pad].on = false;
  if (pad == 1 && hihat){
    drums[pad].data = hihat_open_data;
    drums[pad].length = hihat_open_len;
  } else{
    drums[pad].data = Data[pad];
    drums[pad].length = DataLength[pad];
  }
  drums[pad].vol = vel;
  drums[pad].position = 0;
  drums[pad].on = true;
}


void convertAudio(int16_t *out, int numSamples){
  for (int n = 0; n < numSamples; n++){
    int32_t total = 0;
    for (byte i = 0; i < 8; i++){
      if (!drums[i].on) continue;
      int pos = (int)(drums[i].position);
      total += drums[i].data[pos] * drums[i].vol;
      drums[i].position += pitch;
      if(drums[i].position >= drums[i].length){
        drums[i].on = false;
      }      
    } 
    total = total * maxVol;
    total = constrain(total, -32768, 32767);
    out[n] = total;
  }
}
void playAudio(void *para){
  int16_t mono[audioBuf];
  int16_t stereo[audioBuf * 2];
  while (true){
    convertAudio(mono, audioBuf);
    for (byte i = 0; i < audioBuf; i++){
      stereo[2*i] = mono[i];
      stereo[2*i + 1] = mono[i];
    }
    i2s.write((uint8_t*)stereo, sizeof(stereo));
  }
}


void setup() {
  Calibration();
  for (byte i = 0; i < 8; i++){
    drums[i].on = false;
    drums[i].position = 0;
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  sensor.begin();
  sensor.startRangeContinuous();
  pinMode(Push1, INPUT_PULLUP);
  pinMode(Push2, INPUT_PULLUP);
  encoder.attachHalfQuad(rotA, rotB);
  encoder.setCount(20);

  i2s.setPins(BCLK, LRC, DIN);
  i2s.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
  xTaskCreatePinnedToCore(playAudio, "audio", 4096, NULL, 3, NULL, 0);
}

void loop() {
  // put your main code here, to run repeatedly:
  long rotVol = encoder.getCount();
  hihat = !digitalRead(Push1);
  rotVol = constrain(rotVol, 0, 60);
  encoder.setCount(rotVol);
  maxVol = rotVol/100.0;

  if (sensor.isRangeComplete()){
    int dist = sensor.readRange();
    if (dist < 400){
      SmoothDist +=   0.2 * (dist - SmoothDist);
      float t =(SmoothDist-50.0)/250.0;
      t = constrain(t, 0.0, 1.0);
      float semitones = -12.0 + t * 24.0;
      pitch = pow(2.0, semitones/12.0);
    } else{
      pitch = pitch + 0.1*(1.0 - pitch);
    }
  }
  for (byte i = 0; i < 8; i++){
    int padRead = detectDrum(drums[i], analogRead(DrumPin[i]));
    if (padRead > 0){
      startSound(i, netVelocity(padRead));
    }
  }
}


