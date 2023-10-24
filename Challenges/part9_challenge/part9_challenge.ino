
// Task : Using hardware interrupts, poll an Analog Pin every 100ms, and output
//        the average over the past 1 second when requested via "avg" command over
//        serial, while also simulating a Serial loopback adapter


// Modified from DigiKey FreeRTOS Educational Guide : Challenge 9
// https://www.youtube.com/watch?v=qsflCf6ahXU

// Malek Necibi
// 10/05/2023

/*
Hardware Timer ISR
    ISR samples from ADC 10x per second (8, 1_000_000)
    samples read into a double/circular buffer
    once ten (new unread) samples are connected (1sec)...
        ISR wakes up tasks A

Task A
    Computes average of 10 (oldest unread) samples
    Write updated average to global variable average
        must be protected with mutex/spinlock (not atomic operation)
    
Task B
    Listen to serial terminal
    Echo back any chars received
    if command 'avg' received, print out the most recently computed average (allowing up to 0.99 second incorrectness)
*/


#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#define CLOCK_SPEED_HZ                  80000000            // Based on 80 MHz Clock
// trigger ISR every 100 ms
static const uint16_t timer_divider   = 8;                  // every N clock_ticks, increment by one
static const uint64_t timer_max_count = 1000 * 1000;        // after M increments, trigger ISR

#define SAMPLES_PER_FRAME               10
#define BUFFER_LENGTH                   ( 2 * SAMPLES_PER_FRAME )
#define DTYPE                           float

// IO Pins
static const int adc_pin = A0;

// Globals
static hw_timer_t* timer = NULL;
static SemaphoreHandle_t adc_new_frame = NULL;
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

static volatile DTYPE buffer[ BUFFER_LENGTH ];
static volatile DTYPE average = 0.0;


/// Function Signatures ///
// ISR
void IRAM_ATTR onTimer(void);

// Tasks
void computeAverage(void* param);
void serialInterface(void* param);


/// Function Definitions ///
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // pinMode(adc_pin, INPUT);
    
    Serial.begin(115200);
    Serial.setTimeout(10);
    vTaskDelay(1000 / portTICK_PERIOD_MS);    // Serial initialization needs time
    Serial.println();
    Serial.println("---- FreeRTOS Challenge 9 : Malek ----");
    
    
    adc_new_frame = xSemaphoreCreateBinary();
    if (!adc_new_frame) {
        Serial.println("ERROR: Could not create ADC Frame Signaling Semaphore");
        ESP.restart();
    }
    // redundant but safer
    xSemaphoreGive(adc_new_frame);
    xSemaphoreTake(adc_new_frame, portMAX_DELAY);

    // Start Tasks
    xTaskCreatePinnedToCore(
        computeAverage,
        "Compute ADC Average",
        512 + configMINIMAL_STACK_SIZE,
        NULL,
        2,
        NULL,
        app_cpu
    );

    xTaskCreatePinnedToCore(
        serialInterface,
        "Serial Interface",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        1,
        NULL,
        app_cpu
    );
    
    
    // Start ISR
    // https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/timer.html

    // every N ticks, increment 64bit counter
    timer = timerBegin(0, timer_divider, true);
    
    // what action to perform when counter reaches target
    timerAttachInterrupt(timer, &onTimer, true);
    
    // when counter reaches target, trigger ISR, and auto-restart increment process
    timerAlarmWrite(timer, timer_max_count, true);
        
    timerAlarmEnable(timer);
    
    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    // vTaskDelete(NULL);
}

void loop() {
    // unused, but available
}

// ISR implementation
void IRAM_ATTR onTimer(void) {
    // ISR samples from ADC 10x per second (8, 1_000_000)
    // samples read into a double/circular buffer
    // once ten (new unread) samples are connected (1sec)...
    //     ISR wakes up task A
    
    static int newest_data_index = -1;
    static int num_new_samples = 0;
    // static unsigned long start = micros();

    // unsigned long now = micros();
    // unsigned long delta = (now - start) / 1e6;
    
    int adc_val = analogRead(adc_pin);
    // DTYPE adc_val = delta;
    int next_index = (newest_data_index + 1) % BUFFER_LENGTH;
    
    portENTER_CRITICAL_ISR(&spinlock);
    {
        buffer[next_index] = adc_val;
        newest_data_index = next_index;
    }
    portEXIT_CRITICAL_ISR(&spinlock);
    
    num_new_samples++;
    
    // when new complete frame
    if (num_new_samples >= 10) {
        num_new_samples = 0;    // num_new_samples -= 10;

        BaseType_t task_woken = pdFALSE;
        xSemaphoreGiveFromISR(adc_new_frame, &task_woken);
        if (task_woken) {
            portYIELD_FROM_ISR();   // should always trigger
        }
    }
}

// Task Implementations

void computeAverage(void* param){
    static int oldest_data_index = -1;
    DTYPE rolling_sum;

    while (1) {
        // yield until new complete frame (semaphore/notification)
        xSemaphoreTake(adc_new_frame, portMAX_DELAY);

        rolling_sum = 0;
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            rolling_sum += buffer[(oldest_data_index + i) % BUFFER_LENGTH];
        }

        portENTER_CRITICAL(&spinlock);
        {
            average = rolling_sum / SAMPLES_PER_FRAME;
            oldest_data_index = (oldest_data_index + SAMPLES_PER_FRAME) % BUFFER_LENGTH;
        }
        portEXIT_CRITICAL(&spinlock);
    }
}

void serialInterface(void* param) {
    // loop check for new characters
    while(1) {
        if (Serial.available() > 0) {            
            // serial loopback
            String inputStr = Serial.readString();
            Serial.print(inputStr);

            // listen for special command (avg)
            inputStr.trim();
            if (inputStr == "avg") {
                Serial.print("Average: ");
                Serial.println(average);
            }
        }
    }
}