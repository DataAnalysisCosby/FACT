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
#include "FACT_mpc.h"
#include "FACT_alloc.h"

#include <string.h>
#include <gmp.h>

static inline void
mpf_init_set_z(mpf_t rop, mpz_t op)
{
  mpf_init(rop);
  mpf_set_z(rop, op);
}

void
mpc_init(mpc_t new)
{
  new->fp = false;
  mpz_init(new->intv);
}

void
mpc_clear(mpc_t dead)
{
  if (dead->fp)
    mpf_clear(dead->fltv);
  else
    mpz_clear(dead->intv);
}

void
mpc_set(mpc_t rop, mpc_t op)
{
  if (op->fp) {
    if (!rop->fp) {
      rop->fp = true;
      mpz_clear(rop->intv);
      mpf_init(rop->fltv);
    }
    mpf_set(rop->fltv, op->fltv);
  } else {
    if (rop->fp) {
      rop->fp = false;
      mpf_clear(rop->fltv);
      mpz_init(rop->intv);
    }
    mpz_set(rop->intv, op->intv);
  }
}

void
mpc_set_ui(mpc_t rop, unsigned long op)
{
  if (rop->fp) {
    rop->fp = false;
    mpf_clear(rop->fltv);
    mpz_init(rop->intv);
  }
  mpz_set_ui(rop->intv, op);
}

void mpc_set_si (mpc_t rop, signed long op)
{
  if (rop->fp) {
    rop->fp = false;
    mpf_clear (rop->fltv);
    mpz_init (rop->intv);
  }
  mpz_set_si (rop->intv, op);
}

void mpc_set_str (mpc_t rop, char *str, int base) /* Convert a string to an mpc type. */
{
  if (base < 0 || strchr (str, '.') != NULL) { /* Floating point number. */
    if (!rop->fp) {
      rop->fp = true;
      mpz_clear (rop->intv);
      mpf_init (rop->fltv);
    }
    mpf_set_str (rop->fltv, str, base);
  } else { /* Integer. */
    if (rop->fp) {
      rop->fp = false;
      mpf_clear (rop->fltv);
      mpz_init (rop->intv);
    }
    mpz_set_str (rop->intv, str, base);
  }
}

void mpc_add (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (op1->fp) {
    if (op2->fp) {
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_add (rop->fltv, op1->fltv, op2->fltv);
    } else {
      mpf_t t;
      mpf_init_set_z (t, op2->intv);
      mpf_add (t, op1->fltv, t);
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_set (rop->fltv, t);
      mpf_clear (t);
    }
  } else if (!op2->fp) {
    if (rop->fp) {
      rop->fp = false;
      mpf_clear (rop->fltv);
      mpz_init (rop->intv);
    }
    mpz_add (rop->intv, op1->intv, op2->intv);
  } else {
    mpf_t t;
    mpf_init_set_z (t, op1->intv);
    mpf_add (t, t, op2->fltv);
    if (!rop->fp) {
      rop->fp = true;
      mpz_clear (rop->intv);
      mpf_init (rop->fltv);
    }
    mpf_set (rop->fltv, t);
    mpf_clear (t);
  }
}

void mpc_sub (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (op1->fp) {
    if (op2->fp) {
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_sub (rop->fltv, op1->fltv, op2->fltv);
    } else {
      mpf_t t;
      mpf_init_set_z (t, op2->intv);
      mpf_sub (t, op1->fltv, t);
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_set (rop->fltv, t);
      mpf_clear (t);
    }
  } else if (!op2->fp) {
    if (rop->fp) {
      rop->fp = false;
      mpf_clear (rop->fltv);
      mpz_init (rop->intv);
    }
    mpz_sub (rop->intv, op1->intv, op2->intv);
  } else {
    mpf_t t;
    mpf_init_set_z (t, op1->intv);
    mpf_sub (t, t, op2->fltv);
    if (!rop->fp) {
      rop->fp = true;
      mpz_clear (rop->intv);
      mpf_init (rop->fltv);
    }
    mpf_set (rop->fltv, t);
    mpf_clear (t);
  }
}

void mpc_neg (mpc_t rop, mpc_t op)
{
  if (op->fp) {
    if (!rop->fp) {
      rop->fp = true;
      mpz_clear (rop->intv);
      mpf_init (rop->fltv);
    }
    mpf_neg (rop->fltv, op->fltv);
  } else {
    if (rop->fp) {
      rop->fp = false;
      mpf_clear (rop->fltv);
      mpz_init (rop->intv);
    }
    mpz_neg (rop->intv, op->intv);
  }
}

void mpc_mul (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (op1->fp) {
    if (op2->fp) {
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_mul (rop->fltv, op1->fltv, op2->fltv);
    } else {
      mpf_t t;
      mpf_init_set_z (t, op2->intv);
      mpf_mul (t, op1->fltv, t);
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_set (rop->fltv, t);
      mpf_clear (t);
    }
  } else if (!op2->fp) {
    if (rop->fp) {
      rop->fp = false;
      mpf_clear (rop->fltv);
      mpz_init (rop->intv);
    }
    mpz_mul (rop->intv, op1->intv, op2->intv);
  } else {
    mpf_t t;
    mpf_init_set_z (t, op1->intv);
    mpf_mul (t, t, op2->fltv);
    if (!rop->fp) {
      rop->fp = true;
      mpz_clear (rop->intv);
      mpf_init (rop->fltv);
    }
    mpf_set (rop->fltv, t);
    mpf_clear (t);
  }
}

