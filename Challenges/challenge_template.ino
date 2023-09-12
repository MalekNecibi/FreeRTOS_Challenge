#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

void taskA(void* parameter);

void setup() {
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);    // Serial initialization needs time
    
    // Start Task
    xTaskCreatePinnedToCore(
        taskA,
        "Task A",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        1,
        NULL,
        app_cpu
    );

    
    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    vTaskDelete(NULL);
}

void loop() {}

void taskA(void* parameter) {
    if (NULL == parameter) {
        return;
    }
    void* pVariable = (void*)parameter;
    
    while(1) {
        taskYIELD();
    }
}
