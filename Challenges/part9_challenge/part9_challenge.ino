
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

#define INPUT_STRING_MAX_LENGTH 63
static const char* average_command = "avg";


// IO Pins
static const int adc_pin = A0;

// Globals
static hw_timer_t* timer = NULL;
static SemaphoreHandle_t adc_new_frame = NULL;
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

static volatile int buffer[ BUFFER_LENGTH ];
static volatile float average = 0.0;


/// Function Signatures ///
// ISR
void IRAM_ATTR onTimer(void);

// Tasks
void computeAverage(void* param);
void serialInterface(void* param);

// Helper Functions
uint8_t parseSerialCommands(const char* inputBuffer, uint16_t numChars);
void resetInputBuffer(char* inputBuffer, uint16_t* numChars);

/// Function Definitions ///
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // pinMode(adc_pin, INPUT);
    
    Serial.begin(115200);
    // Serial.setTimeout(10);
    vTaskDelay(1000 / portTICK_PERIOD_MS);    // Serial initialization needs time
    Serial.println();
    Serial.println("------ FreeRTOS Challenge 9 : Malek -------");
    Serial.println("---- Enter 'avg' for current A0 value  ----");
    
    
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
        512 + configMINIMAL_STACK_SIZE,
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
    
    int adc_val = analogRead(adc_pin);
    int next_index = (newest_data_index + 1) % BUFFER_LENGTH;
    
    portENTER_CRITICAL_ISR(&spinlock);
    {
        buffer[next_index] = adc_val;
    }
    portEXIT_CRITICAL_ISR(&spinlock);

    newest_data_index = next_index;
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
    float rolling_sum;

    while (1) {
        // yield until new complete frame (semaphore/notification)
        xSemaphoreTake(adc_new_frame, portMAX_DELAY);

        rolling_sum = 0;
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            rolling_sum += buffer[(oldest_data_index + i) % BUFFER_LENGTH];
        }
        oldest_data_index = (oldest_data_index + SAMPLES_PER_FRAME) % BUFFER_LENGTH;

        portENTER_CRITICAL(&spinlock);
        {
            average = rolling_sum / SAMPLES_PER_FRAME;
        }
        portEXIT_CRITICAL(&spinlock);
    }
}

void serialInterface(void* param) {
    static char inputString[INPUT_STRING_MAX_LENGTH + 1] = {0};  // extra for sentinel
    uint16_t numChars = 0;
    
    // loop check for new characters
    while(1) {
        if (Serial.available() > 0) {
            char newChar = Serial.read();
            
            // edge case: end of command/sentence
            if ('\r' == newChar || '\n' == newChar) {
                Serial.println();
                inputString[numChars] = '\0';
                
                // treat LF and CRLF as equivalent newlines
                if ('\r' == newChar && '\n' == Serial.peek()) {
                    Serial.read();
                }

                parseSerialCommands(inputString, numChars);
                
                // reset buffer
                resetInputBuffer(inputString, &numChars);

            // base case
            } else {
                inputString[numChars] = newChar;
                inputString[numChars+1] = '\0';                     // hacky but safer
                Serial.print(newChar);
                numChars++;
            }
            
            // prevent buffer overflow
            if (numChars >= INPUT_STRING_MAX_LENGTH) {
                inputString[INPUT_STRING_MAX_LENGTH] = '\0';

                if (0 == parseSerialCommands(inputString, numChars)) {
                    Serial.println();                               // TODO : no newline if it was a command
                }
                
                resetInputBuffer(inputString, &numChars);
            }
        }

        // allow other tasks cpu time
        if (0 == numChars % 8) {
            // taskYIELD();
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
    }
}


// Helper Functions

uint8_t parseSerialCommands(const char* inputBuffer, const uint16_t numChars) {
    uint16_t numBytes = (INPUT_STRING_MAX_LENGTH < numChars) ? INPUT_STRING_MAX_LENGTH : numChars;
    // inputBuffer[numChars] = '\0';                                   // should not be necessary, but safer
    
    // Average Command implentation
    if (0 == strncmp(inputBuffer, average_command, 1+numBytes)) {
        Serial.print("Average: ");
        Serial.println(average);
        return 1;
    }
    return 0;
}

void resetInputBuffer(char* inputString, uint16_t* numChars) {
    *numChars = 0;
    inputString[0] = '\0';      // in case memset fails
    memset(inputString, '\0', 1+INPUT_STRING_MAX_LENGTH);
}