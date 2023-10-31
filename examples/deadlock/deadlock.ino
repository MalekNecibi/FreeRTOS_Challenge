#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

struct philosopher {
    char* name;
    SemaphoreHandle_t left;
    SemaphoreHandle_t right;
};

static const int led_pin = LED_BUILTIN;

static char *names[] = {
    "Sor Juana Ines de la Cruz",
    "Frederick Douglass",
    "Hypatia",
    "Kong Qiu",
    "Aristotle",
};
#define NUM_PHILOSOPHERS (sizeof(names) / sizeof(names[0]))
static SemaphoreHandle_t chopsticks[NUM_PHILOSOPHERS];
static struct philosopher philosophers[NUM_PHILOSOPHERS];


/// Function Declarations

// Tasks
void eat(void* parameter);

// Global Helper Functions
void abortError(const char* msg, size_t delay_ms);

// Local Helper Functions
bool initPhilosopher(struct philosopher *p, char* name, SemaphoreHandle_t left, SemaphoreHandle_t right);


/// Function Implementations

void setup() {
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, HIGH);

    Serial.begin(115200);
    vTaskDelay(500 / portTICK_PERIOD_MS);    // Serial initialization takes time
    Serial.println();

    // initialize shared resources
    for (size_t i = 0; i < NUM_PHILOSOPHERS; i++) {
        chopsticks[i] = xSemaphoreCreateBinary();
        if (!chopsticks[i]) {
            abortError("ERROR: failed to create all chopstick semaphores", 1000);
        }
        xSemaphoreGive(chopsticks[i]);
    }

    // Initialize Philosopher info
    for (size_t i = 0; i < NUM_PHILOSOPHERS; i++) {
        int left = i;
        int right = (i+1) % NUM_PHILOSOPHERS;
        if (!initPhilosopher(&(philosophers[i]), names[i], chopsticks[left], chopsticks[right])) {
            abortError("ERROR: Failed to initialize philosopher", 1000);
        }
    }

    // Start Tasks
    for (size_t i = 0; i < NUM_PHILOSOPHERS; i++) {
        xTaskCreatePinnedToCore(
            eat,
            philosophers[i].name,
            4096 + configMINIMAL_STACK_SIZE,
            &(philosophers[i]),
            1,
            NULL,
            app_cpu
        );
    }

    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    // vTaskDelete(NULL);
}

void loop() {}  // unused but available

void eat(void* parameter) {
    struct philosopher *diner = (struct philosopher*) parameter;
    char printBuffer[96];
    
    // allow all tasks to spin up
    vTaskDelay(250 / portTICK_PERIOD_MS);
    
    while(1) {
        sprintf(printBuffer, "%32s wants their left chopstick... (%p)\r\n", diner->name, diner->left);
        Serial.print(printBuffer);
        xSemaphoreTake(diner->left, portMAX_DELAY);
        sprintf(printBuffer, "%32s took their left chopstick\r\n", diner->name);
        Serial.print(printBuffer);

        // taskYIELD();    // simulated context switch

        sprintf(printBuffer, "%32s wants their right chopstick... (%p)\r\n", diner->name, diner->right);
        Serial.print(printBuffer);
        xSemaphoreTake(diner->right, portMAX_DELAY);
        sprintf(printBuffer, "%32s took their right chopstick\r\n", diner->name);
        Serial.print(printBuffer);

        // taskYIELD();    // simulated context switch

        sprintf(printBuffer, "%.32s is eating...\r\n", diner->name);
        Serial.print(printBuffer);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);

        xSemaphoreGive(diner->left);
        xSemaphoreGive(diner->right);

        sprintf(printBuffer, "%.32s is done eating\r\n", diner->name);
        Serial.print(printBuffer);
    }
}


// Global Helper Functions
void abortError(const char* msg, size_t delay_ms) {
    Serial.println(msg);
    if (delay_ms > 0) {
        // prevent yielding if no delay provided
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);
    }
    ESP.restart();
}

// Local Helper Functions
bool initPhilosopher(struct philosopher *p, char* name, SemaphoreHandle_t left, SemaphoreHandle_t right) {
    if (!p || !name || !left || !right) {
        // Serial.println("ERROR: Failed to initialize philosopher");
        return false;
    }

    p->name = name;
    p->left = left;
    p->right = right;
    return true;
}