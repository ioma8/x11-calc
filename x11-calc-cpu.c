/*
 * x11-calc-cpu.c - RPN (Reverse Polish) calculator simulator.
 *
 * Copyright(C) 2020   MT
 *
 * Simulates the ACT processor.
 *
 * This processor simulator is based on the work of a number of individuals
 * including  Jacques LAPORTE, David HICKS, Greg SYDNEY-SMITH, Eric  SMITH,
 * Tony  NIXON and Bernhard EMESE.  Without their efforts and in some cases
 * assistance and encouragement this simulator not have been possible.
 *
 * Thank you.
 *
 * This  program is free software: you can redistribute it and/or modify it
 * under  the terms of the GNU General Public License as published  by  the
 * Free  Software Foundation, either version 3 of the License, or (at  your
 * option) any later version.
 *
 * This  program  is distributed in the hope that it will  be  useful,  but
 * WITHOUT   ANY   WARRANTY;   without even   the   implied   warranty   of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You  should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contains the type definitions and functions definitions used to  emulate
 * the  ACT processor with seven 56-bit Registers.  Each register  consists
 * of 14 4-bit nibbles capable of storing a 10 digit mantissa and a 2 digit
 * exponent with separate signs for both the mantissa and exponent.
 *
 * Each  16-bit  processor  instruction is  retrieved  from  the  currently
 * selected  ROM using either an 8-bit or 12-bit address (depending on  the
 * calculator model) and allows each of the registers (or the selected part
 * of  each  register)  to  be  cleared, copied,  exchanged,   incremented,
 * decremented, shifted left or right and tested.
 *
 * Arithmetic Registers
 *
 *  13   12  11  10  9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * | s | m | m | m | m | m | m | m | m | m | m | s | e | e |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * A, B, C:    General purpose registers.  The C register is used to access
 *             the M register and or memory as well as holding the value of
 *             the 'X' register.
 * Y, Z, T:    Stack registers.
 * M, N:       Memory registers.
 *
 * Special purpose registers
 *
 * F:          F register.
 * P:          A  4-bit register that is used to select which part of  each
 *             register should be used.
 * DATA:       An 8-bit register holding the memory address used to read or
 *             write to memory from the C register.
 * SP:         Stack pointer
 *
 * Processor flags
 *
 * F0          Selects Run / Program mode.
 * F1          Carry.
 * F2          Prev Carry.
 * F3          Delayed ROM select.
 * F4          ROM select
 * F5          Display enabled
 * F6          ???
 * F7          ???
 * F8          Timer.
 * F9          Trace enabled (implementation specific!).
 *
 * Processor status word.
 *
 * S0          Not used.
 * S1  *       Scientific notation (clear for fixed point notation).
 * S2  *       Auto Enter (if set entering digit will push 'X').
 * S3          Set for radians clear for degrees.
 * S4          Power OK (clear for lower power)
 * S5  ?       Set if decimal point has already been entered
 * S6          ?
 * S7          ?
 * S8          ?
 * S9          ?
 * S10         ?
 * S11         ?
 * S12         ?
 * S13         Set if function key has been pressed.
 * S14         Set if EEX has been pressed?
 * S15 *       Set if any key is pressed.
 *
 * Instruction set
 *
 * Special operations - May be one or two word instructions!
 *
 *   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+
 * | n | n | n | n | n | n | n | n | 0 | 0 |
 * +---+---+---+---+---+---+---+---+---+---+
 *
 * Jump Subroutine
 *
 *   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+
 * | n | n | n | n | n | n | n | n | 1 | 1 |
 * +---+---+---+---+---+---+---+---+---+---+
 *
 *   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+
 * | n | n | n | n | n | n | n | n | Address
 * +---+---+---+---+---+---+---+---+
 *
 * Arithmetic operations.
 *
 *   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+
 * | n | n | n | n | n | m | m | m | 1 | 0 |
 * +---+---+---+---+---+---+---+---+---+---+
 *
 * Where mmm is the field modifier
 *
 * 000   P  : determined by P register             ([P])
 * 001  WP  : word up to and including P register  ([0 .. P])
 * 010  XS  : exponent and sign                    ([0 .. 2])
 * 011   X  : exponent                             ([0 .. 1])
 * 100   S  : sign                                 ([13])
 * 101   M  : mantissa                             ([3 .. 12])
 * 110   W  : word                                 ([0 .. 13])
 * 111  MS  : mantissa and sign                    ([3 .. 13])
 *
 * Subroutine calls and long conditional jumps.
 *
 *   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+
 * | l | l | l | l | l | l | l | l | 0 | 1 |
 * +---+---+---+---+---+---+---+---+---+---+
 *
 *   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+
 * | h | h | h | h | h | h | h | h | t | t |
 * +---+---+---+---+---+---+---+---+---+---+
 *
 * Where tt is the type of jump:
 *
 * 00       : subroutine call if carry clear
 * 01       : subroutine call if carry set
 * 10       : jump if carry clear
 * 11       : jump if carry set
 *
 * Address format:
 *
 *  15  14  13  12  11  10   9   8       7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+   +---+---+---+---+---+---+---+---+
 * | h | h | h | h | h | h | h | h |   | l | l | l | l | l | l | l | l |
 * +---+---+---+---+---+---+---+---+   +---+---+---+---+---+---+---+---+
 *
 *
 * 10 Sep 20   0.1   - Initial version - MT
 * 08 Aug 21         - Tidied up spelling errors in the comments and  added
 *                     a section describing the processor registers - MT
 *                   - Changed the status word into an integer - MT
 * 10 Aug 21         - Modified register names - MT
 *                   - Defined register size as a constant - MT
 * 16 Aug 21         - Added   tick()  to  execute  a  single  instruction,
 *                     currently this only decodes two  instructions ('nop'
 *                     and  'hi I'm woodstock')  neither of which  actually
 *                     does anything.  Any unrecognised opcodes are  simply
 *                     ignored - MT
 * 19 Aug 21         - Created  two data structures to hold  the  processor
 *                     properties and the register properties and added the
 *                     code to create a new processor instance - MT
 * 20 Aug 21         - Updated tick() using the new data structures - MT
 * 22 Aug 21         - Can now decode the four main instruction types - MT
 * 24 Aug 21         - Added code to decode the field modifier - MT
 * 26 Aug 21         - Re-implemented load_reg() to use the processor  data
 *                     and renamed it set_register() - MT
 *                   - Reversed order of nibbles in each register to  match
 *                     the actual numbering (when the register format  says
 *                     the left hand nibble is number 13 it means it!) - MT
 *                   - Implemented my first real op code (0 -> a[f]) - MT
 * 27 Aug 21         - Added code to exchange register contents -MT
 * 28 Aug 21         - Created a separate method to display the contents of
 *                     a  register  (replacing the existing debug  code  in
 *                     register load and exchange routines) - MT
 *                   - Implemented routines to add and the contents of  two
 *                     registers  and used this to increment a register  by
 *                     setting the carry flag and then calling add register
 *                     with a single parameter - MT
 *                   - Added an identifier to the register properties  which
 *                     allows  the register name to be shown when  printing
 *                     the value - MT
 * 29 Aug 21         - Tidied up trace output - MT
 *                   - Changed the register names - MT
 * 30 Aug 21         - Changed  carry to use the processor flags, and  made
 *                     the  first  and last values used when selecting  the
 *                     field in a register processor properties - MT
 *                   - Changed all flags and status bits to integers - MT
 *                   - Run mode set by default - MT
 *                   - Tidied up the comments (again) - MT
 *                   - Separated the 'special' instruction into four groups
 *                     to make it easier to parse the parameters - MT
 *                   - Added several more functions - MT
 *                   - Implemented code to handle subroutines - MT
 *                   - Fixed compiler warnings - MT
 *                   - Added ROM select, and register copy - MT
 * 31 Aug 21         - Fixed typo in RAM initialisation code - MT
 *                   - Using a NULL argument with the copy register routine
 *                     means it can be used to replace the function used to
 *                     zero a register - MT
 *                     using the copy routine - MT
 *                   - Added tests for equals and not equals - MT
 *                   - Implemented shift right operations - MT
 *                   - Tidied up code used to trace execution - MT
 *                   - Unimplemented operations now generate errors - MT
 *                   - Removed superfluous breaks - MT
 *  1 Sep 21         - Removed  separate routines to set and  clear  status
 *                     bits - MT
 *                   - Added  a  routine  to display the  contents  of  the
 *                     registers and processor flags - MT
 *                   - Implemented  f -> a, if s(n) = 0, p = n,  if nc goto
 *                     instructions - MT
 *  2 Sep 21         - Added  a routine to subtract two registers,  setting
 *                     carry flag beforehand and passing a single parameter
 *                     allows it to be used to decrement a register - MT
 *                   - Carry cleared at the beginning of each tick - MT
 *                   - Added a subroutine to handle go to - MT
 *  4 Sep 21         - Sorted out bug in p <> 0 test - MT
 *  5 Sep 21         - Moved trace output to the start of each  instruction
 *                     to make debugging easier - MT
 *                   - Fixed calculation of 8-bit branch addresses - MT
 *                   - Made  the handling of the carry flag consistent  (so
 *                     that  incrementing the program counter always clears
 *                     the carry flag and set the previous carry) - MT
 *                   - Implemented code used to read a key code and jump to
 *                     the correct address in memory - MT
 *                   - Added a keycode and keystate properties to store the
 *                     key  code of the key and the state of the actual key
 *                     (necessary as clearing status bit 15 when the key is
 *                     released does NOT work!) - MT
 *                   - Modified  code to allow status bit 15 to be  cleared
 *                     if a key is not pressed - MT
 *  8 Sep 21         - Stack operations now work (ENTER, X-Y, and R) - MT
 *  9 Sep 21         - Modified add and subtract to specify the destination
 *                     separately and allow NULL arguments - MT
 *                   - Logical shift clears carry flags - MT
 *                   - Don't clear status bits when tested! - MT
 *                   - Added 'y -> a', '0 - c - 1 -> c[f]', 'if a >= c[f]',
 *                     'a - c -> c[f]',  and 'if a >= b[f]' - MT
 *                   - Basic arithmetic operations now work!! - MT
 *                   - Fixed bug in clear s - MT
 * 10 Sep 21         - Restructured  decoder for group 2 type  instructions
 *                     and implemented load n - MT
 * 14 Sep 21         - Fixed  'm2 -> c' (registers were reversed) - MT
 *                   - Implemented '0 - c - 1 -> c[f]' - MT
 *                   - Removed  'return'  function(only two lines and  used
 *                     just once) - MT
 *                   - Modified some error messages - MT
 *                   - Tidied up extra spaces - MT
 *             0.2   - It works !!
 * 15 Sep 21         - Added 'clear data registers', and delayed ROM select
 *                     handling to 'jsb' and 'goto' - MT
 *                   - Moved  the processor initialisation into a  separate
 *                     routine  to allow main routine to do a reset without
 *                     having to exit and restart the program - MT
 * 16 Sep 21         - Hopefully now handles bank switching better - MT
 * 22 Sep 21         - Added 'c -> data address' - MT
 *                   - Added the line number to the unexpected opcode error
 *                     message - MT
 *
 * To Do             - Fix handle conditional branch operations properly
 *                   - Overlay program memory storage onto data registers (
 *                     different data structures pointing at the same data).
 *
 *
 */

