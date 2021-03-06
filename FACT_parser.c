/* This file is part of FACT.
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

#include "FACT.h"
#include "FACT_lexer.h"
#include "FACT_parser.h"
#include "FACT_alloc.h"
#include "FACT_error.h"

#include <string.h>
#include <stdarg.h>

/* Most of these functions are static as they need not be used by
 * any other file.
 * See file FACT_grammar.txt for an explanation.
 */

static FACT_tree_t assignment (FACT_lexed_t *);
static FACT_tree_t stmt_list (FACT_lexed_t *);
static FACT_tree_t func_dec (FACT_lexed_t *, bool);

static inline bool check (FACT_lexed_t *set, FACT_nterm_t id) /* Check the current token. */
{
  return ((set->tokens[set->curr].id == id)
	  ? true
	  : false);
}

static inline bool check_forward(FACT_lexed_t *set, FACT_nterm_t id, size_t i)
{
  return ((set->tokens[set->curr + i].id == id)
	  ? true
	  : false);
}

static inline void error (FACT_lexed_t *set, char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  vsnprintf (set->err, MAX_ERR_LEN, fmt, args); /* Fix this. */
  va_end (args);

  longjmp (set->handle_err, 1); 
}
static bool is_ident_valid (char *);
static bool is_num_valid (char *);

static FACT_tree_t accept (FACT_lexed_t *set, FACT_nterm_t id) /* Accept a token. */
{
  FACT_tree_t res;
  
  if (set->tokens[set->curr].id == id
      || id == E_STR_CONST) { /* Token is of acceptable type, create a new node. */
    if (id != E_STR_CONST) {
      if (id == E_VAR
	  && !is_ident_valid (set->tokens[set->curr].lexem)) /* ID is invalid. */
	error (set, "invalid identifier `%s'", set->tokens[set->curr].lexem);
      else if (id == E_NUM
	       && !is_num_valid (set->tokens[set->curr].lexem)) /* Number is invalid. */
	error (set, "invalid numerical constant `%s'", set->tokens[set->curr].lexem);
    }
    res = FACT_malloc (sizeof (struct FACT_tree));
    memset (res, 0, sizeof (struct FACT_tree));
    res->id = set->tokens[set->curr];
    /* Move over one token, unless we're at E_END. */
    if (id != E_END) {
      set->line += set->tokens[set->curr].lines;
      res->line = set->line;
      set->curr++;
    }
    return res;
  }
  return NULL;
}

static FACT_tree_t expect (FACT_lexed_t *set, FACT_nterm_t id) /* Expect a token. */
{
  char *fmt;
  FACT_tree_t res;

  res = accept (set, id);

  /* Flip a bitch if res is NULL. */
  if (res == NULL) {
    set->line += set->tokens[set->curr].lines;
    /* Change this perhaps. */
    error (set, "expected %s before %s",
	   FACT_get_lexem (id),
	   FACT_get_lexem (set->tokens[set->curr].id));
  }
  
  return res;
}

