#ifndef __INDEX_H_537__
#define __INDEX_H_537__

pthread_rwlockattr_t attr;

#define MAXPATH 511
typedef struct index_search_elem_s {
  char file_name[MAXPATH];
  int line_number;
} index_search_elem_t;

typedef struct index_search_results_s {
  int num_results;
  index_search_elem_t results[1];
} index_search_results_t;

int init_index();
int insert_into_index(char * word, char * file_name, int line_number);
index_search_results_t * find_in_index(char * word);

#endif // __INDEX_H_537__
