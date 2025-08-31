//
//  S4CBLiteTests.c
//  S4CBLiteTests
//
//  Created by Michael Papp on 8/28/25.
//  Copyright © 2025 SeaStones Software and Michael Papp.
//


#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "S4CBLite.h"
#include "S4CBLiteTests.h"



#define CBUTIL_TEST_DEBUG

#ifdef CBUTIL_TEST_DEBUG
#define DBG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DBG(...) do { } while(0)
#endif


#ifndef EXPECT_MSG
#define EXPECT_MSG(cond, msg) do { \
	cbutil_tests_run++; \
	if (!(cond)) { \
		cbutil_tests_failed++; \
		fprintf(stderr, "[FAIL] %s:%d: %s -- %s\n", __FILE__, __LINE__, #cond, msg); \
	} \
} while(0)
#endif

// Pull in CBL/Fleece headers for deeper checks & collections API.
// Adjust include paths to your environment as needed.
#if __has_include(<cbl/CBLCollection.h>)
  #include <cbl/CBLCollection.h>
  #include <cbl/CBLDatabase.h>
  #include <cbl/CBLDocument.h>
  #include <cbl/CBLBlob.h>
  #include <fleece/Fleece.h>
#else
  #include "CBLCollection.h"
  #include "CBLDatabase.h"
  #include "CBLDocument.h"
  #include "CBLBlob.h"
  #include "Fleece.h"
#endif

int cbutil_tests_run = 0;
int cbutil_tests_failed = 0;

// ------------------ Local helpers (DRY) ------------------
static void cbutil_make_dir(const char* path) {
#if defined(_WIN32)
	_mkdir(path);
#else
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
	(void)system(cmd);
#endif
}

void cbutil_rm_rf(const char* path) {
#if defined(_WIN32)
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "rmdir /S /Q \"%s\"", path);
	(void)system(cmd);
#else
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
	(void)system(cmd);
#endif
}

static void cbutil_unique_name(char* out, size_t n, const char* prefix) {
	unsigned r = (unsigned)time(NULL) ^ (unsigned)clock();
	snprintf(out, n, "%s_%u", prefix, r);
}

struct CBLU_Db* cbutil_open_fresh_db(const char* baseDir, char* outName, size_t n) {
	cbutil_make_dir(baseDir);
	cbutil_unique_name(outName, n, "cblutil_testdb");
	struct CBLU_Db* db = NULL;
	bool ok = cblu_open(outName, baseDir, &db);
	EXPECT(ok && db);
	return db;
}

void cbutil_close_db(struct CBLU_Db* db, const char* baseDir) {
	(void)baseDir; // keep artifacts unless you want to clean
	cblu_close(db);
}

// Fetch raw props for deep checks (Fleece)
static FLDict cbutil_get_doc_props(struct CBLU_Db* db, const char* docID) {
	CBLU_Session* s = cblu_session_begin(db);
	CBLError err = (CBLError){0};
	// Access bound collection from the opaque handle by mirroring
	// the internal layout (DB + coll). This is test-only introspection.
	typedef struct { CBLDatabase* db; CBLCollection* coll; } _Core;
	struct _OpaqueDb { _Core core; }; // matches cblutil.c
	const struct _OpaqueDb* internal = (const struct _OpaqueDb*)db;
	const CBLDocument* doc =
		CBLCollection_GetDocument(internal->core.coll, (FLString){docID, strlen(docID)}, &err);
	FLDict props = NULL;
	if (doc) { props = CBLDocument_Properties(doc); CBLDocument_Release(doc); }
	cblu_session_end(s);
	return props;
}

// ------------------ Tests ------------------

