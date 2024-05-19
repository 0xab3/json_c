#include "json.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
hash_t typical_hash_function(char *key, void *user_data) {
  (void)user_data;
  int64_t hash = 5381;
  assert(key != NULL && "bro wtf");
  size_t key_len = strlen(key);
  for (int i = 0; i < key_len; i++) {
    hash = ((hash << 5) + hash) + key[i];
  }
  return hash;
}
jsonObject *json_object_new(int init_cap, float load_factor,
                            hash_func_t hash_func) {
  jsonObject *j_obj = NULL;
  if (hash_func == NULL)
    hash_func = typical_hash_function;
  if (load_factor == 0)
    load_factor = 0.75;
  j_obj = malloc(sizeof(*j_obj));
  j_obj->hash_func = hash_func;
  j_obj->load_factor = load_factor;
  j_obj->capacity = init_cap;
  j_obj->entries = malloc(sizeof(*j_obj->entries) * j_obj->capacity);
  memset(j_obj->entries, 0, sizeof(*j_obj->entries) * j_obj->capacity);
  return j_obj;
}
// char *key;
// jsonElement *element;
jsonKeyValuePair *json_key_value_pair_new(char *key, jsonElement *value) {
  jsonKeyValuePair *kv_pair = malloc(sizeof(*kv_pair));
  kv_pair->key = strdup(key);
  kv_pair->element.kind = value->kind;
  // this is a union so it doesn't matter if we do shit like this ig idk tho
  switch (value->kind) {
  case JSON_KIND_NUMBER:
    kv_pair->element._int = value->_int;
    break;
  case JSON_KIND_OBJEKT:
    kv_pair->element._obj = value->_obj;
    break;
  case JSON_KIND_STRING:
    kv_pair->element._string = strdup(value->_string);
    break;
  }
  return kv_pair;
}
jsonKeyValuePair *json_key_value_pair_get_first(jsonKeyValuePair *start,
                                                jsonKeyValuePair *end) {
  if (start == NULL || end == NULL || start > end) {
    return NULL;
  }
  jsonKeyValuePair *current = start;
  int found = 0;
  for (int i = 0; i < (end - start) + 1; i++) {
    if (current[i].key != NULL) {
      found = 1;
      break;
    }
    i++;
  }
  if (found) {
    return current;
  }
  return NULL;
}
// struct jsonObjectIter {
//   jsonKeyValuePair *current;
//   jsonKeyValuePair *next;
// };
jsonObjectIter json_object_iter_new(jsonObject *obj) {
  jsonObjectIter iter = {0};
  iter.current =
      json_key_value_pair_get_first(obj->entries, obj->entries + obj->capacity);
  iter.next = NULL;
  iter.last = obj->entries + obj->capacity;
  return iter;
}
const jsonKeyValuePair *json_object_iter_next(jsonObjectIter *iter) {
  if (iter->current == NULL) {
    return NULL;
  }

  if (iter->next != NULL) {
    iter->current = iter->next;
  }
  jsonKeyValuePair *json_kv = iter->current;
  iter->next = json_key_value_pair_get_first(iter->current + 1, iter->last);
  return json_kv;
}