#define VERSION        "0.2"
#define BUILD          "0008"
#define DATE           "14 Sep 21"
#define AUTHOR         "MT"

#define DEBUG 0        /* Enable/disable debug*/

#include <stdlib.h>    /* malloc(), etc. */
#include <stdio.h>     /* fprintf(), etc. */
#include <stdarg.h>

#include <X11/Xlib.h>  /* XOpenDisplay(), etc. */
#include <X11/Xutil.h> /* XSizeHints etc. */

#include "x11-calc-font.h"
#include "x11-calc-button.h"
#include "x11-calc-cpu.h"

#include "gcc-debug.h" /* print() */
#include "gcc-wait.h"  /* i_wait() */

/* Print the contents of a register */
void v_reg_fprint(FILE *h_file, oregister *h_register) {
   const char c_name[8] = {'A', 'B', 'C', 'Y', 'Z', 'T', 'M', 'N'};
   int i_count;
   if (h_register != NULL) {
      fprintf(h_file, "reg[");
      if (h_register->id < 0)
         fprintf(h_file, "*%c", c_name[h_register->id * -1 - 1]);
      else
         fprintf(h_file, "%02d", h_register->id);
      fprintf(h_file, "] = 0x");
      for (i_count = REG_SIZE - 1; i_count >=0 ; i_count--) {
         fprintf(h_file, "%1x", h_register->nibble[i_count]);
      }
      fprintf(h_file, "  ");
   }
}