static FACT_tree_t stmt (FACT_lexed_t *set)
{
  FACT_tree_t pn, en;

  /* Blocks, if statements, while & for loops, sprout statements, and functions
   * declaration productions are all built into this one, because it was easier
   * that way.
   */
  if ((pn = accept (set, E_OP_CURL)) != NULL) {
    /* Block. */
    pn->children[0] = stmt_list (set);
    pn->next = expect (set, E_CL_CURL);
  } else if ((pn = accept (set, E_IF)) != NULL) {
    /* If statement. */
    expect (set, E_OP_PAREN);
    pn->children[0] = assignment (set);
    expect (set, E_CL_PAREN);
    pn->children[1] = stmt (set);
    
    /* Else clause. */
    if (accept (set, E_ELSE) != NULL)
      pn->children[2] = stmt (set);
  } else if ((pn = accept (set, E_WHILE)) != NULL) {
    /* While loop. */
    expect (set, E_OP_PAREN);
    if (accept (set, E_CL_PAREN) != NULL)
      pn->children[0] = NULL;
    else {
      pn->children[0] = assignment (set);
      expect (set, E_CL_PAREN);
    }
    pn->children[1] = ((accept (set, E_SEMI) != NULL)
		       ? NULL
		       : stmt (set));
  } else if ((pn = accept (set, E_FOR)) != NULL) {
    /* For loop. */
    expect (set, E_OP_PAREN);
    
    /* First optional expression. */
    if (accept (set, E_SEMI) == NULL) {
      pn->children[0] = assignment (set);
      pn->children[0]->next = expect (set, E_SEMI);
    }

    /* Second optional expression. */
    if (accept (set, E_SEMI) == NULL) {
      pn->children[1] = assignment (set);
      expect (set, E_SEMI);
    }

    /* The third. Ends with a CL_PAREN */
    if (accept (set, E_CL_PAREN) == NULL) {
      pn->children[2] = assignment (set);
      expect (set, E_CL_PAREN);
    }

    pn->children[3] = ((accept (set, E_SEMI) != NULL)
		       ? NULL
		       : stmt (set));
  } else if ((pn = accept (set, E_CATCH)) != NULL) {
    pn->children[0] = stmt (set);
    expect (set, E_HANDLE);
    pn->children[1] = stmt (set);
  } else if ((pn = accept (set, E_RETURN)) != NULL) {
    pn->children[0] = assignment (set);
    expect (set, E_SEMI);
  } else if ((pn = accept (set, E_GIVE)) != NULL) {
    pn->children[0] = assignment (set);
    expect (set, E_SEMI);
  } else if ((pn = accept (set, E_BREAK)) != NULL)
    expect (set, E_SEMI);
  else if ((pn = accept (set, E_DEFUNC)) != NULL) {
    en = func_dec (set, true);
    pn->children[0] = en;
    pn->next = FACT_malloc (sizeof (struct FACT_tree));
    memset (pn->next, 0, sizeof (struct FACT_tree));
    pn->next->id.id = E_SEMI;
  } else if (check (set, E_VAR) && check_forward (set, E_IMP_DEF, 1)) {
    en = accept (set, E_VAR);
    pn = accept (set, E_IMP_DEF);
    pn->children[0] = en;
    pn->children[1] = assignment (set);
    pn->next = expect (set, E_SEMI);
  } else {
    /* Just a basic expression. */
    pn = assignment (set);
    
    /* Check for function declaration. */
    if (check (set, E_FUNC_DEF)) {
      en = func_dec (set, false);
      en->children[0] = pn;
      pn = en;
      pn->next = FACT_malloc (sizeof (struct FACT_tree));
      memset (pn->next, 0, sizeof (struct FACT_tree));
      pn->next->id.id = E_SEMI;
    } else
      pn->next = expect (set, E_SEMI);
  }
  return pn;
}     

static FACT_tree_t stmt_list (FACT_lexed_t *set)
{
  FACT_tree_t pn;
  
  pn = stmt (set);

  if (!check (set, E_END)
      && !check (set, E_CL_CURL)) {
    if (pn->next != NULL)
      pn->next->next = stmt_list (set);
    else
      pn->next = stmt_list (set);
  }
  return pn;
}

static FACT_tree_t top_stmt_list (FACT_lexed_t *set)
{
  FACT_tree_t hold;
  FACT_tree_t an, pn;

  if ((pn = accept (set, E_CONST)) != NULL) { /* Constant declaration. */
    pn->children[0] = expect (set, E_VAR);
    if (!check (set, E_OP_PAREN) && !check (set, E_SET))
      error (set, "expected \"=\" or \"(\" after constant declaration");
    if ((pn->children[1] = accept (set, E_SET)) != NULL) {
      /* Number constant. */
      pn->children[2] = assignment (set);
      pn->next = expect (set, E_SEMI);
    } else {
      /* Constant function. */
      expect (set, E_OP_PAREN);
      if ((an = accept (set, E_NUM_DEF)) != NULL
	  || (an = accept (set, E_SCOPE_DEF)) != NULL
	  || (an = accept (set, E_LOCAL_CHECK)) != NULL) {
	an->children[0] = expect (set, E_VAR);
	hold = an;
	while (accept (set, E_COMMA) != NULL) {
	  if ((an = accept (set, E_NUM_DEF)) == NULL
	      && (an = accept (set, E_SCOPE_DEF)) == NULL
	      && (an = accept (set, E_LOCAL_CHECK)) == NULL)
	    error (set, "expected num, scope, or ? token after comma");
	  an->children[0] = expect (set, E_VAR);      
	  an->children[1] = hold;
	  hold = an;
	}
      }

      expect (set, E_CL_PAREN);
      expect (set, E_OP_CURL);
      pn->children[1] = an;

      if (!check (set, E_CL_CURL))
	pn->children[2] = stmt_list (set);
  
      expect (set, E_CL_CURL);
      pn->next = FACT_malloc (sizeof (struct FACT_tree));
      memset (pn->next, 0, sizeof (struct FACT_tree));
      pn->next->id.id = E_SEMI;
    }
  } else
    pn = stmt (set);

  if (!check (set, E_END)
      && !check (set, E_CL_CURL)) {
    if (pn->next != NULL)
      pn->next->next = top_stmt_list (set);
    else
      pn->next = top_stmt_list (set);
  }
  return pn;
}

