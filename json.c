#include "json.h"
#include "utils.h"
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <uchar.h>
#include <unistd.h>

#define json_parse_key json_parse_string

static hash_t typical_hash_function(const char *key, void *user_data)
    __attribute__((pure));
static void json_object_try_insert_kv_pair(jsonObject *object,
                                           const jsonKeyValuePair *kv_pair,
                                           size_t preferred_position);
static jsonKeyValuePair *json_key_value_pair_get_first(jsonKeyValuePair *start,
                                                       jsonKeyValuePair *end);

static jsonStatus strndup(opaque_ptr_t allocator_ctx, const char *cstr,
                          size_t len, char **dest);

// returns what the index would be after trimming
static size_t ltrim(char *start);

#define cstrdup(ctx, cstr, dest) strndup(ctx, cstr, strlen(cstr), dest)

// leaks stack memory to *heap
static jsonStatus box(opaque_ptr_t allocator_ctx, opaque_ptr_t source,
                      opaque_ptr_t *destination, size_t object_size) {
  J_UNUSED(allocator_ctx);
  *destination = J_MALLOC(allocator_ctx, object_size);
  if (destination == NULL) {
    return JSON_ERROR_OUT_OF_MEMORY;
  }
  memcpy(*destination, source, object_size);
  return JSON_STATUS_OK;
}
static hash_t typical_hash_function(const char *key, void *user_data) {
  J_UNUSED(user_data);

  size_t hash = 5381;
  assert(key != NULL && "bro wtf");
  size_t key_len = strlen(key);
  for (size_t i = 0; i < key_len; i++) {
    hash = ((hash << 5) + hash) + (size_t)key[i];
  }
  return hash;
}
jsonStatus json_object_new(jsonObject *obj, size_t init_cap, size_t load_factor,
                           hash_func_t hash_func,
                           opaque_ptr_t hash_func_userdata,
                           opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);

  if (hash_func == NULL)
    hash_func = typical_hash_function;

  assert(load_factor < 100 && "load factor must be between 0 and 100!");

  if (load_factor == 0)
    load_factor = 80;

  obj->hasher.hash_func = hash_func;
  obj->hasher.hash_func_userdata = hash_func_userdata;
  obj->load_factor = load_factor;
  obj->capacity = init_cap;
  obj->entries = (jsonKeyValuePair *)J_MALLOC(
      allocator_ctx, sizeof(*obj->entries) * obj->capacity);
  if (obj->entries == NULL) {
    return JSON_ERROR_OUT_OF_MEMORY;
  }
  memset(obj->entries, 0, sizeof(*obj->entries) * obj->capacity);
  return JSON_STATUS_OK;
}

jsonKeyValuePair *json_object_get(jsonObject *object, const char *key) {
  hash_t hash =
      object->hasher.hash_func(key, object->hasher.hash_func_userdata);
  size_t idx = hash % object->capacity;

  jsonKeyValuePair *entries = object->entries;
  size_t entries_capacity = object->capacity;

  size_t n_iters = 0;

  while (entries[idx].hash != hash && n_iters <= entries_capacity) {
    idx++;
    n_iters++;
    idx = idx % entries_capacity;
  }
  if (n_iters > entries_capacity) {
    return NULL;
  }
  return (entries + idx);
}

jsonStatus json_object_append_key_value_pair(jsonObject *object,
                                             const jsonKeyValuePair *kv_pair,
                                             opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);

  const char *key = kv_pair->key;
  const hash_t hash =
      object->hasher.hash_func(key, object->hasher.hash_func_userdata);
  const size_t cap = object->capacity;
  const size_t index = hash % cap;

  if (json_object_is_resize_required(object)) {
    jsonStatus json_status =
        json_object_resize(object, object->capacity * 2, allocator_ctx);
    if (json_status != JSON_STATUS_OK) {
      assert(0 && "unreachable for now: remove it please!");
      return json_status;
    }
  }
  json_object_try_insert_kv_pair(object, kv_pair, index);
  return JSON_STATUS_OK;
}

