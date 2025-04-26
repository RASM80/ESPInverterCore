//َArd:3.0.1, ESP:v5.2.2
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include <math.h>


#define mcpwm_timer_res 160000000
#define mcpwm_timer_tick_period 3200


#include "SineVal.h"
uint32_t duty_value[3][h_pwm_cycle_count];

static const char *TAG = "Debug";

#define PWM_GPIO 4

#define EXAMPLE_FOC_PWM_UH_GPIO (gpio_num_t)15
#define EXAMPLE_FOC_PWM_UL_GPIO (gpio_num_t)2
#define EXAMPLE_FOC_PWM_VH_GPIO (gpio_num_t)4
#define EXAMPLE_FOC_PWM_VL_GPIO (gpio_num_t)16
#define EXAMPLE_FOC_PWM_WH_GPIO (gpio_num_t)17
#define EXAMPLE_FOC_PWM_WL_GPIO (gpio_num_t)5


float multiplier = 1;
int angle = 0;
mcpwm_timer_handle_t timer = NULL;
mcpwm_cmpr_handle_t comparator[3];
mcpwm_gen_handle_t generators[3][2];

volatile bool update_comparator = true;
volatile uint32_t pwm_cycle_num = 0;


bool IRAM_ATTR on_timer_tez(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_ctx) {
  update_comparator = true;
  pwm_cycle_num++;
  if (pwm_cycle_num >= max_pwm_cyc) pwm_cycle_num = 0;
  return true;  // return true to yield if higher priority tasks are waiting
}


void setup_mcpwm_timer() {

  ESP_LOGI("TAG", "Create timer and operator");
  mcpwm_timer_config_t timer_config = {
    .group_id = 0,
    .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
    .resolution_hz = mcpwm_timer_res,
    .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN,
    .period_ticks = mcpwm_timer_tick_period
  };
  ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

  mcpwm_oper_handle_t oper[3];
  mcpwm_operator_config_t operator_config = {
    .group_id = 0,  // operator must be in the same group to the timer
  };
  for (int i = 0; i < 3; i++) {
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper[i]));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper[i], timer));
  }



  //---------------------

  ESP_LOGI(TAG, "Create comparator and generator from the operator");
  // mcpwm_cmpr_handle_t comparator = NULL;
  mcpwm_comparator_config_t comparator_config = {
    .flags = { .update_cmp_on_tez = true },
  };
  for (int i = 0; i < 3; i++) {
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper[i], &comparator_config, &comparator[i]));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[i], 400));
  }

  int gen_gpios[3][2] = {
    { EXAMPLE_FOC_PWM_UH_GPIO, EXAMPLE_FOC_PWM_UL_GPIO },
    { EXAMPLE_FOC_PWM_VH_GPIO, EXAMPLE_FOC_PWM_VL_GPIO },
    { EXAMPLE_FOC_PWM_WH_GPIO, EXAMPLE_FOC_PWM_WL_GPIO }
  };
  mcpwm_generator_config_t gen_config = {};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 2; j++) {
      gen_config.gen_gpio_num = gen_gpios[i][j];
      ESP_ERROR_CHECK(mcpwm_new_generator(oper[i], &gen_config, &generators[i][j]));
    }
  }

  ESP_LOGI(TAG, "Set generator action on timer and compare event");
  // go high on counter empty
  for (int i = 0; i < 3; i++) {
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generators[i][0],
                                                                 MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[i], MCPWM_GEN_ACTION_HIGH),
                                                                 MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, comparator[i], MCPWM_GEN_ACTION_LOW),
                                                                 MCPWM_GEN_COMPARE_EVENT_ACTION_END()));
  }

  mcpwm_dead_time_config_t dt_config = {
    .posedge_delay_ticks = 5,
  };
  mcpwm_dead_time_config_t inv_dt_config = {
    .negedge_delay_ticks = 5,
    .flags = {.invert_output = true}
  };

  for (int i = 0; i < 3; i++) {
mcpwm_generator_set_dead_time(generators[i][0], generators[i][0], &inv_dt_config);
mcpwm_generator_set_dead_time(generators[i][0], generators[i][1], &dt_config);
  }

  ESP_LOGI(TAG, "Timer TEZ ISR");
  // 2. Register the ISR for TEZ (Timer Event Zero)
  mcpwm_timer_event_callbacks_t cbs = {
    .on_full = NULL,          // Not used in this example
    .on_empty = on_timer_tez  // TEZ event handler
  };
  ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(timer, &cbs, NULL));


  ESP_LOGI(TAG, "Enable and start timer");
  ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
  ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}


void update_comparator_task(void *arg) {
  while (1) {
    if (update_comparator) {
      // ESP_LOGI(TAG, "Angle of rotation %d _ %d", pwm_cycle_num, duty_value[0][pwm_cycle_num]);
      mcpwm_comparator_set_compare_value(comparator[0], (uint32_t)(multiplier*duty_value[0][pwm_cycle_num]));
      mcpwm_comparator_set_compare_value(comparator[1], (uint32_t)(multiplier*duty_value[1][pwm_cycle_num]));
      mcpwm_comparator_set_compare_value(comparator[2], (uint32_t)(multiplier*duty_value[2][pwm_cycle_num]));
      if (pwm_cycle_num >= max_pwm_cyc) pwm_cycle_num = 0;
      update_comparator = false;  // Reset flag after update
    }
  }
}


void setup() {

  pinMode(34, INPUT);
  

  Serial.begin(115200);

  calc_sine_val();


  setup_mcpwm_timer();
  xTaskCreatePinnedToCore(update_comparator_task, "update_comparator_task", 2048, NULL, 5, NULL, 1);  // Pin to core 1
  return;
}




void loop() {

  uint32_t adc = analogRead(34);

  multiplier = ((float)adc / 4095);

  Serial.println(multiplier);

  vTaskDelay(pdMS_TO_TICKS(1));
}