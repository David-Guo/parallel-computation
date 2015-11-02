/* File:     pth_pmck.c
 */        

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

const int MAX_THREADS = 1024;

long thread_count;
long num;
long *subp;

void* primecheck(void* rank);

int main(int argc, char* argv[]) {
   long       thread;  /* Use long in case of a 64-bit system */
   pthread_t* thread_handles;
   long       isprime; 
   /* Get number of threads from command line */
   num = strtol(argv[1], NULL, 10);  
   thread_count = strtol(argv[2], NULL, 10);  
   thread_handles = (pthread_t*) malloc (thread_count*sizeof(pthread_t)); 
   subp = malloc(thread_count*sizeof(long));
   isprime = 0;
    
   for (thread = 0; thread < thread_count; thread++)  
      subp[thread] = 0;   
   for (thread = 0; thread < thread_count; thread++)  
      pthread_create(&thread_handles[thread], NULL,
          primecheck, (void*)thread);  
   for (thread = 0; thread < thread_count; thread++) 
      pthread_join(thread_handles[thread], NULL); 
       
   for (thread = 0; thread < thread_count; thread++)  
      isprime = isprime + subp[thread];
      
   if (isprime>0)
      printf("%ld is not a prime.\n", num);
   else         
      printf("%ld is a prime.\n", num);
   free(thread_handles);

   return 0;
}  /* main */

/*------------------------------------------------------------------
 * Function:       Thread_sum
 * Purpose:        Add in the terms computed by the thread running this 
 * In arg:         rank
 * Ret val:        ignored
 * Globals in:     n, thread_count
 * Global in/out:  sum 
 */
void* primecheck(void* rank) {
   long my_rank = (long) rank;
   long my_n = (num-1-1+thread_count)/thread_count;
   long my_first_i = 2 + my_n*my_rank;
   long my_last_i = my_first_i + my_n;
   long i;

   for (i = my_first_i; i < my_last_i; i++) {
      if (i<num){
          if (num%i == 0) {
             printf("Thread %ld: %ld divides %ld \n", my_rank, i, num);  
             subp[my_rank] = 1;
             return NULL;
          }    
      }
   }

   return NULL;
}  /* primecheck*/
