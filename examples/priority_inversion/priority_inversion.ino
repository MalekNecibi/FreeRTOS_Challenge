// Priority Inversion (example)
// 3 tasks with different priorities (T1, T2, T3)
// 1 shared resource (resource)
// T1 and T3 have a common shared resource
// T1 running, takes resource
// T3 ready, but blocked until resource freed by T1
// T1 continues running
// T2 ready, runs as higher priority
// T2 runs for long time, blocking T3 indefinitely

// Showcases difference between Mutex and Binary Semaphore (setup line 7)

#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

static SemaphoreHandle_t resource = NULL;

void task1(void* parameter);
void task2(void* parameter);
void task3(void* parameter);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);
    vTaskDelay(500 / portTICK_PERIOD_MS);    // Serial initialization needs time
    
    // resource = xSemaphoreCreateMutex();
    resource = xSemaphoreCreateBinary();
    if (!resource) {
        Serial.println("ERROR: failed to create semaphore");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP.restart();
    }
    xSemaphoreGive(resource);

    // Start Tasks
    xTaskCreatePinnedToCore(
        task1,
        "Task 1",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        1,
        NULL,
        app_cpu
    );

    // allow task1 time to take shared resource
    vTaskDelay(200 / portTICK_PERIOD_MS);

    xTaskCreatePinnedToCore(
        task3,
        "Task 3",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        3,
        NULL,
        app_cpu
    );

    // allow time for task3 to defer to task1
    vTaskDelay(200 / portTICK_PERIOD_MS);

    xTaskCreatePinnedToCore(
        task2,
        "Task 2",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        2,
        NULL,
        app_cpu
    );

    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    vTaskDelete(NULL);
}

void loop() {}

void task1(void* parameter) {
    Serial.println("task1 start");

    // take resource
    xSemaphoreTake(resource, portMAX_DELAY);
    Serial.println("task1 acquired shared resource");

    TickType_t start = xTaskGetTickCount();

    // allow other tasks to take precedence
    while((xTaskGetTickCount()-start)/portTICK_PERIOD_MS < 500) {
        Serial.println("task1 in critical section");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    // release resource
    Serial.println("task1 freeing shared resource");
    xSemaphoreGive(resource);
    taskYIELD();

    Serial.println("task1 doing other stuff");
    
    Serial.println("task1 ending");
    vTaskDelete(NULL);
}

void task2(void* parameter) {
    Serial.println("task2 start");
    
    TickType_t start = xTaskGetTickCount();
    
    // allow task3 to start then defer to task1
    // vTaskDelay(100 / portTICK_PERIOD_MS);

    Serial.println("task2 wasting time...");
    // run for a long time (10 seconds)
    while((xTaskGetTickCount()-start)/portTICK_PERIOD_MS < 10000) {
        // Serial.println("in task2");
    }
    
    Serial.println("task2 ending");
    vTaskDelete(NULL);
}

void task3(void* parameter) {
    Serial.println("task3 start");
    
    // save start time
    TickType_t start = xTaskGetTickCount();
    
    // try to take resource
    xSemaphoreTake(resource, portMAX_DELAY);

    // print time delta when resource acquired
    double delta_s = ((xTaskGetTickCount()-start) / portTICK_PERIOD_MS) / 1000.0;
    Serial.print("  task3 wasted ");
    Serial.print(delta_s);
    Serial.println(" seconds waiting to run");

    // use resource for short time
    Serial.println("task3 in critical section");

    // free resource
    Serial.println("task3 freeing shared resource");
    xSemaphoreGive(resource);
    taskYIELD();

    Serial.println("task3 doing other stuff");

    Serial.println("task3 ending");
    vTaskDelete(NULL);
}
