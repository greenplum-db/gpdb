#ifndef PG_UPGRADE_TEST_UTILITIES_ROW_ASSERTIONS_H
#define PG_UPGRADE_TEST_UTILITIES_ROW_ASSERTIONS_H

#include "stdbool.h"


typedef struct RowsData
{
	int  size;
	void *rows[10];
} Rows;

/*
 * Extension points
 */
extern bool (*matcher)(void *expected, void*actual);
extern void (*match_failed)(void *expected);

/* 
 * Library function
 */
extern void assert_rows(Rows *actual_rows, Rows expected_rows);

#endif /* PG_UPGRADE_TEST_UTILITIES_ROW_ASSERTIONS_H */