/* Display the current processor status word */
void v_status_fprint(FILE *h_file, oprocessor *h_processor) {
   int i_count, i_temp = 0;
   if (h_processor != NULL) {
      for (i_count = (sizeof(h_processor->status) / sizeof(*h_processor->status)); i_count >= 0; i_count--)
         i_temp = (i_temp << 1) | h_processor->status[i_count];
      fprintf(h_file, "0x%04x%12c", i_temp, ' ');
   }
}

/* Display the current processor flags */
void v_flags_fprint(FILE *h_file, oprocessor *h_processor) {
   int i_count, i_temp = 0;
   if (h_processor != NULL) {
      for (i_count = 0; i_count < TRACE; i_count++) /* Ignore TRACE flag */
         i_temp += h_processor->flags[i_count] << i_count;
      fprintf(stdout, "Ox%02x (", i_temp);
      for (i_count = 0; i_count < TRACE; i_count++)
         fprintf(stdout, "%d", h_processor->flags[TRACE - 1 - i_count]);
      fprintf(stdout, ")  ");
   }
}

/* Display the current processor flags */
void v_ptr_fprint(FILE *h_file, oprocessor *h_processor) {
   if (h_processor != NULL) fprintf(stdout, "%02d ", h_processor->p);
}

void v_state_fprint(FILE *h_file, oprocessor *h_processor) {
   int i_count;
   for (i_count = 0; i_count < REGISTERS; i_count++) {
      if (i_count % 3 == 0) fprintf(stdout, "\n\t");
      v_reg_fprint(stdout, h_processor->reg[i_count]);
   }
   fprintf(stdout, "\n\tflags[] = ");
   v_flags_fprint(stdout, h_processor);
   fprintf(stdout, "status  = ");
   v_status_fprint(stdout, h_processor);
   fprintf(stdout, "ptr     = ");
   v_ptr_fprint(stdout, h_processor);
   fprintf(stdout, "\n\n");
}

/* Create a new register , */
oregister *h_register_create(char c_id){
   oregister *h_register; /* Pointer to register. */
   int i_count, i_temp;
   if ((h_register = malloc (sizeof(*h_register))) == NULL) {
      fprintf(stderr, "Run-time error\t: %s line : %d : %s", __FILE__, __LINE__, "Memory allocation failed!\n");
   }
   i_temp = sizeof(h_register->nibble) / sizeof(*h_register->nibble);
   h_register->id = c_id;
   for (i_count = 0; i_count < i_temp; i_count++)
      h_register->nibble[i_count] = 0;
   /* debug(v_reg_fprint(stdout, h_register); fprintf(stdout, "\n")); */
   return (h_register);
}

/* Load a register */
void v_reg_load(oregister *h_register, ...) {
   int i_count, i_temp;
   va_list t_args;
   va_start(t_args, h_register);
   i_temp = sizeof(h_register->nibble) / sizeof(*h_register->nibble) - 1;
   for (i_count = i_temp; i_count >= 0; i_count--)
      h_register->nibble[i_count]  = va_arg(t_args, int);
}

/* Exchange the contents of two registers */
static void v_reg_exch(oprocessor *h_processor, oregister *h_destination, oregister *h_source) {
   int i_count, i_temp;
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++) {
      i_temp = h_destination->nibble[i_count];
      h_destination->nibble[i_count] = h_source->nibble[i_count];
      h_source->nibble[i_count] = i_temp;
   }
}

/* Copy the contents of a register */
static void v_reg_copy(oprocessor *h_processor, oregister *h_destination, oregister *h_source) {
   int i_count, i_temp;
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++) {
      if (h_source != NULL) i_temp = h_source->nibble[i_count]; else i_temp = 0;
      h_destination->nibble[i_count] = i_temp;
   }
}

/* Add the contents of two registers */
static void v_reg_add(oprocessor *h_processor, oregister *h_destination, oregister *h_source, oregister *h_argument){
   int i_count, i_temp;
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++){
      if (h_argument != NULL) i_temp = h_argument->nibble[i_count]; else i_temp = 0;
      i_temp = h_source->nibble[i_count] + i_temp + h_processor->flags[CARRY];
      if (i_temp >= h_processor->base){
         i_temp -= h_processor->base;
         h_processor->flags[CARRY] = 1;
      } else {
         h_processor->flags[CARRY] = 0;
      }
      if (h_destination != NULL) h_destination->nibble[i_count] = i_temp; /* Destination can be null */
   }
}

/* Subtract the contents of two registers */
static void v_reg_sub(oprocessor *h_processor, oregister *h_destination, oregister *h_source, oregister *h_argument){
   int i_count, i_temp;
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++){
      if (h_argument != NULL) i_temp = h_argument->nibble[i_count]; else i_temp = 0;
      if (h_source != NULL) i_temp = (h_source->nibble[i_count] - i_temp); else i_temp = (0 - i_temp);
      i_temp = i_temp - h_processor->flags[CARRY];
      if (i_temp < 0) {
         i_temp += h_processor->base;
         h_processor->flags[CARRY] = 1;
      }
      else
         h_processor->flags[CARRY] = 0;
      if (h_destination != NULL) h_destination->nibble[i_count] = i_temp; /* Destination can be null */
   }
}

/* Test if registers are equal */
static void v_reg_test_eq(oprocessor *h_processor, oregister *h_destination, oregister *h_source) {
  int i_count, i_temp;
  h_processor->flags[CARRY] = 0; /* Clear carry - Do If True */
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++){
      if (h_source != NULL) i_temp = h_source->nibble[i_count]; else i_temp = 0;
      if (h_destination->nibble[i_count] != i_temp) {
         h_processor->flags[CARRY] = 1; /* Set carry */
         break;
      }
   }
}

/* Test if registers are not equal */
static void v_reg_test_ne(oprocessor *h_processor, oregister *h_destination, oregister *h_source) {
  int i_count, i_temp;
  h_processor->flags[CARRY] = 1;
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++){
      if (h_source != NULL) i_temp = h_source->nibble[i_count]; else i_temp = 0;
      if (h_destination->nibble[i_count] != i_temp) {
         h_processor->flags[CARRY] = 0; /* Clear carry - Do If True */
         break;
      }
   }
}

/* Negate the contents of a register
static void v_reg_negate(oprocessor *h_processor, oregister *h_register){
   int i_count, i_temp;
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++){
      i_temp = 0 - (h_destination->nibble[i_count]) - h_processor->flags[CARRY];
      if (i_temp < 0) {
         i_temp += h_processor->base;
         h_processor->flags[CARRY] = 1;
      }
      else
         h_processor->flags[CARRY] = 0;
      h_destination->nibble[i_count] = i_temp;
   }
} */

