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

#ifndef FACT_ERROR_H_
#define FACT_ERROR_H_

#include <FACT/FACT_types.h>

#define FACT_MAX_ERR_LEN 100 /* Maximum number of characters in an error string. */

/* Error handling:                                                          */
void FACT_throw_error (FACT_scope_t, const char *, ...); /* Throw an error. */
void FACT_print_error (FACT_error_t);                    /* Print an error. */

#endif /* FACT_ERROR_H_ */