static FACT_tree_t func_dec (FACT_lexed_t *set, bool imp_scope_dec)
{
  FACT_tree_t an, pn;
  FACT_tree_t hold;

  pn = expect (set, (imp_scope_dec) ? E_VAR : E_FUNC_DEF);
  expect (set, E_OP_PAREN);

  /* Argument list. */
  if ((an = accept (set, E_NUM_DEF)) != NULL
      || (an = accept (set, E_SCOPE_DEF)) != NULL
      || (an = accept (set, E_LOCAL_CHECK)) != NULL) {
    an->children[0] = expect (set, E_VAR);
    hold = an;
    while (accept (set, E_COMMA) != NULL) {
      if ((an = accept (set, E_NUM_DEF)) == NULL
	  && (an = accept (set, E_SCOPE_DEF)) == NULL
	  && (an = accept (set, E_LOCAL_CHECK)) == NULL)
	error (set, "expected num, scope, or ? token after comma");
      an->children[0] = expect (set, E_VAR);
      
      an->children[1] = hold;
      hold = an;
    }
  }

  expect (set, E_CL_PAREN);
  expect (set, E_OP_CURL);

  pn->children[1] = an;

  if (!check (set, E_CL_CURL))
    pn->children[2] = stmt_list (set);
  
  expect (set, E_CL_CURL);

  return pn;
}

static FACT_tree_t def_scalar (FACT_lexed_t *set)
{
  FACT_tree_t an, n;

  if ((an = accept (set, E_OP_BRACK)) != NULL) {
    an = assignment (set);
    expect (set, E_CL_BRACK);
    while (accept (set, E_OP_BRACK) != NULL) {
      n = assignment (set);
      expect (set, E_CL_BRACK);
      n->next = an;
      an = n;
    }
  }
  return an;
}

static FACT_tree_t paren (FACT_lexed_t *set)
{
  FACT_tree_t pn, en;

  if ((pn = accept (set, E_OP_CURL)) != NULL) {
    pn->children[0] = stmt_list (set);
    expect (set, E_CL_CURL);
  } else
    pn = assignment (set);

  if (check (set, E_FUNC_DEF)) {
    en = func_dec (set, false);
    en->children[0] = pn;
    pn = en;
  }

  expect (set, E_CL_PAREN);
  return pn;
}

static FACT_tree_t arg_list (FACT_lexed_t *set)
{
  FACT_tree_t pn, an;

  if (accept (set, E_CL_PAREN) == NULL) {
    pn = assignment (set);
    an = pn;
    while (accept (set, E_CL_PAREN) == NULL) {
      expect (set, E_COMMA);
      an->next = assignment (set);
      an = an->next;
    }
    return pn;
  }
  return NULL;
}
	  
