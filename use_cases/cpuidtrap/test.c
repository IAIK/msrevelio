#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <cpuid.h>

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
    printf("sqrt(%f) = %f\n", (double)err_sum / (double)(len - 1), stddev);
    return stddev / sqrt(len);
}

int main() {
    printf("hi!\n");
    
    printf("Brand string: \n");
    unsigned int model;
    int name[4] = {0, 0, 0, 0};
    __cpuid(0, model, name[0], name[2], name[1]);
    printf("%s\n", name);
    
    printf("\nNow benchmarking\n");
    
    for(int i = 0; i < AVERAGE; i++) {
        size_t start = rdtscnf();
//         asm volatile("cpuid" : : "a"(0), "b"(0), "c"(0), "d"(0));
        __cpuid(0, model, name[0], name[2], name[1]);
        size_t end = rdtscnf();
        measurements[i] = end - start;
    }
    printf("CPUID took %zd cycles (+/- %.1f)\n", mean(measurements, AVERAGE), standard_error(measurements, AVERAGE));
    printf("done\n");
    return 0;
}