// note(shahzad): if i do wraparound_idx = idx+1 and iter wraparound then
// i have to introduce conditionals to check if the first position is empty
// if do wraparound_idx = idx-1 and iter idx then it will go in infinite loop
// if idx is 0 ;(
static void json_object_try_insert_kv_pair(jsonObject *object,
                                           const jsonKeyValuePair *kv_pair,
                                           size_t preferred_position) {
  jsonKeyValuePair *entries = object->entries;
  size_t entries_capacity = object->capacity;

  size_t idx = preferred_position;
  assert(idx < object->capacity);

  size_t n_iters = 0;

  while ((entries + idx)->key != NULL && n_iters <= entries_capacity) {
    idx++;
    n_iters++;
    idx = idx % entries_capacity;
  }
  assert(n_iters <= entries_capacity);
  *(entries + idx) = *kv_pair;
  object->n_entries++;
}

// we have to impl this shit
jsonStatus json_object_resize(jsonObject *obj, size_t new_capacity,
                              opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);
  J_UNUSED(obj);
  const jsonObject *old_json_object = obj;
  jsonObject new_object = {0};
  jsonStatus ret =
      json_object_new(&new_object, new_capacity, old_json_object->load_factor,
                      old_json_object->hasher.hash_func,
                      old_json_object->hasher.hash_func_userdata, NULL);

  if (ret != JSON_STATUS_OK) {
    return ret;
  }

  jsonObjectIter iter = json_object_iter_new(old_json_object);
  const jsonKeyValuePair *kv_pair = NULL;
  while ((kv_pair = json_object_iter_next(&iter))) {
    const size_t idx = kv_pair->hash % new_capacity;
    json_object_try_insert_kv_pair(&new_object, kv_pair, idx);
  }

  memcpy(obj, &new_object, sizeof(*obj));
  return JSON_STATUS_OK;
}

bool json_object_is_resize_required(jsonObject *object) {
  return object->capacity * object->load_factor < object->n_entries * 100;
}

void json_object_free(jsonObject *object, opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);
  jsonObjectIter iter = json_object_iter_new(object);
  jsonKeyValuePair *kv_pair = NULL;
  while ((kv_pair = json_object_iter_next(&iter))) {
    J_FREE(allocator_ctx, kv_pair->key);
    json_element_free(&kv_pair->element, NULL);
  }
  J_FREE(allocator_ctx, object->entries);
  J_FREE(allocator_ctx, object);
}

jsonObjectIter json_object_iter_new(const jsonObject *obj) {
  jsonObjectIter iter = {0};
  iter.first = obj->entries;
  iter.last = obj->entries + obj->capacity - 1;
  return iter;
}
jsonKeyValuePair *json_object_iter_next(jsonObjectIter *iter) {
  jsonKeyValuePair *kv_pair = NULL;
  if (iter->current == NULL) {
    kv_pair = json_key_value_pair_get_first(iter->first, iter->last);
    if (kv_pair == NULL) {
      return NULL;
    }
    iter->current = kv_pair + 1;
    return kv_pair;
  } else if (iter->first < iter->current && iter->current <= iter->last) {
    kv_pair = json_key_value_pair_get_first(iter->current, iter->last);
    if (kv_pair == NULL) {
      iter->current = NULL;
      return NULL;
    }
    iter->current = kv_pair + 1;
    return kv_pair;
  } else if (iter->current > iter->last) {
    iter->current = NULL;
    return NULL;
  } else {
    LOG_DEBUG("unreachable: first: %p current:%p last:%p\n",
              (void *)iter->first, (void *)iter->current, (void *)iter->last);
    assert(0 && "unreachable");
  }
}

