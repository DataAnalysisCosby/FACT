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

#include <FACT.h>

/* Intermediete format created by the compiler. Essentially a tree. */
struct inter_node
{
  enum
    {
      INSTRUCTION,
      GROUPING
    } node_type;
  struct inter_node *next;
  union
  {
    struct /* Single instruction. */
    {
      enum Furlow_opcode inst_val;
      struct /* Arguments. */
      {
	enum
	  {
	    NO_ARG = 0,
	    REG_VAL,
	    ADDR_VAL,
	    LABEL,
	  } arg_type;
	union
	{
	  size_t addr;
	  unsigned char reg;
	  char *label;
	} arg_val;
      } args[4]; /* Max of four arguments. */
    } inst;
    struct /* Grouping. */
    {
      struct inter_node **children;
      size_t num_children;
    } grouping;
  } node_val;
};

static bool is_num (char *);
static struct inter_node *create_node ();
static struct inter_node *compile_tree (FACT_tree_t);
static struct inter_node *compile_args (FACT_tree_t);

static struct inter_node *begin_temp_scope ();
static struct inter_node *end_temp_scope ();

static struct inter_node *set_return_val ();

static void load (struct inter_node *, struct inter_node *);
static size_t weight (struct inter_node *);
static inline void spread (char *, size_t);

void
FACT_compile (FACT_tree_t tree)
{
#ifdef ADVANCED_THREADING
  /* Lock the program for offset consistency. */
  Furlow_lock_program ();
#endif /* ADVANCED_THREADING */

  /* Compile and load. */
  load (compile_tree (tree), NULL);

#ifdef ADVANCED_THREADING
  /* Unlock the program. */
  Furlow_unlock_program ();
#endif /* ADVANCED_THREADING */
}

/* Since I do not know of any compilation techniques, so it's sort of just ad-hoc.
 * Things to do:
 *  - Drop (or pop and set to some register) the top of the stack upon end of every
 *    statement, in order to prevent overflow. This may require some simple changes
 *    to the parser.
 *  - Argument checking. This is important.
 *  - ({ block }) doesn't compile correctly.
 */

