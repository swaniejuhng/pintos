#include <stdio.h>
#include <stdlib.h>

#define F 16384

int conv_int_to_fp(int n);
int conv_fp_to_int_rnd_zero(int x);
int conv_fp_to_int_rnd_nearest(int x);
int add_fp_and_fp(int x, int y);
int sub_fp_from_fp(int x, int y);
int add_fp_and_int(int x, int n);
int sub_int_from_fp(int x, int n);
int mul_fp_by_fp(int x, int y);
int mul_fp_by_int(int x, int n);
int div_fp_by_fp(int x, int y);
int div_fp_by_int(int x, int n);