jsonStatus json_key_value_pair_new(jsonKeyValuePair *pair, char *key,
                                   jsonElement *value,
                                   opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);
  jsonKeyValuePair *kv_pair = pair;
  char *key_duped = NULL;
  char *str_value = NULL; // need to dupe the value if string but variable decls
                          // are not allowed in switch cases
  jsonStatus status = cstrdup(allocator_ctx, key, &key_duped);
  if (status != JSON_STATUS_OK) {
    return status;
  }
  kv_pair->key = key_duped;
  kv_pair->element.kind = value->kind;

  switch (value->kind) {
  case JSON_KIND_NUMBER:
    kv_pair->element.as.number = value->as.number;
    break;
  case JSON_KIND_OBJEKT:
    kv_pair->element.as.object = value->as.object;
    break;
  case JSON_KIND_STRING:
    status = cstrdup(allocator_ctx, kv_pair->element.as.string, &str_value);
    if (status != JSON_STATUS_OK) {
      kv_pair->key = NULL;
      J_FREE(allocator_ctx, key_duped);
      return status;
    }
    kv_pair->element.as.string = str_value;
    break;
  case JSON_KIND_ARRAY:
    assert(0 && "unimplemented");
  default:
    assert(false && "unreachable");
  }
  return JSON_STATUS_OK;
}
static jsonKeyValuePair *json_key_value_pair_get_first(jsonKeyValuePair *start,
                                                       jsonKeyValuePair *end) {
  if (start == NULL || end == NULL || start > end) {
    return NULL;
  }
  jsonKeyValuePair *current = start;
  int found = 0;
  int i = 0;
  for (i = 0; i <= (end - start); i++) {
    if (current[i].key != NULL) {
      found = 1;
      break;
    }
  }
  if (found) {
    assert(current[i].key != NULL);
    return current + i;
  }
  return NULL;
}

static jsonParserStatus json_parse_number(char *str_json, size_t *idx,
                                          size_t json_len,
                                          json_number *number) {
  J_UNUSED(json_len);
  size_t json_idx = *idx;
  char *number_end = NULL;
  json_idx += ltrim(str_json + json_idx);

  // rust people will murder me for this shit
  *number = strtod(str_json + json_idx, &number_end);

  assert(number_end != NULL);

  const ssize_t n_bytes = (number_end - (str_json + json_idx));
  assert(n_bytes > 0 && n_bytes <= 21);

  if (n_bytes <= 0) {
    return (jsonParserStatus){.kind = JSON_INTERNAL_FAILURE};
  }
  json_idx += (size_t)n_bytes;
  assert(!isdigit(str_json[json_idx]));

  *idx = json_idx;

  return (jsonParserStatus){.kind = JSON_OK};
}

static jsonParserStatus json_parse_string(char *str_json, size_t *idx,
                                          size_t json_len, size_t *str_start,
                                          size_t *str_end) {

  char quote = 0;
  size_t json_idx = *idx;
  json_idx += ltrim(str_json + json_idx);

  if (str_json[json_idx] == '"' || str_json[json_idx] == '\'' ||
      str_json[json_idx] == '`')
    quote = str_json[json_idx];
  else
    return (jsonParserStatus){.kind = JSON_INVALID_STRING};

  next(json_idx);
  *str_start = json_idx;

  while (str_json[json_idx] && json_idx < json_len) {
    if (str_json[json_idx] == '\n') {
      return (jsonParserStatus){.kind = JSON_INVALID_STRING};
    }
    if (str_json[json_idx] == quote && str_json[json_idx - 1] != '\\') {
      break;
    }
    next(json_idx);
  }
  *str_end = json_idx - 1;
  next(json_idx);
  *idx = json_idx;
  return (jsonParserStatus){.kind = JSON_OK};
}

