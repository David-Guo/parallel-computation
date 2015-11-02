# HW2

## Task-01

`ccntpsieve.c`平行化方法：

1. 申明全局变量 num_thd 从命令行获取线程数；
2. 动态初始化全局变量 sieve 为长度 n+1 的数组；
3. 使用多线程，将 2：n 分区块，指派给 num_thd 个线程
4. 注册线程函数，各个线程在各自区块内异步完成对 sieve 数组的标记。

**注意！需要做好对数组区块的对线程id的分配**
核心函数：
```C
void *primecount(void * rank) {
      int threadid = (long) rank;
      int m, t;
      int step = (n - 2)/num_thd;
      int my_first_i = 2 + threadid * step;
      int my_last_i;
      if (threadid == num_thd - 1)
          my_last_i = my_first_i + step + (n - 2)%num_thd + 1;
      else 
          my_last_i = my_first_i + step;
  
      for (m = my_first_i; m < my_last_i; m++) 
          for (t = 2 * m; t <= n; t += m)
              sieve[t] = 1;                                                  
      return NULL;
}
```

由于只能在遍历一遍 sieve 数组后，才能进行，sum计数，所以sum计算放在mian，效率不高。
num_thd    |    time
--------------- | ----------
1                 | 1
2                 | 2
3                 | 3
