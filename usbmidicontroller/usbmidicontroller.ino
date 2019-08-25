#include "MIDIUSB.h"
#include <SPI.h>

const int softpot_pin = A0;
const int channel_pin = A1;
const int attack_pin = A2;
const int decay_pin = A6;
const int sustain_pin = A7;
const int release_pin = A8;
const int lfo_freq_pin = A10;
const int lfo_amp_pin = A3;
const int square_output_pin = 5;
const int recording_pin = 9;
const int recording_indicator_pin = 7;
bool isRecording = false;
bool isWaiting = false;
bool canCheckRecord = true;
int num_recorded_notes = 0;
int rec_timestamp = 0;
int prevNote = 0;
int prevNote_index = 0;
int prevPot = 0;
int channel = 0;
int noteMap[25][2] = {{349, 48}, {359, 49}, {371, 50}, {382, 51}, {395, 52}, {409, 53},
                {423, 54}, {438, 55}, {455, 56}, {470, 57}, {490, 58}, {510, 59},
                {531, 60}, {556, 61}, {580, 62}, {609, 63}, {640, 64}, {675, 65},
                {710, 66}, {753, 67}, {800, 68}, {850, 69}, {910, 70}, {975, 71}, {1024, 72}};

const int SIZE = 300;
int track[SIZE][3]; // [SIZE][channel, note, timestamp]

