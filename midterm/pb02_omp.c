#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

double fit(double x)
{
    // x**3 - 0.8x**2 - 1000x + 8000
    return fabs(8000.0 + x*(-10000.0+x*(-0.8+x)));
}

/* 定義結構體particle */
typedef struct tag_particle{
    double position;  /* 目前位置, 即x value    */
    double velocity;  /* 目前粒子速度           */
    double fitness ;  /* 適應函式值             */
    double pbest_pos; /* particle 目前最好位置  */
    double pbest_fit; /* particle 目前最佳適應值*/
}particle;

// -----------------------------------------------------------------------
// * 全域變數宣告, 較好佈局為拆pos.h / pos.c , 放在pos.c 宣告成static    *
// * 再做set_param function 為入口, 設定所有static param variable        *
// * 或直接用#define 方式, 也需get_gbest 做入口取得全域最佳值            *
// -----------------------------------------------------------------------

double w, c1, c2;                    /* 相關權重參數   */
double max_v;                        /* 最大速度限制   */
double max_pos, min_pos;             /* 最大,小位置限制*/
unsigned particle_cnt;               /* 粒子數量       */
particle gbest;                      /* 全域最佳值     */
int num;
particle* lbest;

// -----------------------------------------------------------------------
// * pso 相關函式宣告                                                    *
// -----------------------------------------------------------------------

#define RND() ((double)rand()/RAND_MAX) /* 產生[0,1] 亂數         */
particle* AllocateParticle();           /* 配置particle_cnt 個粒子*/
void ParticleInit(particle *p);         /* 粒子初始化             */
void ParticleMove(particle *p);         /* 開始移動               */
void ParticleRelease(particle* p);      /* 釋放粒子記憶體         */
void ParticleDisplay(particle* p);      /* 顯示所有粒子資訊       */

//////////////////////////////////////////////////////////////////////////

// 配置particle_cnt 個粒子
particle* AllocateParticle()
{
    return (particle*)malloc(sizeof(particle)*particle_cnt);
}

// 粒子初始化
void ParticleInit(particle *p)
{
    unsigned i;
    const double pos_range = max_pos - min_pos; // 解寬度
    srand((unsigned)time(NULL));

    // 以下程式碼效率不佳, 但較易懂一點
    for(i=0; i<particle_cnt; i++) {
        // 隨機取得粒子位置, 並設為該粒子目前最佳適應值
        p[i].pbest_pos = p[i].position = RND() * pos_range + min_pos; 
        // 隨機取得粒子速度
        p[i].velocity = RND() * max_v;
        // 計算該粒子適應值, 並設為該粒子目前最佳適應值
        p[i].pbest_fit = p[i].fitness = fit(p[i].position);

        // 全域最佳設定
        if(i==0 || p[i].pbest_fit > gbest.fitness) 
            memcpy((void*)&gbest, (void*)&p[i], sizeof(particle));
    }

    for (i=0; i<num; i++)
        memcpy((void*)&lbest[i], (void*)&gbest, sizeof(particle));
}

// 開始移動
void ParticleMove(particle *p)
{
    unsigned i;
    double v, pos;     // 暫存每個粒子之速度, 位置用
    double ppos, gpos; // 暫存區域及全域最佳位置用
    gpos = gbest.position;

    // 更新速度與位置
    for(i=0; i<particle_cnt; i++){
        v = p[i].velocity; // 粒子目前速度
        pos=p[i].position; // 粒子目前位置
        ppos=p[i].pbest_pos; // 粒子目前曾到到最好位置

        v = w*v + c1*RND()*(ppos-pos) + c2*RND()*(gpos-pos); // 更新速度
        if(v<-max_v) v=-max_v;    // 限制最大速度
        else if(v>max_v) v=max_v; // 限制最大速度

        pos = pos + v;               // 更新位置
        if(pos>max_pos) pos=max_pos; // 限制最大位置
        else if(pos<min_pos) pos=min_pos; // 限制最小位置

        p[i].velocity= v;        // 更新粒子速度      
        p[i].position=pos;       // 更新粒子位置
        p[i].fitness = fit(pos); // 更新粒子適應值

        // 更新該粒子目前找過之最佳值
        if(p[i].fitness > p[i].pbest_fit) {
            p[i].pbest_fit = p[i].fitness ;
            p[i].pbest_pos = p[i].position;
        }
#pragma omp critical
        {
            // 更新全域最佳值
            if(p[i].fitness > gbest.fitness) 
                memcpy((void*)&gbest, (void*)&p[i], sizeof(particle));
        }
    }
}

