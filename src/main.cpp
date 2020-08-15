#include <M5Stack.h>

#define KEYBOARD_I2C_ADDR     0X08          // I2C address of the Calculator FACE
#define KEYBOARD_INT          5             // Data ready pin for Calculator FACE (active low)

#define SCREEN_WIDTH          320           // Horizontal screen size
#define SCREEN_H_CENTER       160           // Horizontal center of screen
#define FG_COLOR              LIGHTGREY     // Arbitrary foreground color
#define BG_COLOR              BLUE          // Arbitrary background color

#define ANN_TOP               0             // Top of the Annunciator, which shows calculator status
#define ANN_HEIGHT            20            // Height of the Annunciator
#define ANN_V_MARGIN          2             // Offset from top to top text
#define ANN_H_MARGIN          16            // Left/right margin for the Annunciator
#define ANN_FONT              2             // Annunciator font
#define ANN_FG_COLOR          BLACK         // Annunciator foreground color
#define ANN_BG_COLOR          DARKGREY      // Annunciator background color

#define ACC_TOP               36            // Top of the Accumulator display, where the sum is shown
#define ACC_HEIGHT            92            // Height of the Accumulator
#define ACC_V_MARGIN          4             // Offset from top to top text
#define ACC_H_MARGIN          16            // Left/right marging of the Accumulator
#define ACC_FONT_1            6             // Preferred Accumulator font
#define ACC_FONT_2            4             // Smaller Accumulator font
#define ACC_FONT_3            2             // Smallest Accumulator font
#define ACC_FG_COLOR          FG_COLOR      // Accumulator foreground color
#define ACC_BG_COLOR          BG_COLOR      // Accumulator background color

#define INFO_TOP              128           // Top of the Info area
#define INFO_HEIGHT           92            // Height of the Info area
#define INFO_H_MARGIN         16            // Left/right margins of the Info area
#define INFO_V_MARGIN         2             // Offset from top to top text
#define INFO_FONT             2             // Info font
#define INFO_FG_COLOR         FG_COLOR      // Info foreground color
#define INFO_BG_COLOR         BG_COLOR      // Info background color

#define LABEL_TOP             220           // Top of the button labels
#define LABEL_HEIGHT          22            // Height of the button labels
#define LABEL_V_MARGIN        2             // Offset from top to top text
#define LABEL_FONT            2             // Font for the button labels
#define LABEL_FG_COLOR        ANN_FG_COLOR  // Button labels foreground color
#define LABEL_BG_COLOR        ANN_BG_COLOR  // Button labels background color
#define LABEL_BTN_A_CENTER    68            // Vertical center for Button A
#define LABEL_BTN_B_CENTER    160           // Vertical center for Button B
#define LABEL_BTN_C_CENTER    252           // Vertical center for Button C


////////////////////////////////////////////////////////////////////////////////
//
//  Calculator Commands
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



////////////////////////////////////////////////////////////////////////////////
//
//  Global Variables
//
const char    dp            = '.';        // Decimal Point: changes in differnet cultures
const char    ts            = ',';        // Thousands separator: changes in differnet cultures
String        accumulator   = "0";        // The number displayed as the main value of the calculator
String        opperand      = "0";        // The number the current command will operate on
String        memory        = "0";        // The invisible memory
Calc_Command  command       = NO_COMMAND; // Nothing to do currently
Calc_Command  previous      = NO_COMMAND; // The previously executed command
bool          restart       = true;       // This is true when the next number should clear the display (after processing a command)
bool          memory_mode   = false;      // The memory key has been pressed once and another press is needed



////////////////////////////////////////////////////////////////////////////////
//
//  Utility Functions
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
//  Keystroke identifying functions
//
bool is_digit(char c)   { return c >= '0' && c <= '9'; }    // Return true if the character is a digit 0 - 9
bool is_command(char c) { return 0 != c && !is_digit(c); }  // Return true if the character is one of the commands


////////////////////////////////////////////////////////////////////////////////
//
//  Create a string that will be the calculator display and storage value for the given number.
//  If there is no fractional part, don't include a decimal.
//  If there is a fractional part, trim trailing zeros (and possibly decimal point as well.)
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
    if('.' ==buffer[strlen(buffer) - 1]) {
      buffer[strlen(buffer) - 1] = '\0';
    }
  }
  return String(buffer);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Convert a display string back into a double.
//
double string_to_double(String str) {
  str.replace(String(ts), "");    // Remove all thousands separators
  str.replace(dp, '.');           // For atof() to work correctly
  return str.toDouble();
}


