#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
 
int fr(int num)
{
    if (num==0)
      return 0; // base case (termination condition)
    else if (num==1)
      return 1; // base case (termination condition)
    else if (num==2) 
      return 2; // base case (termination condition)
    return (fr(num-1)-2*fr(num-2)+3*fr(num-3)-4);
}

int fr_omp(int n) {
    //Write your openMP code here. Do not forget to return value  
    int i, j, k;
    if (n <= 2)
        return n;
    else {
#pragma omp task shared(i) firstprivate(n)
        i = fr_omp(n-1);
#pragma omp task shared(j) firstprivate(n)
        j = fr_omp(n-2);
#pragma omp task shared(k) firstprivate(n)
        k = fr_omp(n-3);

#pragma omp taskwait
        return i - 2 * j + 3 * k - 4;

    }
}

 
// And a main program to display the first 20 recursive numbers
int main(void)
{
  int count;

  for (count=0; count < 33; ++count)
    printf("%d %d %d\n", count, fr(count), fr_omp(count));
 
  return 0;
}