static FACT_tree_t factor (FACT_lexed_t *set)
{
  FACT_tree_t pn, en;
  
  if ((pn = accept (set, E_OP_PAREN)) != NULL)
    pn = paren (set);
  else if ((pn = accept (set, E_THREAD)) != NULL) {
    if ((pn->children[0] = accept (set, E_OP_CURL)) != NULL) {
      pn->children[0]->children[0] = stmt_list (set);
      pn->children[0]->next = expect (set, E_CL_CURL);
    } else
      pn->children[0] = assignment (set);
  } else if ((pn = accept (set, E_DQ)) != NULL) {
    pn->children[0] = accept (set, E_STR_CONST);
    expect (set, E_DQ);
  } else if ((pn = accept (set, E_SQ)) != NULL) {
    pn->children[0] = accept (set, E_STR_CONST);
    expect (set, E_SQ);
  } else if ((pn = accept (set, E_OP_BRACK)) != NULL) { /* Anonymous array. */
    pn->children[0] = assignment (set);
    en = pn;
    while ((en->children[1] = accept (set, E_COMMA)) != NULL) {
      en = en->children[1];
      en->children[0] = assignment (set);
    }
    expect (set, E_CL_BRACK);
  } else if ((pn = accept (set, E_VAR)) != NULL) {
    if ((en = accept (set, E_LOCAL_CHECK)) != NULL
	|| (en = accept (set, E_GLOBAL_CHECK)) != NULL) {
      en->children[0] = pn;
      pn = en;
    }
  } else if ((pn = accept (set, E_NUM)) == NULL) {
    if ((pn = accept (set, E_NUM_DEF)) != NULL
	|| (pn = accept (set, E_SCOPE_DEF)) != NULL) {
      pn->children[0] = def_scalar (set);
      pn->children[1] = expect (set, E_VAR);
    } else
      error (set, ((set->tokens[set->curr].id >= E_VAR
		    && set->tokens[set->curr].id <= E_END)
		   ? "unexpected %s"
		   : "unexpected `%s'"), FACT_get_lexem (set->tokens[set->curr].id));
  }
  
  return pn;  
}

static FACT_tree_t unary (FACT_lexed_t *set)
{
  FACT_tree_t pn;

  if (accept (set, E_ADD) != NULL)
    /* +(num) does absolutely NOTHING. */
    pn = unary (set);
  else if ((pn = accept (set, E_SUB)) != NULL) {
    pn->id.id = E_NEG;
    pn->children[0] = unary (set);
  } else
    pn = factor (set);
  return pn;
}

static FACT_tree_t opt_pb (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = unary (set);

  /* Non-terminal opt_array is built in here. */
  if ((pn = accept (set, E_OP_PAREN)) != NULL
      || (pn = accept (set, E_OP_BRACK)) != NULL
      || (pn = accept (set, E_IN)) != NULL) {
    do {
      switch (pn->id.id) {
      case E_OP_PAREN:
	pn->id.id = E_FUNC_CALL;
	pn->children[1] = ln;
	pn->children[0] = arg_list (set);
	break;

      case E_OP_BRACK:
	pn->id.id = E_ARRAY_ELEM;
	pn->children[1] = ln;
	pn->children[0] = assignment (set);
	expect (set, E_CL_BRACK);
	break;

      default: /* E_IN */
	pn->children[0] = ln;
	pn->children[1] = unary (set);
      }
      ln = pn;
    } while ((pn = accept (set, E_OP_PAREN)) != NULL
	     || (pn = accept (set, E_OP_BRACK)) != NULL
	     || (pn = accept (set, E_IN)) != NULL);
  }
  return ln;
}

static FACT_tree_t term (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = opt_pb (set);
  
  if ((pn = accept (set, E_MUL)) != NULL
      || (pn = accept (set, E_DIV)) != NULL
      || (pn = accept (set, E_MOD)) != NULL) {
    do {
      pn->children[0] = ln;
      pn->children[1] = opt_pb (set);
      ln = pn;
    } while ((pn = accept (set, E_MUL)) != NULL
	     || (pn = accept (set, E_DIV)) != NULL
	     || (pn = accept (set, E_MOD)) != NULL);
  }
  return ln;
}

static FACT_tree_t expression (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = term (set);

  if ((pn = accept (set, E_ADD)) != NULL
      || (pn = accept (set, E_SUB)) != NULL) {
    do {
      pn->children[0] = ln;
      pn->children[1] = term (set);
      ln = pn;
    } while ((pn = accept (set, E_ADD)) != NULL
	     || (pn = accept (set, E_SUB)) != NULL);
  }
  return ln;
}

