/*
 * Copywrite (c) 2022 ThoughtPlot, LLC.
 */
#include <stdlib.h>
#include <check.h>
#include "../src/store.h"

store *st;
void setup(){
	st = store_open("_name");
}

void teardown() {
	store_close(st);
}

START_TEST (open_close) {
	if (!st) {
		fail();
	}
}
END_TEST

START_TEST (aquire_generation) {
	int err = store_lock_generation(st);
	if (err) {
		ck_abort_msg("unable to find newest generation");
	}

	err = store_unlock_generation(st);
	if (err) {
		ck_abort_msg("unable to unlock newest generation");
	}
}
END_TEST

START_TEST (write_data) {
	char *buf = malloc(sizeof(char)*100);
 	int err = store_read(st, (void*)buf, 0, 5);
	if (!err) {
		ck_abort_msg("should not read without a generation");
	}

 	err = store_read(st, (void*)buf, 0, 100);
	if (!err) {
		ck_abort_msg("cant read the header of an empty file");
	}

	err = store_lock_generation(st);
	if (err) {
		ck_abort_msg("unable to find newest generation");
	}
/*
	int len = store_length(st);
	if (len != 5) {
		ck_abort_msg("wrong size");
	}
*/
	err = store_lock(st);
	if (err) {
		ck_abort_msg("unable to aquire lock");
	}

	if (!store_is_locked(st)) {
		ck_abort_msg("lock is not held");
	}

	err = store_write(st,(void*)"asdf",0,5);
	if (err) {
		ck_abort_msg("unable to write");
	}

	err = store_read(st, (void*)buf, 0, 5);
	if (err) {
		ck_abort_msg("unable to read data");
	}
	
	ck_assert_str_eq(buf,"asdf");
	buf[0]= 0;	
	err = store_read(st, (void*)buf, 0, 55);
	if (err != STORE_SHORT_READ) {
		ck_abort_msg("short read did not work");
	}
	ck_assert_str_eq(buf,"asdf");

	err = store_promote_generation(st);
	if (err) {
		ck_abort_msg("unable to promote generation");
	}

	err = store_unlock(st);
	if (err) {
		ck_abort_msg("unable to release lock");
	}
	
	if (store_is_locked(st)) {
		ck_abort_msg("lock is still held");
	}

	err = store_read(st, (void*)buf, 0, 5);
	if (err) {
		ck_abort_msg("unable to read data");
	}

	ck_assert_str_eq(buf,"asdf");

	err = store_unlock_generation(st);
	if (err) {
		ck_abort_msg("unable to unlock newest generation");
	}
}
END_TEST

Suite *store_suite(void) {
	Suite *s;
	TCase *tc_core;
	s = suite_create("Store");

	tc_core = tcase_create("commands");
	tcase_add_checked_fixture(tc_core, setup, teardown);
	tcase_add_test(tc_core, open_close);
	tcase_add_test(tc_core, aquire_generation);
	tcase_add_test(tc_core, write_data);

	suite_add_tcase(s, tc_core);
	return s;
}

int main(void) {
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = store_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed != 0);
  }