TEST(test_basic_roundtrip) {
	char name[128];
	CBLU_Db* db = cbutil_open_fresh_db("./testdb", name, sizeof name);

	// write
	{
		CBLU_Session* s = cblu_session_begin(db);
		CBLU_DocW* d = cblu_docw_begin(s, "doc1");
		cblu_docw_set_i64(d, "answer", 42);
		cblu_docw_set_f64(d, "pi", 3.14159);
		cblu_docw_set_str(d,  "greet", "hello");
		EXPECT(cblu_docw_save(d));
		cblu_session_end(s);
	}

	// read (new handle, cold open)
	CBLU_Db* db2 = NULL;
	EXPECT(cblu_open(name, "./testdb", &db2));

	{
		CBLU_Session* s = cblu_session_begin(db2);
		CBLU_DocR* r = cblu_docr_get(s, "doc1");
		EXPECT(r != NULL);
		int64_t i=0; double d=0.0; char buf[16];
		EXPECT(cblu_docr_get_i64(r, "answer", &i));
		EXPECT_EQ_I64(i, 42);
		EXPECT(cblu_docr_get_f64(r, "pi", &d));
		EXPECT_EQ_DBL(d, 3.14159);
		EXPECT(cblu_docr_get_str(r, "greet", buf, sizeof buf) > 0);
		EXPECT(strcmp(buf, "hello") == 0);
		cblu_docr_free(r);
		cblu_session_end(s);
	}

	cbutil_close_db(db2, "./testdb");
	cbutil_close_db(db,  "./testdb");
}

TEST(test_arrays_and_bools) {
	char name[128];
	CBLU_Db* db = cbutil_open_fresh_db("./testdb", name, sizeof name);

	double arrd[5]  = {0.5, 1.0, 1.5, 2.0, 2.5};
	int64_t arri[4] = {1,2,3,4};

	CBLU_Session* s = cblu_session_begin(db);
	CBLU_DocW* d = cblu_docw_begin(s, "doc2");
	cblu_docw_set_f64_array(d, "arrd", arrd, 5);
	cblu_docw_set_i64_array(d, "arri", arri, 4);
	cblu_docw_set_bool(d, "flag", true);
	EXPECT(cblu_docw_save(d));
	cblu_session_end(s);

	// read back
	s = cblu_session_begin(db);
	CBLU_DocR* r = cblu_docr_get(s, "doc2");
	EXPECT(r != NULL);
	double outd[5] = {0}; int64_t outi[4] = {0}; bool flag=false;
	EXPECT(cblu_docr_get_f64_array(r, "arrd", outd, 5) == 5);
	for (int i=0;i<5;i++) EXPECT_EQ_DBL(outd[i], arrd[i]);
	EXPECT(cblu_docr_get_i64_array(r, "arri", outi, 4) == 4);
	for (int i=0;i<4;i++) EXPECT_EQ_I64(outi[i], arri[i]);
	EXPECT(cblu_docr_get_bool(r, "flag", &flag) && flag);
	cblu_docr_free(r);
	cblu_session_end(s);

	cbutil_close_db(db, "./testdb");
}

TEST(test_blob) {
	char name[128];
	CBLU_Db* db = cbutil_open_fresh_db("./testdb", name, sizeof name);

	const char payload[] = "ABC\x00" "DEF"; // contains a NUL
	CBLU_Session* s = cblu_session_begin(db);
	CBLU_DocW* d = cblu_docw_begin(s, "doc_blob");
	EXPECT(cblu_docw_set_blob(d, "bin", payload, sizeof(payload), "application/octet-stream"));
	EXPECT(cblu_docw_save(d));
	cblu_session_end(s);

	// Debug: inspect stored type
	FLDict propsDbg = cbutil_get_doc_props(db, "doc_blob");
	if (propsDbg) {
		FLValue v = FLDict_Get(propsDbg, FLSTR("bin"));
		DBG("[blob] type=%d (0=null 1=bool 2=number 3=str 4=data 5=array 6=dict)\n", (int)FLValue_GetType(v));
	}

	s = cblu_session_begin(db);
	CBLU_DocR* r = cblu_docr_get(s, "doc_blob");
	EXPECT(r != NULL);
	char ct[64]; unsigned char buf[32];
	size_t n = cblu_docr_get_blob(r, "bin", buf, sizeof buf, ct, sizeof ct);
	DBG("[blob] read size=%zu ct='%s'\n", n, ct);
	EXPECT_MSG(n == sizeof(payload), "blob content length mismatch");
	EXPECT_MSG(memcmp(buf, payload, n) == 0, "blob content bytes differ");
	cblu_docr_free(r);
	cblu_session_end(s);

	cbutil_close_db(db, "./testdb");
}

