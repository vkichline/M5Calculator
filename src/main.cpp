#include <cfloat>
#include <M5Stack.h>

#define KEYBOARD_I2C_ADDR     0X08        // I2C address of the Calculator FACE
#define KEYBOARD_INT          5           // Data ready pin for Calculator FACE (active low)
#define BG_COLOR              BLUE        // Arbitrary background color
#define FG_COLOR              LIGHTGREY   // Arbitrary foreground color
#define SCREEN_WIDTH          320         // Horizontal screen size
#define SCREEN_H_CENTER       160         // Horizontal center of screen
#define ANN_TOP               4           // Top of the Annunciator, which shows calculator status
#define ANN_HEIGHT            16          // Height of the Annunciator
#define ANN_MARGIN            16          // Left/right margin for the Annunciator
#define ANN_FONT              2           // Annunciator font
#define ACC_TOP               40          // Top of the Accumulator display, where the sum is shown
#define ACC_HEIGHT            40          // Height of the Accumulator
#define ACC_MARGIN            16          // Left/right marging of the Accumulator
#define ACC_FONT              6           // Accumulator font
#define INFO_TOP              110         // Top of the Info area
#define INFO_HEIGHT           110         // Height of the Info area
#define INFO_MARGIN           16          // Left/right margins of the Info area
#define INFO_FONT             2           // Info font

// Commands to be executed by the calculator
//
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
bool          restart       = true;       // This is true when the next number should clear the display (after processing a command)


bool is_digit(char c)   { return c >= '0' && c <= '9'; }    // Return true if the character is a digit 0 - 9
bool is_command(char c) { return 0 != c && !is_digit(c); }  // Return true if the character is one of the commands


// Create a string that will be the calculator display and storage value for the number given.
// If there is no fractional part, don't include a decimal.
// If there is, trim trailing zeros (and possibly decimal point as well.)
//
String double_to_string(double value) {
  char  buffer[64];
  if(0.0 == value - (long)value) {            // If this is an exact integer...
    snprintf(buffer, 64, "%d", (int)value);   // Display without decimal point or trailing zeros
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
  M5.Lcd.fillRect(0, ACC_TOP, SCREEN_WIDTH, ACC_HEIGHT, BG_COLOR);
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);  // Blank space erases background w/ background color set
  M5.Lcd.setTextDatum(TR_DATUM);            // Print right-justified, relative to end of string
  M5.Lcd.drawString(accumulator, SCREEN_WIDTH - ACC_MARGIN, ACC_TOP, ACC_FONT);
  M5.Lcd.setTextDatum(TL_DATUM);            // Go back to normal text alignment
}


// Clear the area used to display additional info
//
void clear_info() {
  M5.Lcd.fillRect(0, INFO_TOP, SCREEN_WIDTH, INFO_HEIGHT, BG_COLOR);
}

// Show info about using the memory command when the active command is MEMORY
//
void display_memory_info() {
  if(MEMORY != command) return;
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);  // Blank space erases background w/ background color set
  M5.Lcd.setCursor(INFO_MARGIN, INFO_TOP, INFO_FONT);
  M5.Lcd.drawCentreString("Memory Commands", SCREEN_H_CENTER, INFO_TOP, INFO_FONT);
  M5.Lcd.println();
  M5.Lcd.println();
  M5.Lcd.setCursor(INFO_MARGIN, M5.Lcd.getCursorY());
  M5.Lcd.print("MM  M -> A  Recall");
  M5.Lcd.setCursor(SCREEN_H_CENTER, M5.Lcd.getCursorY());
  M5.Lcd.println("M=  A -> M  Save");
  M5.Lcd.setCursor(INFO_MARGIN, M5.Lcd.getCursorY());
  M5.Lcd.println("MA  0 -> M  Clear");
  M5.Lcd.println();
  M5.Lcd.setCursor(INFO_MARGIN, M5.Lcd.getCursorY());
  M5.Lcd.print("M+  M + A -> M");
  M5.Lcd.setCursor(SCREEN_H_CENTER, M5.Lcd.getCursorY());
  M5.Lcd.println("M-  M - A -> M");
  M5.Lcd.setCursor(INFO_MARGIN, M5.Lcd.getCursorY());
  M5.Lcd.print("M*  M * A -> M");
  M5.Lcd.setCursor(SCREEN_H_CENTER, M5.Lcd.getCursorY());
  M5.Lcd.println("M/  M / A -> M");
}


