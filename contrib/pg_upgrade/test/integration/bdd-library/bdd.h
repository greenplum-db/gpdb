#ifndef BDD_LIBRARY_H
#define BDD_LIBRARY_H

typedef void (*bdd_step_function) (void);

void given(bdd_step_function step);
void when(bdd_step_function step);
void then(bdd_step_function step);
void and(bdd_step_function step);

#endif /* BDD_LIBRARY_H */