////////////////////////////////////////////////////////////////////////////////
//
//  Convert an input character to a Calc_Command.
//  Return NO_COMMAND if the character did not represent a known command.
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


////////////////////////////////////////////////////////////////////////////////
//
//  Identify a deferred command; one which is put in the command global rather
//  being executed immediately
//
bool is_deferred_command(Calc_Command cmd) {
  return (ADD == cmd || SUBTRACT == cmd || MULTIPLY == cmd || DIVIDE == cmd);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Return true if the user can backspace in the accumulator
//
bool can_backspace() {
  return !restart && accumulator != "0" && accumulator != "";
}


////////////////////////////////////////////////////////////////////////////////
//
//  Display Routines
//
//  The display has four distinct parts:
//  The annunciator at the top shows calculator status
//  The Accumulator shows the current total
//  The Info area may pop up instructions
//  Button Labels title the A/B/C buttons
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
//  Clear the area used to display additional info
//
void clear_info() {
  M5.Lcd.fillRect(0, INFO_TOP, SCREEN_WIDTH, INFO_HEIGHT, INFO_BG_COLOR);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Show info about using the memory command when in memory_mode
//
void display_memory_info() {
  if(!memory_mode) return;
  M5.Lcd.setTextColor(INFO_FG_COLOR, INFO_BG_COLOR);  // Blank space erases background w/ background color set
  M5.Lcd.drawCentreString("Memory Commands", SCREEN_H_CENTER, INFO_TOP + INFO_V_MARGIN, INFO_FONT);
  M5.Lcd.drawCentreString("M M  Recall      M =  Save      M AC  Clear", SCREEN_H_CENTER, INFO_TOP + INFO_V_MARGIN + 30, INFO_FONT);
  M5.Lcd.drawCentreString("Also  M+  M-  M*  M/  M%  to change Memory", SCREEN_H_CENTER, INFO_TOP + INFO_V_MARGIN + 55, INFO_FONT);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Show the calculator's status in the annunciator at the top-right of the screen.
//  Display the Memory in the upper left corner.
//  On the right, show an 'M' if in memory mode, followed by any operation that may be pending.
//
void display_annunciator() {
  String display = "                ";                        // With background text color set, this does erasing for us.
  bool memory_is_clear   = memory   == "0" || memory   == ""; // True if nothing stored in memory.
  bool opperand_is_clear = opperand == "0" || opperand == ""; // True if nothing stored in the operand.
  M5.Lcd.fillRect(0, 0, SCREEN_WIDTH, ANN_HEIGHT, ANN_BG_COLOR);  // Erase from the top of the screen
  if(memory_mode) display += "M";                             // Show M if entering a memory command.
  if(!opperand_is_clear) display += " " + opperand;           // Display the number we're opperating one
  switch(command) {
    case ADD:      display += " +"; break;                    // Add any pending operation to the annunciator
    case SUBTRACT: display += " -"; break;
    case MULTIPLY: display += " *"; break;
    case DIVIDE:   display += " /"; break;
    default:                        break;
  }
  M5.Lcd.setTextColor(ANN_FG_COLOR, ANN_BG_COLOR);            // Blank space erases background w/ background color set
  M5.Lcd.setTextDatum(TR_DATUM);                              // Print right-justified, relative to end of string
  M5.Lcd.drawString(display, SCREEN_WIDTH - ANN_H_MARGIN, ANN_TOP + ANN_V_MARGIN, ANN_FONT);
  M5.Lcd.setTextDatum(TL_DATUM);                              // Go back to normal text alignment
  M5.Lcd.setCursor(ANN_H_MARGIN, ANN_TOP + ANN_V_MARGIN, ANN_FONT);
  if(!memory_is_clear) {
    M5.Lcd.print(memory);                                     // Show memory in upper left corner
  }
  clear_info();                                               // Erase the area used to display extra info
  if(memory_mode) display_memory_info();                      // Display help on using memory
}



////////////////////////////////////////////////////////////////////////////////
//
//  Show the main number of interest near the top of the screen.
//  If it's too wide to fit, use a smaller font.
//
void display_accumulator() {
  uint8_t   font  = ACC_FONT_1;
  uint16_t  wid   = SCREEN_WIDTH - (2 * ACC_H_MARGIN);
  // Select the font that fits the display
  M5.Lcd.setTextFont(font);
  if(wid < M5.Lcd.textWidth(accumulator)) {
    font = ACC_FONT_2;
    M5.Lcd.setTextFont(font);
    if(wid < M5.Lcd.textWidth(accumulator)) {
      font = ACC_FONT_3;
    }
  }
  M5.Lcd.fillRect(0, ACC_TOP, SCREEN_WIDTH, ACC_HEIGHT, ACC_BG_COLOR);
  M5.Lcd.setTextColor(ACC_FG_COLOR, ACC_BG_COLOR);  // Blank space erases background w/ background color set
  M5.Lcd.setTextDatum(TR_DATUM);                    // Print right-justified, relative to end of string
  M5.Lcd.drawString(accumulator, SCREEN_WIDTH - ACC_H_MARGIN, ACC_TOP + ACC_V_MARGIN, font);
  M5.Lcd.setTextDatum(TL_DATUM);                    // Go back to normal text alignment
}


////////////////////////////////////////////////////////////////////////////////
//
//  Show the button labels, which may be state dependent
//
void display_button_labels() {
  M5.Lcd.setTextColor(LABEL_FG_COLOR, LABEL_BG_COLOR);  // Blank space erases background w/ background color set
  M5.Lcd.fillRect(0, LABEL_TOP, SCREEN_WIDTH, LABEL_HEIGHT, LABEL_BG_COLOR);
  if(can_backspace()) M5.Lcd.drawCentreString("BKSPC", LABEL_BTN_A_CENTER, LABEL_TOP + LABEL_V_MARGIN, LABEL_FONT);
  // M5.Lcd.drawCentreString("V", LABEL_BTN_B_CENTER, LABEL_TOP + LABEL_V_MARGIN, LABEL_FONT);
  // M5.Lcd.drawCentreString("V", LABEL_BTN_C_CENTER, LABEL_TOP + LABEL_V_MARGIN, LABEL_FONT);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Command Processing Routines
//
//  Comands respond in two different modes: Normal and Memory
//  Memory commands are two-key commands, so once M is pressed, the next command is a Memory Command.
//  At all other times, commands are executed in normal mode.
//  Some are immediate (CLEAR, SIGN, TOTAL) while others are deferred (ADD, SUBTRACT)
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
//  Do the actual work of calculation by performing a deferred command.
//  the opperation to perform is in command.
//
void perform_operation() {
  double acc = string_to_double(accumulator);
  double opp = string_to_double(opperand);

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
      restart     = true;   // New numbers replace accumulator rather than add to it.
      break;
  }
  command = NO_COMMAND;
  display_accumulator();    // All these operations affect the display
}


////////////////////////////////////////////////////////////////////////////////
//
//  Percentage is a little more complicated than other operations.
//  If there is no previous command, divide accumulator by 100.
//  If previous command was * or /, divide accumulator by 100 and perform_operation.
//  If previous command was + or -, replace accumulator with current accumulator % of opperand and perform_operation.
//
void perform_percentage() {
  double acc = string_to_double(accumulator);

  if(NO_COMMAND == command) {
    accumulator = double_to_string(acc / 100.0);
    display_accumulator();
  }
  else if(ADD == command || SUBTRACT == command) {
    double opp = string_to_double(opperand);
    accumulator = double_to_string(acc / 100.0 * opp);
    perform_operation();
  }
  else if(MULTIPLY == command || DIVIDE == command) {
    accumulator = double_to_string(acc / 100.0);
    perform_operation();
  }
}


////////////////////////////////////////////////////////////////////////////////
//
//  If the M key has been pressed, the next command operates on memory, a little unconventionally:
//  MA (AC) clears memory. The accumulator and opperand are unchanged.
//  M+ adds the accumulator to memory. The accumulator and opperand are unchanged.
//  M- subtracts the accumulator from memory. The accumulator and opperand are unchanged.
//  M* multiplies memory by the accumulator. The accumulator and opperand are unchanged.
//  M/ divides memory by the accumulator. The accumulator and opperand are unchanged.
//  M= sets memory to the value of the accumulator. The accumulator and opperand are unchanged.
//  M% sets memory to memory / accumulator / 100.
//  MM sets the accumulator to the value of memory. The opperand is unchanged.
//  Return true if a memory command was processed, else return false.
//
void process_memory_command(Calc_Command cmd) {
  double acc = string_to_double(accumulator);
  double mem = string_to_double(memory);
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
    case PERCENT:
      memory  = double_to_string(mem / acc / 100.0);
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
}


////////////////////////////////////////////////////////////////////////////////
//
//  Handle a calculator command.
//
void process_calculator_command(Calc_Command cmd) {
  switch(cmd) {
    case CLEAR:
      accumulator = "0";        // Special case for the empty number.
      display_accumulator();    // Unary operator; display immediately.
      if(CLEAR == previous) {   // If this is a double-AC
        memory    = "0";        // Clear everything
        opperand  = "0";
        command   = NO_COMMAND;
      }
      return;
    case DECIMAL:
      // Make sure there's not already a decimal point in the accumulator.
      if(-1 == accumulator.indexOf(dp)) {
        if(restart) {
          accumulator = "0.";   // Special case for when we've just entered a number
          restart     = false;  // terminate restart mode
        }
        else {
          accumulator += dp;    // Normal case: decimal point always goes at the end.
        }
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
      memory_mode = true;       // Enter memory mode, which will effect future commands.
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



////////////////////////////////////////////////////////////////////////////////
//
//  Input Processing
//
//  Read keys from Calculator Keyboard, process digits to build accumulator string.
//  Identify and dispatch commands to command processors.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
//  Handle a button press
//  Currently there is only one button action; backspace on the A button
//
void process_button(uint8_t button) {
  if(BUTTON_A == button && can_backspace()) {
    accumulator.remove(accumulator.length() - 1);                   // Remove the last character
    accumulator = double_to_string(string_to_double(accumulator));  // Fixes up misc problems
    display_accumulator();
  }
}


////////////////////////////////////////////////////////////////////////////////
//
//  Handle a command from user input. Decide which processor gets  the command.
//  If it's non-memory-mode, determine if we need to execute a pending command
//  first (for example: 1 + 1 + 1.)
//
void process_command(Calc_Command cmd) {
  // If the M key has been pressed, then this command should be handled specially.
  if(memory_mode) {
    process_memory_command(cmd);
    memory_mode = false;
  }
  else {
    // If the user presses +, then *, allow the processor to change + to *.
    // Otherwise, we need to see if there's a pending command to execute.
    if(is_deferred_command(cmd) && !is_deferred_command(previous)) {
      if(is_deferred_command(command)) {
        perform_operation();
      }
    }
    // Now continue with the current operation
    process_calculator_command(cmd);
    previous = cmd;
  }
}


////////////////////////////////////////////////////////////////////////////////
//
//  A digit key has been typed in NORMAL_INPUT or POST_DECIMAL_INPUT mode.
//  Push it into the accumulator and display it.
//
void process_digit(uint8_t digit) {
  assert(digit < 10);                         // Make sure we're just getting 0 - 9.
  previous = NO_COMMAND;                      // Last thing done was not a command.
  if(accumulator == "0") accumulator = "";    // Special case for the 'empty' number
  if(restart)            accumulator = "";    // A command was entered; start getting second value
  accumulator += String(digit);               // Always simply append digit to the end
  restart = false;                            // Make sure we don't keep clearing the accumulator
  display_accumulator();                      // Show the new value
}


////////////////////////////////////////////////////////////////////////////////
//
//  Return true and change ref to input char if key is available, otherwise returns false.
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


////////////////////////////////////////////////////////////////////////////////
//
//  Read a key from the keyboard, or a button from the M5Stack.
//  If it's a digit, push it into the accumulator and display it.
//  If it's a command, execute it.
//
bool process_input() {
  char input;
  M5.update();
  if(M5.BtnA.wasReleased()) { process_button(BUTTON_A); return true; }
  if(M5.BtnB.wasReleased()) { process_button(BUTTON_B); return true; }
  if(M5.BtnC.wasReleased()) { process_button(BUTTON_C); return true; }

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



////////////////////////////////////////////////////////////////////////////////
//
//  Arduino Runtime: setup() and loop()
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
//  Standard Arduino program initialization function.
//
void setup() {
  M5.begin();
  Wire.begin();
  M5.Lcd.setTextFont(4);
  pinMode(KEYBOARD_INT, INPUT_PULLUP);
  M5.Lcd.fillScreen(BG_COLOR);
  display_accumulator();
  display_annunciator();
  display_button_labels();
}


////////////////////////////////////////////////////////////////////////////////
//
//  Standard Arduino program loop
//  Continually look for input and process it.
//
void loop() {
  // If anything happend, make sure the annunciator is updated
  if(process_input()) {
    display_annunciator();
    display_button_labels();
  }
}
