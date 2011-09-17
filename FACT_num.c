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

static FACT_num_t *make_num_array (char *, size_t, size_t *, size_t);
static FACT_num_t get_element (FACT_num_t, size_t, size_t *, size_t);
static FACT_num_t copy_num (FACT_num_t);
static void free_num (FACT_num_t);

FACT_num_t
FACT_get_local_num (FACT_scope_t curr, char *name) /* Search for a number. */
{
  int res;
  FACT_num_t *low, *mid, *high;

  low = *curr->num_stack;
  high = low + *curr->num_stack_size;

  while (low < high)
    {
      mid = low + (high - low) / 2;
      res = strcmp (name, (*mid)->name);

      if (res < 0)
	high = mid;
      else if (res > 0)
	low = mid + 1;
      else
	return *mid;
    }

  /* No number variable was found. */
  return NULL;
}

FACT_num_t
FACT_add_num (FACT_scope_t curr, char *name) /* Add a number variable to a scope. */
{
  size_t i;
  
  /* Check if the variable already exists. */
  if (FACT_get_local_num (curr, name) != NULL)
    /* It already exists, throw an error. */
    FACT_throw_error (curr, "local variable %s already exists; use \"del\" before redefining", name);

  if (FACT_get_local_scope (curr, name) != NULL)
    FACT_throw_error (curr, "local scope %s already exists; use \"del\" before redefining", name);

  /* Reallocate the var stack and add the variable. */
  *curr->num_stack = FACT_realloc (*curr->num_stack, sizeof (FACT_num_t **) * (*curr->num_stack_size + 1));
  (*curr->num_stack)[*curr->num_stack_size] = FACT_alloc_num ();
  (*curr->num_stack)[*curr->num_stack_size]->name = name;

  /* Move the var back to the correct place. */
  for (i = (*curr->num_stack_size)++; i > 0; i--)
    {
      if (strcmp ((*curr->num_stack)[i - 1]->name, name) > 0)
	{
	  /* Swap the elements. */
	  FACT_num_t hold;
	  
	  hold = (*curr->num_stack)[i - 1];
	  (*curr->num_stack)[i - 1] = (*curr->num_stack)[i];
	  (*curr->num_stack)[i] = hold;
	}
      else
	break;
    }

  return (*curr->num_stack)[i];
}

void
FACT_def_num (char *args, bool anonymous) /* Define a local or anonymous number variable. */
{
  mpc_t elem_value;
  FACT_t push_val;
  size_t i;
  size_t *dim_sizes; /* Size of each dimension. */
  size_t dimensions; /* Number of dimensions.   */

  /* Get the number of dimensions. TODO: add checking here. */
  dimensions = mpc_get_ui (((FACT_num_t) Furlow_reg_val (args[0], NUM_TYPE))->value);

  /* Add or allocate the variable. */
  push_val.ap = (anonymous
		 ? FACT_alloc_num () /* Perhaps add to a heap? */
		 : FACT_add_num (CURR_THIS, args + 1));
  push_val.type = NUM_TYPE;

  if (!dimensions)
    /* If the variable has no dimensions, simply push it and return. */
    goto end;

  /* Add the dimensions. */
  dim_sizes = NULL;
  
  /* Keep popping the stack until we have all the dimension sizes. */ 
  for (i = 0; i < dimensions; i++)
    {
      dim_sizes = FACT_realloc (dim_sizes, sizeof (size_t *) * (i + 1));
      /* Pop the stack and get the value. */
      elem_value[0] = ((FACT_num_t) Furlow_reg_val (R_POP, NUM_TYPE))->value[0];
      /* Check to make sure we aren't grossly out of range. */
      if (mpc_cmp_ui (elem_value, ULONG_MAX) > 0
	  || mpz_sgn (elem_value->value) < 0)
	FACT_throw_error (CURR_THIS, "out of bounds error"); 
      dim_sizes[i] = mpc_get_ui (elem_value);
      if (dim_sizes[i] <= 1)
	{
	  FACT_free (dim_sizes);
	  FACT_throw_error (CURR_THIS, "dimension size must be larger than 1");
	}
    }

  /* Make the variable an array. */
  ((FACT_num_t) push_val.ap)->array_up = make_num_array (((FACT_num_t) push_val.ap)->name, dimensions, dim_sizes, 0);
  ((FACT_num_t) push_val.ap)->array_size = dim_sizes[0];
  FACT_free (dim_sizes);

  /* Push the variable and return. */
 end:
  push_v (push_val);
}