/* Increment the contents of a register */
static void v_reg_inc(oprocessor *h_processor, oregister *h_register){
   h_processor->flags[CARRY] = 1; /* Set carry */
   v_reg_add (h_processor, h_register, h_register, NULL); /* Add carry to register */
}

/* Logical shift right a register */
static void v_reg_shr(oprocessor *h_processor, oregister *h_register){
   int i_count;
   h_processor->flags[CARRY] = 0; /* Clear carry */
   for (i_count = h_processor->first; i_count <= h_processor->last; i_count++)
      if (i_count == h_processor->last)
         h_register->nibble[i_count] = 0;
      else
         h_register->nibble[i_count] = h_register->nibble[i_count + 1];
}

/* Logical shift left a register */
static void v_reg_shl(oprocessor *h_processor, oregister *h_register){
   int i_count;
   for (i_count = h_processor->last; i_count >= h_processor->first; i_count--)
      if (i_count == h_processor->first)
         h_register->nibble[i_count] = 0;
      else
         h_register->nibble[i_count] = h_register->nibble[i_count - 1];
   h_processor->flags[PREV_CARRY] = h_processor->flags[CARRY] = 0;
}

/* Clear registers */
void v_processor_clear_registers(oprocessor *h_processor) {
   int i_count;
   h_processor->first = 0; h_processor->last = REG_SIZE - 1;
   for (i_count = 0; i_count < REGISTERS; i_count++)
      v_reg_copy(h_processor, h_processor->reg[i_count], NULL); /* Copying nothing to a register clears it */
   for (i_count = 0; i_count < STACK_SIZE; i_count++)
      h_processor->stack[i_count] = 0; /* Clear the processor stack */
}

/* Clear data registers */
void v_processor_clear_data_registers(oprocessor *h_processor) {
   int i_count;
   h_processor->first = 0; h_processor->last = REG_SIZE - 1;
   for (i_count = 0; i_count < DATA_REGISTERS; i_count++)
      v_reg_copy(h_processor, h_processor->ram[i_count], NULL); /* Copying nothing to a register clears it */
}

/* Reset processor */
void v_processor_init(oprocessor *h_processor) {
   int i_count;
   v_processor_clear_registers(h_processor); /*Clear the CPU registers and stack */
   v_processor_clear_data_registers(h_processor); /* Clear the memory registers*/
   for (i_count = 0; i_count < (sizeof(h_processor->status) / sizeof(h_processor->status[0]) ); i_count++)
      h_processor->status[i_count] = 0; /* Clear the processor status word */
   for (i_count = 0; i_count < FLAGS; i_count++)
      h_processor->flags[i_count] = False; /* Clear the processor flags */
   h_processor->status[5] = 1; /* TO DO - Check which flags should be set by default */
   h_processor->status[3] = 1;
   h_processor->flags[MODE] = 1; /* Select RUN mode */
   h_processor->pc = 0;
   h_processor->sp = 0;
   h_processor->p = 0;
   h_processor->f = 0;
   h_processor->keycode = 0;
   h_processor->keydown = 0;
   h_processor->base = 10;
   h_processor->delayed_rom_number = 0;
   h_processor->rom_number = 0;
}

/* Create a new processor , */
oprocessor *h_processor_create(int *h_rom){
   oprocessor *h_processor;
   int i_count;
   if ((h_processor = malloc(sizeof(*h_processor)))==NULL) v_error("Memory allocation failed!"); /* Attempt to allocate memory to hold the processor structure. */
   for (i_count = 0; i_count < REGISTERS; i_count++)
      h_processor->reg[i_count] = h_register_create((i_count + 1) * -1); /* Allocate storage for the registers */
   for (i_count = 0; i_count < DATA_REGISTERS; i_count++)
      h_processor->ram[i_count] = h_register_create(i_count); /* Allocate storage for the RAM */
   h_processor->rom = h_rom ; /* Address of ROM */
   v_processor_init(h_processor);
   return(h_processor);
}

/* Delayed ROM select */
static void delayed_rom_switch(oprocessor *h_processor) { /* Delayed ROM select */
   if (h_processor->flags[DELAYED_ROM] != 0) {
      if (h_processor->flags[TRACE]) fprintf(stdout, " ** ");
      h_processor->pc = h_processor->delayed_rom_number << 8 | (h_processor->pc & 00377);
      h_processor->flags[DELAYED_ROM] = 0; /* Clear flag */
   }
}

/* Increment program counter */
static void v_op_inc_pc(oprocessor *h_processor) {
   if (h_processor->pc++ >= (ROM_SIZE - 1)) h_processor->pc = 0;
   h_processor->flags[PREV_CARRY] = h_processor->flags[CARRY];
   h_processor->flags[CARRY] = 0;
}

/* Jump to subroutine */
void op_jsb(oprocessor *h_processor, int i_count){
   h_processor->stack[h_processor->sp] = h_processor->pc; /* Push program counter on the stack */
   h_processor->sp = (h_processor->sp + 1) & (STACK_SIZE - 1); /* Update stack pointer */
   h_processor->pc = ((h_processor->pc & 0xff00) | i_count) - 1; /* Program counter will be auto incremented before next fetch */
   delayed_rom_switch(h_processor);
}

/* Return from subroutine */
void v_op_rtn(oprocessor *h_processor) {
   h_processor->sp = (h_processor->sp - 1) & (STACK_SIZE - 1); /* Update stack pointer */
   h_processor->pc = h_processor->stack[h_processor->sp]; /* Pop program counter on the stack */
}

/* Conditional go to */
void v_op_goto(oprocessor *h_processor){
   if (h_processor->flags[TRACE]) fprintf(stdout, "\n%1o-%04o %04o    then goto %01o-%04o",
      h_processor->rom_number, h_processor->pc, h_processor->rom[h_processor->pc], h_processor->rom_number, h_processor->rom[h_processor->pc]);
   if (h_processor->flags[PREV_CARRY] == 0) { /* Do if True */
      h_processor->pc = h_processor->rom[h_processor->pc] - 1; /* Program counter will be auto incremented before next fetch */
      /* h_processor->pc = ((h_processor->pc & 0xff00) | h_processor->rom[h_processor->pc]) - 1; /* Program counter will be auto incremented before next fetch */
      delayed_rom_switch(h_processor);
   }
}


