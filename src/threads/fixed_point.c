#include "threads/fixed_point.h"

int conv_int_to_fp(int n) {
    return n * F;
}

int conv_fp_to_int_rnd_zero(int x) {
    return x / F;
}

int conv_fp_to_int_rnd_nearest(int x) {
    if(x >= 0) return (x + F / 2) / F;
    else       return (x - F / 2) / F;
}

int add_fp_and_fp(int x, int y) {
    return x + y;
}

int sub_fp_from_fp(int x, int y) {
    return x - y;
}

int add_fp_and_int(int x, int n) {
    return x + n * F;
}

int sub_int_from_fp(int x, int n) {
    return x - n * F;
}

int mul_fp_by_fp(int x, int y) {
    return ((int64_t) x) * y / F;
}

int mul_fp_by_int(int x, int n) {
    return x * n;
}

int div_fp_by_fp(int x, int y) {
    return ((int64_t) x) * F / y;
}

int div_fp_by_int(int x, int n) {
    return x / n;
}
