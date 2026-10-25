#include <ESP_I2S.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "samples.h"

const int piezoPin[8] = {1, 2, 3, 4, 5, 6, 7, 8}; //kick, hi-hat, crash, clap, snare, tom1, tom2, tom3
const int16_t *sampleData[8] = {kick_data, hihat_closed_data, crash_data, clap_data, snare_data, tom1_data, tom2_data, tom3_data};
const int sampleLength[8] = {kick_len, hihat_closed_len, crash_len, clap_len, snare_len, tom1_len, tom2_len, tom3_len};

#define SDA_PIN 9
#define SCL_PIN 10
#define DIN 11
#define BCLK 12
#define LRC 13
#define rotButton 14
#define rotA 15
#define rotB 16
#define Push1 43
#define Push2 44

const int sampleRate = 32000;
const int audioBuffer = 128;
const int piezoThresh = 100;
const int scanLength = 3;
const int piezoCool = 30;
volatile bool hihatOpen = false;
volatile float pitchBend = 1.0;
volatile float maxVol = 0.2;
volatile float smoothDist = 200;

Adafruit_VL53L0X sensor;
ESP32Encoder encoder;
I2SClass i2s;

enum piezoState {Reading, HitDetect, Cooldown};

struct piezo{
  piezoState state;
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

piezo piezos[8];

void calibratePiezo(){
  for (byte i = 0; i < 8; i++){
    unsigned long total = 0;
    for(byte j = 0; j < 200; j++){
      total = total + analogRead(piezoPin[i]);
      delayMicroseconds(200);
    }
    piezos[i].base = total/200;
    piezos[i].state = Reading;
    piezos[i].peak = 0;
    piezos[i].hitStart = 0;
    piezos[i].coolEnd = 0;
  }
}

int checkPiezo(piezo &p, int reading){
  unsigned long now = millis();
  int level = abs(reading - (int)p.base);
  switch (p.state){
    case Reading:
      if (level > piezoThresh){
        p.state = HitDetect;
        p.peak = level;
        p.hitStart = now;
      }
      break;
    case HitDetect:
      if (level > p.peak){
        p.peak = level;
      }
      if (now - p.hitStart > scanLength){
        p.state = Cooldown;
        p.coolEnd = now + piezoCool;
        return p.peak;
      }
      break;
    case Cooldown:
      if ((long)(now - p.coolEnd) >= 0){
        p.state = Reading;
      }    
      break;
  }
  return 0;
}

float netVelocity(int peak){
  float vel = peak - piezoThresh;
  vel = vel/1000.0;
  vel = constrain(vel, 0.0, 1.0);
  return vel;
}

void startSound(int pad, float vel){
  piezos[pad].on = false;
  if (pad == 1 && hihatOpen){
    piezos[pad].data = hihat_open_data;
    piezos[pad].length = hihat_open_len;
  } else{
    piezos[pad].data = sampleData[pad];
    piezos[pad].length = sampleLength[pad];
  }

  piezos[pad].vol = vel;
  piezos[pad].position = 0;
  piezos[pad].on = true;

}

void convertAudio(int16_t *out, int numSamples){
  for (int n = 0; n < numSamples; n++){
    int32_t total = 0;
    for (byte i = 0; i < 8; i++){
      if (!piezos[i].on) continue;
      int pos = (int)(piezos[i].position);
      total += piezos[i].data[pos] * piezos[i].vol;
      piezos[i].position += pitchBend;
      if(piezos[i].position >= piezos[i].length){
        piezos[i].on = false;
      }      
    } 
    total = total * maxVol;
    total = constrain(total, -32768, 32767);
    out[n] = total;
  }
}

void playAudio(void *param){
  int16_t mono[audioBuffer];
  int16_t stereo[audioBuffer * 2];
  while (true){
    convertAudio(mono, audioBuffer);
    for (byte i = 0; i < audioBuffer; i++){
      stereo[2*i] = mono[i];
      stereo[2*i + 1] = mono[i];
    }
    i2s.write((uint8_t*)stereo, sizeof(stereo));
  }
}

void setup() {
  calibratePiezo();
  for (byte i = 0; i < 8; i++){
    piezos[i].on = false;
    piezos[i].position = 0;
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

  hihatOpen = !digitalRead(Push1);
  long rotVol = encoder.getCount();
  rotVol = constrain(rotVol, 0, 60);
  encoder.setCount(rotVol);

  maxVol = rotVol/100.0;

  if (sensor.isRangeComplete()){
    int dist = sensor.readRange();
    if (dist < 400){
      smoothDist =  smoothDist + 0.2 * (dist - smoothDist);
      float t =(SmoothDist -50.0)/250.0;
      t = constrain(t, 0.0, 1.0);
      float semitones = -12.0 + t * 24.0;
      pitchBend = pow(2.0, semitones/12.0);
    } else{
      pitchBend = pitchBend + 0.1*(1.0 - pitchBend);
    }
  }

  for (byte i = 0; i < 8; i++){
    int padRead = checkPiezo(piezos[i], analogRead(piezoPin[i]));
    if (padRead > 0){
      startSound(i, netVelocity(padRead));
    }
  }

}