TEST(test_nested_dict_array) {
	char name[128];
	CBLU_Db* db = cbutil_open_fresh_db("./testdb", name, sizeof name);

	// write nested
	CBLU_Session* s = cblu_session_begin(db);
	CBLU_DocW* d = cblu_docw_begin(s, "doc_nested");
	FLMutableDict sub = cblu_docw_begin_dict(d, "sub");
	cblu_docw_dict_set_i64(sub,  "i",  7);
	cblu_docw_dict_set_f64(sub,  "x",  2.5);
	cblu_docw_dict_set_bool(sub, "ok", true);
	cblu_docw_end_dict(d, sub);

	FLMutableArray arr = cblu_docw_begin_array(d, "nums");
	cblu_docw_array_append_i64(arr, 1);
	cblu_docw_array_append_i64(arr, 2);
	cblu_docw_array_append_i64(arr, 3);
	cblu_docw_end_array(d, arr);
	EXPECT(cblu_docw_save(d));
	cblu_session_end(s);

	// Deep check via Fleece
	FLDict props = cbutil_get_doc_props(db, "doc_nested");
	EXPECT(props != NULL);
	FLValue vsub = FLDict_Get(props, FLSTR("sub"));
	FLValue varr = FLDict_Get(props, FLSTR("nums"));
	DBG("[nested] sub.type=%d nums.type=%d\n", (int)FLValue_GetType(vsub), (int)FLValue_GetType(varr));
	EXPECT(FLValue_GetType(vsub) == kFLDict);
	FLDict dsub = FLValue_AsDict(vsub);
	EXPECT((int)FLValue_AsInt(FLDict_Get(dsub, FLSTR("i"))) == 7);
	EXPECT(fabs(FLValue_AsDouble(FLDict_Get(dsub, FLSTR("x"))) - 2.5) <= CBUTIL_TOL);
	EXPECT(FLValue_AsBool(FLDict_Get(dsub, FLSTR("ok"))) == true);

	EXPECT(FLValue_GetType(varr) == kFLArray);
	FLArray a = FLValue_AsArray(varr);
	DBG("[nested] nums.count=%u\n", (unsigned)FLArray_Count(a));
	EXPECT(FLArray_Count(a) == 3);
	EXPECT((int)FLValue_AsInt(FLArray_Get(a, 0)) == 1);
	EXPECT((int)FLValue_AsInt(FLArray_Get(a, 1)) == 2);
	EXPECT((int)FLValue_AsInt(FLArray_Get(a, 2)) == 3);

	cbutil_close_db(db, "./testdb");
}