static struct inter_node *
compile_tree (FACT_tree_t curr) /* Compile a tree, recursively. */
{
  struct inter_node *res;
  char *dims_str;
  size_t i, j;
  size_t dims;
  FACT_tree_t n;

  static Furlow_opc_t lookup_table [] =
    {
      [E_ADD] = ADD,
      [E_SUB] = SUB,
      [E_MUL] = MUL,
      [E_DIV] = DIV,
      [E_MOD] = MOD,
      [E_ADD_AS] = ADD,
      [E_SUB_AS] = SUB,
      [E_MUL_AS] = MUL,
      [E_DIV_AS] = DIV,
      [E_MOD_AS] = MOD,
      [E_NE] = CNE,
      [E_EQ] = CEQ,
      [E_MT] = CMT,
      [E_ME] = CME,
      [E_LT] = CLT,
      [E_LE] = CLE,
    };

  if (curr == NULL)
    return NULL;

  res = create_node ();
  switch (curr->id.id)
    {
    case E_VAR:
      res->node_type = INSTRUCTION;
      if (!strcmp (curr->id.lexem, "this"))
	res->node_val.inst.inst_val = THIS;
      else
	{
	  res->node_val.inst.inst_val = ((is_num (curr->id.lexem))
					 ? CONST
					 : VAR);
	  res->node_val.inst.args[0].arg_type = LABEL;
	  res->node_val.inst.args[0].arg_val.label = curr->id.lexem;
	}
      break;

    case E_NEG:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 2);
      res->node_val.grouping.num_children = 2;
      res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[1] = create_node ();
      res->node_val.grouping.children[1]->node_type = INSTRUCTION;
      res->node_val.grouping.children[1]->node_val.inst.inst_val = NEG;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_TOP;
      break;
      
    case E_ADD:
    case E_SUB:
    case E_MUL:
    case E_DIV:
    case E_MOD:
    case E_NE:
    case E_EQ:
    case E_MT:
    case E_ME:
    case E_LT:
    case E_LE:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 4);
      res->node_val.grouping.num_children = 4;
      /* Create a temporary variable. */
      res->node_val.grouping.children[0] = create_node ();
      res->node_val.grouping.children[0]->node_type = INSTRUCTION;
      res->node_val.grouping.children[0]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_val.label = "0";
      /* Compile the arguments. */
      res->node_val.grouping.children[1] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[2] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[3] = create_node ();
      res->node_val.grouping.children[3]->node_type = INSTRUCTION;
      res->node_val.grouping.children[3]->node_val.inst.inst_val = lookup_table[curr->id.id];
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[3]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[3]->node_val.inst.args[1].arg_val.reg = R_POP;
      res->node_val.grouping.children[3]->node_val.inst.args[2].arg_type = REG_VAL;
      res->node_val.grouping.children[3]->node_val.inst.args[2].arg_val.reg = R_TOP;
      break;

    case E_ADD_AS:
    case E_SUB_AS:
    case E_MUL_AS:
    case E_DIV_AS:
    case E_MOD_AS:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 5);
      res->node_val.grouping.num_children = 5;
      res->node_val.grouping.children[0] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[1] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[2] = create_node ();
      res->node_val.grouping.children[2]->node_type = INSTRUCTION;
      res->node_val.grouping.children[2]->node_val.inst.inst_val = REF;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_val.reg = R_TOP;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_val.reg = R_A;
      res->node_val.grouping.children[3] = create_node ();
      res->node_val.grouping.children[3]->node_type = INSTRUCTION;
      res->node_val.grouping.children[3]->node_val.inst.inst_val = SWAP;
      res->node_val.grouping.children[4] = create_node ();
      res->node_val.grouping.children[4]->node_type = INSTRUCTION;
      res->node_val.grouping.children[4]->node_val.inst.inst_val = lookup_table[curr->id.id];
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_val.reg = R_A;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_val.reg = R_POP;
      res->node_val.grouping.children[4]->node_val.inst.args[2].arg_type = REG_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[2].arg_val.reg = R_TOP;
      break;

    case E_AND:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);
      res->node_val.grouping.num_children = 7;
      res->node_val.grouping.children[0] = create_node ();
      res->node_val.grouping.children[0]->node_type = INSTRUCTION;
      res->node_val.grouping.children[0]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_val.label = "0";
      res->node_val.grouping.children[1] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[2] = create_node ();
      res->node_val.grouping.children[2]->node_type = INSTRUCTION;
      res->node_val.grouping.children[2]->node_val.inst.inst_val = JIF;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_type = ADDR_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_val.addr = 6;
      res->node_val.grouping.children[3] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[4] = create_node ();
      res->node_val.grouping.children[4]->node_type = INSTRUCTION;
      res->node_val.grouping.children[4]->node_val.inst.inst_val = JIF;
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_type = ADDR_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_val.addr = 6;
      res->node_val.grouping.children[5] = create_node ();
      res->node_val.grouping.children[5]->node_type = INSTRUCTION;
      res->node_val.grouping.children[5]->node_val.inst.inst_val = DROP;
      res->node_val.grouping.children[6] = create_node ();
      res->node_val.grouping.children[6]->node_type = INSTRUCTION;
      res->node_val.grouping.children[6]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_val.label = "1";
      break;

    case E_OR:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);
      res->node_val.grouping.num_children = 7;
      res->node_val.grouping.children[0] = create_node ();
      res->node_val.grouping.children[0]->node_type = INSTRUCTION;
      res->node_val.grouping.children[0]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_val.label = "1";
      res->node_val.grouping.children[1] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[2] = create_node ();
      res->node_val.grouping.children[2]->node_type = INSTRUCTION;
      res->node_val.grouping.children[2]->node_val.inst.inst_val = JIT;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_type = ADDR_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_val.addr = 6;
      res->node_val.grouping.children[3] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[4] = create_node ();
      res->node_val.grouping.children[4]->node_type = INSTRUCTION;
      res->node_val.grouping.children[4]->node_val.inst.inst_val = JIT;
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_type = ADDR_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_val.addr = 6;
      res->node_val.grouping.children[5] = create_node ();
      res->node_val.grouping.children[5]->node_type = INSTRUCTION;
      res->node_val.grouping.children[5]->node_val.inst.inst_val = DROP;
      res->node_val.grouping.children[6] = create_node ();
      res->node_val.grouping.children[6]->node_type = INSTRUCTION;
      res->node_val.grouping.children[6]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_val.label = "0";
      break;
      
    case E_ARRAY_ELEM:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 4);
      res->node_val.grouping.num_children = 4;
      res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[1] = create_node ();
      res->node_val.grouping.children[1]->node_type = INSTRUCTION;
      res->node_val.grouping.children[1]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.label = "1";
      res->node_val.grouping.children[2] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[3] = create_node ();
      res->node_val.grouping.children[3]->node_type = INSTRUCTION;
      res->node_val.grouping.children[3]->node_val.inst.inst_val = ELEM;
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[3]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[3]->node_val.inst.args[1].arg_val.reg = R_POP;
      break;

    case E_IN:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 5);
      res->node_val.grouping.num_children = 5;
      res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[1] = create_node ();
      res->node_val.grouping.children[1]->node_type = INSTRUCTION;
      res->node_val.grouping.children[1]->node_val.inst.inst_val = USE;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[2] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[3] = create_node ();
      res->node_val.grouping.children[3]->node_type = INSTRUCTION;
      res->node_val.grouping.children[3]->node_val.inst.inst_val = EXIT;
      res->node_val.grouping.children[4] = create_node ();
      res->node_val.grouping.children[4]->node_type = INSTRUCTION;
      res->node_val.grouping.children[4]->node_val.inst.inst_val = DROP;
      break;

    case E_FUNC_CALL:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 12);
      res->node_val.grouping.num_children = 12;
      /* Compile the arguments being passed. */
      res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
      /* Create a lambda scope. */
      res->node_val.grouping.children[1] = create_node ();
      res->node_val.grouping.children[1]->node_type = INSTRUCTION;
      res->node_val.grouping.children[1]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.label = "0";
      res->node_val.grouping.children[2] = create_node ();
      res->node_val.grouping.children[2]->node_type = INSTRUCTION;
      res->node_val.grouping.children[2]->node_val.inst.inst_val = NEW_S;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_val.reg = R_POP;
      /* Compile the function being called. */
      res->node_val.grouping.children[3] = compile_tree (curr->children[1]);
      /* Set the up variable of the lambda scope to it. */
      res->node_val.grouping.children[4] = create_node ();
      res->node_val.grouping.children[4]->node_type = INSTRUCTION;
      res->node_val.grouping.children[4]->node_val.inst.inst_val = REF;
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[4]->node_val.inst.args[1].arg_val.reg = R_A;
      /* Briefly enter the scope to do so.*/
      res->node_val.grouping.children[5] = create_node ();
      res->node_val.grouping.children[5]->node_type = INSTRUCTION;
      res->node_val.grouping.children[5]->node_val.inst.inst_val = USE;
      res->node_val.grouping.children[5]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[5]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[6] = create_node ();
      res->node_val.grouping.children[6]->node_type = INSTRUCTION;
      res->node_val.grouping.children[6]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_val.label = "0";
      res->node_val.grouping.children[7] = create_node ();
      res->node_val.grouping.children[7]->node_type = INSTRUCTION;
      res->node_val.grouping.children[7]->node_val.inst.inst_val = DEF_S;
      res->node_val.grouping.children[7]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[7]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[7]->node_val.inst.args[1].arg_type = LABEL;
      res->node_val.grouping.children[7]->node_val.inst.args[1].arg_val.label = "up";
      res->node_val.grouping.children[8] = create_node ();
      res->node_val.grouping.children[8]->node_type = INSTRUCTION;
      res->node_val.grouping.children[8]->node_val.inst.inst_val = STO;
      res->node_val.grouping.children[8]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[8]->node_val.inst.args[0].arg_val.reg = R_A;
      res->node_val.grouping.children[8]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[8]->node_val.inst.args[1].arg_val.reg = R_POP;
      res->node_val.grouping.children[9] = create_node ();
      res->node_val.grouping.children[9]->node_type = INSTRUCTION;
      res->node_val.grouping.children[9]->node_val.inst.inst_val = EXIT;
      /* Set the lambda scope's code address and call it. */
      res->node_val.grouping.children[10] = create_node ();
      res->node_val.grouping.children[10]->node_val.inst.inst_val = SET_F;
      res->node_val.grouping.children[10]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[10]->node_val.inst.args[0].arg_val.reg = R_A;
      res->node_val.grouping.children[10]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[10]->node_val.inst.args[1].arg_val.reg = R_TOP;
      res->node_val.grouping.children[11] = create_node ();
      res->node_val.grouping.children[11]->node_type = INSTRUCTION;
      res->node_val.grouping.children[11]->node_val.inst.inst_val = CALL;
      res->node_val.grouping.children[11]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[11]->node_val.inst.args[0].arg_val.reg = R_POP;
      break;

    case E_FUNC_DEF:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);
      res->node_val.grouping.num_children = 7;
      res->node_val.grouping.children[0] = create_node ();
      res->node_val.grouping.children[0]->node_type = INSTRUCTION;
      res->node_val.grouping.children[0]->node_val.inst.inst_val = JMP;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_type = ADDR_VAL;
      res->node_val.grouping.children[0]->node_val.inst.args[0].arg_val.addr = 4;
      res->node_val.grouping.children[1] = compile_args (curr->children[1]);
      res->node_val.grouping.children[2] = compile_tree (curr->children[2]);
      res->node_val.grouping.children[3] = create_node ();
      res->node_val.grouping.children[3]->node_type = INSTRUCTION;
      res->node_val.grouping.children[3]->node_val.inst.inst_val = CONST;
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = LABEL;
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.label = "0";
      res->node_val.grouping.children[4] = create_node ();
      res->node_val.grouping.children[4]->node_type = INSTRUCTION;
      res->node_val.grouping.children[4]->node_val.inst.inst_val = RET;
      res->node_val.grouping.children[5] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[6] = create_node ();
      res->node_val.grouping.children[6]->node_type = INSTRUCTION;
      res->node_val.grouping.children[6]->node_val.inst.inst_val = SET_C;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[6]->node_val.inst.args[0].arg_val.reg = R_TOP;
      res->node_val.grouping.children[6]->node_val.inst.args[1].arg_type = ADDR_VAL;
      res->node_val.grouping.children[6]->node_val.inst.args[1].arg_val.addr = 1;
      break;

    case E_RETURN:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 2);
      res->node_val.grouping.num_children = 2;
      res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[1] = create_node ();
      res->node_val.grouping.children[1]->node_type = INSTRUCTION;
      res->node_val.grouping.children[1]->node_val.inst.inst_val = RET;
      break;
      
    case E_SET:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
      res->node_val.grouping.num_children = 3;
      res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[1] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[2] = create_node ();
      res->node_val.grouping.children[2]->node_type = INSTRUCTION;
      res->node_val.grouping.children[2]->node_val.inst.inst_val = STO;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.grouping.children[2]->node_val.inst.args[1].arg_val.reg = R_TOP;
      break;

      
    case E_NUM_DEF:
    case E_SCOPE_DEF:
      /* Count the number of dimensions the variable has. */
      dims = 0;
      for (n = curr->children[0]; n != NULL; n = n->next)
	dims++;

      res->node_type = GROUPING;
      if (dims == 0) /* If there are no dimensions. */
	{
	  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 2);
	  res->node_val.grouping.num_children = 2;
	  res->node_val.grouping.children[0] = create_node ();
	  res->node_val.grouping.children[0]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[0]->node_val.inst.inst_val = CONST;
	  res->node_val.grouping.children[0]->node_val.inst.args[0].arg_type = LABEL;
	  res->node_val.grouping.children[0]->node_val.inst.args[0].arg_val.label = "0";
	  res->node_val.grouping.children[1] = create_node ();
	  res->node_val.grouping.children[1]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[1]->node_val.inst.inst_val = ((curr->id.id == E_NUM_DEF)
									? DEF_N
									: DEF_S);
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_POP;
	  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_type = LABEL;
	  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_val.label
	    = curr->children[1]->id.lexem;
	}
      else
	{
	  /* Convert dims to a string. */
	  i = 0;
	  dims_str = NULL;
	  do
	    {
	      dims_str = FACT_realloc (dims_str, i + 2);
	      dims_str[i++] = (dims % 10) + '0';
	    }
	  while ((dims /= 10) > 0);

	  dims_str[i--] = '\0';

	  /* Reverse the string. Use XOR swap because I'd don't want to declare
	   * another variable in the case block.
	   */
	  for (j = 0; j < i; j++, i--)
	    {
	      dims_str[j] ^= dims_str[i];
	      dims_str[i] ^= dims_str[j];
	      dims_str[j] ^= dims_str[i];
	    }

	  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
	  res->node_val.grouping.num_children = 3;
	  res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
	  res->node_val.grouping.children[1] = create_node ();
	  res->node_val.grouping.children[1]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[1]->node_val.inst.inst_val = CONST;
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = LABEL;
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.label = dims_str;
	  res->node_val.grouping.children[2] = create_node ();
	  res->node_val.grouping.children[2]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[2]->node_val.inst.inst_val = ((curr->id.id == E_NUM_DEF)
									? DEF_N
									: DEF_S);
	  res->node_val.grouping.children[2]->node_val.inst.args[0].arg_type = REG_VAL;
	  res->node_val.grouping.children[2]->node_val.inst.args[0].arg_val.reg = R_POP;
	  res->node_val.grouping.children[2]->node_val.inst.args[1].arg_type = LABEL;
	  res->node_val.grouping.children[2]->node_val.inst.args[1].arg_val.label
	    = curr->children[1]->id.lexem;
	}
      break;  

    case E_IF:
      res->node_type = GROUPING;
      if (curr->children[2] != NULL)
	{
	  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 5);
	  res->node_val.grouping.num_children = 5;
	  res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
	  res->node_val.grouping.children[1] = create_node ();
	  res->node_val.grouping.children[1]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[1]->node_val.inst.inst_val = JIF;
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_POP;
	  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_type = ADDR_VAL;
	  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_val.addr = 3;
	  res->node_val.grouping.children[2] = compile_tree (curr->children[1]);
	  res->node_val.grouping.children[3] = create_node ();
	  res->node_val.grouping.children[3]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[3]->node_val.inst.inst_val = JMP;
	  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = ADDR_VAL;
	  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.addr = 4;
	  res->node_val.grouping.children[4] = compile_tree (curr->children[2]);
	}
      else
	{
	  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
	  res->node_val.grouping.num_children = 3;
	  res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
	  res->node_val.grouping.children[1] = create_node ();
	  res->node_val.grouping.children[1]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[1]->node_val.inst.inst_val = JIF;
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
	  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_POP;
	  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_type = ADDR_VAL;
	  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_val.addr = 2;
	  res->node_val.grouping.children[2] = compile_tree (curr->children[1]);
	}
      break;

    case E_WHILE:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 4);
      res->node_val.grouping.num_children = 4;
      res->node_val.grouping.children[0] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[1] = create_node ();
      res->node_val.grouping.children[1]->node_type = INSTRUCTION;
      res->node_val.grouping.children[1]->node_val.inst.inst_val = JIF;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.grouping.children[1]->node_val.inst.args[1].arg_type = ADDR_VAL;
      res->node_val.grouping.children[1]->node_val.inst.args[1].arg_val.addr = 3;
      res->node_val.grouping.children[2] = compile_tree (curr->children[1]);
      res->node_val.grouping.children[3] = create_node ();
      res->node_val.grouping.children[3]->node_type = INSTRUCTION;
      res->node_val.grouping.children[3]->node_val.inst.inst_val = JMP;
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = ADDR_VAL;
      res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.addr = 0;
      break;

    case E_FOR:
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 10);
      res->node_val.grouping.num_children = 10;

      res->node_val.grouping.children[0] = begin_temp_scope ();
      res->node_val.grouping.children[1] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[2] = compile_tree (curr->children[1]);

      if (curr->children[1] == NULL)
	res->node_val.grouping.children[3] = NULL;
      else
	{
	  res->node_val.grouping.children[3] = create_node ();
	  res->node_val.grouping.children[3]->node_type = INSTRUCTION;
	  res->node_val.grouping.children[3]->node_val.inst.inst_val = JIF;
	  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = REG_VAL;
	  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.reg = R_POP;
	  res->node_val.grouping.children[3]->node_val.inst.args[1].arg_type = ADDR_VAL;
	  res->node_val.grouping.children[3]->node_val.inst.args[1].arg_val.addr = 7;
	}

      /* Do not create a new scope for brackets. */
      if (curr->children[3]->id.id == E_OP_CURL)
	res->node_val.grouping.children[4] = compile_tree (curr->children[3]->children[0]);
      else
	res->node_val.grouping.children[4] = compile_tree (curr->children[3]);
      
      res->node_val.grouping.children[5] = compile_tree (curr->children[2]);

      /* Drop the return value of every statement. */
      res->node_val.grouping.children[6] = create_node ();
      res->node_val.grouping.children[6]->node_type = INSTRUCTION;
      res->node_val.grouping.children[6]->node_val.inst.inst_val = DROP;
      
      res->node_val.grouping.children[7] = create_node ();
      res->node_val.grouping.children[7]->node_type = INSTRUCTION;
      res->node_val.grouping.children[7]->node_val.inst.inst_val = JMP;
      res->node_val.grouping.children[7]->node_val.inst.args[0].arg_type = ADDR_VAL;
      res->node_val.grouping.children[7]->node_val.inst.args[0].arg_val.addr = 2;

      res->node_val.grouping.children[8] = end_temp_scope ();
      res->node_val.grouping.children[9] = set_return_val ();
      break;

    case E_OP_CURL: /* This REALLY needs to be optimized. */
      res->node_type = GROUPING;
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
      res->node_val.grouping.num_children = 3;
      res->node_val.grouping.children[0] = begin_temp_scope ();
      res->node_val.grouping.children[1] = compile_tree (curr->children[0]);
      res->node_val.grouping.children[2] = end_temp_scope ();
      break;

    case E_END:
    case E_SEMI:
    case E_CL_CURL:
      /* Per every expression, pop the top element off the stack and set the
       * X register to it. That way the stack will never overflow and the
       * return value can still be checked.
       */
      res->node_type = INSTRUCTION;
      res->node_val.inst.inst_val = REF;
      res->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.inst.args[1].arg_val.reg = R_X;
      break;

    default:
      abort ();
    }

  /* Compile the next statement. */
  res->next = compile_tree (curr->next);
  return res;
}

