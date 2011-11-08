/* This file is part of Furlow VM.
 *
 * FACT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FACT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FACT. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FACT_VM_H_
#define FACT_VM_H_

/* Register specifications:                                   */
#define T_REGISTERS 256 /* Total number of registers (G + S). */
#define G_REGISTERS 253 /* General registers.                 */
#define S_REGISTERS 3   /* Special registers.                 */

/* Definitions of special register values. The index registers (R_I, R_J,
 * etc) are not technically special, but are defined here.
 */
enum
  {
    R_POP = 0, /* Pop register.          */
    R_TOP = 1, /* Top of stack register. */
    R_TID = 2, /* Thread ID register.    */
    R_I,       /* "i" register.          */
    R_J,       /* "j" register.          */
    R_K,       /* "k" register.          */
    R_A,       /* "A" register.          */
    R_X,       /* "X" register.          */
  };

struct cstack_t 
{
  size_t ip;         /* Instruction pointer being used. */
  FACT_scope_t this; /* 'this' scope being used.        */
};

#define CYCLES_ON_COLLECT 500 /* Garbage collect every n number of cycles. */ 

/* Threading is handled on the program level in the Furlow VM, for the most
 * part. Each thread has its own stacks and instruction pointer. After an
 * instruction is evaluated, the next thread's data is used.
 */
typedef struct FACT_thread
{
  /* Virtual machine stacks:                                       */
  FACT_t *vstack;           /* Variable stack.                     */
  FACT_t *vstackp;          /* Points to the top of the var stack. */
  size_t vstack_size;       /* Memory allocated to the var stack.  */
  struct cstack_t *cstack;  /* Call stack.                         */
  struct cstack_t *cstackp; /* Top of the call stack.              */
  size_t cstack_size;       /* Memory allocated to call stack.     */

  /* User traps:                                                       */
  size_t (*traps)[2];    /* Trap stack. Delegates user error handling. */
  size_t num_traps;      /* Number of traps set.                       */ 
  FACT_error_t curr_err; /* The last error thrown.                     */

  /* Virtual machine registers:                                 */
  FACT_t registers[T_REGISTERS]; /* NOT to be handled directly. */

  /* Threading data: */
  enum T_FLAG
    {
      T_LIVE = 0, /* Thread is running. */
      T_DEAD,     /* Thread has halted. */
    } run_flag;

  pthread_t thread_id; 
  struct FACT_thread *next; /* Next thread in the list. */
  /* TODO: Add a message queue for ITC. */
} *FACT_thread_t;

/* Threading and stacks:                                                 */
extern size_t num_threads;                 /* Number of threads.         */
extern FACT_thread_t threads;              /* Thread data.               */
extern __thread FACT_thread_t curr_thread; /* Data specific to a thread. */

/* Error recovery:                                   */
extern jmp_buf handle_err; /* Handles thrown errors. */ 
extern jmp_buf recover;    /* When no traps are set. */

#define THIS_OF(t) (t)->cstackp->this
#define IP_OF(t)   (t)->cstackp->ip
#define CURR_THIS  curr_thread->cstackp->this
#define CURR_IP    curr_thread->cstackp->ip

/* Stack functions:                                                            */
FACT_t pop_v (void);                /* Pop the var stack.                      */
struct cstack_t pop_c (void);       /* Pop the call stack.                     */
void push_v (FACT_t);               /* Push to the var stack.                  */
void push_c (size_t, FACT_scope_t); /* Push to the call stack.                 */
/* Push a constant value to the var stack: */
void push_constant_str (char *);
void push_constant_ui  (unsigned long int);
void push_constant_si  (  signed long int);

/* Register functions:                                              */
FACT_t *Furlow_register (int);         /* Access a register.        */
void *Furlow_reg_val (int, FACT_type); /* Safely access a register. */

/* Execution functions:                                      */
void Furlow_run (); /* Run one cycle of the current program. */ 

/* Init functions:                                         */
void Furlow_init_vm (); /* Initialize the virtual machine. */

/* Code handling functions:                                                 */
void Furlow_add_instruction (char *); /* Add an instruction to the program. */
inline void Furlow_lock_program ();   /* Wait for a chance and lock.        */
inline void Furlow_unlock_program (); /* Unlock the program.                */
inline size_t Furlow_offset ();       /* Get the instruction offset.        */

/* Scope handling:                                                      */
void FACT_get_either (char *); /* Search for a variable of either type. */

#endif /* FACT_VM_H_ */
