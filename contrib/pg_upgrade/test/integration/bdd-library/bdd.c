#include "bdd.h"

void
given(bdd_step_function arrangeFunction)
{
	arrangeFunction();
}

void
when(bdd_step_function actFunction)
{
	actFunction();
}

void
then(bdd_step_function assertionFunction)
{
	assertionFunction();
}

void
and(bdd_step_function conjunctionFunction)
{
	conjunctionFunction();
}

