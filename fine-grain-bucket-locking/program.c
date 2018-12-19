#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "index.h"
#include <fcntl.h>
#include <errno.h>
#include <math.h>

pthread_mutex_t lock[10000];

//Data Structure for the Bounded Buffer
typedef struct{
  pthread_mutex_t *buffer_lock;
  pthread_cond_t *not_full;
  pthread_cond_t *not_empty;
  int maxSize;
  int head;
  int tail;
  int currSize;//The current number of items in the Bounded Buffer
  char **buffer;
} bounded_buffer;

//Data Structure for the Hast Table
typedef struct{
  pthread_cond_t *canRead;
  pthread_cond_t *canWrite;
  pthread_mutex_t *lock;
  int waitWriters;
  int waitReaders;
  int numReaders;
  int numWriters;
} readers_writers;


readers_writers *ht_monitor;
bounded_buffer *file_buffer;

pthread_mutex_t *file_index_lock;
pthread_cond_t *file_indexed;

int doneScanning;//Asserted when scanning of all the files in the list-of-files is completed
int doneIndexing;//Asserted when all the files are indexed in the Hast Table
int totalFiles;  //Count of the total number of files in the list-of-files.txt

//Used to parse words and insert them into the Hash Table
void indexer(char *file_name);

int MAX_PATH = 511;
int Num_Files=3000;//Denotes the maximum number of files that can be indexed
int Index=0;			//Points the next available location in the array
char **File_Index;//Table containing the list of file names after indexing

//Used to insert file name into an array data structure after they have been indexed into the hash table
void indexer_3(char *file_name) {
  char **New_File_Index;
  int i;
  if(Index==Num_Files-1) {
    Num_Files=Num_Files * 2;
    New_File_Index = realloc(File_Index,Num_Files);
    if(!New_File_Index)
      free(New_File_Index);
    for(i=Index;i<Num_Files;i++) {
      File_Index[i+1]=malloc((MAX_PATH+1)*sizeof (char));
    }
  }
  //update the stats in the array
  strcpy(File_Index[Index],file_name);
  Index++;
}

//Used to add filenames for indexer threads to file_buffer
void *scanner_thread(void *file_list){
  
  
  FILE *fd = fopen((char *)file_list,"rt");
  char *filename = malloc(sizeof(char) * 513);

  totalFiles = 0;
  
    while(fgets(filename,513,fd) != NULL){
      strtok(filename, "\n\r");

      pthread_mutex_lock(file_buffer->buffer_lock);

      //wait for buffer to not be full
      while(file_buffer->currSize == file_buffer->maxSize)
        pthread_cond_wait(file_buffer->not_full,file_buffer->buffer_lock);

      //enter filename in buffer
      file_buffer->buffer[file_buffer->tail] = filename;
      pthread_cond_broadcast(file_buffer->not_empty);

      //update tail pointer
      file_buffer->tail = (file_buffer->tail + 1) % file_buffer->maxSize;
      file_buffer->currSize = file_buffer->currSize + 1;

      filename = malloc(sizeof(char) * 513);
      
      totalFiles++;

      pthread_mutex_unlock(file_buffer->buffer_lock);
    }

  free(filename);

  
  pthread_mutex_lock(file_buffer->buffer_lock);
  doneScanning = 1;
  pthread_cond_broadcast(file_buffer->not_empty);
  pthread_mutex_unlock(file_buffer->buffer_lock);

  return NULL;
}