jsonParserStatus json_parse_object(jsonObject *object, char *str_json,
                                   size_t *json_idx, size_t json_len,
                                   opaque_ptr_t allocator_ctx) {
  size_t idx = *json_idx;
  idx += ltrim(str_json);
  if (str_json[idx] != '{') {
    LOG_WARN("TODO: give row and column while returning error!\n");
    return (jsonParserStatus){.kind = JSON_INVALID};
  }

  next(idx);

  while (str_json[idx] && idx < json_len) {
    jsonKeyValuePair kv_pair = {0};
    idx += ltrim(str_json + idx);

    size_t key_start_idx = 0;
    size_t key_end_idx = 0;

    char *key = NULL;
    size_t key_len = 0;
    char *key_duped = NULL; // here cause kv_pair.key is const char *

    jsonStatus json_status = JSON_STATUS_OK;
    jsonParserStatus parser_status =
        json_parse_key(str_json, &idx, json_len, &key_start_idx, &key_end_idx);

    key = str_json + key_start_idx;
    key_len = key_end_idx - key_start_idx + 1;

    if (parser_status.kind != JSON_OK) {
      return parser_status;
    }

    idx += ltrim(str_json + idx);
    if (str_json[idx] != ':') {
      json_object_free(object, allocator_ctx);
      return (jsonParserStatus){.kind = JSON_NO_SEPERATOR};
    }

    json_status = strndup(allocator_ctx, key, key_len, &key_duped);
    if (json_status != JSON_STATUS_OK) {
      assert(json_status == JSON_ERROR_OUT_OF_MEMORY);
      return (jsonParserStatus){.kind = JSON_INTERNAL_OUT_OF_MEMORY};
    }
    kv_pair.key = key_duped;
    const hash_t hash = object->hasher.hash_func(
        kv_pair.key, object->hasher.hash_func_userdata);
    kv_pair.hash = hash;

    next(idx);
    idx += ltrim(str_json + idx);

    parser_status = json_parse_value(&kv_pair.element, &object->hasher,
                                     str_json, &idx, json_len, allocator_ctx);
    if (parser_status.kind != JSON_OK) {
      kv_pair.key = NULL;
      J_FREE(allocator_ctx, key_duped);
      json_object_free(object, allocator_ctx);
      return parser_status;
    }

    json_status =
        json_object_append_key_value_pair(object, &kv_pair, allocator_ctx);
    if (json_status != JSON_STATUS_OK) {
      assert(json_status == JSON_ERROR_OUT_OF_MEMORY);
      J_FREE(allocator_context, key_duped);
      assert(0);
      json_element_free(&kv_pair.element, NULL);
      json_object_free(object, allocator_ctx);
      kv_pair.key = NULL;
      return (jsonParserStatus){.kind = JSON_INTERNAL_OUT_OF_MEMORY};
    }

    idx += ltrim(str_json + idx);
    *json_idx = idx;

    if (str_json[idx] != ',' && str_json[idx] != '}') {
      LOG_ERROR("unexpected token while parsing: %.*s\n", 50,
                str_json + idx - 10);
      LOG_ERROR("unexpected token while parsing: %*c%c\n", 10, ' ', '^');
      assert(0 && "free stuff here");
      return (jsonParserStatus){.kind = JSON_INVALID};
    }
    if (str_json[idx] == '}') {
      *json_idx = idx + 1;

      return (jsonParserStatus){.kind = JSON_OK};
    }
    next(idx);
  }
  return (jsonParserStatus){.kind = JSON_OK};
}

