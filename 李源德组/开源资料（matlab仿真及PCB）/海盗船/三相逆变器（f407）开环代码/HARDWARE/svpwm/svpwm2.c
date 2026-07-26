#include <math.h>
#include <arm_math.h>
#include <stdint.h>
#include <svpwm2.h>

float32_t t_a;
float32_t t_b;
float32_t t_c;

extern float32_t U_alpha, U_beta;

void svpwm2() {
    const float32_t ts = 1;

    float32_t u1 = U_beta;
    float32_t u2 = -0.8660254037844386 * U_alpha - 0.5 * U_beta;
    float32_t u3 = 0.8660254037844386 * U_alpha - 0.5 * U_beta;
	float32_t k_svpwm;

    uint8_t sector = (u1 > 0.0) + ((u2 > 0.0) << 1) + ((u3 > 0.0) << 2);

    if (sector == 5) {
      float32_t t4 = u3;
      float32_t t6 = u1;
      float32_t sum = t4 + t6;
      if (sum > ts) {
        k_svpwm = ts / sum;
        t4 = k_svpwm * t4;
        t6 = k_svpwm * t6;
      }
      float32_t t7 = (ts - t4 - t6) / 2;
      t_a = t4 + t6 + t7;
      t_b = t6 + t7;
      t_c = t7;
    } else if (sector == 1) {
      float32_t t2 = -u3;
      float32_t t6 = -u2;
      float32_t sum = t2 + t6;
      if (sum > ts) {
        k_svpwm = ts / sum;
        t2 = k_svpwm * t2;
        t6 = k_svpwm * t6;
      }
      float32_t t7 = (ts - t2 - t6) / 2;
      t_a = t6 + t7;
      t_b = t2 + t6 + t7;
      t_c = t7;
    } else if (sector == 3) {
      float32_t t2 = u1;
      float32_t t3 = u2;
      float32_t sum = t2 + t3;
      if (sum > ts) {
        k_svpwm = ts / sum;
        t2 = k_svpwm * t2;
        t3 = k_svpwm * t3;
      }
      float32_t t7 = (ts - t2 - t3) / 2;
      t_a = t7;
      t_b = t2 + t3 + t7;
      t_c = t3 + t7;
    } else if (sector == 2) {
      float32_t t1 = -u1;
      float32_t t3 = -u3;
      float32_t sum = t1 + t3;
      if (sum > ts) {
        k_svpwm = ts / sum;
        t1 = k_svpwm * t1;
        t3 = k_svpwm * t3;
      }
      float32_t t7 = (ts - t1 - t3) / 2;
      t_a = t7;
      t_b = t3 + t7;
      t_c = t1 + t3 + t7;
    } else if (sector == 6) {
      float32_t t1 = u2;
      float32_t t5 = u3;
      float32_t sum = t1 + t5;
      if (sum > ts) {
        k_svpwm = ts / sum;
        t1 = k_svpwm * t1;
        t5 = k_svpwm * t5;
      }
      float32_t t7 = (ts - t1 - t5) / 2;
      t_a = t5 + t7;
      t_b = t7;
      t_c = t1 + t5 + t7;
    } else if (sector == 4) {
      float32_t t4 = -u2;
      float32_t t5 = -u1;
      float32_t sum = t4 + t5;
      if (sum > ts) {
        k_svpwm = ts / sum;
        t4 = k_svpwm * t4;
        t5 = k_svpwm * t5;
      }
      float32_t t7 = (ts - t4 - t5) / 2;
      t_a = t4 + t5 + t7;
      t_b = t7;
      t_c = t5 + t7;
    }
  }

