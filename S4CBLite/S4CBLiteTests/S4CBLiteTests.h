//
//  S4CBLiteTests.h
//  S4CBLiteTests
//
//  Created by Michael Papp on 8/28/25.
//  Copyright © 2025 SeaStones Software and Michael Papp.
//


#ifndef S4CBLiteTests_h
#define S4CBLiteTests_h

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Public entry to run everything; returns number of failures.
int run_all_cbutil_tests(void);

// ---- Tiny assertion framework ----
extern int cbutil_tests_run;
extern int cbutil_tests_failed;

#define CBUTIL_TOL 1e-9

#define TEST(name) void name(void)

#define EXPECT(cond) do { \
	cbutil_tests_run++; \
	if (!(cond)) { \
		cbutil_tests_failed++; \
		fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
	} \
} while(0)

#define EXPECT_EQ_I64(a,b) EXPECT((long long)(a) == (long long)(b))
#define EXPECT_EQ_U64(a,b) EXPECT((unsigned long long)(a) == (unsigned long long)(b))
#define EXPECT_EQ_DBL(a,b) EXPECT(fabs((double)(a) - (double)(b)) <= CBUTIL_TOL)
#define EXPECT_STREQ(a,b)  EXPECT(((a) && (b)) ? strcmp((a),(b))==0 : (a)==(b))

// ---- DRY helpers (decls) ----
struct CBLU_Db;   // opaque from cblutil.h

// Create ./testdb if needed, open a fresh database with a unique name.
// Returns handle; also fills outName with the chosen DB name.
struct CBLU_Db* cbutil_open_fresh_db(const char* baseDir, char* outName, size_t n);

// Close DB handle. (We keep on-disk files by default for post-mortem.)
void cbutil_close_db(struct CBLU_Db* db, const char* baseDir);

// Remove a path recursively (used by main() to clean test dir before run)
void cbutil_rm_rf(const char* path);


// Expose one function per test; each returns the number of failures (0 = pass).
int cbutil_test_basic_roundtrip(void);
int cbutil_test_arrays_and_bools(void);
int cbutil_test_blob(void);
int cbutil_test_nested_dict_array(void);
int cbutil_test_transactions_commit_and_rollback(void);
int cbutil_test_collections_if_available(void);


// Optional `main` for command-line builds.
// In Xcode, if your test target is a console executable, keep this;
// if you’re using an XCTest bundle, you can ignore this and call
// run_all_cbutil_tests() from an ObjC XCTest case instead.
#ifdef CBUTIL_TEST_WITH_MAIN
int main(void);
#endif
#endif /* cbutil_test_h */