jsonParserStatus json_parse_array(jsonArray *array, char *str_json,
                                  size_t *json_idx, size_t json_len,
                                  const jsonHash *hash_vtable,
                                  opaque_ptr_t allocator_ctx) {

  J_UNUSED(allocator_ctx);
  size_t idx = *json_idx;
  jsonParserStatus parser_status = {0};
  jsonStatus json_status = JSON_STATUS_OK;
  idx += ltrim(str_json + idx);
  if (str_json[idx] != '[') {
    return (jsonParserStatus){.kind = JSON_INVALID_ARRAY};
  }
  next(idx);
  while (str_json[idx] != ']' && idx < json_len) {
    idx += ltrim(str_json + idx);
    jsonElement json_elem = {0};
    parser_status = json_parse_value(&json_elem, hash_vtable, str_json, &idx,
                                     json_len, allocator_ctx);
    if (parser_status.kind != JSON_OK) {
      return parser_status;
    }
    idx += ltrim(str_json + idx);
    if (str_json[idx] != ',' && str_json[idx] != ']') {
      LOG_ERROR("unexpected token while parsing: %.*s\n", 50,
                str_json + idx - 10);
      LOG_ERROR("unexpected token while parsing: %*c%c\n", 10, ' ', '^');
      assert(0);
      json_element_free(&json_elem, NULL);
      json_array_free(array, NULL);
      return (jsonParserStatus){.kind = JSON_INVALID_ARRAY};
    }

    json_status = json_array_append(array, json_elem, allocator_ctx);
    if (json_status != JSON_STATUS_OK) {
      assert(json_status == JSON_ERROR_OUT_OF_MEMORY);
      assert(0);
      json_element_free(&json_elem, NULL);
      json_array_free(array, NULL);
      return (jsonParserStatus){.kind = JSON_INTERNAL_OUT_OF_MEMORY};
    }
    if (str_json[idx] == ']') {
      *json_idx = idx + 1;
      return (jsonParserStatus){.kind = JSON_OK};
    }
    // it can only be ',' now so we have to consume it and continue
    next(idx);
  }
  return (jsonParserStatus){.kind = JSON_OK};
}
jsonParserStatus json_parse_value(jsonElement *json_elem,
                                  const jsonHash *hash_vtable, char *str_json,
                                  size_t *json_idx, size_t json_len,
                                  opaque_ptr_t allocator_ctx) {
  size_t idx = *json_idx;
  jsonStatus json_status = JSON_STATUS_OK;
  jsonParserStatus parser_status = {0};
  switch (str_json[idx]) {
  case '{': {
    jsonObject value_object = {0};
    json_status =
        json_object_new(&value_object, 8, 80, hash_vtable->hash_func,
                        hash_vtable->hash_func_userdata, allocator_ctx);
    if (json_status != JSON_STATUS_OK) {
      assert(json_status == JSON_ERROR_OUT_OF_MEMORY);
      return (jsonParserStatus){.kind = JSON_INTERNAL_OUT_OF_MEMORY};
    }
    parser_status =
        json_parse_object(&value_object, str_json, &idx, json_len, NULL);

    if (parser_status.kind != JSON_OK) {
      assert(0 && "free da memory in here\n");
      return parser_status;
    }

    json_elem->kind = JSON_KIND_OBJEKT;
    box(allocator_ctx, &value_object, (void **)&json_elem->as.object,
        sizeof(value_object));
    break;
  }

  case '[': {
    jsonArray array = {0};
    json_status = json_array_new(&array, 4, allocator_ctx);
    if (json_status != JSON_STATUS_OK) {
      assert(json_status == JSON_ERROR_OUT_OF_MEMORY);
      assert(0 && "free da memory in here\n");
      return (jsonParserStatus){.kind = JSON_INTERNAL_OUT_OF_MEMORY};
    }
    parser_status = json_parse_array(&array, str_json, &idx, json_len,
                                     hash_vtable, allocator_ctx);
    if (parser_status.kind != JSON_OK) {
      return parser_status;
    }
    json_elem->kind = JSON_KIND_ARRAY;
    box(allocator_ctx, &array, (void **)&json_elem->as.array,
        sizeof(*json_elem->as.array));
    break;
  }

  case '`':
  case '"':
  case '\'': {
    size_t value_start_idx = SIZE_MAX;
    size_t value_end_idx = SIZE_MAX;

    parser_status = json_parse_string(str_json, &idx, json_len,
                                      &value_start_idx, &value_end_idx);
    if (parser_status.kind != JSON_OK) {
      return parser_status;
    }
    assert(value_start_idx != SIZE_MAX && value_end_idx != SIZE_MAX &&
           "unreachable");
    json_elem->kind = JSON_KIND_STRING;
    json_status =
        strndup(allocator_ctx, str_json + value_start_idx,
                value_end_idx - value_start_idx + 1, &json_elem->as.string);
    if (json_status != JSON_STATUS_OK) {
      assert(json_status == JSON_ERROR_OUT_OF_MEMORY);
      return (jsonParserStatus){.kind = JSON_INTERNAL_OUT_OF_MEMORY};
    }
    break;
  }

  default: {
    if (str_json[idx] == '-' || str_json[idx] == '+' ||
        (str_json[idx] >= '0' && str_json[idx] <= '9')) {
      json_number value = 0;
      parser_status = json_parse_number(str_json, &idx, json_len, &value);
      if (parser_status.kind != JSON_OK) {
        return parser_status;
      }
      json_elem->kind = JSON_KIND_NUMBER;
      json_elem->as.number = value;
      break;
    }
    LOG_ERROR("unexpected token while parsing: %.*s\n", 50,
              str_json + idx - 10);
    LOG_ERROR("unexpected token while parsing: %*c%c\n", 10, ' ', '^');

    assert(0 && "unreachable");
  }
  }
  *json_idx = idx;
  return (jsonParserStatus){.kind = JSON_OK};
}
jsonParserStatus json_from_string(jsonObject *object, char *json_start,
                                  size_t json_len, opaque_ptr_t allocator_ctx) {
  size_t idx = 0;
  return json_parse_object(object, json_start, &idx, json_len, allocator_ctx);
}