/* Decode and execute a single instruction */
void v_processor_tick(oprocessor *h_processor) {

   static const int i_set_p[16] = { 14,  4,  7,  8, 11,  2, 10, 12,  1,  3, 13,  6,  0,  9,  5, 14 };
   static const int i_tst_p[16] = { 4 ,  8, 12,  2,  9,  1,  6,  3,  1, 13,  5,  0, 11, 10,  7,  4 };

   unsigned int i_opcode;
   unsigned int i_field; /* Field modifier */
   const char *s_field; /* Holds pointer to field name */

   if (h_processor->flags[TRACE])
      fprintf(stdout, "%1o-%04o %04o  ", h_processor->rom_number, h_processor->pc, h_processor->rom[h_processor->pc]);

   i_opcode = h_processor->rom[h_processor->pc];

   switch (i_opcode & 03) {
   case 00: /* Special operations */
      switch ((i_opcode >> 2) & 03) {
      case 00: /* Group 0 */
         switch ((i_opcode >> 4) & 03) {
         case 00: /* nop */
            if (h_processor->flags[TRACE]) fprintf(stdout, "nop");
            break;
         case 01:
            switch (i_opcode){
            case 00020: /* keys -> rom address */
               if (h_processor->flags[TRACE]) fprintf(stdout, "keys -> rom address ");
               h_processor->pc &= 0x0f00;
               h_processor->pc += h_processor->keycode - 1;
               break;
            case 00420: /* binary */
               if (h_processor->flags[TRACE]) fprintf(stdout, "binary");
               h_processor->base = 16;
               break;
            case 00620: /* p - 1 -> p */
               if (h_processor->flags[TRACE]) fprintf(stdout, "p - 1 -> p");
               if (h_processor->p == 0 ) h_processor->p = REG_SIZE; else h_processor->p--;
               break;
            case 00720: /* p - 1 -> p */
               if (h_processor->flags[TRACE]) fprintf(stdout, "p - 1 -> p");
               if (h_processor->p == REG_SIZE ) h_processor->p = 0; else h_processor->p++;
               break;
            case 01020: /* return */
               if (h_processor->flags[TRACE]) fprintf(stdout, "return");
               h_processor->sp = (h_processor->sp - 1) & (STACK_SIZE - 1); /* Update stack pointer */
               h_processor->pc = h_processor->stack[h_processor->sp]; /* Pop program counter on the stack */
               break;
            default:
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
            }
            break;
         case 02: /* select rom */
            if (h_processor->flags[TRACE]) fprintf(stdout, "select rom %02d", i_opcode >> 6);
            h_processor->pc = (i_opcode >> 6) * 256 + (h_processor->pc % 256);
            break;
         case 03:
            switch (i_opcode) {
            case 01160: /* c -> data address  */
               if (h_processor->flags[TRACE]) fprintf(stdout, "c -> data address ");
               h_processor->address = (h_processor->reg[C_REG]->nibble[1] << 4) + h_processor->reg[C_REG]->nibble[0];
               if (h_processor->address >= ROM_SIZE * ROM_BANKS)
                  v_error("Address %05o out of range in  %s line : %d\n", h_processor->address, __FILE__, __LINE__);
               break;
            case 01260: /* clear data registers */
               if (h_processor->flags[TRACE]) fprintf(stdout, "clear data registers");
               v_processor_clear_data_registers(h_processor);
               break;
            case 01760: /* hi I'm woodstock */
               if (h_processor->flags[TRACE]) fprintf(stdout, "hi I'm woodstock");
               break;
            default:
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
            }
         }
         break;
      case 01: /* Group 1 */
         switch ((i_opcode >> 4) & 03) {
         case 00: /* 1 -> s(n) */
            if (h_processor->flags[TRACE]) fprintf(stdout, "1 -> s(%d)", i_opcode >> 6);
            h_processor->status[i_opcode >> 6] = 1;
            break;
         case 01: /* if 1 = s(n) */
            if (h_processor->flags[TRACE]) fprintf(stdout, "if 1 = s(%d)", i_opcode >> 6);
            if (h_processor->flags[TRACE]) fprintf(stdout, " (s(%d) == %d)", i_opcode >> 6, h_processor->status[i_opcode >> 6]);
            h_processor->flags[CARRY] = (h_processor->status[i_opcode >> 6] != 1);
            switch (i_opcode >> 6) {
               case 1:  /* Scientific notation */
               case 2:  /* Auto Enter (if set entering digit will push 'X') */
               case 5:  /* Set if decimal point has already been entered */
               case 15: /* Set if any key is pressed */
                  break;
               /* default:
                  h_processor->status[i_opcode >> 6] = 0; /* Testing the status clears it ??? */
            }
            v_op_inc_pc(h_processor); /* Increment program counter */
            v_op_goto(h_processor);
            break;
         case 02: /* if p = n */
            if (h_processor->flags[TRACE]) fprintf(stdout, "if p = %d", i_tst_p[i_opcode >> 6]);
            h_processor->flags[CARRY]= !(h_processor->p == i_tst_p[i_opcode >> 6]);
            v_op_inc_pc(h_processor); /* Increment program counter */
            v_op_goto(h_processor);
            break;
         case 03: /* delayed select rom n */
            if (h_processor->flags[TRACE]) fprintf(stdout, "delayed select rom %d", i_opcode >> 6);
            h_processor->delayed_rom_number = i_opcode >> 6;
            h_processor->flags[DELAYED_ROM] = 1;
         }
         break;
      case 02: /* Group 2 */
         switch ((i_opcode >> 4) & 03) {
         case 00: /* Sub group 0 */
            switch (i_opcode) {
            case 00010: /* clear registers */
               if (h_processor->flags[TRACE]) fprintf(stdout, "clear registers");
               v_processor_clear_registers(h_processor);
               break;
            case 00110: /* clear s */
               if (h_processor->flags[TRACE]) fprintf(stdout, "clear s");
               {
                  int i_count;
                  for (i_count = 0; i_count < sizeof(h_processor->status) / sizeof(*h_processor->status); i_count++)
                     switch (i_count) {
                        case 1:  /* Scientific notation */
                        case 2:  /* Auto Enter (if set entering digit will push 'X') */
                        case 5:  /* Set if decimal point has already been entered */
                        case 15: /* Set if any key is pressed */
                           break;
                        default:
                           h_processor->status[i_count] = 0; /* Clear all bits except bits 1, 2, 5, 15 */
                     }
               }
               break;
            case 00210: /* display toggle */
               if (h_processor->flags[TRACE]) fprintf(stdout, "display toggle");
               if (h_processor->flags[DISPLAY_ENABLE] == 0)
                  h_processor->flags[DISPLAY_ENABLE] = 1;
               else
                  h_processor->flags[DISPLAY_ENABLE] = 0;
               break;
            case 00310: /* display off */
               if (h_processor->flags[TRACE]) fprintf(stdout, "display off");
               h_processor->flags[DISPLAY_ENABLE] = 0;
               break;
            case 00410: /* m1 exch c */
               if (h_processor->flags[TRACE]) fprintf(stdout, "m1 exch c");
               h_processor->first = 0; h_processor->last = REG_SIZE - 1;
               v_reg_exch(h_processor, h_processor->reg[M_REG], h_processor->reg[C_REG]);
               break;
            case 00510: /* m1 -> c */
               if (h_processor->flags[TRACE]) fprintf(stdout, "m1 -> c");
               h_processor->first = 0; h_processor->last = REG_SIZE - 1;
               v_reg_copy(h_processor, h_processor->reg[C_REG], h_processor->reg[M_REG]);
               break;
            case 00610: /* m2 exch c */
               if (h_processor->flags[TRACE]) fprintf(stdout, "m2 exch c");
               h_processor->first = 0; h_processor->last = REG_SIZE - 1;
               v_reg_exch(h_processor, h_processor->reg[N_REG], h_processor->reg[C_REG]);
               break;
            case 00710: /* m2 -> c */
               if (h_processor->flags[TRACE]) fprintf(stdout, "m2 -> c");
               h_processor->first = 0; h_processor->last = REG_SIZE - 1;
               v_reg_copy(h_processor, h_processor->reg[C_REG], h_processor->reg[N_REG]);
               break;
            case 01010: /* stack -> a */
               if (h_processor->flags[TRACE]) fprintf(stdout, "stack -> a");
                  h_processor->first = 0; h_processor->last = REG_SIZE - 1;
                  v_reg_copy(h_processor, h_processor->reg[A_REG], h_processor->reg[Y_REG]); /* T = Z  */
                  v_reg_copy(h_processor, h_processor->reg[Y_REG], h_processor->reg[Z_REG]); /* T = Z  */
                  v_reg_copy(h_processor, h_processor->reg[Z_REG], h_processor->reg[T_REG]); /* T = Z  */
               break;
            case 01110: /* down rotate */
               if (h_processor->flags[TRACE]) fprintf(stdout, "stack -> a");
                  h_processor->first = 0; h_processor->last = REG_SIZE - 1;
                  v_reg_exch(h_processor, h_processor->reg[T_REG], h_processor->reg[C_REG]); /* T = Z  */
                  v_reg_exch(h_processor, h_processor->reg[C_REG], h_processor->reg[Y_REG]); /* T = Z  */
                  v_reg_exch(h_processor, h_processor->reg[Y_REG], h_processor->reg[Z_REG]); /* T = Z  */
               break;
            case 01210: /* y -> a */
               if (h_processor->flags[TRACE]) fprintf(stdout, "y -> a");
               h_processor->first = 0; h_processor->last = REG_SIZE - 1;
               v_reg_copy(h_processor, h_processor->reg[A_REG], h_processor->reg[Y_REG]);
               break;
            case 01310: /* c -> stack */
               if (h_processor->flags[TRACE]) fprintf(stdout, "stack -> a");
                  h_processor->first = 0; h_processor->last = REG_SIZE - 1;
                  v_reg_copy(h_processor, h_processor->reg[T_REG], h_processor->reg[Z_REG]); /* T = Z  */
                  v_reg_copy(h_processor, h_processor->reg[Z_REG], h_processor->reg[Y_REG]); /* T = Z  */
                  v_reg_copy(h_processor, h_processor->reg[Y_REG], h_processor->reg[C_REG]); /* T = Z  */
               break;
            case 01410: /* decimal */
               if (h_processor->flags[TRACE]) fprintf(stdout, "decimal");
               h_processor->base = 10;
               break;
            case 01610: /* f -> a */
               h_processor->reg[A_REG]->nibble[0] = h_processor->f;
               if (h_processor->flags[TRACE]) fprintf(stdout, "f -> a");
               break;
            case 01710: /* f exch a */
               if (h_processor->flags[TRACE]) fprintf(stdout, "f exch a");
               {
                  int i_temp;
                  i_temp = h_processor->reg[A_REG]->nibble[0];
                  h_processor->reg[A_REG]->nibble[0] = h_processor->f;
                  h_processor->f = i_temp;
               }
               break;
            default:
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
            }
            break;
         case 01: /* load n */
            if (h_processor->flags[TRACE]) fprintf(stdout, "load (%d)", i_opcode >> 6);
            h_processor->reg[C_REG]->nibble[h_processor->p] = i_opcode >> 6;
            if (h_processor->p > 0) h_processor->p--; else h_processor->p = REG_SIZE - 1;
            break;
         case 02: /* c -> data register(n) */
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
            break;
         case 03: /* c -> addr or data register(n)-> c (for n > 0) */
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
            break;
         default:
            v_error("Unexpected error in  %s line : %d\n", __FILE__, __LINE__);
         }
         break;

      case 03: /* Group 3 */
         switch ((i_opcode >> 4) & 03) {
         case 00: /* 0 -> s(n) */
            if (h_processor->flags[TRACE]) fprintf(stdout, "0 -> s(%d)", i_opcode >> 6);
            switch (i_opcode >> 6) {
               case 5:  /* TO DO - Why is this set immediately after it is cleared? */
               case 15: /* Don't clear if a key is pressed */
                  if (h_processor->keydown == 0) h_processor->status[15] = 0;
                  break;
               default:
                  h_processor->status[i_opcode >> 6] = 0;
            }
            break;
         case 01: /* if 0 = s(n) */
            if (h_processor->flags[TRACE]) fprintf(stdout, "if 0 = s(%d)", i_opcode >> 6);
            if (h_processor->flags[TRACE]) fprintf(stdout, " (s(%d) == %d)", i_opcode >> 6, h_processor->status[i_opcode >> 6]);
            if (h_processor->status[i_opcode >> 6] != 0)
               h_processor->flags[CARRY]  = 1;
            else
               h_processor->flags[CARRY]  = 0;
            v_op_inc_pc(h_processor); /* Increment program counter */
            v_op_goto(h_processor);
            break;
         case 02: /* if p <> n */
            /* 01354 if p <>  0  00554 if p <>  1  00354 if p <>  2
             * 00754 if p <>  3  00054 if p <>  4  01254 if p <>  5
             * 00654 if p <>  6  01654 if p <>  7  00154 if p <>  8
             * 00454 if p <>  9  01554 if p <> 10  01454 if p <> 11
             * 00254 if p <> 12  01154 if p <> 13  N/A   if p <> 14
             * N/A   if p <> 15
             */
            if (h_processor->flags[TRACE]) fprintf(stdout, "if p # %d", i_tst_p[i_opcode >> 6]);
            h_processor->flags[CARRY] = (h_processor->p == i_tst_p[i_opcode >> 6]);
            /* TO DO */
            v_op_inc_pc(h_processor); /* Increment program counter */
            v_op_goto(h_processor);
            break;
         case 03: /* p = n */
            /* 01474  0 -> p  01074  1 -> p  00574  2 -> p
             * 01174  3 -> p  00174  4 -> p  01674  5 -> p
             * 01374  6 -> p  00274  7 -> p  00374  8 -> p
             * 01574  9 -> p  00674 10 -> p  00474 11 -> p
             * 00774 12 -> p  01274 13 -> p  N/A   14 -> p
             * N/A   15 -> p
             * */
            if (h_processor->flags[TRACE]) fprintf(stdout, "p = %d", i_set_p[i_opcode >> 6]);
            h_processor->p = i_set_p[i_opcode >> 6];
            break;
         }
         break;
      }
      break;

   case 01: /* jsb */
      if (h_processor->flags[TRACE]) fprintf(stdout, "jsb %01o-%04o", h_processor->rom_number, ((h_processor->pc & 0xff00) | i_opcode >> 2));
      /* op_jsb(h_processor, ((h_processor->pc & 0xff00) | i_opcode >> 2)); */
      op_jsb(h_processor, (i_opcode >> 2));
      break;
   case 02: /* Arithmetic operations */
      i_field = (i_opcode >> 2) & 7;

      switch (i_field) {
      case 0: /* 000   P  : determined by P register             ([P]) */
         h_processor->first = h_processor->p; h_processor->last = h_processor->p;
         s_field = "p";
         if (h_processor->p >= REG_SIZE) {
            v_error("Unexpected error in  %s line : %d\n", __FILE__, __LINE__);
            h_processor->last = 0;
         }
         break;
      case 1: /* 100  WP  : word up to and including P register  ([0 .. P])  */
         h_processor->first =  0; h_processor->last =  h_processor->p; /* break; bug in orig??? */
         s_field = "wp";
         s_field = "wp";
         if (h_processor->p >= REG_SIZE) {
            v_error("Unexpected error in  %s line : %d\n", __FILE__, __LINE__);
            h_processor->last = REG_SIZE - 1;
         }
         break;
      case 2: /* 110  XS  : exponent and sign                    ([0 .. 2])  */
         h_processor->first = EXP_SIZE - 1; h_processor->last = EXP_SIZE - 1;
         s_field = "xs";
         break;
      case 3: /* 010   X  : exponent                             ([0 .. 1])  */
         h_processor->first = 0; h_processor->last = EXP_SIZE - 1;
         s_field = "x";
         break;
      case 4: /* 111   S  : sign                                 ([13])      */
         h_processor->first = REG_SIZE - 1; h_processor->last = REG_SIZE - 1;
         s_field = "s";
         break;
      case 5: /* 001   M  : mantissa                             ([3 .. 12]) */
         h_processor->first = EXP_SIZE; h_processor->last = REG_SIZE - 2;
         s_field = "m";
         break;
      case 6: /* 011   W  : word                                 ([0 .. 13]) */
         h_processor->first = 0; h_processor->last = REG_SIZE - 1;
         s_field = "w";
         break;
      case 7: /* 101  MS  : mantissa and sign                    ([3 .. 13]) */
         h_processor->first = EXP_SIZE; h_processor->last = REG_SIZE - 1;
         s_field = "ms";
         break;
      }

      switch (i_opcode >> 5)
      {
      case 0000: /* 0 -> a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "0 -> a[%s]", s_field);
         v_reg_copy(h_processor, h_processor->reg[A_REG], NULL);
         break;
      case 0001: /* 0 -> b[f] */
         if (h_processor->flags[TRACE])fprintf(stdout, "0 -> b[%s]", s_field);
         v_reg_copy(h_processor, h_processor->reg[B_REG], NULL);
         break;
      case 0002: /* a exch b[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a exch b[%s]", s_field);
         v_reg_exch(h_processor, h_processor->reg[A_REG], h_processor->reg[B_REG]);
         break;
      case 0003: /* a -> b[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a -> b[%s]", s_field);
         v_reg_copy(h_processor, h_processor->reg[B_REG], h_processor->reg[A_REG]);
         break;
      case 0004: /* a exch c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a exch c[%s]", s_field);
         v_reg_exch(h_processor, h_processor->reg[A_REG], h_processor->reg[C_REG]);
         break;
      case 0005: /* c -> a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "c -> a[%s]", s_field);
         v_reg_copy(h_processor, h_processor->reg[A_REG], h_processor->reg[C_REG]);
         break;
      case 0006: /* b -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "b -> c[%s]", s_field);
         v_reg_copy(h_processor, h_processor->reg[C_REG], h_processor->reg[B_REG]);
         break;
      case 0007: /* b exchange c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "b exch c[%s]", s_field);
         v_reg_exch(h_processor, h_processor->reg[B_REG], h_processor->reg[C_REG]);
         break;
      case 0010: /* 0 -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "0 -> c[%s]", s_field);
         v_reg_copy(h_processor, h_processor->reg[C_REG], NULL);
         break;
      case 0011: /* a + b -> a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a + b -> a[%s]", s_field);
         v_reg_add(h_processor, h_processor->reg[A_REG], h_processor->reg[A_REG], h_processor->reg[B_REG]);
         break;
      case 0012: /* a + c -> a[f] */
         v_reg_add(h_processor, h_processor->reg[A_REG], h_processor->reg[A_REG], h_processor->reg[C_REG]);
         if (h_processor->flags[TRACE]) fprintf(stdout, "a + c -> a[%s]", s_field);
         break;
      case 0013: /* c + c -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "c + c -> c[%s]", s_field);
         v_reg_add(h_processor, h_processor->reg[C_REG], h_processor->reg[C_REG], h_processor->reg[C_REG]);
         break;
      case 0014: /* a + c -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a + c -> c[%s]", s_field);
         v_reg_add(h_processor, h_processor->reg[C_REG], h_processor->reg[C_REG], h_processor->reg[A_REG]);
         break;
      case 0015: /* a + 1 -> a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a + 1 -> a[%s]", s_field);
         v_reg_inc(h_processor, h_processor->reg[A_REG]);
         break;
      case 0016: /* shift left a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "shift left a[%s]", s_field);
         fflush(stdout);
         v_reg_shl(h_processor, h_processor->reg[A_REG]);
         break;
      case 0017: /* c + 1 -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "c + 1 -> c[%s]\t", s_field);
         v_reg_inc(h_processor, h_processor->reg[C_REG]);
         break;
      case 0020: /* a - b -> a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a - b -> a[%s]", s_field);
         v_reg_sub(h_processor, h_processor->reg[A_REG], h_processor->reg[A_REG], h_processor->reg[B_REG]);
         break;
      case 0021: /* a - c -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a - c -> c[%s]", s_field);
         v_reg_sub(h_processor, h_processor->reg[C_REG], h_processor->reg[A_REG], h_processor->reg[C_REG]);
         break;
      case 0022: /* a - 1 -> a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a - 1 -> a[%s]", s_field);
         h_processor->flags[CARRY] = 1; /* Set carry */
         v_reg_sub(h_processor, h_processor->reg[A_REG], h_processor->reg[A_REG], NULL);
         break;
      case 0023: /* c - 1 -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "c - 1 -> c[%s]", s_field);
         h_processor->flags[CARRY] = 1; /* Set carry */
         v_reg_sub(h_processor, h_processor->reg[C_REG], h_processor->reg[C_REG], NULL);
         break;
      case 0024: /* 0 - c -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "0 - c -> c[%s]", s_field);
         v_reg_sub(h_processor, h_processor->reg[C_REG], NULL, h_processor->reg[C_REG]);
         break;
      case 0025: /* 0 - c - 1 -> c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "0 - c - 1 -> c[%s]", s_field);
         h_processor->flags[CARRY] = 1; /* Set carry */
         v_reg_sub(h_processor, h_processor->reg[C_REG], NULL, h_processor->reg[C_REG]);
         break;
      case 0026: /* if b[f] = 0 */
         if (h_processor->flags[TRACE]) fprintf(stdout, "if b[%s] = 0", s_field);
         v_reg_test_eq(h_processor, h_processor->reg[B_REG], NULL);
         v_op_inc_pc(h_processor); /* Increment program counter */
         v_op_goto(h_processor);
         break;
      case 0027: /* if c[f] = 0 */
         if (h_processor->flags[TRACE]) fprintf(stdout, "if c[%s] = 0", s_field);
         v_reg_test_eq(h_processor, h_processor->reg[C_REG], NULL);
         v_op_inc_pc(h_processor); /* Increment program counter */
         v_op_goto(h_processor);
         break;
      case 0030: /* if a >= c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "if a >= c[%s]", s_field);
         v_reg_sub(h_processor, NULL, h_processor->reg[A_REG], h_processor->reg[C_REG]);
         v_op_inc_pc(h_processor); /* Increment program counter */
         v_op_goto(h_processor);
         break;
      case 0031: /* if a >= b[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "if a >= b[%s]", s_field);
         v_reg_sub(h_processor, NULL, h_processor->reg[A_REG], h_processor->reg[B_REG]);
         v_op_inc_pc(h_processor); /* Increment program counter */
         v_op_goto(h_processor);
         break;
      case 0032: /* if a[f] <> 0 */
         if (h_processor->flags[TRACE]) fprintf(stdout, "if a[%s] <> 0", s_field);
         v_reg_test_ne(h_processor, h_processor->reg[A_REG], NULL);
         v_op_inc_pc(h_processor); /* Increment program counter */
         v_op_goto(h_processor);
         break;
      case 0033: /* if c[f] <> 0 */
         if (h_processor->flags[TRACE]) fprintf(stdout, "if c[%s] <> 0", s_field);
         v_reg_test_ne(h_processor, h_processor->reg[C_REG], NULL);
         v_op_inc_pc(h_processor); /* Increment program counter */
         v_op_goto(h_processor);
         break;
      case 0034: /* a - c -> a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "a - c -> a[%s]", s_field);
         v_reg_sub(h_processor, h_processor->reg[A_REG], h_processor->reg[A_REG], h_processor->reg[C_REG]);
         break;
      case 0035: /* shift right a[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "shift right a[%s]", s_field);
         v_reg_shr(h_processor, h_processor->reg[A_REG]);
         break;
      case 0036: /* shift right b[f] */
         v_reg_shr(h_processor, h_processor->reg[B_REG]);
         if (h_processor->flags[TRACE]) fprintf(stdout, "shift right b[%s]", s_field);
         break;
      case 0037: /* shift right c[f] */
         if (h_processor->flags[TRACE]) fprintf(stdout, "shift right c[%s]", s_field);
         v_reg_shr(h_processor, h_processor->reg[C_REG]);
         break;
      default:
         v_error("Unexpected error in  %s line : %d\n", __FILE__, __LINE__);
      }
      break;

   case 03:/* Subroutine calls and long conditional jumps */
      switch (i_opcode & 03) {
      case 00:
         if (h_processor->flags[TRACE]) fprintf(stdout, "call\t%01o-%04o", h_processor->rom_number, i_opcode >> 2);
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
         break;
      case 01:
         if (h_processor->flags[TRACE]) fprintf(stdout, "call\t%01o-%04o", h_processor->rom_number, i_opcode >> 2);
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
         break;
      case 02:
         if (h_processor->flags[TRACE]) fprintf(stdout, "jump\t%01o-%04o", h_processor->rom_number, i_opcode >> 2);
               v_error("Unexpected opcode %04o at %1o-%04o in  %s line : %d\n", i_opcode, h_processor->rom_number, h_processor->pc, __FILE__, __LINE__);
         break;
      case 03: /* if nc goto */
         if (h_processor->flags[TRACE]) fprintf(stdout, "if nc goto %01o-%04o",
            h_processor->rom_number, (h_processor->pc & 0xff00) | i_opcode >> 2);
         if (h_processor->flags[PREV_CARRY] == 0)
             h_processor->pc = ((h_processor->pc & 0xff00) | i_opcode >> 2) - 1;
         delayed_rom_switch(h_processor);
         break;
      default:
         v_error("Unexpected error in  %s line : %d\n", __FILE__, __LINE__);
      }
      break;
   default:
      v_error("Unexpected error in  %s line : %d\n", __FILE__, __LINE__);
   }
   if (h_processor->flags[TRACE]) {
      fprintf(stdout, "\n");
      debug(v_state_fprint(stderr, h_processor));
   }

   v_op_inc_pc(h_processor); /* Increment program counter */
}
