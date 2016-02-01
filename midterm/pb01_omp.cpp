#include <iostream>
#include <stdlib.h>
#include <omp.h>
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

int thread_num = 1;

double fit(double x1,double x2) {
    return a*pow((x2-b*x1*x1 + c*x1 -r),2)+s*(1-t)*cos(x1) +s;
}

int main(int argc, char *argv[]){
    if (argc !=2) {
        cout << "Usage: 1pbomp [num]" << endl;
        exit(1);
    }
    
    thread_num = atoi(argv[1]);
    double x1_low,x1_up,x2_low,x2_up;
    x1_low = -5;
    x1_up = 10;
    x2_low = 0;
    x2_up = 15;	
    double step = 1E-3;
    double min_x1,min_x2,min;

    double *themin = (double *) malloc(thread_num * sizeof(double));
    double *min_1 = (double *) malloc(thread_num * sizeof(double));
    double *min_2 = (double *) malloc(thread_num * sizeof(double));

    min = fit(x1_low ,x2_low);
    clock_t start,end;
    start = clock();
    omp_set_num_threads(thread_num);

#pragma omp parallel
    {
        int id = omp_get_thread_num();
        themin[id] = min;
        double temp;
        double x1, x2;
#pragma omp for
        for (int i = 0; i < 15000; i++) {
            x1 = x1_low + (double)i * step;
            for(x2= x2_low; x2<= x2_up; x2+=step){
                temp = a*pow((x2-b*x1*x1 + c*x1 -r),2)+s*(1-t)*cos(x1) +s;
                if(temp  < themin[id]){
                    min_1[id] = x1;
                    min_2[id] = x2;
                    themin[id] = temp;
                }
            }
        }
    }

    end = clock();
    cout.precision(5);
    
    min = themin[0];
    min_x1 = min_1[0];
    min_x2 = min_2[0];
    for (int i = 1; i < thread_num; i++)
        if (min > themin[i]) {
            min = themin[i];
            min_x1 = min_1[i];
            min_x2 = min_2[i];
        }


    cout << "max fit =  fit(" << min_x1 << ","<< min_x2<<") = " << min << endl;
    cout << "time = " << double(end-start)/CLOCKS_PER_SEC/thread_num << " sec"<< endl;

    return 0;
}