static struct inter_node *
begin_temp_scope (void) /* Create a temporary scope with the up variable set. */
{
  struct inter_node *res;

  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);
  res->node_val.grouping.num_children = 7;

  /* Set the A register to the current scope for later use.
   *  this
   *  ref,$pop,$a
   */
  res->node_val.grouping.children[0] = create_node ();
  res->node_val.grouping.children[0]->node_type = INSTRUCTION;
  res->node_val.grouping.children[0]->node_val.inst.inst_val = THIS;
  res->node_val.grouping.children[1] = create_node ();
  res->node_val.grouping.children[1]->node_type = INSTRUCTION;
  res->node_val.grouping.children[1]->node_val.inst.inst_val = REF;
  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_POP;
  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_type = REG_VAL;
  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_val.reg = R_A;
  /* Create an anonymous scope: 
   *  const,%0
   *  new_s,$top
   */
  res->node_val.grouping.children[2] = create_node ();
  res->node_val.grouping.children[2]->node_type = INSTRUCTION;
  res->node_val.grouping.children[2]->node_val.inst.inst_val = CONST;
  res->node_val.grouping.children[2]->node_val.inst.args[0].arg_type = LABEL;
  res->node_val.grouping.children[2]->node_val.inst.args[0].arg_val.label = "0";
  res->node_val.grouping.children[3] = create_node ();
  res->node_val.grouping.children[3]->node_type = INSTRUCTION;
  res->node_val.grouping.children[3]->node_val.inst.inst_val = NEW_S;
  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.reg = R_TOP;
  /* Push the scope to the call stack:
   *  use,$pop
   */
  res->node_val.grouping.children[4] = create_node ();
  res->node_val.grouping.children[4]->node_type = INSTRUCTION;
  res->node_val.grouping.children[4]->node_val.inst.inst_val = USE;
  res->node_val.grouping.children[4]->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.grouping.children[4]->node_val.inst.args[0].arg_val.reg = R_POP;
  /* Create an "up" variable for the scope:
   *  def_s,$pop,%up
   */
  res->node_val.grouping.children[5] = create_node ();
  res->node_val.grouping.children[5]->node_type = INSTRUCTION;
  res->node_val.grouping.children[5]->node_val.inst.inst_val = DEF_S;
  res->node_val.grouping.children[5]->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.grouping.children[5]->node_val.inst.args[0].arg_val.reg = R_POP;
  res->node_val.grouping.children[5]->node_val.inst.args[1].arg_type = LABEL;
  res->node_val.grouping.children[5]->node_val.inst.args[1].arg_val.label = "up";
  /* Set the "up" variable to the A register:
   *  sto,$A,$pop
   */
  res->node_val.grouping.children[6] = create_node ();
  res->node_val.grouping.children[6]->node_type = INSTRUCTION;
  res->node_val.grouping.children[6]->node_val.inst.inst_val = STO;
  res->node_val.grouping.children[6]->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.grouping.children[6]->node_val.inst.args[0].arg_val.reg = R_A;
  res->node_val.grouping.children[6]->node_val.inst.args[1].arg_type = REG_VAL;
  res->node_val.grouping.children[6]->node_val.inst.args[1].arg_val.reg = R_POP;

  return res;
}