bool channel_ready[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int track_length = 0;
bool track_ready = false;
unsigned long track_start = 0;
int currNote = 0;

bool play_semitones = false;
int start_pot = 0;

int cs_pin = 3;
unsigned long attack_start = 0;
int last_vol_level = 0;
bool released = false;
unsigned long release_start = 0;
bool decaying = false;
unsigned long decay_start = 0;

void setup() {
  pinMode(softpot_pin, INPUT);
  pinMode(channel_pin, INPUT);
  pinMode(square_output_pin, OUTPUT);
  pinMode(recording_pin, INPUT_PULLUP);
  pinMode(recording_indicator_pin, OUTPUT);
  pinMode(attack_pin, INPUT);
  pinMode(decay_pin, INPUT);
  pinMode(sustain_pin, INPUT);
  pinMode(release_pin, INPUT);
  pinMode(cs_pin, OUTPUT);
  pinMode(lfo_freq_pin, INPUT);
  pinMode(lfo_amp_pin, INPUT);
  SPI.begin();
  Serial.begin(9600);
  
}

void loop() {

  digitalWrite(cs_pin, LOW);
  SPI.transfer(0);

  // Attack
  if (attack_start != 0) {
    released = false;
    int a_duration = 2 * analogRead(attack_pin); // "A" potentiometer
    delay(2);
    float a_done = (float)(millis() - attack_start) / (float)a_duration;
    
    if (a_done <= 1.0) {
      
      last_vol_level = (int)(a_done * 128);
    } else {
      last_vol_level = 128;
      attack_start = 0;
      decaying = true;
      decay_start = millis();
    }
    SPI.transfer(last_vol_level);
  }
  
  // Decay to sustain level
  if (decaying) {
    int sustain_level = map(analogRead(sustain_pin), 0, 1023, 0, 128);
    delay(2);
    int d_duration = analogRead(decay_pin);
    delay(2);
    if (d_duration == 0) d_duration = 10;
    float d_done = (float)(millis() - decay_start) / (float)d_duration; // percent done with decay stage
    if (d_done <= 1.0) {
      last_vol_level = map(d_done * 100.0, 0, 100, 128, sustain_level); // map percent completion to max volume -> sustain level
    } else {
      last_vol_level = sustain_level;
      decaying = false;
    }
    SPI.transfer(last_vol_level);
  }

  // Release
  if (released) {
    decaying = false;
    int r_duration = analogRead(release_pin); // "R" potentiometer
    delay(2);
    float r_done = (float)(millis() - release_start) / (float)r_duration;
    if (r_done <= 1.0) {
      SPI.transfer((int) ((1 - r_done) * last_vol_level)); // Start at previous volume level rather than the max
    } else {
      SPI.transfer(0);
      noTone(square_output_pin);
    }
  }
  
  //int bend = analogRead(joystick_y_pin);
  //Serial.println(map(bend, 0, 1023, -8192, 8192));
  //pitchBend(channel, map(bend, 0, 1023, -8190, 8190));

  //Serial.print(bend); Serial.print(", "); Serial.println(y);
  int softpot = analogRead(softpot_pin);
  delay(2);
  if (softpot < 10) softpot = 0;
  //Serial.println(softpot);
  if (softpot != 0) {
    int note = checkNote(softpot);

    /////////////////// SQUARE WAVE //////////////////////////////////////
    ////////////////////////////////////////////////////////////////////
    
    int noteIndex = 0;
      for (int i = 0; i < 25; i++) {
        if (noteMap[i][1] == note) {
          noteIndex = i;
          break;
        }
      }
    int prev_pot_val;
    if (noteIndex == 0) prev_pot_val = 0;
    else prev_pot_val = noteMap[noteIndex - 1][0];
    
    // Calculate semitones to be added if sliding is enabled
    float semitone = (float)(softpot - prev_pot_val) / (float)(noteMap[noteIndex][0] - prev_pot_val);

    // Calculate LFO variables for vibrato
    float lfo_amplitude = analogRead(lfo_amp_pin) / 500.0;
    delay(2);
    float lfo_freq = map(analogRead(lfo_freq_pin), 0, 1023, 50, 15); // 25 sounds normal, higher = slower; lower = faster
    delay(2);
    float lfo_offset = lfo_amplitude * sin((float)millis() / lfo_freq);
    
    unsigned int midi_to_freq;
    if (play_semitones) {
      midi_to_freq = (unsigned int) (440.0 * (pow(2.0, (float)(((note + semitone + lfo_offset) - 69) / 12.0))));
      if (abs(prevNote_index - noteIndex > 1)) {
        start_pot = softpot;
        SPI.transfer(0);
        attack_start = millis();
      }
    } else {
      midi_to_freq = (unsigned int) (440.0 * (pow(2.0, (float)(((note + lfo_offset) - 69) / 12.0))));
      if (prevNote == 0 || abs(prevNote_index - noteIndex) > 1) {
        start_pot = softpot;
        SPI.transfer(0);
        attack_start = millis();
      }
    }
    tone(square_output_pin, midi_to_freq);

    // Check if finger slides along softpot or if it jumps notes
    if (abs(softpot - start_pot) > ((noteMap[noteIndex][0] - prev_pot_val) * 0.5) && prevNote != 0 && abs(prevNote_index - noteIndex) < 2)
      play_semitones = true;
    if (abs(prevNote_index - noteIndex) > 1) {
      play_semitones = false;
      SPI.transfer(0);
      attack_start = millis();
    }

    /////////////////// END SQUARE WAVE /////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////

    
    if (prevNote != note && !channel_ready[channel]) {
      
      //noteOn(channel, note, 64);    // Live playing
      //allNotesOff(note, channel);
      
      if (isRecording && num_recorded_notes < SIZE) {
        track[num_recorded_notes][0] = channel;      // Recording
        track[num_recorded_notes][1] = note;
        track[num_recorded_notes][2] = millis() - rec_timestamp;
        num_recorded_notes++;
      }
    }
    prevNote = note;
    prevPot = softpot;
    prevNote_index = noteIndex;
  }
  else {
    if (!channel_ready[channel]) {
      // Midi
      //allNotesOff(0, channel);    // Live
      // Set release phase to begin (square wave output only)
      if (prevNote != 0) {
        released = true;
        release_start = millis();
      }
      attack_start = 0;
      play_semitones = false;
      
      prevNote = 0;
      
      if (isRecording && num_recorded_notes < SIZE && track[num_recorded_notes - 1][1] != -1) {
        track[num_recorded_notes][0] = channel; // Recording
        track[num_recorded_notes][1] = -1; // absurd note number to indicate all notes off
        track[num_recorded_notes][2] = millis() - rec_timestamp;
        num_recorded_notes++;
      }
    }
  }
  // Looping
  if (track_ready) {
    // If time has passed when the next note is scheduled to play
    if (millis() - track_start >= track[currNote][2]) {
      // Notes off message
      if (track[currNote][1] == -1) {
        //allNotesOff(0, track[currNote][0]);
      }
      else {
        // All other messages, play only if channel is ready (i.e. not currently recording on the channel)
        if (track[currNote][1] != 0 && channel_ready[track[currNote][0]]) {
          //noteOn(track[currNote][0], track[currNote][1], 64);
          //allNotesOff(track[currNote][1], track[currNote][0]);
        }
      }
      currNote++;
    }
    if ((unsigned long)(millis() - track_start) >= track_length) {
      // All additional tracks beyond the first, so all tracks are the same length
      if (isRecording) {
        isRecording = false;
        sort(track);
        channel_ready[channel] = true;
      }
      // Start 2nd, 3rd, etc. track at the same time as the others repeat so all tracks are the same length
      if (isWaiting) {
        isWaiting = false;
        isRecording = true;
        rec_timestamp = millis();
      }
      // General reset
      currNote = 0;
      track_start = millis();
    }
  }
  
  //MidiUSB.flush();

  checkRecordingStatus(isRecording);
  digitalWrite(recording_indicator_pin, isRecording);

  int channel_data = analogRead(channel_pin);
  //Serial.println(channel_data);
  channel = map(channel_data, 0, 1023, 0, 5);
  if (channel_data > 1000) channel = 15;

  digitalWrite(cs_pin, HIGH);
  delay(10);

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

// Turns off all notes EXCEPT the parameter on the given channel
void allNotesOff(int on, int ch) {
  for (int i = 0; i < 25; i++) {
    if (noteMap[i][1] != on) {
      noteOff(ch, noteMap[i][1], 64);
    }
  }
}

// Map potentiometer input to midi note values
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

void recordNewTrack() {
  isRecording = true;
}

// Insertion sort
void sort(int arr[SIZE][3]) {
  for (int i = 1; i < SIZE; i++) {
    if (arr[i][2] != 0) {
      for (int j = i; j > 0 && arr[j - 1][2] > arr[j][2]; j--) {
        int temp[3];
        temp[0] = arr[j - 1][0];
        temp[1] = arr[j - 1][1];
        temp[2] = arr[j - 1][2];
        arr[j - 1][0] = arr[j][0];
        arr[j - 1][1] = arr[j][1];
        arr[j - 1][2] = arr[j][2];
        arr[j][0] = temp[0];
        arr[j][1] = temp[1];
        arr[j][2] = temp[2];
        
      }
    }
  }
}

void checkRecordingStatus(bool wasRecording) {
  int button_pressed = digitalRead(recording_pin);
  if (millis() - rec_timestamp > 250 && canCheckRecord) {
    if (wasRecording == false && button_pressed == 0) {
      // First track
      if (track_length == 0) {
        isRecording = true;
        rec_timestamp = millis();
        channel_ready[channel] = false;
      } else {
        // Some tracks already exist
        isWaiting = true;
      }
      canCheckRecord = false;
    }
    if (wasRecording == true && button_pressed == 0) {
      // End of recording first track, set up & prepare track
      if (track_length == 0) {
        isRecording = false;
        track_length = millis() - rec_timestamp;
        track_ready = true;
        channel_ready[channel] = true;
        track_start = millis();
        rec_timestamp = millis();
        canCheckRecord = false;
      }
    }
  }
  if (button_pressed == 1) {
    canCheckRecord = true;
  }
}
