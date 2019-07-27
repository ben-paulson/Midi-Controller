#include "MIDIUSB.h"

const int softpot_pin = A0;
int prevNote = 0;


void setup() {
  pinMode(softpot_pin, INPUT);
  Serial.begin(9600);
  controlChange(0, 7, 1);
  MidiUSB.flush();
  pinMode(5, OUTPUT);
}

void loop() {
  //  int softpot = analogRead(softpot_pin);
  //  int softPotPosition = map(softpot, 0, 1023, 0, 40);
  //
  //  if (softPotPosition != 0 && prevNote != softPotPosition) {
  //    noteOn(0, softPotPosition + 60, 64);
  ////    MidiUSB.flush();
  ////    delay(50);
  //    noteOff(0, prevNote + 60, 64);
  //    MidiUSB.flush();
  //  }
  //
  //  prevNote = softPotPosition;
  //  Serial.println(softpot);
  //
  //  delay(50);
  tone(5, 150);

  /*noteOn(0, 60, 127);
  MidiUSB.flush();
  //  for (int i = -8190; i < 8190; i++) {
  //    pitchBend(0, i);
  //  }
  //  MidiUSB.flush();
  delay(500);
  noteOff(0, 60, 64);
  MidiUSB.flush();
  delay(500);*/


}

//////////////////////////////////////////
//////////// MIDIUSB functions //////////

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

void programChange(byte channel, byte program) {
  midiEventPacket_t pc = {0x0C, 0xC0 | channel, program, 0};
  MidiUSB.sendMIDI(pc);
}

// Value range: -8192 to +8192
void pitchBend(byte channel, int value) {
  unsigned int change = 0x2000 + value;  //  0x2000 = 8192 ---> Convert +/- 8192 to 0 - 16383
  unsigned char low = change & 0x7F;  // Low 7 bits ----> & 127 removes all upper bits
  unsigned char high = (change >> 7) & 0x7F;  // High 7 bits ----> Shift 7 to remove lsb, then & 127 to get only msb

  midiEventPacket_t bend = {0x0E, 0xE0 | channel, low, high};
  MidiUSB.sendMIDI(bend);
}

