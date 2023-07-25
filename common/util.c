#include "util.h"

bool is_386() {
  /* 386's always clear the AC flag in EFLAGS */
  uint32_t eflags = read_eflags();
  write_eflags(eflags | EFLAGS_AC);
  eflags = read_eflags();
  return (eflags & EFLAGS_AC) == 0;
}

bool is_486() {
  /* 386's always clear the AC flag in EFLAGS */
  uint32_t eflags = read_eflags();
  write_eflags(eflags | EFLAGS_ID);
  eflags = read_eflags();
  return (eflags & EFLAGS_ID) == 0;
}
