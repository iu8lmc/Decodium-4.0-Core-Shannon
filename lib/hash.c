/*
 * hash.c — C replacement for lib/hash.f90 + lib/hashing.f90
 *
 * Provides the Fortran-callable subroutine:
 *   subroutine hash(string, len, ihash)
 *     character*1, target :: string
 *     integer, intent(in)  :: len
 *     integer, intent(out) :: ihash
 *
 * Computes ihash = nhash(string, len, 146) & 0x7FFF
 *
 * Calling convention: gfortran passes character dummy arguments as
 * (pointer, ..., hidden_charlen) where hidden_charlen is appended after
 * all explicit arguments as size_t.
 */

#include <stddef.h>
#include <stdint.h>

/* nhash is implemented in lib/wsprd/nhash.c */
uint32_t nhash(const void *key, size_t length, uint32_t initval);

void hash_(const char *string, const int *len, int *ihash,
           size_t string_hidden_len)
{
    (void)string_hidden_len;  /* length is passed explicitly via *len */
    *ihash = (int)(nhash(string, (size_t)*len, 146u) & 0x7FFFu);
}
