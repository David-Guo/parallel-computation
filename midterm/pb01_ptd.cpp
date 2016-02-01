#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
using namespace std;
#define pi  3.141592653589793
double a=1;
double b = 5.1/(4*pi*pi);
double c = 5/pi;
double r = 6;
double s = 10;
double t = 1/(8*pi);

double x1_low = -5;
double x1_up = 10;
double x2_low = 0;
double x2_up = 15;	
double step = 1E-3;

int thread_num = 1;
double *themin;
double *min_x1;
double *min_x2;

double fit(double x1,double x2) {
    return a*pow((x2-b*x1*x1 + c*x1 -r),2)+s*(1-t)*cos(x1) +s;
}

void *threadCalcu(void *rank) {
    long threadId = (long) rank;
    double dist = 15000/thread_num;
    double my_first_x1 = threadId * dist;
    double my_last_x1 = my_first_x1 + dist;
    double temp;
    double x1, x2;

    for(int i = my_first_x1; i < my_last_x1; i++){
        x1 = x1_low + (double) i * step;
        for(x2= x2_low; x2<= x2_up; x2+=step){
            temp = fit(x1, x2);
            if(temp < themin[threadId]){
                min_x1[threadId] = x1;
                min_x2[threadId] = x2;
                themin[threadId] = temp;
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]){

    if (argc !=2) {
        cout << "Usage: pb01_ptd [num]" << endl;
        exit(1);
    }

    thread_num = atoi(argv[1]);

    pthread_t * thread_p;
    thread_p = (pthread_t *) malloc(thread_num * sizeof(pthread_t));
    themin = (double *) malloc(thread_num * sizeof(double));
    min_x1 = (double *) malloc(thread_num * sizeof(double));
    min_x2 = (double *) malloc(thread_num * sizeof(double));
    double temp = fit(x1_low, x2_low);
    for (int i = 0; i < thread_num; i++) {
        themin[i] = temp;
        min_x1[i] = x1_low;
        min_x2[i] = x2_low;
    }
        

    clock_t start,end;
    start = clock();

    for (long i = 0; i < thread_num; i++)
        pthread_create(&thread_p[i], NULL, threadCalcu, (void *)i);

    for (int i = 0; i < thread_num; i++)
        pthread_join(thread_p[i], NULL);

    end = clock();
    cout.precision(5);

    double min = themin[0];
    double min1 = min_x1[0];
    double min2 = min_x2[0];
    for (int i = 1; i < thread_num; i++)
        if (min > themin[i]) {
            min = themin[i];
            min1 = min_x1[i];
            min2 = min_x2[i];
        }

    cout << "max fit =  fit(" << min1 << ","<< min2<<") = " << min << endl;
    cout << "time = " << double(end-start)/CLOCKS_PER_SEC/thread_num << " sec"<< endl;

    return 0;
}
