#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

static const int led_pin = LED_BUILTIN;

static TickType_t xTicksLEDDuration = 500 / portTICK_PERIOD_MS;

void blinkLED(void* parameter);
void changeBlinkRate(void* parameter);

void setup() {
    pinMode(led_pin, OUTPUT);

    Serial.begin(115200);
    Serial.setTimeout(100);
    vTaskDelay ( 1000 / portTICK_PERIOD_MS );    // Serial initialization needs time
    Serial.flush();
    
//    TaskHandle_t xHandleBlinkLED = NULL;
    
    // Start Blinking LED
    xTaskCreatePinnedToCore(
        blinkLED,
        "Blink LED",
        256 + configMINIMAL_STACK_SIZE,
        &xTicksLEDDuration,
        1,
        NULL,                               // &xHandleBlinkLED,
        app_cpu
    );
    
    // Change Blink Rate from Serial Input
    xTaskCreatePinnedToCore(
        changeBlinkRate,
        "Change Blink Rate",
        256 + configMINIMAL_STACK_SIZE,
        &xTicksLEDDuration,
        1,
        NULL,
        app_cpu
    );

    // vTaskStartScheduler();   // only Vanilla FreeRTOS
    vTaskDelete(NULL);
}

void loop() {}

void blinkLED(void* parameter) {
    if (NULL == parameter) {
        return;
    }
    TickType_t* xTicksLEDDuration = (TickType_t*) parameter;

    while(1) {
        digitalWrite(led_pin, HIGH);
        vTaskDelay( *xTicksLEDDuration );
        
        digitalWrite(led_pin, LOW);
        vTaskDelay( *xTicksLEDDuration );

        // TODO: what if want to update frequency without waiting for previous cycle to complete
        // e.g. accidentally input 100000 ms, don't wait 100 seconds before updating blink rate
    }
}

void changeBlinkRate(void* parameter) {
    if (NULL == parameter) {
        return;
    }
    TickType_t* xTicksLEDDuration = (TickType_t*) parameter;
    int new_frequency;
    
    while (1) {
        // wait for user to input new blink frequency
        if (Serial.available() > 0) {
            new_frequency = Serial.parseInt();
            if (new_frequency > 0) {
                *xTicksLEDDuration = (TickType_t)new_frequency / portTICK_PERIOD_MS;
            }
        }
        // vTaskDelay( 500 / portTICK_PERIOD_MS );
        taskYIELD();
    }
}