#include "internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */

#define MEMREF_PLACEHOLDER_ADDRESS 0xFFFFFFFF

rc_memref_t* rc_alloc_memref(rc_parse_state_t* parse, unsigned address, char size, char is_indirect) {
  rc_memref_t** next_memref;
  rc_memref_t* memref;

  if (!is_indirect) {
    /* attempt to find an existing memref that can be shared */
    next_memref = parse->first_memref;
    while (*next_memref) {
      memref = *next_memref;
      if (!memref->value.is_indirect && memref->address == address && memref->value.size == size)
        return memref;

      next_memref = &memref->next;
    }

    /* no match found, create a new entry */
    memref = RC_ALLOC_SCRATCH(rc_memref_t, parse);
    *next_memref = memref;
  }
  else {
    /* indirect references always create a new entry because we can't guarantee that the 
     * indirection amount will be the same between references. because they aren't shared,
     * don't bother putting them in the chain.
     */
    memref = RC_ALLOC(rc_memref_t, parse);
  }

  memset(memref, 0, sizeof(*memref));
  memref->address = address;
  memref->value.size = size;
  memref->value.is_indirect = is_indirect;

  return memref;
}

static unsigned rc_peek_value(unsigned address, char size, rc_peek_t peek, void* ud) {
  unsigned value;

  if (!peek)
    return 0;

  switch (size)
  {
    case RC_MEMSIZE_BIT_0:
      value = (peek(address, 1, ud) >> 0) & 1;
      break;

    case RC_MEMSIZE_BIT_1:
      value = (peek(address, 1, ud) >> 1) & 1;
      break;

    case RC_MEMSIZE_BIT_2:
      value = (peek(address, 1, ud) >> 2) & 1;
      break;

    case RC_MEMSIZE_BIT_3:
      value = (peek(address, 1, ud) >> 3) & 1;
      break;

    case RC_MEMSIZE_BIT_4:
      value = (peek(address, 1, ud) >> 4) & 1;
      break;

    case RC_MEMSIZE_BIT_5:
      value = (peek(address, 1, ud) >> 5) & 1;
      break;

    case RC_MEMSIZE_BIT_6:
      value = (peek(address, 1, ud) >> 6) & 1;
      break;

    case RC_MEMSIZE_BIT_7:
      value = (peek(address, 1, ud) >> 7) & 1;
      break;

    case RC_MEMSIZE_LOW:
      value = peek(address, 1, ud) & 0x0f;
      break;

    case RC_MEMSIZE_HIGH:
      value = (peek(address, 1, ud) >> 4) & 0x0f;
      break;

    case RC_MEMSIZE_8_BITS:
      value = peek(address, 1, ud);
      break;

    case RC_MEMSIZE_16_BITS:
      value = peek(address, 2, ud);
      break;

    case RC_MEMSIZE_24_BITS:
      /* peek 4 bytes - don't expect the caller to understand 24-bit numbers */
      value = peek(address, 4, ud) & 0x00FFFFFF;
      break;

    case RC_MEMSIZE_32_BITS:
      value = peek(address, 4, ud);
      break;

    default:
      value = 0;
      break;
  }

  return value;
}

void rc_update_memref_value(rc_memref_value_t* memref, unsigned new_value) {
  if (memref->value == new_value) {
    memref->changed = 0;
  }
  else {
    memref->prior = memref->value;
    memref->value = new_value;
    memref->changed = 1;
  }
}

void rc_update_memref_values(rc_memref_t* memref, rc_peek_t peek, void* ud) {
  while (memref) {
    /* indirect memory references are not shared and will be updated in rc_get_memref_value */
    if (!memref->value.is_indirect)
      rc_update_memref_value(&memref->value, rc_peek_value(memref->address, memref->value.size, peek, ud));

    memref = memref->next;
  }
}

void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_t** memrefs) {
  parse->first_memref = memrefs;
  *memrefs = 0;
}

unsigned rc_get_memref_value(rc_memref_t* memref, int operand_type, rc_eval_state_t* eval_state) {
  /* if this is an indirect reference, handle the indirection. */
  if (memref->value.is_indirect) {
    const unsigned new_address = memref->address + eval_state->add_address;
    rc_update_memref_value(&memref->value, rc_peek_value(new_address, memref->value.size, eval_state->peek, eval_state->peek_userdata));
  }

  return rc_get_memref_value_value(&memref->value, operand_type);
}

unsigned rc_get_memref_value_value(rc_memref_value_t* memref, int operand_type) {
  switch (operand_type)
  {
    /* most common case explicitly first, even though it could be handled by default case.
     * this helps the compiler to optimize if it turns the switch into a series of if/elses */
    case RC_OPERAND_ADDRESS:
      return memref->value;

    case RC_OPERAND_DELTA:
      if (!memref->changed) {
        /* fallthrough */
    default:
        return memref->value;
      }
      /* fallthrough */
    case RC_OPERAND_PRIOR:
      return memref->prior;
  }
}
