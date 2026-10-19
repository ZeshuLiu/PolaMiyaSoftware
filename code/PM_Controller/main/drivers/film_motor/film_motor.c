#include "film_motor.h"

#include <inttypes.h>

#include "PIN.h"
#include "driver/ledc.h"
#include "esp_check.h"

/* Backlight owns timer/channel 0. Keep the motor on its own timer/channel. */
#define MOTOR_MODE       LEDC_LOW_SPEED_MODE
#define MOTOR_TIMER      LEDC_TIMER_1
#define MOTOR_CHANNEL    LEDC_CHANNEL_1
#define MOTOR_PWM_STEPS  (1U << 10)

static const char *TAG = "film_motor";
static bool ready;

esp_err_t film_motor_init(void)
{
    /* Establish braking before connecting PWM to IN1 at zero drive duty. */
    ESP_RETURN_ON_ERROR(gpio_set_level(PIN_MOTORPWM_A, 1), TAG, "IN1 idle");
    ESP_RETURN_ON_ERROR(gpio_set_level(PIN_MOTOR_IN2, 1), TAG, "IN2 idle");
    const gpio_config_t pins = {
        .pin_bit_mask = (1ULL << PIN_MOTORPWM_A) | (1ULL << PIN_MOTOR_IN2),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pins), TAG, "motor GPIO");

    const ledc_timer_config_t timer = {
        .speed_mode = MOTOR_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = MOTOR_TIMER,
        .freq_hz = FILM_MOTOR_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "motor PWM timer");
    const ledc_channel_config_t channel = {
        .gpio_num = PIN_MOTORPWM_A,
        .speed_mode = MOTOR_MODE,
        .channel = MOTOR_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = MOTOR_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 1,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "motor PWM channel");
    ready = true;
    ESP_LOGI(TAG, "ready: IN1=GPIO%d inverted PWM, IN2=GPIO%d high; %d Hz, "
             "run IN1 high=%d%%, start/stop ramp=%d ms",
             PIN_MOTORPWM_A, PIN_MOTOR_IN2, FILM_MOTOR_PWM_HZ,
             FILM_MOTOR_IN1_HIGH_PERCENT, FILM_MOTOR_RAMP_MS);
    return ESP_OK;
}

esp_err_t film_motor_set_running(bool running)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    /* Freeze an unfinished ramp before changing its target, retaining the
     * current duty. The LCD initialization installs the shared fade service. */
    ESP_RETURN_ON_ERROR(ledc_fade_stop(MOTOR_MODE, MOTOR_CHANNEL), TAG, "stop previous ramp");
    /* LEDC output is inverted: convert the physical IN1 high-level percentage
     * to the internal drive duty. 0% high maps to 1024 (IN1 always low);
     * stop maps to 0 (IN1 always high). */
    const uint32_t duty = running
                              ? MOTOR_PWM_STEPS * (100 - FILM_MOTOR_IN1_HIGH_PERCENT) / 100
                              : 0;
    /* Read the hardware duty after freezing the fade. A later stop command
     * also records the running endpoint, without sampling from an ISR. */
    const uint32_t current_duty = ledc_get_duty(MOTOR_MODE, MOTOR_CHANNEL);
    ESP_LOGI(TAG, "%s: internal duty=%" PRIu32 "/%u -> %" PRIu32 "/%u, "
             "PWM=%" PRIu32 " Hz, ramp=%d ms",
             running ? "start" : "stop", current_duty, MOTOR_PWM_STEPS,
             duty, MOTOR_PWM_STEPS, ledc_get_freq(MOTOR_MODE, MOTOR_TIMER),
             FILM_MOTOR_RAMP_MS);
    return ledc_set_fade_time_and_start(MOTOR_MODE, MOTOR_CHANNEL, duty,
                                       FILM_MOTOR_RAMP_MS, LEDC_FADE_NO_WAIT);
}
