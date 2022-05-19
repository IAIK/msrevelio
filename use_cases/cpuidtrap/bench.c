#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

uint64_t rdtscnf() {
  uint64_t a, d;
  asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
  a = (d << 32) | a;
  return a;
}

#define AVERAGE 100000

size_t measurements[AVERAGE];

size_t mean(size_t* val, size_t len) {
    size_t sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum += val[i];
    }
    return sum / len;
}

double standard_error(size_t* val, size_t len) {
    size_t m = mean(val, len);
    size_t err_sum = 0;
    for(size_t i = 0; i < len; i++) {
        err_sum += (val[i] - m) * (val[i] - m);
    }
    double stddev = sqrt((double)err_sum / (double)(len - 1));
    //printf("sqrt(%f) = %f\n", (double)err_sum / (double)(len - 1), stddev);
    return stddev / sqrt(len);
}

void rdrand(){
    for(int i = 0; i < AVERAGE; i++) {
        size_t start = rdtscnf();
        uint64_t random_number;
        asm volatile("rdrand %0":"=r"(random_number):);
        size_t end = rdtscnf();
        measurements[i] = end - start;
    }
}

void cpuid(){
    for(int i = 0; i < AVERAGE; i++) {
        size_t start = rdtscnf();
        unsigned int model;
        int rax, rbx, rcx, rdx;
        asm volatile("cpuid" : "+a"(rax), "+b"(rbx), "+c"(rcx), "+d"(rdx));
        size_t end = rdtscnf();
        measurements[i] = end - start;
    }
}


int main(int argc, char *argv[]) {
    printf("hi!\n");
    
    if (argc != 3) {
      printf("usage: %s [rdrand=0|cpuid=1] trap\n", argv[0]);
      return -1;
    }

    int bench = atoi(argv[1]);
    int trap = atoi(argv[2]);
  
    while (1) {
      if (bench == 0) {
        rdrand();
        printf("RDRAND took %zd cycles (+/- %.1f)\n", mean(measurements, AVERAGE), standard_error(measurements, AVERAGE));
      } else {
        cpuid();
        printf("CPUID took %zd cycles (+/- %.1f)\n", mean(measurements, AVERAGE), standard_error(measurements, AVERAGE));
      }
      
    }
    
    printf("done\n");
    return 0;
}