void mpc_div (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (op1->fp) {
    if (op2->fp) {
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_div (rop->fltv, op1->fltv, op2->fltv);
    } else {
      mpf_t t;
      mpf_init_set_z (t, op2->intv);
      mpf_div (t, op1->fltv, t);
      if (!rop->fp) {
	rop->fp = true;
	mpz_clear (rop->intv);
	mpf_init (rop->fltv);
      }
      mpf_set (rop->fltv, t);
      mpf_clear (t);
    }
  } else if (!op2->fp) {
    if (rop->fp) {
      rop->fp = false;
      mpf_clear (rop->fltv);
      mpz_init (rop->intv);
    }
    mpz_tdiv_q (rop->intv, op1->intv, op2->intv);
  } else {
    mpf_t t;
    mpf_init_set_z (t, op1->intv);
    mpf_div (t, t, op2->fltv);
    if (!rop->fp) {
      rop->fp = true;
      mpz_clear (rop->intv);
      mpf_init (rop->fltv);
    }
    mpf_set (rop->fltv, t);
    mpf_clear (t);
  }
}

/* Bitwise operators and the mod function return an undefined value for real numbers. */

void mpc_mod (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (rop->fp) {
    rop->fp = false;
    mpf_clear (rop->fltv);
    mpz_init (rop->intv);
  }
  mpz_mod (rop->intv, op1->intv, op2->intv);
}

void mpc_and (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (rop->fp) {
    rop->fp = false;
    mpf_clear (rop->fltv);
    mpz_init (rop->intv);
  }
  mpz_and (rop->intv, op1->intv, op2->intv);
}

void mpc_ior (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (rop->fp) {
    rop->fp = false;
    mpf_clear (rop->fltv);
    mpz_init (rop->intv);
  }
  mpz_ior (rop->intv, op1->intv, op2->intv);
}

void mpc_xor (mpc_t rop, mpc_t op1, mpc_t op2)
{
  if (rop->fp) {
    rop->fp = false;
    mpf_clear (rop->fltv);
    mpz_init (rop->intv);
  }
  mpz_xor (rop->intv, op1->intv, op2->intv);
}

int mpc_cmp (mpc_t op1, mpc_t op2)
{
  if (op1->fp) {
    if (op2->fp)
      return mpf_cmp (op1->fltv, op2->fltv);
    else {
      int r;
      mpf_t t;
      mpf_init_set_z (t, op2->intv);
      r = mpf_cmp (op1->fltv, t);
      mpf_clear (t);
      return r;
    }
  } else if (!op2->fp)
    return mpz_cmp (op1->intv, op2->intv);
  else {
    int r;
    mpf_t t;
    mpf_init_set_z (t, op1->intv);
    r = mpf_cmp (t, op2->fltv);
    mpf_clear (t);
    return r;
  }
}

int mpc_cmp_ui (mpc_t op1, unsigned long int op2)
{
  if (op1->fp)
    return mpf_cmp_ui (op1->fltv, op2);
  return mpz_cmp_ui (op1->intv, op2);
}

int mpc_cmp_si (mpc_t op1, signed long int op2)
{
  if (op1->fp)
    return mpf_cmp_si (op1->fltv, op2);
  return mpz_cmp_si (op1->intv, op2);
}

unsigned long int mpc_get_ui (mpc_t op)
{
  if (op->fp)
    return mpf_get_ui (op->fltv);
  return mpz_get_ui (op->intv);
}

signed long int mpc_get_si (mpc_t op)
{
  if (op->fp)
    return mpf_get_si (op->fltv);
  return mpz_get_si (op->intv);
}

char *
mpc_get_str (mpc_t op)
{
  if (op->fp) { 
    int neg;
    char *str;
    char *ls, *rs;
    size_t len;
    mp_exp_t exp;
  
    str = mpf_get_str(NULL, &exp, 10, 0, op->fltv);
    
    if (exp == 0) /* no need to do anything further */
      return str;
    
    neg = mpf_sgn(op->fltv) < 0 ? 1 : 0;
    len = strlen(str) - neg;
    str += neg;

    if (exp > 0) { /* op is greater than one */
      if (exp > len) { /* no decimal, but trailing zeroes */
	ls = str;
	rs = FACT_malloc(exp - len);
	memset(rs, '0', exp - len);
	rs[exp - len] = '\0';
      } else if (exp < len) { /* decimal */
	ls = FACT_malloc(exp + 2 + neg);
	if (neg)
	  ls[0] = '-', ls++;
	memcpy(ls, str, exp);
	ls[exp] = '.';
	ls[exp + 1] = '\0';
	rs = FACT_malloc(len - exp + 1);
	memcpy(rs, str + exp, len - exp);
	rs[len - exp] = '\0';
      } else /* leave string as-is */
	return str - neg;
    } else { /* op is less than one, greater than zero */
      ls = FACT_malloc(neg + len + -exp + 2);
      if (neg)
	ls[0] = '-', ls++;
      ls[0] = '0';
      ls[1] = '.';
      memset(ls + 2, '0', -exp);
      rs = str;
    }
    ls = FACT_realloc(ls - neg, strlen(ls - neg) + strlen(rs) + 1);
    strcat(ls, rs);
    return ls;

  } 
  return mpz_get_str (NULL, 10, op->intv);
}