//Empties the bounded buffer and called the indexer thread to hash values in the hash table
void *indexer_thread(){

  char *filename;

  while(1){
    //part 1
    pthread_mutex_lock(file_buffer->buffer_lock);

    while(file_buffer->currSize == 0 && doneScanning == 0)
      pthread_cond_wait(file_buffer->not_empty,file_buffer->buffer_lock);

    if(doneScanning == 1 && file_buffer->currSize == 0){
      pthread_mutex_unlock(file_buffer->buffer_lock);
      return NULL;
    }
    
    filename = file_buffer->buffer[file_buffer->head];
    file_buffer->buffer[file_buffer->head] = NULL;;

    file_buffer->head = (file_buffer->head + 1) % file_buffer->maxSize;
    file_buffer->currSize = file_buffer->currSize - 1;

    pthread_cond_broadcast(file_buffer->not_full);  
    
    pthread_mutex_unlock(file_buffer->buffer_lock);
    //end part 1

    //part 2
    //inserts values into the hash table
    indexer(filename);
    
    //part 3
		//updates values into the array data structure
    pthread_mutex_lock(file_index_lock);
    indexer_3(filename);
    pthread_cond_broadcast(file_indexed);

    pthread_mutex_lock(file_buffer->buffer_lock);
    if(doneScanning == 1 && Index == totalFiles)
      doneIndexing = 1;
    pthread_mutex_unlock(file_buffer->buffer_lock);

    pthread_mutex_unlock(file_index_lock);
    //end part 3
  }

  return NULL;
}

//Handles the Basic and Advanced Searches
void *query_thread(void *input){
  

  //query code starts here
  char *user_input = (char*)input;
  
  //parse user input, detect basic/advanced searches
  char *user_input_parsed[5000];
  char *tmp;
  int num_cmds = 0;

  strtok(user_input, "\n\r");
  
  tmp = strtok(user_input," ");
    
  while (tmp) 
    {
      user_input_parsed[num_cmds] = tmp;
      num_cmds++;
      tmp = strtok(NULL," ");
    }


  if(num_cmds == 1)
    ;
  else if(num_cmds == 2){
    pthread_mutex_lock(file_index_lock);
    int found = 0;
    int index = 0;
    while(!found){
      if(index == Index)
	pthread_cond_wait(file_indexed,file_index_lock);
      if(strcmp(user_input_parsed[0],File_Index[index]) == 0)
	found = 1;
      index++;
      if(doneIndexing && !found && index == Index){
	printf("ERROR: File %s not found\n",user_input_parsed[0]);
	pthread_mutex_unlock(file_index_lock);

	return NULL;
      }
    }
    pthread_mutex_unlock(file_index_lock);

    char *temp = user_input_parsed[0];
    user_input_parsed[0] = user_input_parsed[1];
    user_input_parsed[1] = temp;  }
    //end query parsing

  //TEST QUERY CODE

  //////////pthread_mutex_lock(ht_monitor->lock);

  index_search_results_t *result = find_in_index(user_input_parsed[0]);
  
  /////////pthread_mutex_unlock(ht_monitor->lock);

  int advancedFound = 0;

  if(result == NULL)
    printf("Word not found.\n");
  else{
    int i;
    for(i = 0; i < result->num_results; i++){
      if(num_cmds == 1 || (num_cmds == 2 && strcmp(result->results[i].file_name,user_input_parsed[1]) == 0)){
	printf("FOUND: %s %d\n",result->results[i].file_name,result->results[i].line_number);
	if(num_cmds == 2)
	  advancedFound = 1;
      }
    } 
    if(num_cmds == 2 && !advancedFound)
      printf("Word not found.\n");
  }

  //END TEST QUERY CODE

  //query code ends here  

  return NULL;
}

void usage() 
{
  fprintf(stderr, "Usage: search-engine num-indexer-threads file-list\n");
  exit(1);
}

