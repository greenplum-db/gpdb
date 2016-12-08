#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "../cdb_dump_util.c"

void 
test__isFilteringAllowed1(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = true;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_false(result);
}

void 
test__isFilteringAllowed2(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = false;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed3(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = true;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed4(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = false;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed5(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = true;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed6(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = false;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed7(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = true;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed8(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = false;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void
test__validateTimeStamp7(void **state)
{
	char* ts = "20100729093000";
	bool result = ValidateTimestampKey(ts);
	assert_true(result);
}

void
test__validateTimeStamp8(void **state)
{
	// too few chars
	char* ts = "2010072909300";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp9(void **state)
{
	// too many chars
	char* ts = "201007290930000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp10(void **state)
{
	// as long as its a digit its valid
	char* ts = "00000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_true(result);
}

void
test__validateTimeStamp11(void **state)
{
	// as long as its a digit its valid
	char* ts = "0a000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp12(void **state)
{
	// as long as its a digit its valid
	char* ts = "0q000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp13(void **state)
{
	// leading space
	char* ts = " 00000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp14(void **state)
{
	// trailing space
	char* ts = "00000000000000 ";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp15(void **state)
{
	// NULL pointer
	char* ts = NULL;
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp16(void **state)
{
	// non-terminated string
	char ts[20];
	int i = 0;
	for (i = 0; i < sizeof(ts); ++i){
		ts[i] = '1';
	}
	bool result = ValidateTimestampKey((char*)ts);
	assert_false(result);
}

void
test__getTimeStampKey17(void **state)
{
	char* ts = "20100729093000";
	assert_true(!strcmp(ts, GetTimestampKey(ts)));
}

void
test__getTimeStampKey18(void **state)
{
	char* ts = NULL;
	assert_true(GetTimestampKey(ts) != NULL);
}

void test__shouldExpandChildren1(void **state)
{
    bool g_gp_supportsPartitioning = false;
    bool no_expand_children = false;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_false(result);
}

void test__shouldExpandChildren2(void **state)
{
    bool g_gp_supportsPartitioning = true;
    bool no_expand_children = false;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_true(result);
}

void test__shouldExpandChildren3(void **state)
{
    bool g_gp_supportsPartitioning = true;
    bool no_expand_children = true;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_false(result);
}

void test__shouldExpandChildren4(void **state)
{
    bool g_gp_supportsPartitioning = false;
    bool no_expand_children = true;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_false(result);
}

void test__shouldDumpSchemaOnly1(void **state)
{
    int role = ROLE_SEGDB;
    bool incrementalBackup = true;
    void *list = NULL;   
 
    bool result = shouldDumpSchemaOnly(role, incrementalBackup, list);
    assert_true(result);
}

void test__shouldDumpSchemaOnly2(void **state)
{
    int role = ROLE_MASTER;
    bool incrementalBackup = true;
    void *list = NULL;   
 
    bool result = shouldDumpSchemaOnly(role, incrementalBackup, list);
    assert_false(result);
}

void test__shouldDumpSchemaOnly3(void **state)
{
    int role = ROLE_SEGDB;
    bool incrementalBackup = true;
    void *list = "test";
 
    bool result = shouldDumpSchemaOnly(role, incrementalBackup, list);
    assert_false(result);
}
/* Tests for hash table used in cdb_dump_include.c getTableAttributes function */

void 
test__initialize_hash_table(void **state)
{   int a = 90;   
	int result = initializeHashTable(a);
    assert_true(result == 0);
    assert_true(hash_table != NULL);
    assert_true(HASH_TABLE_SIZE == 90); 
	cleanUpTable();
}

void 
test__insert_into_hash_table(void **state)
{   
    initializeHashTable(90);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
    result = getTypstorage(o);
    assert_true(result == 'x');
	cleanUpTable();
}

void 
test__insert_into_hash_table_duplicate(void **state)
{   
    initializeHashTable(100);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
	result = insertIntoHashTable(o, t);
    assert_true(result == -1);
	cleanUpTable();
}

void 
test__insert_into_hash_table_large_num_keys(void **state)
{   
    initializeHashTable(10);
    Oid o = 139753;
    int result = 0;
    int i = 0;
    for(; i < 20; i++)
    {
        result = insertIntoHashTable(o + i, 'x');    
        assert_true(result == 0);
    }	

    for(i = 0; i < 20; i++)
    {
        result = removeNode(o + i);    
        assert_true(result == 0);
    }

    for(i = 0; i < 20; i++)
    {
        char typstorage = getTypstorage(o + i);    
		assert_true(typstorage == EMPTY_TYPSTORAGE);
    }
	cleanUpTable();
}

void 
test__hash_func(void **state)
{   
    HASH_TABLE_SIZE = 1000;
    Oid o = 100;
	int result = hashFunc(o);
    assert_true(result == 100);
}

void 
test__hash_func_range(void **state)
{   
    HASH_TABLE_SIZE = 1000;
    Oid o = 1000000;
	int result = hashFunc(o);
    assert_true(result < HASH_TABLE_SIZE && result >= 0);
}

void 
test__get_typstorage(void **state)
{   
    initializeHashTable(100);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
    char typstorage = getTypstorage(o);
    assert_true(typstorage == t);
	cleanUpTable();
}

void 
test__get_typstorage_invalid_key(void **state)
{   
    initializeHashTable(100);
	char typstorage = getTypstorage(100);
    assert_true(typstorage == EMPTY_TYPSTORAGE);
	cleanUpTable();
}

void 
test__remove_node(void **state)
{   
    initializeHashTable(100);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
	result = removeNode(o);
    assert_true(result == 0);
    char typstorage = getTypstorage(o);
    assert_true(typstorage == EMPTY_TYPSTORAGE);
	cleanUpTable();
}

void 
test__remove_node_not_present(void **state)
{   
    initializeHashTable(100);
	int result = removeNode(101);
    assert_true(result == -1);
	cleanUpTable();
}

void 
test__remove_node_not_present_in_list(void **state)
{   
    initializeHashTable(10);
    Oid o = 139753;
    int i = 0;
    int result = 0;
    for(; i < 20; i++)
    {
        result = insertIntoHashTable(o + i, 'x');    
        assert_true(result == 0);
    }	
	result = removeNode(o + 21);
	assert_true(result == -1);
	cleanUpTable();
}

void 
test__clean_up_table(void **state)
{   
    initializeHashTable(100);
    Oid o = 123456;
    int i = 0;
    int result = 0;
    for(; i < 20; i++)
    {
        result = insertIntoHashTable(o + i, 'x');    
        assert_true(result == 0);
    }
    cleanUpTable();	
    assert_true(hash_table == NULL);
    assert_true(HASH_TABLE_SIZE == 0);
}

/*
 * Test the base64 implementation in cdb_dump_util.c.
 */
void
test__base64(void **state)
{
	unsigned char orig[256];
	char	   *encoded;
	unsigned char *decoded;
	int			backlen;
	int			i;

	for (i = 0; i < 256; i++)
		orig[i] = i;

	encoded = DataToBase64((char *) orig, 256);
	decoded = (unsigned char *) Base64ToData(encoded, &backlen);
	assert_int_equal(backlen, 256);

	for (i = 0; i < 256; i++)
		assert_int_equal(decoded[i], i);
}

int 
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__isFilteringAllowed1),
			unit_test(test__isFilteringAllowed2),
			unit_test(test__isFilteringAllowed3), 
			unit_test(test__isFilteringAllowed4), 
			unit_test(test__isFilteringAllowed5),
			unit_test(test__isFilteringAllowed6),
			unit_test(test__isFilteringAllowed7), 
			unit_test(test__isFilteringAllowed8), 
			unit_test(test__validateTimeStamp7),
			unit_test(test__validateTimeStamp8),
			unit_test(test__validateTimeStamp9),
			unit_test(test__validateTimeStamp10),
			unit_test(test__validateTimeStamp11),
			unit_test(test__validateTimeStamp12),
			unit_test(test__validateTimeStamp13),
			unit_test(test__validateTimeStamp14),
			unit_test(test__validateTimeStamp15),
			unit_test(test__validateTimeStamp16),
			unit_test(test__getTimeStampKey17),
			unit_test(test__getTimeStampKey18),
			unit_test(test__shouldExpandChildren1),
			unit_test(test__shouldExpandChildren2),
			unit_test(test__shouldExpandChildren3),
			unit_test(test__shouldExpandChildren4),
            unit_test(test__shouldDumpSchemaOnly1),
            unit_test(test__shouldDumpSchemaOnly2),
            unit_test(test__shouldDumpSchemaOnly3),
            //hash table tests
            unit_test(test__initialize_hash_table),       
            unit_test(test__insert_into_hash_table),       
            unit_test(test__insert_into_hash_table_duplicate),       
            unit_test(test__insert_into_hash_table_large_num_keys),       
            unit_test(test__hash_func),       
            unit_test(test__hash_func_range),       
            unit_test(test__get_typstorage),       
            unit_test(test__get_typstorage_invalid_key),       
            unit_test(test__remove_node),       
            unit_test(test__remove_node_not_present),       
            unit_test(test__remove_node_not_present_in_list),       
            unit_test(test__clean_up_table),
            unit_test(test__base64),
	};
	return run_tests(tests);
}
