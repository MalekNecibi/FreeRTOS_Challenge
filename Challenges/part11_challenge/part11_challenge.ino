// Challenge 11 : Prevent priority inversion without using a (Priority Inheritance) Mutex
// Protect empty while loops with spinlock critical sections
// Malek Necibi
// 11/01/2023

// Modified from:
/**
 * ESP32 Priority Inversion Demo
 * 
 * Demonstrate priority inversion.
 * 
 * Date: February 12, 2021
 * Author: Shawn Hymel
 * License: 0BSD
 */

// You'll likely need this on vanilla FreeRTOS
//#include <semphr.h>

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#define LOOP 1  // 0

// Settings
TickType_t cs_wait = 250;   // Time spent in critical section (ms)
TickType_t med_wait = 5000; // Time medium task spends working (ms)

// Globals
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

//*****************************************************************************
// Tasks

// Task L (low priority)
void doTaskL(void *parameters) {

    TickType_t timestamp;

    // Do forever
    do {
        // Take lock
        Serial.print("Task L taking spinlock.\r\n");
        portENTER_CRITICAL(&spinlock);
        {
            // Hog the processor for a while doing nothing
            timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
            while ( (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < cs_wait);
            
            Serial.print("Task L releasing spinlock.\r\n");
        } // Release lock
        portEXIT_CRITICAL(&spinlock);
        

        // Go to sleep
        vTaskDelay(500 / portTICK_PERIOD_MS);
    } while (LOOP);
    
    vTaskDelete(NULL);
}

// Task M (medium priority)
void doTaskM(void *parameters) {

    TickType_t timestamp;

    // Do forever
    do {
        // Hog the processor for a while doing nothing
        Serial.print("Task M doing some work...\r\n");
        timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while ( (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < med_wait);

        // Go to sleep
        Serial.print("Task M done!\r\n");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    } while (LOOP);

    vTaskDelete(NULL);
}

// Task H (high priority)
void doTaskH(void *parameters) {

    TickType_t timestamp;

    // Do forever
    do {
        // Take lock
        Serial.print("Task H taking spinlock.\r\n");
        portENTER_CRITICAL(&spinlock);
        {
            // Hog the processor for a while doing nothing
            timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
            while ( (xTaskGetTickCount() * portTICK_PERIOD_MS) - timestamp < cs_wait);
            
            Serial.print("Task H releasing spinlock.\r\n");
        } // Release lock
        portEXIT_CRITICAL(&spinlock);
        

        // Go to sleep
        vTaskDelay(500 / portTICK_PERIOD_MS);
    } while (LOOP);
    
    vTaskDelete(NULL);
}

//*****************************************************************************
// Main (runs as its own task with priority 1 on core 1)

void setup() {
    // visual indicator that upload/reset complete
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    
    // Configure Serial
    Serial.begin(115200);

    // Wait a moment to start (so we don't miss Serial output)
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.print("\r\n---FreeRTOS Priority Inversion : Challenge 11---\r\n");

    // The order of starting the tasks matters to force priority inversion

    // Start Task L (low priority)
    xTaskCreatePinnedToCore(doTaskL,
                          "Task L",
                          2048,
                          NULL,
                          1,
                          NULL,
                          app_cpu);
    
    // Introduce a delay to force priority inversion
    vTaskDelay(1 / portTICK_PERIOD_MS);

    // Start Task H (high priority)
    xTaskCreatePinnedToCore(doTaskH,
                          "Task H",
                          2048,
                          NULL,
                          3,
                          NULL,
                          app_cpu);

    // Start Task M (medium priority)
    xTaskCreatePinnedToCore(doTaskM,
                          "Task M",
                          2048,
                          NULL,
                          2,
                          NULL,
                          app_cpu);

    // Delete "setup and loop" task
    // vTaskDelete(NULL);
}

void loop() {}