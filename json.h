#ifndef __JSON_H
#define __JSON_H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define next(iter) iter++
#define json_parse_key json_parse_string
#define JSON_ELEMENT_NUMBER(value)  &(jsonElement){.kind =  JSON_KIND_NUMBER, ._int = value}
#define JSON_ELEMENT_STRING(value)  &(jsonElement){.kind = JSON_KIND_STRING, ._string = value}
#define JSON_ELEMENT_OBJEKT(value)  &(jsonElement){.kind = JSON_KIND_OBJEKT, ._obj = value}
#define JSON_STATUS_OK 0
#define JSON_STATUS_ERROR 1

//not to be confused with jsonParserStatus
typedef int jsonStatus;
typedef int64_t hash_t;
typedef hash_t (*hash_func_t)(char *key, void *user_data);
typedef struct jsonElement jsonElement;
typedef struct jsonKeyValuePair jsonKeyValuePair;
typedef struct jsonObject jsonObject;
struct jsonObject {
  // jsonKeyValuePair **list; /* a dynlist that contains pointer in a list
  // kindof way ig*/
  jsonKeyValuePair *entries;
  uint32_t len;
  uint32_t capacity;
  float load_factor;
  hash_func_t hash_func;
};

typedef enum jsonParserErrorKind {
  JSON_OK,
  JSON_INVALID,
  JSON_INVALID_NUMBER,
  JSON_INVALID_STRING,
  JSON_NO_SEPERATOR,
  JSON_EXHAUSTED,
} jsonParserErrorKind;

typedef struct _jsonStatus {
  jsonParserErrorKind kind;
  char *place;
} jsonParserStatus;

typedef enum _jsonElementKind {
  JSON_KIND_NUMBER,
  JSON_KIND_STRING,
  JSON_KIND_OBJEKT,
} jsonElementKind;

struct jsonElement {
  jsonElementKind kind;
  union {
    char *_string;
    int64_t _int;
    jsonObject* _obj;
  };
};
typedef struct jsonKeyValuePair {
  char *key;
  hash_t hash;  //store the hash cause we are using open addressing
  jsonElement element;
  jsonKeyValuePair *next;
} jsonKeyValuePair;

typedef struct jsonObjectIter {
  jsonKeyValuePair *current;
  jsonKeyValuePair *next;
  jsonKeyValuePair *last;
}jsonObjectIter;

//well this is basically a hashmap i guess so idk

char *ltrim(char *str);
char *rtrim(char *str, char *end);
jsonObject *json_object_new(int init_cap, float load_factor, hash_func_t hash_func);
jsonParserStatus json_parse_object(char **json_start, char *json_end,
                            jsonObject *object);
jsonParserStatus json_parse_string(char **json_start, char *json_end, char **string);
jsonParserStatus parse_key(char **str_json, int json_len, char **key);
jsonParserStatus json_from_string(char *str_json, int json_len, jsonObject *object);
jsonKeyValuePair *json_key_value_pair_new(char *key, jsonElement *value);
jsonObjectIter json_object_iter_new(jsonObject *obj);
const jsonKeyValuePair *json_object_iter_next(jsonObjectIter *iter);
jsonObject *json_object_clone(jsonObject *j_obj);
jsonStatus json_object_append_key_value_pair(jsonObject *object,
                                            jsonKeyValuePair *kv_pair,
                                            void *user_data);
#endif
