// libraries
#include "TimerOne\TimerOne.cpp"

// pin definitions
#define RATE_PIN      A0
#define GATE_PIN      13
#define CLOCK_PIN     2
#define CLOCK_SEL_PIN 5

// rate selection range
unsigned int min_period = 25;   // ms
unsigned int max_period = 2500; // ms (note: time1 max period is 8388.480 ms)

byte min_division = 1;
byte max_division = 8;

// clock globals
bool  tic       = false;
byte  counter   = 0;
byte  division  = 1;
unsigned int period = 1000;

// current and previous states of clock select pin
bool cur_clock_sel_state  = true; // true = internal clock / false = external clock
bool prev_clock_sel_state = true;

void setup() {
  // pinmodes
  pinMode(RATE_PIN,      INPUT);
  pinMode(GATE_PIN,      OUTPUT);
  pinMode(CLOCK_PIN,     INPUT);
  pinMode(CLOCK_SEL_PIN, INPUT_PULLUP);

  // set up serial for debugging
  //Serial.begin(9600);

  // set up clock select
  cur_clock_sel_state = digitalRead(CLOCK_SEL_PIN) == HIGH;
  prev_clock_sel_state = cur_clock_sel_state;
  read_rate_pot();

  // set up timer1
  Timer1.initialize((unsigned long) 1000 * (unsigned long) period);
  if (cur_clock_sel_state)
    Timer1.attachInterrupt(internal_clock);
  else
    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), external_clock, FALLING);
}

void loop() {
  write_char('S');
  write_char('o');
  write_char('s');
  write_char(' ');
}

// function to execute while waiting for the next step
void do_while_waiting() {
  read_clock_select();
  read_rate_pot();
}

// read the clock select switch and enable/disable internal/external clock
void read_clock_select() {
  cur_clock_sel_state = digitalRead(CLOCK_SEL_PIN) == HIGH;

  // switch from internal to external
  if (prev_clock_sel_state && (!cur_clock_sel_state)) {
    read_rate_pot();
    Timer1.detachInterrupt();
    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), external_clock, FALLING);
  }

  // switch from external to internal
  if (cur_clock_sel_state && (!prev_clock_sel_state)) {
    read_rate_pot();
    detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));
    Timer1.attachInterrupt(internal_clock);
  }

  prev_clock_sel_state = cur_clock_sel_state;
}

// read the rate pot and set division and period values
void read_rate_pot() {
  // some ugly float math to get the correct division and period values

  float value = ((float) (1023 - analogRead(RATE_PIN)) / 1023.); // 0-1

  if (cur_clock_sel_state) {
    // simulate log pot, see http://benholmes.co.uk/posts/2017/11/logarithmic-potentiometer-laws
    period = (pow(51., value) - 1) * 0.02 * (max_period - min_period) + min_period;
    Timer1.setPeriod((unsigned long) 1000 * (unsigned long) period);
    //Serial.print("Period: ");
    //Serial.println(period);
  }

  else {
    division = value * ((float) (max_division - min_division)) + (float) min_division + 0.5; // +0.5 to get correct rounding into int
    //Serial.print("Division: ");
    //Serial.println(division);
  }
}

// internal clock interrupt function
void internal_clock() {
  tic = true;
}

// external clock interrupt function
void external_clock() {
  counter++;
  counter %= division;
  if (counter == 0)
    tic = true;
}

// write character into Morse code
void write_char(char character) {
  // handle spaces
  if (character == 32) {
    //Serial.println(' ');
    // we already did 3 low tics after the previous character, so just 4 more here
    wait_for_tic(1);
    digitalWrite(GATE_PIN, LOW);
    wait_for_tic(3);
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

// wait for the next edge on the clock to happen, reset tic and proceed
void wait_for_tic(byte n) {
  for (byte i = 0; i < n; i++) {
    while (!tic) {
      do_while_waiting();
    }
    tic = false;
  }
}

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
