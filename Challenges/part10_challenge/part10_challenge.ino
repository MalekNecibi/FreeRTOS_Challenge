// Challenge 10 Implementation : "Arbitrator" mutex
// Malek Necibi
// 10/31/2023

// Prior to reaching for first chopstick, philosopher must first acquire permission from the arbitrator
// Arbitrator allows only 1 philosopher to eat at any given moment
// Permission represented by a mutex
// Downside: Negates all benefits of concurrency/parallelizism, as only 1 task allowed to run at any given moment

// Modified from:
/**
 * ESP32 Dining Philosophers
 * 
 * The classic "Dining Philosophers" problem in FreeRTOS form.
 * 
 * Based on http://www.cs.virginia.edu/luther/COA2/S2019/pa05-dp.html
 * 
 * Date: February 8, 2021
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

// Settings
enum { NUM_TASKS = 5 };             // Number of tasks (philosophers)
enum { TASK_STACK_SIZE = 2048 };    // Bytes in ESP32, words in vanilla FreeRTOS

// Globals
static SemaphoreHandle_t bin_sem;       // Wait for parameters to be read
static SemaphoreHandle_t arbitrator;    // Force 1 task instance at a time
static SemaphoreHandle_t done_sem;      // Notifies main task when done
static SemaphoreHandle_t chopstick[NUM_TASKS];


//*****************************************************************************
// Tasks

// The only task: eating
void eat(void *parameters) {

    int num;
    int left;
    int right;
    char buf[96];
    
    // Copy parameter and increment semaphore count
    num = *(int *)parameters;
    xSemaphoreGive(bin_sem);

    // Alias for left and right chopsticks
    left = num;
    right = (num+1) % NUM_TASKS;
    
    // Ask waiter for permission to eat, limit 10 seconds
    if (!xSemaphoreTake(arbitrator, 10000 / portTICK_PERIOD_MS)) {
        // error: timed out waiting for other philosophers
        sprintf(buf, "WARNING: Philosopher %d timed out waiting for permission", num);
        Serial.println(buf);

    } else {
        // Take left chopstick
        xSemaphoreTake(chopstick[left], portMAX_DELAY);
        sprintf(buf, "Philosopher %d took chopstick %d", num, left);
        Serial.println(buf);

        // Add some delay to force deadlock
        vTaskDelay(1 / portTICK_PERIOD_MS);

        // Take right chopstick
        xSemaphoreTake(chopstick[right], portMAX_DELAY);
        sprintf(buf, "Philosopher %d took chopstick %d", num, right);
        Serial.println(buf);

        // Do some eating
        sprintf(buf, "Philosopher %d is eating", num);
        Serial.println(buf);
        vTaskDelay(10 / portTICK_PERIOD_MS);

        // Put down right chopstick
        xSemaphoreGive(chopstick[right]);
        sprintf(buf, "Philosopher %d returned chopstick %d", num, right);
        Serial.println(buf);

        // Put down left chopstick
        xSemaphoreGive(chopstick[left]);
        sprintf(buf, "Philosopher %d returned chopstick %d\r\n", num, left);
        Serial.println(buf);

        // Notify waiter that next philosopher can eat
        xSemaphoreGive(arbitrator);

        // Notify main task and delete self
        xSemaphoreGive(done_sem);
    }
    
    vTaskDelete(NULL);
}

//*****************************************************************************
// Main (runs as its own task with priority 1 on core 1)

void setup() {

    char task_name[20];

    // turn on LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Configure Serial
    Serial.begin(115200);

    // Wait a moment to start (so we don't miss Serial output)
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println();
    Serial.println("---FreeRTOS Dining Philosophers Challenge---");

    // Create kernel objects before starting tasks
    bin_sem = xSemaphoreCreateBinary();
    arbitrator = xSemaphoreCreateMutex();
    done_sem = xSemaphoreCreateCounting(NUM_TASKS, 0);
    for (int i = 0; i < NUM_TASKS; i++) {
        chopstick[i] = xSemaphoreCreateMutex();
        if (!chopstick[i]) {
            Serial.println("ERROR: failed to initialize chopstick semaphores");
            ESP.restart();
        }
    }
    if (!bin_sem || !arbitrator || !done_sem) {
        Serial.println("ERROR: failed to initialize signaling semaphores");
        ESP.restart();
    }

    // initialize arbitrator for basic use


    // Have the philosphers start eating
    for (int i = 0; i < NUM_TASKS; i++) {
        sprintf(task_name, "Philosopher %d", i);
        xTaskCreatePinnedToCore(eat,
                                task_name,
                                TASK_STACK_SIZE,
                                (void *)&i,
                                1,
                                NULL,
                                app_cpu);
        xSemaphoreTake(bin_sem, portMAX_DELAY);
    }


    // Wait until all the philosophers are done
    for (int i = 0; i < NUM_TASKS; i++) {
        xSemaphoreTake(done_sem, portMAX_DELAY);
    }

    // Say that we made it through without deadlock
    Serial.println("Done! No deadlock occurred!");
}

void loop() {
    // Do nothing in this task
}