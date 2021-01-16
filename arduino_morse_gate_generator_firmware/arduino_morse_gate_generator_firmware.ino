////////////////////////////////////////////////////////////////////////////////////////////////////

/* 
 * Arduino Morse Gate Generator
 * synthesizer module firmware
 * 
 * by TimMJN
 * 
 * v1.0 
 * 16-01-2021
 * 
 * For schematics and other information, see
 * https://github.com/TimMJN/Arduino-Morse-Gate-Generator
 */

////////////////////////////////////////////////////////////////////////////////////////////////////
 
// libraries
#include "TimerOne\TimerOne.cpp"
#include <SPI.h>
#include <SD.h>

// pin definitions
#define RATE_PIN      A0
#define GATE_PIN      4
#define CLOCK_PIN     2
#define CLOCK_SEL_PIN 5
#define SS_PIN        10

// rate selection range
unsigned long min_period = 25000;   // us
unsigned long max_period = 2500000; // us (note: time1 max period is 8388480 ms)

byte min_division = 1;
byte max_division = 8;

// clock globals
bool  tic       = false;
byte  counter   = 0;
byte  division  = 1;
unsigned long period = 1000000;

// current and previous states of clock select pin
bool cur_int_clock_state  = true; // true = internal clock / false = external clock
bool prev_int_clock_state = true;

// sd card variables/objects
bool sd_valid = false;
File root;
File cur_file;

// morse variables
bool last_was_space = false; // was the last printed character a space?

////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  // pinmodes
  pinMode(RATE_PIN,      INPUT);
  pinMode(GATE_PIN,      OUTPUT);
  pinMode(CLOCK_PIN,     INPUT);
  pinMode(CLOCK_SEL_PIN, INPUT_PULLUP);

  // set up serial for debugging
  //Serial.begin(9600);
  //while (!Serial) {}

  // open sd card
  sd_valid = SD.begin(SS_PIN); // returns false if no SD was found
  root = SD.open("/");
  if (sd_valid)
    sd_valid = open_next_file(); // returns false if no valid file was found

  // set up clock select
  cur_int_clock_state  = digitalRead(CLOCK_SEL_PIN) == HIGH;
  prev_int_clock_state = cur_int_clock_state;
  read_rate_pot();

  // set up timer1
  Timer1.initialize(period);
  if (cur_int_clock_state)
    Timer1.attachInterrupt(internal_clock);
  else
    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), external_clock, FALLING);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  if (sd_valid) {
    // read the file untill the end is reached
    while (cur_file.available()) {
      write_char((char) cur_file.read());
    }
    cur_file.close(); // close the file
    sd_valid = open_next_file(); // open the next one
  }
  // no valid sd/file is available, repeatedly sent "SOS "
  else {
    write_char('S');
    write_char('O');
    write_char('S');
    write_char(' ');
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// function to execute while waiting for the next step
void do_while_waiting() {
  read_rate_pot();
  read_clock_select();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// read the clock select switch and enable/disable internal/external clock
void read_clock_select() {
  cur_int_clock_state = digitalRead(CLOCK_SEL_PIN) == HIGH;

  // switch from internal to external
  if (prev_int_clock_state && (!cur_int_clock_state)) {
    Timer1.detachInterrupt();
    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), external_clock, FALLING);
  }

  // switch from external to internal
  if (cur_int_clock_state && (!prev_int_clock_state)) {
    detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));
    Timer1.attachInterrupt(internal_clock);
  }

  prev_int_clock_state = cur_int_clock_state;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// read the rate pot and set division and period values
