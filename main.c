#include "json.h"
#include "utils.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "pelase provide argument\n");
    fprintf(stderr, "argument: %s [json file]\n", *argv);
    return 1;
  }
  FILE *f = fopen(argv[1], "rb");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  if (fsize == -1) {
    LOG_ERROR("ftell failed with %s\n", strerror(errno));
  }

  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  char *json = (char *)malloc((size_t)fsize + 1);
  fread(json, (size_t)fsize, 1, f);
  fclose(f);
  json[fsize] = 0;

  jsonObject obj = {0};
  jsonStatus ret = json_object_new(&obj, 16, 80, NULL, NULL, NULL);
  assert(ret == JSON_STATUS_OK);
  json_from_string(&obj, json, (size_t)fsize, NULL);
  jsonKeyValuePair *value = json_object_get(&obj, "testing1");
  jsonArray *array = value->element.as.array;
  jsonElement *elem = json_array_get(array, 0);
  LOG_DEBUG("%lf\n", elem->as.number);
  elem = json_array_get(array, 4);
  LOG_DEBUG("%s\n", elem->as.string);

  elem = json_array_get(array, 5);
  jsonObject *nested_object = elem->as.object;
  value = json_object_get(nested_object, "testin2");
  jsonElement *nested_element = &value->element;
  LOG_DEBUG("%s\n", nested_element->as.string);
  value = json_object_get(nested_object, "testing4");

  nested_element = &value->element;
  jsonArray *nested_array = nested_element->as.array;
  elem = json_array_get(nested_array, 0);
  LOG_DEBUG("%s\n", elem->as.string);

  elem = json_array_get(array, 6);
  nested_array = elem->as.array;

  elem = json_array_get(nested_array, 0);
  LOG_DEBUG("%s\n", elem->as.string);
}