// 釋放粒子記憶體
void ParticleRelease(particle* p)
{
    free(p);
}

// 顯示所有粒子資訊
void ParticleDisplay(particle* p)
{
    /*unsigned i;*/
    //   for(i=0; i<particle_cnt; i++)
    //         printf("#%d : %lf , %lf . %lf\n", i+1, p[i].position, p[i].fitness, p[i].velocity);
    //   puts("------------------------------\n");
    printf("global PSO     solution : %10.6lf , %lf\n", gbest.position, gbest.fitness); 

}

int main(int argc, char *argv[])
{   
    if (argc != 2) {
        printf("Usage: ./pb02_omp [thread_num]\n");
        exit(-1);
    }
    num = atoi(argv[1]);
    omp_set_num_threads(num);
    lbest = (particle*)malloc(sizeof(particle)*num);

    /* 變數宣告*/
    unsigned j;
    unsigned max_itera=1000000;           /* max_itera : 最大演化代數*/
    particle* p;                         /* p         : 粒子群      */

    /* 設定參數*/
    min_pos = -100.0 , max_pos=+100.0;  /* 位置限制, 即解空間限制   */
    w = 1.00, c1=2.0, c2=2.0 ;          /* 慣性權重與加速常數設定   */
    particle_cnt = 200;                  /* 設粒子個數               */
    max_v = (max_pos-min_pos) * 1.0;    /* 設最大速限               */

    /* 開始進行*/
    p = AllocateParticle();      // 配置記憶體
    ParticleInit(p);             // 粒子初始化

    #pragma omp parallel for
    for(j=0; j<max_itera; j++)   // 進行迭代
    {
        unsigned i;
        double v, pos;     // 暫存每個粒子之速度, 位置用
        double ppos, gpos; // 暫存區域及全域最佳位置用
        int id = omp_get_thread_num();
        gpos = lbest[id].position;
        

        // 更新速度與位置
        for(i=0; i<particle_cnt; i++){
            v = p[i].velocity; // 粒子目前速度
            pos=p[i].position; // 粒子目前位置
            ppos=p[i].pbest_pos; // 粒子目前曾到到最好位置

            int seed1;
            int seed2;
            double r1 = (double)rand_r(&seed1)/RAND_MAX;
            double r2 = (double)rand_r(&seed2)/RAND_MAX;
            v = w*v + c1*r1*(ppos-pos) + c2*r2*(gpos-pos); // 更新速度
            if(v<-max_v) v=-max_v;    // 限制最大速度
            else if(v>max_v) v=max_v; // 限制最大速度

            pos = pos + v;               // 更新位置
            if(pos>max_pos) pos=max_pos; // 限制最大位置
            else if(pos<min_pos) pos=min_pos; // 限制最小位置

            p[i].velocity= v;        // 更新粒子速度      
            p[i].position=pos;       // 更新粒子位置
            p[i].fitness = fit(pos); // 更新粒子適應值

            // 更新該粒子目前找過之最佳值
            if(p[i].fitness > p[i].pbest_fit) {
                p[i].pbest_fit = p[i].fitness ;
                p[i].pbest_pos = p[i].position;
            }

            // 更新全域最佳值
            if(p[i].fitness > lbest[id].fitness) 
                memcpy((void*)&lbest[id], (void*)&p[i], sizeof(particle));
        }
    }
    int i;
    for (i = 0; i < num; i++) {
        if (lbest[i].fitness > gbest.fitness)
            memcpy((void*)&gbest, (void*)&lbest[i], sizeof(particle));
    }
    ParticleDisplay(p);          // 顯示最後結果
    ParticleRelease(p);          // 釋放記憶體

    // 暴力取得之較佳值
    printf("optimal solution : %10.6lf , %lf\n", -57.469, fit(-57.469));
    return 0;
}