//Used to parse words and insert them into the Hash Table
void indexer(char *file_name)
{
  char buffer[1000];
  

  strtok(file_name, "\n\r");

  FILE *file = fopen(file_name, "rt");
  if (file == NULL) 
    {
      char error_message[30] = "Can't open file.\n";
      write(STDERR_FILENO, error_message, strlen(error_message));
      return;
      exit(1);
    }

  int count = 0;
  int line_number = 0;

  while(fgets(buffer,130,file) != NULL)
  {
      char *word;
      char *saveptr;
      
      strtok(buffer, "\n\r");
      
      word = strtok_r(buffer, " \n\t-_!@#$%^&*()_+=,./<>?", &saveptr);

      while (word != NULL) {
	if(strlen(word) >= 3){

	  insert_into_index(word, file_name, line_number);	  

	}
	word = strtok_r(NULL, " \n\t-_!@#$%^&*()_+=,./<>?", &saveptr);
      }
      count++;

      line_number = line_number+1;
    }
  fclose(file);

  return;
}
  

int main(int argc, char *argv[])
{
  int i = 0;
  for(i = 0 ; i < 10000 ; i++) 
    pthread_mutex_init(&lock[i], NULL);

  if(argc != 3) 
    {
      usage();
    }

  int num_threads = atoi(argv[1]);
  char *file_list = argv[2];

  init_index();

  doneScanning = 0;
  doneIndexing = 0;

  //Initialize indexer 3
  //int i;
  File_Index = malloc(Num_Files*sizeof(char *));
  for(i=0;i<Num_Files;i++) {
    File_Index[i]=malloc((MAX_PATH+1)*sizeof (char));
  }


  pthread_rwlockattr_init(&attr);

  //Initialize ht_monitor
  ht_monitor = malloc(sizeof(readers_writers));

  ht_monitor->waitWriters = 0;
  ht_monitor->waitReaders = 0;
  ht_monitor->numReaders = 0;
  ht_monitor->numWriters = 0;
  ht_monitor->canRead = malloc(sizeof(pthread_cond_t));
  ht_monitor->canWrite = malloc(sizeof(pthread_cond_t));
  ht_monitor->lock = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(ht_monitor->lock,NULL);

  //init file index lock
  file_index_lock = malloc(sizeof(pthread_mutex_t));
  file_indexed = malloc(sizeof(pthread_cond_t));

  //Initialize filebuffer
  file_buffer = malloc(sizeof(bounded_buffer));

  file_buffer->buffer_lock = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(file_buffer->buffer_lock,NULL);

  file_buffer->not_full = malloc(sizeof(pthread_cond_t));
  file_buffer->not_empty = malloc(sizeof(pthread_cond_t));

  file_buffer->maxSize = 10;
  file_buffer->head = 0;
  file_buffer->tail = 0;
  file_buffer->currSize = 0;

  file_buffer->buffer = malloc(sizeof(char *) * file_buffer->maxSize);

  pthread_t scanner;
  int pid_scanner;
  pthread_t *indexer = malloc(sizeof(pthread_t) * num_threads);
  int *pid_indexer = malloc(sizeof(int) * num_threads);

  pid_scanner = pthread_create(&scanner,NULL,scanner_thread,(void *)file_list);

  for(i = 0; i < num_threads; i++)
    pid_indexer[i] = pthread_create(&(indexer[i]),NULL,indexer_thread,NULL);

  char *user_input = malloc(sizeof(char) * 130);
  

  int numQuery = 0;
  pthread_t *query = malloc(sizeof(pthread_t) * 10000);
  int pid_query;

  while(fgets(user_input,128,stdin) != NULL){    
    pid_query = pthread_create(&query[numQuery],NULL,query_thread,(void *)user_input);
    user_input = malloc(sizeof(char) * 130);
    numQuery++;
  }

  int j;
  
  for(j = 0; j < num_threads; j++)
    {
      pthread_join(indexer[j], NULL);
    }

  free(indexer);

  for(j = 0; j < numQuery; j++)
    {
      pthread_join(query[j], NULL);
    }

  free(file_buffer->not_full);
  free(file_buffer->not_empty);
  free(file_buffer->buffer_lock);
  free(file_buffer);

  free(ht_monitor->canRead);
  free(ht_monitor->canWrite);
  free(ht_monitor->lock);
  free(ht_monitor);

  free(File_Index);
  
  return 0;

}
