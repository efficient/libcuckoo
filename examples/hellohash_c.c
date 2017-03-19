#include <stdio.h>

#include "int_str_table.h"

int main() {
  int_str_table *tbl = int_str_table_init(0);
  const char *insert_item = "hello";
  for (int i = 0; i < 100; i++) {
    int_str_table_insert(tbl, &i, &insert_item);
  }

  for (int i = 0; i < 101; i++) {
    const char *find_item;
    if (int_str_table_find(tbl, &i, &find_item)) {
      printf("%d  %s\n", i, find_item);
    } else {
      printf("%d  NOT FOUND\n", i);
    }
  }
}