void read_rate_pot() {
  unsigned long value = 1023 - analogRead(RATE_PIN); // we need unsigned long for the period calculation

  division = map(value, 0, 1023, min_division - 1, max_division + 1);
  division = constrain(division, min_division, max_division);

  period = value * value * value / ((unsigned long) 1023 * 1023); // mimic a log pot by using val^3
  period *= (max_period - min_period) / ((unsigned long) 1023);
  period += min_period;
  Timer1.setPeriod(period);

  //Serial.print("Period: ");
  //Serial.print(period);
  //Serial.print(" ; Division: ");
  //Serial.println(division);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// internal clock interrupt function
void internal_clock() {
  tic = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// external clock interrupt function
void external_clock() {
  counter++;
  counter %= division;
  if (counter == 0)
    tic = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// open the next valid file on the sd card
bool open_next_file() {
  bool found_valid_file = false;
  bool have_rewinded    = false;   // have we tried rewinding the dir?

  while (!found_valid_file) {
    // try to open the next file
    cur_file = root.openNextFile();

    if (!cur_file) {              // no file was found
      if (!have_rewinded) {        // if we haven't rewinded, try that
        root.rewindDirectory();    // rewind
        have_rewinded = true;      // make sure we only rewind once
        cur_file.close();
        continue;                  // go back to start of loop
      }
      else {
        //Serial.println("No further files found");
        cur_file.close();
        break;                     // if we have already rewinded, give up
      }
    }

    // skip any directories
    if (cur_file.isDirectory()) {
      //Serial.print(cur_file.name());
      //Serial.println(" is a dir");
      cur_file.close();
      continue;
    }

    // skip any non '.txt' files
    char* filename = cur_file.name();
    if (!strstr(strlwr(filename + (strlen(filename) - 4)), ".txt")) {
      //Serial.print(cur_file.name());
      //Serial.println(" is not a txt file");
      cur_file.close();
      continue;
    }

    // skip any empty files
    if (cur_file.size() == 0) {
      //Serial.print(cur_file.name());
      //Serial.println(" is empty");
      cur_file.close();
      continue;
    }

    // we've made it, this file seems valid
    //Serial.print(cur_file.name());
    //Serial.println(" is valid");
    found_valid_file = true;
  }
  return found_valid_file;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// write character into Morse code
void write_char(char character) {

  // handle spaces / linebreaks
  if (character == 32 || character == 10) {
    if (!last_was_space) { // don't print consecutive spaces
      //Serial.println(' ');

      last_was_space = true;
      // we already did 3 low tics after the previous character, so just 4 more here
      wait_for_tic(1);
      digitalWrite(GATE_PIN, LOW);
      wait_for_tic(3);
    }
  }

  // handle other characters
  else {
    // convert lowercase characters to uppercase
    if (isLowerCase(character)) {
      character = toupper(character);
    }

    // get corresponding Morse code and length
    byte str = morse_string(character);
    byte len = morse_length(character);

    // skip invalid characters
    if (len > 0) {
      //Serial.print("Writing: ");
      //Serial.println(character);

      last_was_space = false;

      // loop over the dit/dahs in the Morse code
      for (byte j = 0; j < len; j++) {

        // dah
        if (bitRead(str, j)) {
          //Serial.print('-');
          wait_for_tic(1);
          digitalWrite(GATE_PIN, HIGH);
          wait_for_tic(2);
        }

        // dit
        else {
          //Serial.print('.');
          wait_for_tic(1);
          digitalWrite(GATE_PIN, HIGH);
        }

        // do 1 low tic after each dit/dah
        wait_for_tic(1);
        digitalWrite(GATE_PIN, LOW);
      }

      // end of character
      // we already did 1 low tic1 after the previous dit/dah, so just 2 more here
      //Serial.print('/');
      wait_for_tic(1);
      digitalWrite(GATE_PIN, LOW);
      wait_for_tic(1);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// wait for the next edge on the clock to happen, reset tic and proceed
void wait_for_tic(byte n) {
  for (byte i = 0; i < n; i++) {
    while (!tic) {
      do_while_waiting();
    }
    tic = false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// return Morse code "string" for each character
byte morse_string(char character) {
  switch (character) {
    case 33: return B110101; // ! -.-.--
    case 34: return B010010; // " .-..-.
    case 38: return B00010;  // & .-...
    case 39: return B011110; // ' .----.
    case 40: return B101101; // ) -.--.-
    case 41: return B01101;  // ( -.--.
    case 44: return B110011; // , --..--
    case 45: return B100001; // - -....-
    case 46: return B101010; // . .-.-.-
    case 47: return B01001;  // / -..-.

    case 48: return B11111;  // 0 -----
    case 49: return B11110;  // 1 .----
    case 50: return B11100;  // 2 ..---
    case 51: return B11000;  // 3 ...--
    case 52: return B10000;  // 4 ....-
    case 53: return B00000;  // 5 .....
    case 54: return B00001;  // 6 -....
    case 55: return B00011;  // 7 --...
    case 56: return B00111;  // 8 ---..
    case 57: return B01111;  // 9 ----.

    case 58: return B111000; // : ---...
    case 59: return B10101;  // ; -.-.-
    case 61: return B10001;  // = -...-
    case 63: return B001100; // ? ..--..
    case 64: return B010110; // @ .--.-.

    case 65: return B10;     // A .-
    case 66: return B0001;   // B -...
    case 67: return B0101;   // C -.-.
    case 68: return B001;    // D -..
    case 69: return B0;      // E .
    case 70: return B0100;   // F ..-.
    case 71: return B011;    // G --.
    case 72: return B0000;   // H ....
    case 73: return B00;     // I ..
    case 74: return B1110;   // J .---
    case 75: return B101;    // K -.-
    case 76: return B0010;   // L .-..
    case 77: return B11;     // M --
    case 78: return B01;     // N -.
    case 79: return B111;    // O ---
    case 80: return B0110;   // P .--.
    case 81: return B1011;   // Q --.-
    case 82: return B010;    // R .-.
    case 83: return B000;    // S ...
    case 84: return B1;      // T -
    case 85: return B100;    // U ..-
    case 86: return B1000;   // V ...-
    case 87: return B110;    // W .--
    case 88: return B0110;   // X .--.
    case 89: return B1101;   // Y -.--
    case 90: return B0011;   // Z --..

    default: return B0;      // invalid
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// return length of Morse code string for each character
byte morse_length(char character) {
  switch (character) {
    case 33: return 6; // ! -.-.--
    case 34: return 6; // " .-..-.
    case 38: return 5; // & .-...
    case 39: return 6; // ' .----.
    case 40: return 6; // ) -.--.-
    case 41: return 5; // ( -.--.
    case 44: return 6; // , --..--
    case 45: return 6; // - -....-
    case 46: return 6; // . .-.-.-
    case 47: return 5; // / -..-.

    case 48: return 5; // 0 -----
    case 49: return 5; // 1 .----
    case 50: return 5; // 2 ..---
    case 51: return 5; // 3 ...--
    case 52: return 5; // 4 ....-
    case 53: return 5; // 5 .....
    case 54: return 5; // 6 -....
    case 55: return 5; // 7 --...
    case 56: return 5; // 8 ---..
    case 57: return 5; // 9 ----.

    case 58: return 6; // : ---...
    case 59: return 5; // ; -.-.-
    case 61: return 5; // = -...-
    case 63: return 6; // ? ..--..
    case 64: return 6; // @ .--.-.

    case 65: return 2; // A .-
    case 66: return 4; // B -...
    case 67: return 4; // C -.-.
    case 68: return 3; // D -..
    case 69: return 1; // E .
    case 70: return 4; // F ..-.
    case 71: return 3; // G --.
    case 72: return 4; // H ....
    case 73: return 2; // I ..
    case 74: return 4; // J .---
    case 75: return 3; // K -.-
    case 76: return 4; // L .-..
    case 77: return 2; // M --
    case 78: return 2; // N -.
    case 79: return 3; // O ---
    case 80: return 4; // P .--.
    case 81: return 4; // Q --.-
    case 82: return 3; // R .-.
    case 83: return 3; // S ...
    case 84: return 1; // T -
    case 85: return 3; // U ..-
    case 86: return 4; // V ...-
    case 87: return 3; // W .--
    case 88: return 4; // X .--.
    case 89: return 4; // Y -.--
    case 90: return 4; // Z --..

    default: return 0; // invalid
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
