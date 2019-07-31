#include "MIDIUSB.h"

const int softpot_pin = A0;
const int channel_pin = A1;
const int recording_pin = 9;
const int recording_indicator_pin = 8;
bool isRecording = false;
bool canCheckRecord = true;
int num_recorded_notes = 0;
int rec_timestamp = 0;
int prevNote = 0;
int channel = 0;
int noteMap[25][2] = {{349, 48}, {359, 49}, {371, 50}, {382, 51}, {395, 52}, {409, 53},
                {423, 54}, {438, 55}, {455, 56}, {470, 57}, {490, 58}, {510, 59},
                {531, 60}, {556, 61}, {580, 62}, {609, 63}, {640, 64}, {675, 65},
                {710, 66}, {753, 67}, {800, 68}, {850, 69}, {910, 70}, {975, 71}, {1024, 72}};

int track1[300][3];
int track2[300][3];

int track1_length = 0;
bool track1_ready = false;
unsigned long track1_start = 0;
int currNote = 0;


void setup() {
  pinMode(softpot_pin, INPUT);
  pinMode(channel_pin, INPUT);
  pinMode(recording_pin, INPUT_PULLUP);
  pinMode(recording_indicator_pin, OUTPUT);
  Serial.begin(9600);
  controlChange(0, 7, 1);
  MidiUSB.flush();
  
}

void loop() {

  
    
    int softpot = analogRead(softpot_pin);
    if (softpot != 0) {
      int note = checkNote(softpot);
      if (prevNote != note) {
        noteOn(channel, note, 64);
        allNotesOff(note, channel);
        if (isRecording && num_recorded_notes < 300) {
          track1[num_recorded_notes][0] = channel;
          track1[num_recorded_notes][1] = note;
          track1[num_recorded_notes][2] = millis() - rec_timestamp;
          num_recorded_notes++;
        }
      }
      prevNote = note;
    }
    else {
      allNotesOff(0, channel);
      prevNote = 0;
      if (isRecording && num_recorded_notes < 300) {
        track1[num_recorded_notes][0] = 100; // absurd channel number to indicate all notes off
      }
    }
    if (track1_ready) {
      if (millis() - track1_start >= track1[currNote][2]) {
        if (track1[currNote][0] == 100) {
          allNotesOff(0, 0);
          //Serial.println("Silence");
        }
        else {
          if (track1[currNote][1] != 0) {
            noteOn(track1[currNote][0], track1[currNote][1], 64);
            allNotesOff(track1[currNote][1], track1[currNote][0]);
          }
        }
        currNote++;
      }
      if ((unsigned long)(millis() - track1_start) >= track1_length) {
        currNote = 0;
        track1_start = millis();
      }
    }
    
    MidiUSB.flush();

    checkRecordingStatus(isRecording);
    digitalWrite(recording_indicator_pin, isRecording);

    int channel_data = analogRead(channel_pin);
    channel = map(channel_data, 0, 1023, 0, 5);
    if (channel_data > 1000) channel = 15;
    delay(50);


}

//////////////////////////////////////////////////
//////////// MIDIUSB control functions //////////

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

///////////////////////////// END Midi USB control functions ////////////////
////////////////////////////////////////////////////////////////////////////

// Turns off all notes EXCEPT the parameter
void allNotesOff(int on, int ch) {
  for (int i = 0; i < 25; i++) {
    if (noteMap[i][1] != on) {
      noteOff(ch, noteMap[i][1], 64);
    }
  }
}

int checkNote(int pot) {
  int i = 24;
  int num = noteMap[i][0];
  while ((pot < num) && (i > 0)) {
    i--;
    num = noteMap[i][0];
  }
  if (pot < 349) return noteMap[i][1];
  return noteMap[i + 1][1];
}

void checkRecordingStatus(bool wasRecording) {
  int button_pressed = digitalRead(recording_pin);
  if (millis() - rec_timestamp > 250 && canCheckRecord) {
    if (wasRecording == false && button_pressed == 0) {
      isRecording = true;
      rec_timestamp = millis();
      canCheckRecord = false;
    }
    if (wasRecording == true && button_pressed == 0) {
      isRecording = false;
      track1_length = millis() - rec_timestamp;
      track1_ready = true;
      track1_start = millis();
      rec_timestamp = millis();
      canCheckRecord = false;
    }
  }
  if (button_pressed == 1) {
    canCheckRecord = true;
  }
}
