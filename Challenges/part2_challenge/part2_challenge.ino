#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

static const int led_pin = LED_BUILTIN;

static TickType_t xTicksLEDDuration_A = 321 / portTICK_PERIOD_MS;
static TickType_t xTicksLEDDuration_B = 512 / portTICK_PERIOD_MS;

void toggleLED(void* parameter);

void setup() {
    // Serial.begin(9600);
    // vTaskDelay ( 1000 / postTICK_PERIOD_MS );    // Serial setup needs time
    pinMode(led_pin, OUTPUT);
    
    // TaskHandle_t xHandleLED_A = NULL;
    // TaskHandle_t xHandleLED_B = NULL;
    
    // Start Blinking LED : A
    xTaskCreatePinnedToCore(
        toggleLED,
        "Blink LED : rate A",
        256 + configMINIMAL_STACK_SIZE,
        &xTicksLEDDuration_A,
        1,
        NULL,                               // &xHandleLED_A,
        app_cpu
    );
    
    // Start Blinking LED : B
    xTaskCreatePinnedToCore(
        toggleLED,
        "Blink LED : rate B",
        256 + configMINIMAL_STACK_SIZE,
        &xTicksLEDDuration_B,
        1,
        NULL,                               // &xHandleLED_B,
        app_cpu
    );    
}

void loop() {}

void toggleLED(void* parameter) {
    if (NULL == parameter) {
        return;
    }
    TickType_t xTicksLEDDuration = *((TickType_t*)parameter);

    while(1) {
        digitalWrite(led_pin, HIGH);
        vTaskDelay( xTicksLEDDuration );

        digitalWrite(led_pin, LOW);
        vTaskDelay( xTicksLEDDuration );
    }
}
