#include <cfloat>
#include <M5Stack.h>

#define KEYBOARD_I2C_ADDR     0X08        // I2C address of the Calculator FACE
#define KEYBOARD_INT          5           // Data ready pin for Calculator FACE (active low)
#define BG_COLOR              BLUE        // Arbitrary background color
#define FG_COLOR              LIGHTGREY   // Arbitrary foreground color
#define ANN_TOP               4           // Top of the Annunciator, which shows calculator status
#define ANN_HEIGHT            16          // Height of the Annunciator
#define ANN_MARGIN            16          // Left/right margin for the Annunciator
#define ANN_FONT              2           // Annunciator font
#define ACC_TOP               28          // Top of the Accumulator display, where the sum is shown
#define ACC_HEIGHT            40          // Height of the Accumulator
#define ACC_MARGIN            16          // Left/right marging of the Accumulator
#define ACC_FONT              6           // Accumulator font

// Commands to be executed by the calculator
enum Calc_Command {
  NO_COMMAND,   // Nothing pending
  CLEAR,        // Immidiate command: if command == MEMORY clear memory, else clear accumulator
  TOTAL,        // If command == MEMORY, copy memory to accumulator. Else perform command, clear opperand and set accumulator to result
  MEMORY,       // Set command mode to MEMORY. Affects the next command issued.
  DECIMAL,      // If accumulator does not contain a decimal point, add a decimal point
  ADD,          // If command == MEMORY, add accumulator to memory, else add accumulator to opperand, clear opperand
  SUBTRACT,     // If command == MEMORY, subtract accumulator from memory, else subtract accumulator from opperand, clear opperand
  MULTIPLY,     // If command == MEMORY, multiply memory by accumulator, else multiply accumulator by opperand, clear opperand
  DIVIDE,       // If command == MEMORY, divide memory by accumulator, else divide opperand by accumulator, clear opperand
  PERCENT,      // Depends on status of command; see function comments
  SIGN          // Reverse the sign of the accumulator
};


const char    dp            = '.';        // Changes in differnet cultures
String        accumulator   = "0";        // The number displayed as the main value of the calculator
String        opperand      = "0";        // The number the current command will operate on
String        memory        = "0";        // The invisible memory
Calc_Command  command       = NO_COMMAND; // Nothing to do currently
bool          clear_accum   = false;      // This is true when the next number should clear the display (after a command)


bool is_digit(char c)   { return c >= '0' && c <= '9'; }    // Return true if the character is a digit 0 - 9
bool is_command(char c) { return 0 != c && !is_digit(c); }  // Return true if the character is one of the commands


// Create a string that will be the calculator display and storage value for the number given.
// If there is no fractional part, don't include a decimal.
// If there is, trim trailing zeros (and possibly decimal point as well.)
//
String double_to_string(double value) {
  char  buffer[64];
  if(0.0 == value - (long)value) {
    snprintf(buffer, 64, "%d", (long)value);
  }
  else {
    snprintf(buffer, 64, "%.6f", value);
    while('0' == buffer[strlen(buffer) - 1]) {
      buffer[strlen(buffer) - 1] = '\0';
    }
  }
  return String(buffer);
}


// Show the main number of interest near the top of the screen.
//
void display_accumulator() {
  M5.Lcd.fillRect(0, ACC_TOP, M5.Lcd.width(), ACC_HEIGHT, BG_COLOR);
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.drawString(accumulator, M5.Lcd.width() - ACC_MARGIN, ACC_TOP, ACC_FONT);
  M5.Lcd.setTextDatum(TL_DATUM);
}


// Show the calculator's status in the annunciator at the top-right of the screen.
//
void display_annunciator() {
  String display = "                ";
  bool memory_is_clear = memory == "0" || memory == "";
  if(MEMORY == command || !memory_is_clear) display += "M ";
  M5.Lcd.fillRect(0, ANN_TOP, M5.Lcd.width(), ANN_HEIGHT, BG_COLOR);
  switch(command) {
    case ADD:      display += "+"; break;
    case SUBTRACT: display += "-"; break;
    case MULTIPLY: display += "*"; break;
    case DIVIDE:   display += "/"; break;
    default:                       break;
  }
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.drawString(display, M5.Lcd.width() - ANN_MARGIN, ANN_TOP, ANN_FONT);
  M5.Lcd.setTextDatum(TL_DATUM);
}


// Convert an input character to a Calc_Command. Return NO_COMMAND if the character did not represent a known command.
//
Calc_Command input_to_command(char c) {
  switch(c) {
    case 'A': return CLEAR;
    case 'M': return MEMORY;
    case '%': return PERCENT;
    case '/': return DIVIDE;
    case '*': return MULTIPLY;
    case '-': return SUBTRACT;
    case '+': return ADD;
    case '`': return SIGN;  // Unexpected character: 0x60
    case '=': return TOTAL;
    case '.': return DECIMAL;
    default:  return NO_COMMAND;
  }
}