static struct inter_node *
end_temp_scope (void)
{
  struct inter_node *res;
  
  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *));
  res->node_val.grouping.num_children = 1;

  /* Exit the scope. */
  res->node_val.grouping.children[0] = create_node ();
  res->node_val.grouping.children[0]->node_type = INSTRUCTION;
  res->node_val.grouping.children[0]->node_val.inst.inst_val = EXIT;

  return res;
}

static struct inter_node *
set_return_val ()
{
  struct inter_node *res;

  res = create_node ();

  res->node_type = INSTRUCTION;
  res->node_val.inst.inst_val = REF;
  res->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.inst.args[0].arg_val.reg = R_POP;
  res->node_val.inst.args[1].arg_type = REG_VAL;
  res->node_val.inst.args[1].arg_val.reg = R_X;

  return res;
}

static struct inter_node *
compile_args (FACT_tree_t curr)
{
  struct inter_node *res;

  if (curr == NULL)
    return NULL;

  res = create_node ();
  res->node_type = GROUPING;;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 4);
  res->node_val.grouping.num_children = 4;
  res->node_val.grouping.children[0] = create_node ();
  res->node_val.grouping.children[0]->node_type = INSTRUCTION;
  res->node_val.grouping.children[0]->node_val.inst.inst_val = CONST;
  res->node_val.grouping.children[0]->node_val.inst.args[0].arg_type = LABEL;
  res->node_val.grouping.children[0]->node_val.inst.args[0].arg_val.label = "0";
  res->node_val.grouping.children[1] = create_node ();
  res->node_val.grouping.children[1]->node_type = INSTRUCTION;
  res->node_val.grouping.children[1]->node_val.inst.inst_val = ((curr->id.id == E_NUM_DEF)
								? DEF_N
								: DEF_S);
  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.grouping.children[1]->node_val.inst.args[0].arg_val.reg = R_POP;
  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_type = LABEL;
  res->node_val.grouping.children[1]->node_val.inst.args[1].arg_val.label = curr->children[0]->id.lexem;
  res->node_val.grouping.children[2] = create_node ();
  res->node_val.grouping.children[2]->node_type = INSTRUCTION;
  res->node_val.grouping.children[2]->node_val.inst.inst_val = SWAP;
  res->node_val.grouping.children[3] = create_node ();
  res->node_val.grouping.children[3]->node_type = INSTRUCTION;
  res->node_val.grouping.children[3]->node_val.inst.inst_val = STO;
  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.grouping.children[3]->node_val.inst.args[0].arg_val.reg = R_POP;
  res->node_val.grouping.children[3]->node_val.inst.args[1].arg_type = REG_VAL;
  res->node_val.grouping.children[3]->node_val.inst.args[1].arg_val.reg = R_POP;

  res->next = compile_args (curr->children[1]);

  return res;
}

