# M5Stack Calculator with Calculator Face Module

I recently obtained a [Calculator Face Module](https://docs.m5stack.com/#/en/module/faces_calculator) for the M5Stack.
While there is a [sample program](https://github.com/m5stack/M5-ProductExampleCodes/blob/master/Module/CALCULATOR/CALCULATOR.ino) for Arduino
it isn't a complete calculator. I found several others searching for a calculator program, so I decided to build one.

This is a very simple calculator (it does not nest operations to perform operation precedence; 1 + 5 * 2 is handled just like 1 + 1 + 1.)  
However, it has some nice memory functions to make it useful.  
The coding has been kep deliberately simple and straight-forward to make it easy for others to modify.

## Terminology

* The gray bar at the top is called the Annunciator. It displays the value of memory and the pending operation.
* The area that contains with a large zero is called the Accumulator. It's where values are input and results displayed.
* The next area of the screen is called the Info area. When you press M, for example, info is displayed there.
* The gray bar at the bottom displays the functions assigned to the buttons. Currently, this only includes backspacing when entering a value.

## Instructions

* Enter any number by pressing `0` - `9` keys. Enter a decimal point with `.` key.
* While inputting numbers, `Button A` can be used to backspace.
* Change input number's sign by pressing `+/-`.
* Clear the input by pressing `AC`. Press `AC` again to clear memory and pending operation.
* Press `+`, `-`, `*` or `/` followed by another number and `=` to perform a calculation.
* The `%` key depends on the calculator's state:
  * If no operation is pending, `%` divides the value in the Accumulator by 100.  
    Example: `AC AC 10 %` results in `0.1` in the Accumulator.
  * If a `+` or `-` is pending, the Accumulator provides the percentage of memory added/subtracted to memory.  
    Example: `AC AC 100 + 7 %` results in `107` in the Accumulator.
  * If a `*` or `/` is pending, the Accumulator is divided by 100 and memory is multiplied/divided by it.  
    Example: `AC AC 500 * 2 %` results in `10` in the Accumulator.
* If the `M` key is pressed:
  * `M` is displayed in the Annunciator
  * The next operation pressed operates on memory:
    * `MM` recalls the value of memory and replaces the value in the Accumulator with it.  Memory is unchanged.
    * `M=` stores the value in the Accumulator in memory. The Accumulator is unchanged, Memory is replaced.
    * `MA` where `A` is the `AC` key, erases memory. The Accumulator is unchanged.
    * `M+` adds the Accumulator to Memory. The Accumulator is unchanged.
    * `M-` subtracts the Accumulator from Memory. The Accumulator is unchanged.
    * `M*` multiplies Memory by the Accumulator. The Accumulator is unchanged.
    * `M%` multiplies Memory by the Accumulator / 100

## Issues

* Evaluation is not nested, so `10 + 4 * 2 = 28`, not `18` as on a normal calculator.  
  (I am not lazy, but I thought the stack evaluation would discourage beginners from tinkering with the code.)  
  Note that you can do any calculation you want, just pay attention to execution order.
* Implementation is procedural, not object oriented. (Same reason.)
* Currently the display font gets smaller if the value gets very large, but it never switches to scientific notation (like a calculator.)
* Overflow/Underflow are not detected.
* There is no calculation history.
