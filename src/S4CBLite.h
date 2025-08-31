//
//  S4CBLite.h
//  S4CBLite
//
//  Copyright © 2025 SeaStones Software and Michael Papp.
//
//  This header is synchronized with S4CBLite.c and declares:
//   - DB lifecycle + collection selection
//   - Sessions with optional transactions
//   - Write API (scalars, arrays, bool, blob, nested dict/array)
//   - Read API (scalars, arrays, strings, bool, blob)
//


#ifndef S4CBLite_h
#define S4CBLite_h
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif


// Opaque handles — callers don't need Couchbase Database/Collection headers
typedef struct CBLU_Db      CBLU_Db;      // holds db + a bound collection
typedef struct CBLU_Session CBLU_Session; // lightweight context (optionally transactional)
typedef struct CBLU_DocW    CBLU_DocW;    // writeable document builder
typedef struct CBLU_DocR    CBLU_DocR;    // readable document view

// ---- Database lifecycle ----
// Opens (or creates) a database in `dir` and binds to the default collection.
// Returns false on failure; on success, *out_db owns the handle and must be closed with cblu_close.
bool cblu_open(const char* db_name, const char* dir, CBLU_Db** out_db);
void cblu_close(CBLU_Db* db);

// ---- Collections (scopes/collections) ----
// Creates a lightweight handle bound to `scope.collection` within the same database.
// The returned handle shares the underlying CBLDatabase; close it with cblu_close() when done.
bool cblu_create_collection(CBLU_Db* base, const char* collectionName, const char* scopeName);
bool cblu_open_collection(CBLU_Db* base, const char* scope, const char* collection, CBLU_Db** out_handle);

// ---- Session (optional transaction boundary) ----
// Non-transactional convenience wrappers
CBLU_Session* cblu_session_begin(CBLU_Db* db);
void          cblu_session_end  (CBLU_Session* s);

// Transactional session API
CBLU_Session* cblu_session_begin_txn(CBLU_Db* db, bool use_txn);
void          cblu_session_end_txn  (CBLU_Session* s, bool commit);

// ---- Write document API ----
CBLU_DocW* cblu_docw_begin(CBLU_Session* s, const char* doc_id); // create/overwrite by id

// Scalar setters
void cblu_docw_set_i64 (CBLU_DocW* d, const char* key, int64_t v);
void cblu_docw_set_u64 (CBLU_DocW* d, const char* key, uint64_t v);
void cblu_docw_set_f64 (CBLU_DocW* d, const char* key, double v);
void cblu_docw_set_bool(CBLU_DocW* d, const char* key, bool v);
void cblu_docw_set_str (CBLU_DocW* d, const char* key, const char* s); // UTF-8

// Array setters
void cblu_docw_set_f64_array(CBLU_DocW* d, const char* key, const double*  a, size_t n);
void cblu_docw_set_i64_array(CBLU_DocW* d, const char* key, const int64_t* a, size_t n);

// Blob setter — stores an inline blob value in the doc body.
// `contentType` may be NULL (defaults to "application/octet-stream").
bool cblu_docw_set_blob(CBLU_DocW* d, const char* key, const void* data, size_t size, const char* contentType);

// Nested dict helpers
FLMutableDict  cblu_docw_begin_dict(CBLU_DocW* d, const char* key);
void           cblu_docw_dict_set_i64 (FLMutableDict dict, const char* key, int64_t v);
void           cblu_docw_dict_set_u64 (FLMutableDict dict, const char* key, uint64_t v);
void           cblu_docw_dict_set_f64 (FLMutableDict dict, const char* key, double v);
void           cblu_docw_dict_set_bool(FLMutableDict dict, const char* key, bool v);
void           cblu_docw_dict_set_str (FLMutableDict dict, const char* key, const char* s);
void           cblu_docw_end_dict     (CBLU_DocW* d, FLMutableDict dict);

// Nested array helpers
FLMutableArray cblu_docw_begin_array(CBLU_DocW* d, const char* key);
void           cblu_docw_array_append_i64 (FLMutableArray arr, int64_t v);
void           cblu_docw_array_append_u64 (FLMutableArray arr, uint64_t v);
void           cblu_docw_array_append_f64 (FLMutableArray arr, double v);
void           cblu_docw_array_append_bool(FLMutableArray arr, bool v);
void           cblu_docw_array_append_str (FLMutableArray arr, const char* s);
void           cblu_docw_end_array        (CBLU_DocW* d, FLMutableArray arr);

// Finalize write
bool cblu_docw_save(CBLU_DocW* d);   // commits and frees the builder on success
void cblu_docw_free(CBLU_DocW* d);   // frees without saving (safe to call anytime)

// ---- Read document API ----
CBLU_DocR* cblu_docr_get(CBLU_Session* s, const char* doc_id); // NULL if missing
bool       cblu_docr_has(CBLU_DocR* d, const char* key);

// Strict typed getters — return true if present & of a compatible type
bool   cblu_docr_get_i64 (CBLU_DocR* d, const char* key, int64_t*  out);
bool   cblu_docr_get_u64 (CBLU_DocR* d, const char* key, uint64_t* out);
bool   cblu_docr_get_f64 (CBLU_DocR* d, const char* key, double*   out);
bool   cblu_docr_get_bool(CBLU_DocR* d, const char* key, bool*     out);

// String getter — copies at most dst_size-1 bytes and NUL-terminates; returns bytes written
size_t cblu_docr_get_str(CBLU_DocR* d, const char* key, char* dst, size_t dst_size);

// Array getters — return number of items copied (<= maxn). Missing/non-array → 0.
size_t cblu_docr_get_f64_array(CBLU_DocR* d, const char* key, double*  out, size_t maxn);
size_t cblu_docr_get_i64_array(CBLU_DocR* d, const char* key, int64_t* out, size_t maxn);

// Blob getter — copies blob bytes into `dst` (up to dstSize). Returns bytes copied.
// If contentTypeDst != NULL and ctDstSize>0, copies the content-type string (NUL-terminated).
size_t cblu_docr_get_blob(CBLU_DocR* d, const char* key, void* dst, size_t dstSize,
						  char* contentTypeDst, size_t ctDstSize);

void cblu_docr_free(CBLU_DocR* d);

#ifdef __cplusplus
}
#endif

#endif /* cblutil_h */
