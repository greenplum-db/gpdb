#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../appendonlywriter.c"

void
test__AppendOnlyRelHashNew_replace_unused_entry(void **state)
{
	AppendOnlyWriterData writer;
	AORelHashEntryData entry;
	bool exists = false;

	Gp_role = GP_ROLE_DISPATCH;
	MaxAppendOnlyTables = 1;

	writer.num_existing_aorels = 1;
	AppendOnlyWriter = &writer;

	/*
	 * mock AORelLookupHashEntry so we do not find an entry
	 */
	expect_any(hash_search, hashp);
	expect_any(hash_search, keyPtr);
	expect_any(hash_search, action);
	expect_any(hash_search, foundPtr);
	will_return(hash_search, NULL);

	/*
	 * mock loop through all entries for an unused one
	 */
	expect_any(hash_seq_init, status);
	expect_any(hash_seq_init, hashp);
	will_be_called(hash_seq_init);

	/*
	 * mock we found an unused entry
	 */
	expect_any(hash_seq_search, status);
	will_return(hash_seq_search, &entry);

	/*
	 * ensure there is no transaction using this relation
	 */
	entry.txns_using_rel = 0;

	/*
	 * mock AORelRemoveHashEntry so that we remove this unused entry
	 */
	expect_any(hash_search, hashp);
	expect_any(hash_search, keyPtr);
	expect_any(hash_search, action);
	expect_any(hash_search, foundPtr);
	will_return(hash_search, &entry);

	/*
	 * mock sequential search end
	 */
	expect_any(hash_seq_term, status);
	will_be_called(hash_seq_term);

	/*
	 * mock new entry creation
	 */
	expect_any(hash_search, hashp);
	expect_any(hash_search, keyPtr);
	expect_any(hash_search, action);
	expect_any(hash_search, foundPtr);
	will_return(hash_search, &entry);

	/*
	 * TEST
	 */
	AORelHashEntry actual = AppendOnlyRelHashNew(0, &exists);

	/*
	 * make sure we get our entry back
	 */
	assert_true(actual == &entry);

	/*
	 * since we removed the unused entry, we should decrease num_existing_aorels
	 */
	assert_int_equal(writer.num_existing_aorels, 0);
}

void
test__AppendOnlyRelHashNew_existing_entry(void **state)
{
	AppendOnlyWriterData writer;
	AORelHashEntryData entry;
	bool exists = false;

	Gp_role = GP_ROLE_DISPATCH;
	MaxAppendOnlyTables = 1;

	writer.num_existing_aorels = 1;
	AppendOnlyWriter = &writer;

	/*
	 * mock AORelLookupHashEntry so that we do find an entry
	 */
	expect_any(hash_search, hashp);
	expect_any(hash_search, keyPtr);
	expect_any(hash_search, action);
	expect_any(hash_search, foundPtr);
	will_return(hash_search, &entry);

	/*
	 * TEST
	 */
	AppendOnlyRelHashNew(0, &exists);

	/*
	 * We got existing entry
	 */
	assert_true(exists);
}

void
test__AppendOnlyRelHashNew_give_up(void **state)
{
	AppendOnlyWriterData writer;
	AORelHashEntryData entry;
	bool exists = false;

	Gp_role = GP_ROLE_DISPATCH;
	MaxAppendOnlyTables = 1;

	writer.num_existing_aorels = 1;
	AppendOnlyWriter = &writer;

	/*
	 * mock AORelLookupHashEntry so that we cannot find an entry
	 */
	expect_any(hash_search, hashp);
	expect_any(hash_search, keyPtr);
	expect_any(hash_search, action);
	expect_any(hash_search, foundPtr);
	will_return(hash_search, NULL);

	/*
	 * mock loop through all entries for an unused one
	 */
	expect_any(hash_seq_init, status);
	expect_any(hash_seq_init, hashp);
	will_be_called(hash_seq_init);

	/*
	 * mock we found an entry
	 */
	expect_any(hash_seq_search, status);
	will_return(hash_seq_search, &entry);

	/*
	 * but all the entries are being used in a transaction
	 */
	entry.txns_using_rel = 1;

	/*
	 * mock we cannot find more entries
	 */
	expect_any(hash_seq_search, status);
	will_return(hash_seq_search, NULL);

	/*
	 * TEST
	 */
	AORelHashEntry actual = AppendOnlyRelHashNew(0, &exists);

	/*
	 * give up, no entry created, and no existing one found
	 */
	assert_true(actual == NULL);
	assert_false(exists);

	/*
	 * nothing changed to the num_existing_aorels
	 */
	assert_int_equal(writer.num_existing_aorels, 1);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const	UnitTest tests[] = {
		unit_test(test__AppendOnlyRelHashNew_replace_unused_entry),
		unit_test(test__AppendOnlyRelHashNew_existing_entry),
		unit_test(test__AppendOnlyRelHashNew_give_up),
	};

	return run_tests(tests);
}
