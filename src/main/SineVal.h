

#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559

#define grid_freq 50
#define pwm_freq 100000
#define pwm_cycle_count (int)(pwm_freq / grid_freq)
#define h_pwm_cycle_count (int)(pwm_cycle_count / 2)
#define max_pwm_cyc (h_pwm_cycle_count)

#define HIGH_THRESHOLD 95.0
#define LOW_THRESHOLD 5.0


//duty value for pwm_cycle_count cycles for each switch
extern uint32_t duty_value[3][h_pwm_cycle_count];


void calc_sine_val() {
  double sin_val[3];
  for (int i = 0; i < h_pwm_cycle_count; i++) {
    for (int j = 0; j < 3; j++) {
      switch (j) {
        case 0:
          sin_val[j] = sin((2*PI * i / h_pwm_cycle_count)) + 1;
          break;
        case 1:
          sin_val[j] = sin(fmod(((2*PI * i / h_pwm_cycle_count) + (2 * PI / 3)), 2*PI)) + 1;
          break;
        case 2:
          sin_val[j] = sin(fmod(((2*PI * i / h_pwm_cycle_count) + (4 * PI / 3)), 2*PI)) + 1;
          break;
      }
      // sin_val[j] = fmod(sin_val[j], PI);
      sin_val[j] *= (double)mcpwm_timer_tick_period;
      sin_val[j] /= 4; // /2 forUP_DOWN Timer & 2/ for scaling sin + 1

      duty_value[j][i] = (uint32_t)(sin_val[j] - fmod(sin_val[j], 1.0));
      // duty_value[j][i + h_pwm_cycle_count] = (mcpwm_timer_tick_period/2) - duty_value[j][i] ;
    }

  }
}


// void validate_duty_cycles() {
//   for (int i = 0; i < h_pwm_cycle_count; i++) {
//     for (int j = 0; j < 3; j++) {
//       if (duty_value[j][i] > HIGH_THRESHOLD)
//         duty_value[j][i] = 100.0;
//       else if (duty_value[j][i] < LOW_THRESHOLD)
//         duty_value[j][i] = 0.0;
//     }
//   }
// }


