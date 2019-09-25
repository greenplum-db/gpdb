#ifndef BDD_LIBRARY_H
#define BDD_LIBRARY_H

void given(void (*arrangeFunction) (void));
void when(void (*actFunction) (void));
void then(void (*assertionFunction) (void));
void and(void (*assertionFunction) (void));

#endif /* BDD_LIBRARY_H */
