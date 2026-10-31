/* The protocol steps of `langc list-steps` (Q-S8-8 (a)). */
#ifndef LANG_STEPS_H
#define LANG_STEPS_H

#include <stddef.h>

/* One Step constructor of domain/domain.lang and its surface form in a
   `protocol` (SPEC 5.2). */
typedef struct {
  const char *step;
  const char *form;
} StepRow;

/* The number of rows: the 12 Step constructors. */
size_t step_count(void);

/* Row INDEX in the order of domain/domain.lang, or NULL when INDEX is not
   below step_count(). */
const StepRow *step_at(size_t index);

#endif