jsonStatus json_array_new(struct jsonArray *array, size_t capacity,
                          opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);
  array->capacity = capacity;
  array->elems = (jsonElement *)J_MALLOC(ctx, capacity * sizeof(*array->elems));
  if (array->elems == NULL) {
    return JSON_ERROR_OUT_OF_MEMORY;
  }
  array->len = 0;
  return JSON_STATUS_OK;
}

// we do not care about thread safety
jsonStatus json_array_append(struct jsonArray *array, struct jsonElement elem,
                             opaque_ptr_t allocator_ctx) {
  if (array->len == array->capacity) {
    jsonStatus ret =
        json_array_resize(array, array->capacity * 2, allocator_ctx);
    if (ret != JSON_STATUS_OK) {
      assert(ret == JSON_ERROR_OUT_OF_MEMORY);
      return ret;
    }
  }
  array->elems[array->len] = elem;
  array->len++;
  return JSON_STATUS_OK;
}

jsonElement *json_array_get(struct jsonArray *array, size_t idx) {
  assert(array->len > idx);
  return &array->elems[idx];
}
jsonStatus json_array_resize(struct jsonArray *array, size_t new_capacity,
                             opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);
  array->capacity = new_capacity;
  array->elems = (jsonElement *)J_REALLOC(
      allocator_ctx, array->elems, array->capacity * sizeof(*array->elems));
  if (array->elems == NULL) {
    return JSON_ERROR_OUT_OF_MEMORY;
  }
  return JSON_STATUS_OK;
}
jsonStatus json_array_remove_swap(struct jsonArray *array, size_t idx) {
  J_UNUSED(array);
  J_UNUSED(idx);
  assert(0 && "unimplemented");
}

jsonStatus json_array_remove_ordered(struct jsonArray *array, size_t idx) {
  J_UNUSED(array);
  J_UNUSED(idx);
  assert(0 && "unimplemented");
}
void json_array_free(struct jsonArray *array, opaque_ptr_t allocator_ctx) {
  J_UNUSED(allocator_ctx);
  for (size_t i = 0; i < array->len; i++) {
    json_element_free(&array->elems[i], allocator_ctx);
  }
  J_FREE(allocator_ctx, array->elems);
  J_FREE(allocator_ctx, array);
}

void json_element_free(jsonElement *element, opaque_ptr_t allocator_ctx) {
  switch (element->kind) {
  case JSON_KIND_STRING: {
    J_FREE(allocator_ctx, element->as.string);
    break;
  }
  case JSON_KIND_ARRAY: {
    json_array_free(element->as.array, allocator_ctx);
    break;
  }

  case JSON_KIND_OBJEKT: {
    json_object_free(element->as.object, allocator_ctx);
    break;
  }
  case JSON_KIND_NUMBER: {
    // we not allocating this shit
    break;
  }
  default:
    assert(0 && "unreachable");
  }
}

static size_t ltrim(char *start) {
  size_t idx = 0;
  while (isspace(start[idx]) && start[idx])
    idx++;
  return idx;
}
static jsonStatus strndup(opaque_ptr_t allocator_ctx, const char *cstr,
                          size_t len, char **dest) {
  J_UNUSED(allocator_ctx);
  *dest = (char *)J_MALLOC(allocator_ctx, len + 1);
  if (*dest == NULL) {
    return JSON_ERROR_OUT_OF_MEMORY;
  }
  memcpy(*dest, cstr, len);
  (*dest)[len] = 0;
  return JSON_STATUS_OK;
}

// as it points to static function
#undef cstrdup