// Show the calculator's status in the annunciator at the top-right of the screen.
// Display an 'M' if memory is set (or if the MEMORY command is active and memory is about to be set)
// Followed by any operation that may be pending.
//
void display_annunciator() {
  String display = "                ";                        // With background text color set, this does erasing for us.
  bool memory_is_clear   = memory   == "0" || memory   == ""; // True if nothing stored in memory
  bool opperand_is_clear = opperand == "0" || opperand == ""; // True if nothing stored in the operand
  if(MEMORY == command || !memory_is_clear) display += "M";
  M5.Lcd.fillRect(0, ANN_TOP, SCREEN_WIDTH, ANN_HEIGHT, BG_COLOR);
  switch(command) {
    case ADD:      display += " +"; break;    // Add any pending operation to the annunciator
    case SUBTRACT: display += " -"; break;
    case MULTIPLY: display += " *"; break;
    case DIVIDE:   display += " /"; break;
    default:                        break;
  }
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);      // Blank space erases background w/ background color set
  M5.Lcd.setTextDatum(TR_DATUM);                // Print right-justified, relative to end of string
  M5.Lcd.drawString(display, SCREEN_WIDTH - ANN_MARGIN, ANN_TOP, ANN_FONT);
  M5.Lcd.setTextDatum(TL_DATUM);                // Go back to normal text alignment
  M5.Lcd.setCursor(ANN_MARGIN, ANN_TOP, ANN_FONT);
  if(!memory_is_clear) {
    M5.Lcd.print(String("M = ") + memory + " ");
  }
  if(!opperand_is_clear) {
    M5.Lcd.print(String("O = ") + opperand);
  }
  clear_info();
  if(MEMORY == command) display_memory_info();  // Display help on using memory
}


// Convert an input character to a Calc_Command.
// Return NO_COMMAND if the character did not represent a known command.
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
      restart     = true;   // New numbers replace accumulator rather than add to it.
      opperand    = "";
      break;
    case SUBTRACT:
      accumulator = double_to_string(opp - acc);
      restart     = true;
      opperand    = "";
      break;
    case MULTIPLY:
      accumulator = double_to_string(opp * acc);
      restart     = true;
      opperand    = "";
      break;
    case DIVIDE:
      accumulator = double_to_string(opp / acc);
      restart     = true;
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
  display_accumulator();    // All these operations affect the display
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


// Handle a command from user input.
// If the M key has been pressed, the next command operates on memory, a little unconventionally:
// MA (AC) clears memory. The accumulator and opperand are unchanged.
// M+ adds the accumulator to memory. The accumulator and opperand are unchanged.
// M- subtracts the accumulator from memory. The accumulator and opperand are unchanged.
// M* multiplies memory by the accumulator. The accumulator and opperand are unchanged.
// M/ divides memory by the accumulator. The accumulator and opperand are unchanged.
// M= sets memory to the value of the accumulator. The accumulator and opperand are unchanged.
// MM sets the accumulator to the value of memory. The opperand is unchanged.
//
void process_command(Calc_Command cmd) {
  // If the current command is MEMROY, then this command should be handled specially.
  if(MEMORY == command) {
    double acc = atof(accumulator.c_str());
    double mem = atof(memory.c_str());

    switch(cmd) {
      case CLEAR:
        memory  = "0";
        display_accumulator();  // Update display immediately
        restart = true;         // New numbers replace accumulator rather than add to it.
        break;
      case ADD:
        memory  = double_to_string(mem + acc);
        restart = true;
        break;
      case SUBTRACT:
        memory  = double_to_string(mem - acc);
        restart = true;
        break;
      case MULTIPLY:
        memory  = double_to_string(mem * acc);
        restart = true;
        break;
      case DIVIDE:
        memory  = double_to_string(mem / acc);
        restart = true;
        break;
      case TOTAL:
        memory  = accumulator;
        restart = true;
        break;
      case MEMORY:
        accumulator = memory;   // Memory recall
        restart     = true;
        display_accumulator();  // Update display immediately
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
      accumulator = "0";        // Special case for the empty number.
      display_accumulator();    // Unary operator; display immediately.
      return;
    case DECIMAL:
      // Make sure there's not already a decimal point in the accumulator.
      if(-1 == accumulator.indexOf(dp)) {
        accumulator += dp;
        display_accumulator();  // Unary operator; display immediately.
      }
      break;
    case ADD:
      command   = ADD;          // Command that will be executed upon receiving TOTAL
      opperand  = accumulator;  // Number that will be acted upon
      restart   = true;         // New numbers replace accumulator rather than add to it.
      break;
    case SUBTRACT:
      command   = SUBTRACT;
      opperand  = accumulator;
      restart   = true;
      break;
    case MULTIPLY:
      command   = MULTIPLY;
      opperand  = accumulator;
      restart   = true;
      break;
    case DIVIDE:
      command   = DIVIDE;
      opperand  = accumulator;
      restart   = true;
      break;
    case MEMORY:
      command   = MEMORY;       // Enter memory mode, which will effect future commands.
      break;
    case SIGN:
      command   = SIGN;
      perform_operation();      // command set expressly (SIGN)
      break;
    case PERCENT:
      perform_percentage();     // unary command
      break;
    case TOTAL:
      perform_operation();      // command was set by previous input
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
  if(restart)        accumulator = "";    // A command was entered; start getting second value
  accumulator += String(digit);               // Always simply append digit to the end
  restart = false;                        // Make sure we don't keep clearing the accumulator
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