void
FACT_get_num_elem (FACT_num_t base, char *args)
{
  mpc_t elem_value;
  FACT_t push_val;
  size_t i;
  size_t *elems;     /* element of each dimension to access. */
  size_t dimensions; /* Number of dimensions.                */

  /* Get the number of dimensions. */
  dimensions = mpc_get_ui (((FACT_num_t) Furlow_reg_val (args[0], NUM_TYPE))->value);
  
  /* Add the dimensions. */
  elems = NULL;
  
  /* Keep popping the stack until we have all the dimension sizes. */ 
  for (i = 0; i < dimensions; i++)
    {
      elems = FACT_realloc (elems, sizeof (size_t *) * (i + 1));
      elem_value[0] = ((FACT_num_t) Furlow_reg_val (R_POP, NUM_TYPE))->value[0];
      /* Check to make sure we aren't grossly out of range. */
      if (mpc_cmp_ui (elem_value, ULONG_MAX) > 0
	  || mpz_sgn (elem_value->value) < 0)
	FACT_throw_error (CURR_THIS, "out of bounds error"); /* should elaborate here. */
      elems[i] = mpc_get_ui (elem_value); 
    }

  push_val.type = NUM_TYPE;
  push_val.ap = get_element (base, dimensions, elems, 0);

  FACT_free (elems);

  push_v (push_val);
}

void
FACT_set_num (FACT_num_t rop, FACT_num_t op)
{
  size_t i;

  /* Free rop. */
  for (i = 0; i < rop->array_size; i++)
    free_num (rop->array_up[i]);

  mpc_set (rop->value, op->value);
  rop->array_size = op->array_size;

  if (rop->array_size)
    rop->array_up = FACT_realloc (rop->array_up, sizeof (FACT_num_t) * op->array_size);
  else
    {
      FACT_free (rop->array_up);
      rop->array_up = NULL;
      return; /* Nothing left to do here. */
    }

  for (i = 0; i < rop->array_size; i++)
    rop->array_up[i] = copy_num (op->array_up[i]);
}
  
static FACT_num_t *
make_num_array (char *name, size_t dims, size_t *dim_sizes, size_t curr_dim)
{
  size_t i;
  FACT_num_t *root;
  
  if (curr_dim >= dims)
    return NULL;

  root = FACT_alloc_num_array (dim_sizes[curr_dim]);

  for (i = 0; i < dim_sizes[curr_dim]; i++)
    {
      root[i]->name = name;
      root[i]->array_up = make_num_array (name, dims, dim_sizes, curr_dim + 1);
      /* Could be optimized not to check every single time. */
      if (root[i]->array_up != NULL)
	root[i]->array_size = dim_sizes[curr_dim + 1];
    }

  return root;
}

static FACT_num_t
get_element (FACT_num_t base, size_t dims, size_t *elems, size_t curr_dim)
{
  size_t hold;
  
  if (curr_dim >= dims)
    return base;

  if (base->array_size <= elems[curr_dim])
    {
      hold = elems[curr_dim];
      FACT_free (elems);
      FACT_throw_error (CURR_THIS, "out of bounds error; expected value in [0, %lu), but value is %lu",
			base->array_size, hold);
    }

  if (base->array_size == 1)
    {
      FACT_free (elems);
      if (curr_dim + 1 != dims)
	FACT_throw_error (CURR_THIS, "out of bounds error; unexpected dimensions");
      return base;
    }

  base = base->array_up[elems[curr_dim]];

  return get_element (base, dims, elems, curr_dim + 1);
}
      
static FACT_num_t
copy_num (FACT_num_t root) /* Copy a number array recursively. */
{
  size_t i;
  FACT_num_t res;

  if (root == NULL)
    return NULL;
  
  res = FACT_alloc_num ();
  res->array_size = root->array_size;
  res->array_up = ((root->array_up != NULL)
		   ? FACT_malloc (sizeof (FACT_num_t) * res->array_size)
		   : NULL);
  mpc_set (res->value, root->value);

  for (i = 0; i < root->array_size; i++)
    res->array_up[i] = copy_num (root->array_up[i]);

  return res;
}

static void
free_num (FACT_num_t root) /* Free a number array recursively. */
{
  size_t i;

  if (root == NULL)
    return;

  for (i = 0; i < root->array_size; i++)
    free_num (root->array_up[i]);

  mpc_clear (root->value);
  FACT_free (root->array_up);
  FACT_free (root);
}