// Do the actual work of calculation.
//
void perform_operation() {
  double acc = atof(accumulator.c_str());
  double opp = atof(opperand.c_str());

  switch(command) {
    case ADD:
      accumulator = double_to_string(opp + acc);
      clear_accum = true;
      opperand    = "";
      break;
    case SUBTRACT:
      accumulator = double_to_string(opp - acc);
      clear_accum = true;
      opperand    = "";
      break;
    case MULTIPLY:
      accumulator = double_to_string(opp * acc);
      clear_accum = true;
      opperand    = "";
      break;
    case DIVIDE:
      accumulator = double_to_string(opp / acc);
      clear_accum = true;
      opperand    = "";
      break;
    case SIGN:
      if(accumulator == "0") break;   // Special case for zero
      if(accumulator.startsWith("-"))
        accumulator.remove(0, 1);
      else
        accumulator = String("-") + accumulator;
      break;
    default:
      break;
  }
  command = NO_COMMAND;
  display_accumulator();
}


// Percentage is a little more complicated than other operations.
// If there is no previous command, divide accumulator by 100.
// If previous command was * or /, divide accumulator by 100 and perform_operation.
// If previous command was + or -, replace accumulator with current accumulator % of opperand and perform_operation.
//
void perform_percentage() {
  double acc = atof(accumulator.c_str());

  if(NO_COMMAND == command) {
    accumulator = double_to_string(acc / 100.0);
    display_accumulator();
  }
  else if(ADD == command || SUBTRACT == command) {
    double opp = atof(opperand.c_str());
    accumulator = double_to_string(acc / 100.0 * opp);
    perform_operation();
  }
  else if(MULTIPLY == command || DIVIDE == command) {
    accumulator = double_to_string(acc / 100.0);
    perform_operation();
  }
}


// Handle a command from the keyboard.
//
void process_command(Calc_Command cmd) {
  // If the current command is MEMROY, then this command should be handled specially.
  if(MEMORY == command) {
    double acc = atof(accumulator.c_str());
    double mem = atof(memory.c_str());

    switch(cmd) {
      case CLEAR:
        memory = "0";
        break;
      case ADD:
        memory = double_to_string(mem + acc);
        break;
      case SUBTRACT:
        memory = double_to_string(mem - acc);
        break;
      case MULTIPLY:
         memory = double_to_string(mem * acc);
        break;
      case DIVIDE:
        memory = double_to_string(mem / acc);
        break;
      case TOTAL:
        accumulator = memory;
        display_accumulator();
        clear_accum = true;
        break;
      default :
        break;
    }
    command = NO_COMMAND;
    return;
  }

  // Normal (non-memory) command handling
  switch(cmd) {
    case CLEAR:
      accumulator = "0";
      display_accumulator();
      return;
    case DECIMAL:
      // Make sure there's not already a decimal point in the accumulator
      if(-1 == accumulator.indexOf(dp)) accumulator += dp;
      break;
    case ADD:
      command     = ADD;
      opperand    = accumulator;
      clear_accum = true;
      break;
    case SUBTRACT:
      command     = SUBTRACT;
      opperand    = accumulator;
      clear_accum = true;
      break;
    case MULTIPLY:
      command     = MULTIPLY;
      opperand    = accumulator;
      clear_accum = true;
      break;
    case DIVIDE:
      command     = DIVIDE;
      opperand    = accumulator;
      clear_accum = true;
      break;
    case MEMORY:
      // enter memory mode, which will effect future commands.
      command     = MEMORY;
      break;
    case SIGN:
      command     = SIGN;
      perform_operation();
      break;
    case PERCENT:
      perform_percentage();
      break;
    case TOTAL:
      // command was set previously
      perform_operation();
      break;
    default:
      break;
  }
}


// A digit key has been typed in NORMAL_INPUT or POST_DECIMAL_INPUT mode.
// Push it into the accumulator and display it.
//
void process_digit(uint8_t digit) {
  assert(digit < 10);                         // Make sure we're just getting 0 - 9.
  if(accumulator == "0") accumulator = "";    // Special case for the 'empty' number
  if(clear_accum)        accumulator = "";    // A command was entered; start getting second value
  accumulator += String(digit);               // Always simply append digit to the end
  clear_accum = false;                        // Make sure we don't keep clearing the accumulator
  display_accumulator();                      // Show the new value
}


// Returns true and changes ref to char if key is available, otherwise returns false.
//
bool read_key(char& input) {
  if(digitalRead(KEYBOARD_INT) == LOW) {      // If a character is ready
    Wire.requestFrom(KEYBOARD_I2C_ADDR, 1);   // request 1 byte from keyboard
    if(Wire.available()) {
      char key_val = Wire.read();             // receive a byte as character
      if(0 != key_val) {
        input = key_val;
        return true;
      }
    }
  }
  return false;
}


// Read a key from the keyboard.
// If it's a digit, push it into the accumulator and display it.
// If it's a command, execute it.
//
bool process_input() {
  char input;
  if(read_key(input)) {
    if(is_digit(input)) process_digit((uint8_t)input - '0');
    else {
      Calc_Command cmd = input_to_command(input);
      if(NO_COMMAND != cmd)
        process_command(cmd);
      else
        return false;
    }
    return true;  // something was processed
  }
  return false;
}


// Standard Arduino program initialization function.
//
void setup() {
  M5.begin();
  Wire.begin();
  M5.Lcd.setTextFont(4);
  pinMode(KEYBOARD_INT, INPUT_PULLUP);
  M5.Lcd.fillScreen(BG_COLOR);
  display_accumulator();
}


// Standard Arduino program loop
// Continually look for input and process it.
void loop() {
  // If anything happend, make sure the annunciator is updated
  if(process_input()) display_annunciator();
  delay(10);
}
