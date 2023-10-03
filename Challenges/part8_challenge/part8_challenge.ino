#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

static const int led_pin = LED_BUILTIN;
static const int led_backlight_freq = 5000 / portTICK_PERIOD_MS;
static TimerHandle_t serial_led_backlight_timer = NULL;


void taskSerialLoopback(void* parameter);
void SerialLEDTimerCallback(TimerHandle_t xTimer);
void updateLEDBacklight();


void setup() {
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, HIGH);

    Serial.begin(115200);
    vTaskDelay(500 / portTICK_PERIOD_MS);    // Serial initialization needs time
    
    Serial.println("---- FreeRTOS Timer Solution : Malek ----");
    
    // create LED Serial Backlight timer
    serial_led_backlight_timer = xTimerCreate(
        "Serial LED Timer",
        led_backlight_freq,
        false,
        (void*) 0,
        SerialLEDTimerCallback
    );

    if ( !serial_led_backlight_timer ) {
        Serial.println("ERROR: Failed to create Serial LED Timer");
    
    } else {
        xTimerStart(serial_led_backlight_timer, portMAX_DELAY);
    }

    
    // TODO : No reason this can't just run in loop()
    // start serial loopback
    xTaskCreatePinnedToCore(
        taskSerialLoopback,
        "Serial Loopback Task",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        1,
        NULL,
        app_cpu
    );
    
    Serial.println("Serial Loopback Ready:");
    
    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    vTaskDelete(NULL);
}

void loop() {}


void taskSerialLoopback(void* parameter) {
    static char newChar;

    // loop check for new characters
    while(1) {
        // if new char
        if (Serial.available() > 0) {
            updateLEDBacklight();
            
            // serial loopback
            newChar = Serial.read();
            Serial.print(newChar);
        }
    }
}

void SerialLEDTimerCallback(TimerHandle_t xTimer) {
    // turn off led
    digitalWrite(led_pin, LOW);
}

void updateLEDBacklight() {
    // turn on led
    digitalWrite(led_pin, HIGH);
    
    // restart led timer
    xTimerStart(serial_led_backlight_timer, led_backlight_freq);
}