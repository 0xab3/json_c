#ifndef __JSON_H
#define __JSON_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/cdefs.h>
#include <unistd.h>

#ifndef opaque_ptr_t
#define opaque_ptr_t void *
#endif

#ifndef J_UNUSED
#define J_UNUSED(var) (void)var
#endif

#define J_MALLOC(ctx, ...) malloc(__VA_ARGS__)
#define J_FREE(ctx, ...) free(__VA_ARGS__)
#define J_REALLOC(ctx, ...) realloc(__VA_ARGS__)
#define next(iter) iter++
#define JSON_ELEMENT_NUMBER(value)                                             \
  &(jsonElement) { .kind = JSON_KIND_NUMBER, ._int = value }
#define JSON_ELEMENT_STRING(value)                                             \
  &(jsonElement) { .kind = JSON_KIND_STRING, ._string = value }
#define JSON_ELEMENT_OBJEKT(value)                                             \
  &(jsonElement) { .kind = JSON_KIND_OBJEKT, ._obj = value }

typedef size_t hash_t;
typedef bool jbool_t;

// todo(shahzad): json supports int of 256 bytes i wanna kms
typedef double json_number;
typedef hash_t (*hash_func_t)(const char *key, void *user_data);

typedef enum jsonParserErrorKind {
  JSON_OK,
  JSON_INVALID,
  JSON_INVALID_ARRAY,
  JSON_INVALID_NUMBER,
  JSON_INVALID_STRING,
  JSON_NO_SEPERATOR,
  JSON_EXHAUSTED,
  JSON_INTERNAL_FAILURE, // when internal function fails
  JSON_INTERNAL_OUT_OF_MEMORY,
} jsonParserErrorKind;

typedef enum {
  JSON_STATUS_OK,
  JSON_ERROR_OUT_OF_MEMORY = JSON_INTERNAL_OUT_OF_MEMORY,
} jsonStatus;

typedef struct {
  uint32_t column;
  uint32_t row;
  jsonParserErrorKind kind;
} jsonParserStatus;

struct jsonHash {

  hash_func_t hash_func;
  opaque_ptr_t hash_func_userdata;
};

struct jsonObject {
  struct jsonKeyValuePair *entries;
  size_t n_entries;
  size_t capacity;
  struct jsonHash hasher;
  size_t load_factor;
};

struct jsonArray {
  struct jsonElement *elems;
  size_t len;
  size_t capacity;
};

typedef enum _jsonElementKind {
  JSON_KIND_NUMBER,
  JSON_KIND_STRING,
  JSON_KIND_OBJEKT,
  JSON_KIND_ARRAY,
} jsonElementKind;

struct jsonElement {
  union {
    char *string;
    json_number number; // TODO: make this integer shit big
    struct jsonObject *object;
    struct jsonArray *array;
  } as;
  jsonElementKind kind;
  char padding[4];
};

struct jsonKeyValuePair {
  const char *key;
  hash_t hash;
  struct jsonElement element;
};

struct jsonObjectIter {
  struct jsonKeyValuePair *first;
  struct jsonKeyValuePair *current;
  struct jsonKeyValuePair *last;
};

typedef struct jsonObject jsonObject;
typedef struct jsonArray jsonArray;
typedef struct jsonObjectIter jsonObjectIter;
typedef struct jsonKeyValuePair jsonKeyValuePair;
typedef struct jsonElement jsonElement;
typedef struct jsonHash jsonHash;

jsonStatus json_object_new(jsonObject *obj, size_t init_cap, size_t load_factor,
                           hash_func_t hash_func,
                           opaque_ptr_t hash_func_userdata,
                           opaque_ptr_t allocator_ctx) __nonnull((1));
jsonKeyValuePair *json_object_get(jsonObject *object, const char *key) __nonnull((1,2));
jsonStatus json_object_append_key_value_pair(jsonObject *object,
                                             const jsonKeyValuePair *kv_pair,
                                             opaque_ptr_t allocator_ctx)
    __nonnull((1, 2));
jsonStatus json_object_resize(jsonObject *obj, size_t new_capacity,
                              opaque_ptr_t allocator_ctx) __nonnull((1));

bool json_object_is_resize_required(jsonObject *object) __nonnull((1));
void json_object_free(jsonObject *object) __nonnull((1));
jsonObjectIter json_object_iter_new(const jsonObject *obj) __nonnull((1));
const jsonKeyValuePair *json_object_iter_next(jsonObjectIter *iter)
    __nonnull((1));

jsonParserStatus json_parse_value(jsonElement *json_elem,
                                  const jsonHash *hash_vtable, char *str_json,
                                  size_t *json_idx, size_t json_len,
                                  opaque_ptr_t allocator_ctx);
jsonParserStatus json_parse_array(jsonArray *array, char *str_json,
                                  size_t *json_idx, size_t json_len,
                                  const jsonHash *hash_vtable,
                                  opaque_ptr_t allocator_ctx);
jsonParserStatus json_parse_object(jsonObject *object, char *str_json,
                                   size_t *json_idx, size_t json_len,
                                   opaque_ptr_t allocator_ctx);
jsonParserStatus json_from_string(jsonObject *object, char *json_start,
                                  size_t json_len, opaque_ptr_t allocator_ctx)
    __nonnull((1,2));
jsonStatus json_key_value_pair_new(jsonKeyValuePair *pair, char *key,
                                   jsonElement *value,
                                   opaque_ptr_t allocator_ctx)
    __nonnull((1, 2, 3));

jsonStatus json_array_new(struct jsonArray *array, size_t capacity,
                          opaque_ptr_t allocator_ctx) __nonnull((1));

jsonStatus json_array_append(struct jsonArray *array, struct jsonElement elem,
                             opaque_ptr_t allocator_ctx) __nonnull((1));
jsonElement * json_array_get(struct jsonArray *array, size_t idx) __nonnull((1));
jsonStatus json_array_resize(struct jsonArray *array, size_t new_capacity,
                             opaque_ptr_t allocator_ctx) __nonnull((1));
jsonStatus json_array_remove_swap(struct jsonArray *array, size_t idx)
    __nonnull((1));
jsonStatus json_array_remove_ordered(struct jsonArray *array, size_t idx)
    __nonnull((1));
jsonStatus json_array_free(struct jsonArray *array) __nonnull((1));

jsonStatus json_element_free(jsonElement *element) __nonnull((1));

// jsonObject *json_object_clone(jsonObject *j_obj);
#endif
