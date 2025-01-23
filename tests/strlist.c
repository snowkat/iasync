#include "unity.h"

#include "strlist.h"

#include <stdlib.h>
#include <string.h>

void
setUp(void)
{
}

void
tearDown(void)
{
}

void
test_strlist_init(void)
{
    strlist_t list = strlist_new(NULL, 512);
    TEST_ASSERT_NOT_NULL(list);

    TEST_ASSERT_EQUAL(list->capacity, 8);
    TEST_ASSERT_EQUAL(list->len, 0);

    strlist_free(list);
}

void
test_strlist_pushpop(void)
{
    strlist_t list = strlist_new(NULL, 0);
    TEST_ASSERT_NOT_NULL(list);

    for (int i = 1; i < 51; i++) {
        char* str = calloc(16, sizeof(char));
        TEST_ASSERT_NOT_NULL_MESSAGE(str,
                                     "memory error -- likely not a test fail");
        snprintf(str, 16, "hello%d", i);
        TEST_ASSERT_EQUAL(i, strlist_push(list, str));
    }

    for (int i = 50; i > 0; i--) {
        char* cmp = calloc(16, sizeof(char));
        TEST_ASSERT_NOT_NULL_MESSAGE(cmp,
                                     "memory error -- likely not a test fail");
        snprintf(cmp, 16, "hello%d", i);
        char* str = strlist_pop(list);
        TEST_ASSERT_NOT_NULL(str);
        TEST_ASSERT_EQUAL_STRING(cmp, str);
        free(str);
        free(cmp);
    }

    strlist_free(list);
}

void
test_strlist_cat(void)
{
    strlist_t list = strlist_new(NULL, 0);
    TEST_ASSERT_NOT_NULL(list);
    strlist_t list2 = strlist_new(NULL, 0);
    TEST_ASSERT_NOT_NULL(list2);

    for (int i = 1; i < 20; i++) {
        char* str = calloc(16, sizeof(char));
        TEST_ASSERT_NOT_NULL_MESSAGE(str,
                                     "memory error -- likely not a test fail");
        snprintf(str, 16, "hello%d", i);
        strlist_push(list, str);
    }

    for (int i = 20; i < 41; i++) {
        char* str = calloc(16, sizeof(char));
        TEST_ASSERT_NOT_NULL_MESSAGE(str,
                                     "memory error -- likely not a test fail");
        snprintf(str, 16, "hello%d", i);
        strlist_push(list2, str);
    }

    TEST_ASSERT_EQUAL(strlist_cat(list, list2), 40);

    for (int i = 40; i > 0; i--) {
        char* cmp = calloc(16, sizeof(char));
        TEST_ASSERT_NOT_NULL_MESSAGE(cmp,
                                     "memory error -- likely not a test fail");
        snprintf(cmp, 16, "hello%d", i);
        char* str = strlist_pop(list);
        TEST_ASSERT_NOT_NULL(str);
        TEST_ASSERT_EQUAL_STRING(cmp, str);
    }

    strlist_free(list);
}

void
test_strlist_claim(void)
{
    char* my_vals[] = { "hello", "world", "hi", NULL };
    strlist_t list = strlist_new(my_vals, -1);
    char* val = strlist_claim(list, 1);

    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("world", val);
    TEST_ASSERT_NULL(list->begin[1]);

    free(list);
}