TEST(test_transactions_commit_and_rollback) {
	char name[128];
	CBLU_Db* db = cbutil_open_fresh_db("./testdb", name, sizeof name);

	// Rollback path
	CBLU_Session* s = cblu_session_begin_txn(db, true);
	CBLU_DocW* d1 = cblu_docw_begin(s, "txn_a");
	cblu_docw_set_i64(d1, "v", 1);
	EXPECT(cblu_docw_save(d1));
	CBLU_DocW* d2 = cblu_docw_begin(s, "txn_b");
	cblu_docw_set_i64(d2, "v", 2);
	EXPECT(cblu_docw_save(d2));
	cblu_session_end_txn(s, false); // rollback

	// Verify absent
	s = cblu_session_begin(db);
	EXPECT(cblu_docr_get(s, "txn_a") == NULL);
	EXPECT(cblu_docr_get(s, "txn_b") == NULL);
	cblu_session_end(s);

	// Commit path
	s = cblu_session_begin_txn(db, true);
	d1 = cblu_docw_begin(s, "txn_c");
	cblu_docw_set_i64(d1, "v", 3);
	EXPECT(cblu_docw_save(d1));
	d2 = cblu_docw_begin(s, "txn_d");
	cblu_docw_set_i64(d2, "v", 4);
	EXPECT(cblu_docw_save(d2));
	cblu_session_end_txn(s, true); // commit

	s = cblu_session_begin(db);
	CBLU_DocR* r = cblu_docr_get(s, "txn_c");
	EXPECT(r != NULL);
	int64_t v = 0;
	EXPECT(cblu_docr_get_i64(r, "v", &v) && v == 3);
	cblu_docr_free(r);

	r = cblu_docr_get(s, "txn_d");
	EXPECT(r != NULL);
	EXPECT(cblu_docr_get_i64(r, "v", &v) && v == 4);
	cblu_docr_free(r);
	cblu_session_end(s);

	cbutil_close_db(db, "./testdb");
}

TEST(test_collections_if_available) {
	char name[128];
	CBLU_Db* db = cbutil_open_fresh_db("./testdb", name, sizeof name);

	// Try to create scope/collection via CBL API
	bool coll_ready = cblu_create_collection(db, "tx", "radio");
	if (coll_ready)
	{
		CBLU_Db* txh = NULL;
		if (cblu_open_collection(db, "radio", "tx", &txh)) {
			CBLU_Session* s = cblu_session_begin(txh);
			CBLU_DocW* d = cblu_docw_begin(s, "coll_doc");
			cblu_docw_set_str(d, "where", "radio.tx");
			EXPECT(cblu_docw_save(d));
			cblu_session_end(s);

			s = cblu_session_begin(txh);
			CBLU_DocR* r = cblu_docr_get(s, "coll_doc");
			EXPECT(r != NULL);
			char buf[32];
			EXPECT(cblu_docr_get_str(r, "where", buf, sizeof buf) > 0);
			EXPECT(strcmp(buf, "radio.tx") == 0);
			cblu_docr_free(r);
			cblu_session_end(s);

			cblu_close(txh);
		}
		else
		{
			EXPECT(0 && "cblu_open_collection failed unexpectedly");
		}
	}
	else
	{
		printf("[ERROR] could not create the collection \n");
	}
	cbutil_close_db(db, "./testdb");
}

// ---- Per-test exported wrappers ----
// Each wrapper runs exactly one TEST() function and returns failures delta.

static int _run_one(void (*fn)(void)) {
	extern int cbutil_tests_failed;
	int before = cbutil_tests_failed;
	fn();
	return cbutil_tests_failed - before;
}

int cbutil_test_basic_roundtrip(void) { return _run_one(test_basic_roundtrip); }
int cbutil_test_arrays_and_bools(void) { return _run_one(test_arrays_and_bools); }
int cbutil_test_blob(void) { return _run_one(test_blob); }
int cbutil_test_nested_dict_array(void) { return _run_one(test_nested_dict_array); }
int cbutil_test_transactions_commit_and_rollback(void) { return _run_one(test_transactions_commit_and_rollback); }
int cbutil_test_collections_if_available(void) { return _run_one(test_collections_if_available); }

// ------------------ Runner ------------------

int run_all_cbutil_tests(void) {
	// clean test dir first run
	cbutil_rm_rf("./testdb");

	test_basic_roundtrip();
	test_arrays_and_bools();
	test_blob();
	test_nested_dict_array();
	test_transactions_commit_and_rollback();
	test_collections_if_available();

	printf("\nTests run: %d  failed: %d\n", cbutil_tests_run, cbutil_tests_failed);
	return cbutil_tests_failed;
}

#ifdef CBUTIL_TEST_WITH_MAIN
int main(void) {
	return run_all_cbutil_tests() ? 1 : 0;
}
#endif