static FACT_tree_t comparison (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = expression (set);

  if ((pn = accept (set, E_LT)) != NULL
      || (pn = accept (set, E_LE)) != NULL
      || (pn = accept (set, E_MT)) != NULL
      || (pn = accept (set, E_ME)) != NULL) {
    do {
      pn->children[0] = ln;
      pn->children[1] = expression (set);
      ln = pn;
    } while ((pn = accept (set, E_LT)) != NULL
	     || (pn = accept (set, E_LE)) != NULL
	     || (pn = accept (set, E_MT)) != NULL
	     || (pn = accept (set, E_ME)) != NULL);
  }
  return ln;
}

static FACT_tree_t equality (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = comparison (set);

  if ((pn = accept (set, E_EQ)) != NULL
      || (pn = accept (set, E_NE)) != NULL) {
    do {
      pn->children[0] = ln;
      pn->children[1] = comparison (set);
      ln = pn;
    } while ((pn = accept (set, E_EQ)) != NULL
	     || (pn = accept (set, E_NE)) != NULL);
  }
  return ln;
}

static FACT_tree_t land (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = equality (set);

  if ((pn = accept (set, E_AND)) != NULL) {
    do {
      pn->children[0] = ln;
      pn->children[1] = equality (set);
      ln = pn;
    } while ((pn = accept (set, E_AND)) != NULL);
  }
  return ln;
}

static FACT_tree_t lor (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = land (set);

  if ((pn = accept (set, E_OR)) != NULL) {
    do {
      pn->children[0] = ln;
      pn->children[1] = land (set);
      ln = pn;
    } while ((pn = accept (set, E_OR)) != NULL);
  }
  return ln;
}

static FACT_tree_t assignment (FACT_lexed_t *set)
{
  FACT_tree_t ln, pn;

  ln = lor (set);

  if ((pn = accept (set, E_SET)) != NULL
      || (pn = accept (set, E_MOD_AS)) != NULL
      || (pn = accept (set, E_BIT_AND_AS)) != NULL
      || (pn = accept (set, E_MUL_AS)) != NULL
      || (pn = accept (set, E_ADD_AS)) != NULL
      || (pn = accept (set, E_SUB_AS)) != NULL
      || (pn = accept (set, E_DIV_AS)) != NULL
      || (pn = accept (set, E_BIT_XOR_AS)) != NULL
      || (pn = accept (set, E_BIT_IOR_AS)) != NULL) {
    pn->children[0] = ln;
    pn->children[1] = assignment (set);
    return pn;
  }
  
  return ln;
}

/* is_ident_valid and is_num_valid should both be done in the lexing
 * phase, but I'm putting them here for now to make things easier to
 * code.  
 */

static bool is_ident_valid (char *ident)
{
  if (!isalpha (*ident) && *ident != '_')
    return false;

  for (ident++; *ident != '\0'; ident++) {
    if (!isalpha (*ident) && *ident != '_' && !isdigit (*ident))
      return false;
  }

  return true;
}

/* This is really kludgy. My bad. It could probably be simplified to
 * a very large degree but I wrote this after very little sleep.
 */
static bool is_num_valid (char *num)
{
  bool decimal;

  if (!isdigit (*num) && *num != '.')
    return false;
  if (*num == '0' && tolower (*(num + 1)) == 'x') {
    num += 2;
    if (!isdigit (*num) && (tolower (*num) < 'a' || tolower (*num) > 'f'))
      return false;
    for (num++; *num != '\0'; num++) {
      if (!isdigit (*num) && (tolower (*num) < 'a' || tolower (*num) > 'f'))
	return false;
    }
    return true;
  }
  if (*num == '.') {
    if (!isdigit (*(num + 1)))
      return false;
    decimal = true;
  } else
    decimal = false;

  for (num++; *num != '\0'; num++) {
    if (*num == '.') {
      if (decimal)
	return false;
      decimal = true;
    } else if (!isdigit (*num))
      return false;
  }
  return true;
}

FACT_tree_t FACT_parse (FACT_lexed_t *tokens) /* Just an alias for top_stmt_list. */
{
  return top_stmt_list (tokens);
}

