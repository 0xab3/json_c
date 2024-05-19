#include "json.h"
#include <stdio.h>
int main(int argc, char **argv) {
  if (argc < 2) {
    printf("pelase provide argument\n");
    return 1;
  }
  FILE *f = fopen(argv[1], "rb");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET); /* same as rewind(f); */

  char *json = malloc(fsize + 1);
  fread(json, fsize, 1, f);
  fclose(f);
  json[fsize] = 0;

  jsonObject *obj = json_object_new(10, 0.75, NULL);
  jsonObject *new_obj = json_object_new(10, 0.75, NULL);

  jsonKeyValuePair *kv_pair_child1 = json_key_value_pair_new(
      "testing2", JSON_ELEMENT_STRING("leeessssgooooo\n"));
  jsonKeyValuePair *kv_pair_child2 =
      json_key_value_pair_new("testing3", JSON_ELEMENT_NUMBER(9));

  json_object_append_key_value_pair(new_obj, kv_pair_child1, NULL);
  json_object_append_key_value_pair(new_obj, kv_pair_child2, NULL);

  jsonKeyValuePair *kv_pair_parent_object = json_key_value_pair_new(
      "testing1", JSON_ELEMENT_OBJEKT(json_object_clone(new_obj)));

  json_object_append_key_value_pair(obj, kv_pair_parent_object, NULL);

  // jsonStatus ret = json_from_string(json, strlen(json), &obj);
  // if (ret.kind != JSON_OK) {
  //   fprintf(stderr, "got error while parsing json %d %s\n", ret.kind,
  //           ret.place);
  // }
}