static void
load (struct inter_node *curr, struct inter_node *up)
{
  char *fmt;
  size_t i, j, k;
  size_t len, addr;

  if (curr == NULL)
    return;

  fmt = NULL;
  if (curr->node_type == INSTRUCTION)
    {
      len = 1;
      fmt = FACT_malloc (1);
      fmt[0] = curr->node_val.inst.inst_val;
      for (i = 0; i < 4; i++)
	{
	  switch (curr->node_val.inst.args[i].arg_type)
	    {
	    case REG_VAL:
	      fmt = FACT_realloc (fmt, ++len);
	      fmt[len - 1] = curr->node_val.inst.args[i].arg_val.reg;
	      break;

	    case ADDR_VAL:
	      if (up != NULL)
		{
		  for (j = 0; j < up->node_val.grouping.num_children; j++)
		    {
		      if (up->node_val.grouping.children[j] == curr)
			break;
		    }
		  assert (up->node_val.grouping.children[j] == curr
			  && curr->node_val.inst.args[i].arg_val.addr != j);
		  addr = Furlow_offset ();
		  if (j < curr->node_val.inst.args[i].arg_val.addr)
		    {
		      for (k = j; k <= curr->node_val.inst.args[i].arg_val.addr; k++)
			addr += weight (up->node_val.grouping.children[k]);
		    }
		  else
		    {
		      for (k = curr->node_val.inst.args[i].arg_val.addr; k < j; k++)
			addr -= weight (up->node_val.grouping.children[k]);
		    }
		}
	      else
		abort ();
	      fmt = FACT_realloc (fmt, len + 4);
	      spread (fmt + len, addr);
	      len += 4;
	      break;

	    case LABEL:
	      fmt = FACT_realloc (fmt, len + strlen (curr->node_val.inst.args[i].arg_val.label) + 1);
	      strcpy (fmt + len, curr->node_val.inst.args[i].arg_val.label);
	      /* Fall through. */
	      
	    default:
	      goto load_fmt;
	    }
	}

      /* Load the instruction. */
    load_fmt:
      Furlow_add_instruction (fmt);
    }
  else
    {
      for (i = 0; i < curr->node_val.grouping.num_children; i++)
	load (curr->node_val.grouping.children[i], curr);
    }
  
  load (curr->next, NULL);
}