// create another json instead of previous shit ig idk tho
jsonParserStatus json_object_resize(jsonObject *obj, uint32_t new_capacity) {
  uint32_t cap = obj->capacity;
  uint32_t old_len = obj->len;
  jsonKeyValuePair *new_map;
  jsonKeyValuePair *old_map;
  jsonKeyValuePair *entry;
  if (new_capacity < cap) {
    new_capacity = cap * 2;
  }
  new_map = malloc(sizeof(*new_map) * (new_capacity));
  old_map = obj->entries;

  obj->entries = new_map;
  obj->capacity = new_capacity;
  // reset all the shiit
  obj->len = 0;

  // json_object_append_key_value_pair(obj, jsonKeyValuePair * kv_pair,
  //                                   void *user_data)
  assert(new_map != NULL && "buy more ram lol");

  return (jsonParserStatus){.kind = JSON_OK};
}
jsonStatus json_object_insert_kv_pair(jsonObject *object,
                                      jsonKeyValuePair *kv_pair,
                                      uint32_t position, void *user_data) {
  jsonKeyValuePair *current;
  if (position > object->capacity) {
    return JSON_STATUS_ERROR;
  }
  current = object->entries + position;
  assert(current != NULL);
  if (current->key == NULL) {
    // not a pointer
    *current = *kv_pair;
    object->len++;
  } else {
    // pointer
    while (current->next) {
      current = current->next;
    }
    current->next = kv_pair;
  }

  return JSON_STATUS_OK;
}
// TODO: idk what i did but need to fix this
// takes ownership
jsonStatus json_object_append_key_value_pair(jsonObject *object,
                                             jsonKeyValuePair *kv_pair,
                                             void *user_data) {
  if (((float)object->len / (float)object->capacity) == 0.75) {
    json_object_resize(object, object->capacity * 2);
  }
  char *key = kv_pair->key;
  void *value = &kv_pair->element;
  hash_t hash = object->hash_func(key, user_data);
  size_t cap = object->capacity;
  uint32_t index = hash % cap;
  return json_object_insert_kv_pair(object, kv_pair, index, user_data);
}
jsonParserStatus json_parse_value(char **json_start, char *json_end,
                                  jsonElement **el) {
  char *iter = *json_start;
  if (isdigit(*iter)) {
    errno = 0;
    char *end = NULL;
    size_t number = strtol(iter, &end, 0);
    if (errno == ERANGE)
      return (jsonParserStatus){.kind = JSON_INVALID_NUMBER, .place = iter};
    *el = malloc(sizeof(**el));
    assert(*el != NULL);
    (*el)->kind = JSON_KIND_NUMBER;
    (*el)->_int = number;
    iter = end;
    *json_start = iter;
    return (jsonParserStatus){.kind = JSON_OK};
  } else {
    if (*iter == '"' || *iter == '\'' || *iter == '`') {
      char *value;
      jsonParserStatus ret = json_parse_string(json_start, json_end, &value);
      if (ret.kind != JSON_OK) {
        return ret;
      }
      *el = malloc(sizeof(**el));
      (*el)->kind = JSON_KIND_STRING;
      (*el)->_string = value;
    }
    if (*iter == '{') {
      printf("parsing object again ig idk tho\n");
      jsonParserStatus ret = json_parse_object(json_start, json_end, NULL);
      (*el)->kind = JSON_KIND_OBJEKT;
      memset(&(*el)->_obj, 0, sizeof((*el)->_obj));
    }
  }
  return (jsonParserStatus){.kind = JSON_OK};
}
jsonParserStatus json_parse_string(char **str_json, char *json_end,
                                   char **string) {
  char *iter = *str_json;
  char quote = 0;
  if (*iter == '"' || *iter == '\'' || *iter == '`')
    quote = *iter;
  else
    return (jsonParserStatus){.kind = JSON_INVALID_STRING, .place = iter};
  if (quote == 0)
    return (jsonParserStatus){.kind = JSON_INVALID, .place = iter};
  next(iter);
  while (*iter && iter < json_end) {
    if (*iter == '\n') {
      return (jsonParserStatus){.kind = JSON_INVALID_STRING, .place = iter};
    }
    if (*iter == quote && *(iter - 1) != '\\') {
      break;
    }
    iter++;
  }
  next(iter);
  // skipping the quotes
  int key_len = iter - *str_json - 2;
  *string = strndup(*str_json + 1, key_len);
  *str_json = iter;
  return (jsonParserStatus){.kind = JSON_OK};
}
/*doesn't need end (parses until finds closing bracket)*/
jsonParserStatus json_parse_object(char **str_json, char *json_end,
                                   jsonObject *object) {
  char *iter = *str_json;
  if (*iter != '{') {
    return (jsonParserStatus){.kind = JSON_INVALID, .place = iter};
  }
  next(iter);
  while (*iter && iter < json_end) {
    iter = ltrim(iter);
    if (*iter == '}') {
      return (jsonParserStatus){.kind = JSON_OK};
    }
    char *key = NULL;
    jsonParserStatus ret = json_parse_key(&iter, json_end, &key);
    if (ret.kind != JSON_OK) {
      return ret;
    }
    iter = ltrim(iter);
    if (*iter != ':') {
      return (jsonParserStatus){.kind = JSON_NO_SEPERATOR, .place = iter};
    }
    next(iter);
    iter = ltrim(iter);
    jsonElement *el = NULL;
    ret = json_parse_value(&iter, json_end, &el);
    if (ret.kind != JSON_OK) {
      return ret;
    }
    iter = ltrim(iter);
    printf("key :%s\n", key);
    if (el->kind == JSON_KIND_STRING) {
      printf("value :%s\n", el->_string);
    } else if (el->kind == JSON_KIND_NUMBER) {
      printf("int value :%lu\n", el->_int);
    }
    if (*iter != ',' && *iter != '}') {
      // means the json is invalid
      return (jsonParserStatus){.kind = JSON_INVALID, .place = iter};
    }
    if (*iter == '}') {
      *str_json = ++iter;
      return (jsonParserStatus){.kind = JSON_OK};
    }
    next(iter);
  }
  if (!(iter < json_end)) {
    return (jsonParserStatus){.kind = JSON_EXHAUSTED, .place = *str_json};
  }
  return (jsonParserStatus){.kind = JSON_OK};
}
jsonParserStatus json_from_string(char *json_start, int json_len,
                                  jsonObject *object) {
  char *iter = json_start;
  char *json_end = json_start + json_len;
  while (iter < json_end) {
    iter = ltrim(iter);
    json_end = rtrim(json_start, json_end);
    if (*iter != '{') {
      return (jsonParserStatus){.kind = JSON_INVALID, .place = json_start};
    }
    jsonParserStatus ret = json_parse_object(&iter, json_end, NULL);
    return ret;
  }
  return (jsonParserStatus){.kind = JSON_OK};
}
jsonObject *json_object_clone(jsonObject *j_obj) {
  const jsonKeyValuePair *kv_pair = NULL;
  jsonKeyValuePair *kv_pair_clone = NULL;
  jsonObject *cloned_obj =
      json_object_new(j_obj->capacity, j_obj->load_factor, j_obj->hash_func);
  jsonObjectIter j_obj_iter = json_object_iter_new(j_obj);
  // no need to check if kv_pair is NULL
  while ((kv_pair = json_object_iter_next(&j_obj_iter))) {
    switch (kv_pair->element.kind) {
    case JSON_KIND_STRING:
    case JSON_KIND_NUMBER:
      kv_pair_clone = json_key_value_pair_new(kv_pair->key,
                                              (jsonElement *)&kv_pair->element);
      break;
    case JSON_KIND_OBJEKT:
      // custom shit cause we are cloning ykwim
      kv_pair_clone->key = strdup(kv_pair->key);
      kv_pair_clone->element._obj =
          json_object_clone((jsonObject *)&kv_pair->element._obj);
      assert(0 && "TODO: unimplemented");
      break;
    }
    json_object_append_key_value_pair(cloned_obj,
                                      (jsonKeyValuePair *)kv_pair_clone, NULL);
  }
  return cloned_obj;
}

char *ltrim(char *str) {
  while (isspace(*str) && *str)
    str++;
  return str;
}
/*end shoud not have null terminator ig idk*/
char *rtrim(char *str, char *end) {
  while (isspace(*end) && end > str) {
    end--;
  }
  return end;
}
