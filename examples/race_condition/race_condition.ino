// Replace -Os compile flag with -O0 in platform.txt
// May require re-launching Arduino IDE before taking effect

// Race Condition Example:
// Expected output :  5000000,
// Actual output   : <5000000 

#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#define INCREMENT_LOOP_COUNT (5000000 / 2)

static int global_variable = 0;

void task(void* parameter);
void taskB(void* parameter);

void setup() {
    Serial.begin(115200);
    vTaskDelay(100 / portTICK_PERIOD_MS);    // Serial initialization needs time
    
    // Start Task A
    xTaskCreatePinnedToCore(
        task,
        "Task A",
        4096 + configMINIMAL_STACK_SIZE,
        NULL,
        2,
        NULL,
        app_cpu
    );

    // Start Task B
    xTaskCreatePinnedToCore(
        task,
        "Task B",
        4096 + configMINIMAL_STACK_SIZE,
        NULL,
        2,
        NULL,
        app_cpu
    );
    
    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    // vTaskDelete(NULL);
}

void loop() {
    vTaskDelay (1000 / portTICK_PERIOD_MS);
}

void task(void* parameter) {
    vTaskDelay(100 / portTICK_PERIOD_MS);

    for (size_t i = 0; i < INCREMENT_LOOP_COUNT; i++) {
        global_variable++;
    }
    
    Serial.println(global_variable);

    while(1) { vTaskDelay (1000 / portTICK_PERIOD_MS); }
}