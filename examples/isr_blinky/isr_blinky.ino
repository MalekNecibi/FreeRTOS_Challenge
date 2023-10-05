// Blink LED every 1 second
// Modified from DigiKey FreeRTOS YouTube Series - Part 9
// https://www.youtube.com/watch?v=qsflCf6ahXU

// Malek Necibi

#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

// trigger ISR after one second
#define CLOCK_SPEED_HZ                  (80 * 1000 * 1000)  // Based on 80 MHz Clock
static const uint16_t timer_divider   = 80;                 // every N clock_ticks, increment by one
static const uint64_t timer_max_count = 1000 * 1000;        // after M increments, trigger ISR
// double isr_trigger_freq_hz = (double) CLOCK_SPEED_HZ / (timer_divider * timer_max_count)
static hw_timer_t* timer = NULL;

static const int led_pin = LED_BUILTIN;

// defining ISR in internal RAM, rather than FLASH (for speed)
void IRAM_ATTR onTimer(void) {
    // invert led state
    int pin_state = digitalRead(led_pin);
    digitalWrite(led_pin, !pin_state);
}

void setup() {
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, HIGH);
    
    // https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/timer.html
    
    // every N ticks of, increment 64bit counter
    // hw_timer_t * timerBegin(uint8_t num, uint16_t divider, bool countUp);
    timer = timerBegin(0, timer_divider, true);
    
    // what action to perform when counter reaches target
    // void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge);
    timerAttachInterrupt(timer, &onTimer, true);
    
    // when counter reaches target, trigger ISR, and auto-restart increment process
    // void timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload);
    timerAlarmWrite(timer, timer_max_count, true);
    
    // void timerAlarmEnable(hw_timer_t *timer);
    timerAlarmEnable(timer);
    
    
    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    vTaskDelete(NULL);
}

void loop() {}