static size_t
weight (struct inter_node *curr) /* Recursively calculate the weight of a node. */
{
  size_t i, res;

  if (curr == NULL)
    return 0;

  if (curr->node_type == INSTRUCTION)
    res = 1;
  else
    {
      res = 0;
      for (i = 0; i < curr->node_val.grouping.num_children; i++)
	res += weight (curr->node_val.grouping.children[i]);
    }

  return res + weight (curr->next);
}

static inline void
spread (char *rop, size_t op)
{
  rop[0] = (op >> 24) & 0xFF;
  rop[1] = (op >> 16) & 0xFF;
  rop[2] = (op >> 8) & 0xFF;
  rop[3] = op & 0xFF;
}

static bool
is_num (char *tok) /* Check if a token is a valid number. */
{
  bool hex, flp;
  size_t i;

  hex = ((tok[0] == '0' && tolower ((int) tok[1]) == 'x')
	 ? true
	 : false);

  for (i = hex ? 2 : 0, flp = false; tok[i] != '\0'; i++)
    {
      if (tok[i] == '.')
	{
	  if (flp || tok[i + 1] == '\0')
	    return false;
	  flp = true;
	}
      else if (!isdigit ((int) tok[i]))
        {
          if (!hex || tolower ((int) tok[i]) < 'a' || tolower ((int) tok[i]) > 'f')
            return false;
        }
    }
  return ((hex && tok[2] == '\0')
	  ? false
	  : true);
}

static struct inter_node *
create_node ()
{
  struct inter_node *res;

  res = FACT_malloc (sizeof (struct inter_node));
  memset (res, 0, sizeof (struct inter_node));

  return res;
}