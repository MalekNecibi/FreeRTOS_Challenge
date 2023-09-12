#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#define BLINK_NOTIFY_INTERVAL       100
#define SERIAL_BUFFER_LENGTH        32

#define DELAY_STRING                "delay "
#define DELAY_STRING_LENGTH         6   // strlen("delay ")


static const int led_pin =          LED_BUILTIN;

typedef struct _BlinkState {
    char message[8];    // ['b', 'l', 'i', 'n', 'k', 'e', 'd', '\0']
    int counter;
} BlinkState;

static const UBaseType_t queue_1_delay_length = 5;
static const UBaseType_t queue_1_delay_size = sizeof( int );
static QueueHandle_t queue_1_delay; 

static const UBaseType_t queue_2_blinked_length = 5;
static const UBaseType_t queue_2_blinked_size = sizeof( BlinkState );
static QueueHandle_t queue_2_blinked; 


void printMessages(void* parameter);

void setup() {
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);      // Serial initialization needs time
    
    pinMode(led_pin, OUTPUT);
    
    // Initialize Queue
    queue_1_delay =   xQueueCreate(queue_1_delay_length,   queue_1_delay_size);
    queue_2_blinked = xQueueCreate(queue_2_blinked_length, queue_2_blinked_size);
    
    // Start Serial Handler Task
    xTaskCreatePinnedToCore(
        serialHandler,
        "Serial Handler",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        1,
        NULL,
        app_cpu
    );
    
    // Start LED Blinker Task
    xTaskCreatePinnedToCore(
        blinkLED,
        "Blink LED",
        256 + configMINIMAL_STACK_SIZE,
        NULL,
        1,
        NULL,
        app_cpu
    );

    // vTaskStartScheduler();   // required on Vanilla FreeRTOS
    vTaskDelete(NULL);
}

void loop() {
    //
}

void serialHandler(void* parameter) {   
    // if (NULL == parameter) {
    //     return;
    // }
    // void* pVariable = (void*)parameter;
    
    Serial.println();
    Serial.println("--- FreeRTOS Challenge 5 ---");
    Serial.println("---     Malek Necibi     ---");
    Serial.println();
    
    char buffer[SERIAL_BUFFER_LENGTH];
    int buffer_index = 0;
    char input_char;

    BlinkState blinkState;
    
    while(1) {
        // listen for serial input
        if ( Serial.available() ) {
            input_char = Serial.read();
            
            if ( '\n' == input_char || '\r' == input_char || BLINK_NOTIFY_INTERVAL == buffer_index+1 ) {
                // edge case, end of line/buffer
                Serial.println("");
                buffer[buffer_index] = '\0';
                buffer_index = 0;

                // check if a delay string provided
                if ( 0 == strncmp(buffer, DELAY_STRING, DELAY_STRING_LENGTH) ) {
                    int new_delay = atoi(buffer + DELAY_STRING_LENGTH);
                    if (new_delay > 0) {
                        if ( xQueueSend(queue_1_delay, &new_delay, 10) ) {
                            Serial.print("Updated Delay: ");
                            Serial.print(new_delay);
                            Serial.println("ms");
                        } else {
                            Serial.println("ERROR: couldn't update delay, too many updates already queued");
                        }
                    }
                }

            } else {
                // base case: relay input back
                Serial.print(input_char);

                if (127 == input_char) {
                    // edge case : Backspace
                    buffer_index = (buffer_index > 0) ? buffer_index-1 : 0;
                    
                } else {
                    buffer[buffer_index] = input_char;
                    buffer_index = (buffer_index+1 ) % SERIAL_BUFFER_LENGTH;
                }
            }
            
        }

        // Check for blink state messages
        if ( xQueueReceive(queue_2_blinked, &blinkState, 0) ) {
            Serial.println();
            Serial.print("Message Received: ");
            Serial.print(blinkState.message);
            Serial.print(" ");
            Serial.print(blinkState.counter);
            Serial.println(" times");
        }
        
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        taskYIELD();
    }
}


int getNewDelay(const int old_delay) {
    int new_delay = 0;   //  = old_delay;
    if ( xQueueReceive(queue_1_delay, &new_delay, 0) ) {
        // new_delay (already) successfully over-written
        
    } else {
        new_delay = old_delay;
    }
    return new_delay;
}

void blinkLED(void* parameter) {
    int blink_delay_ms = 500;
    
    BlinkState blinkState;
    blinkState.counter = 0;
    strcpy( blinkState.message, "Blinked" );

    while(1) {
        digitalWrite(led_pin, LOW);
        blink_delay_ms = getNewDelay(blink_delay_ms);
        vTaskDelay(blink_delay_ms / portTICK_PERIOD_MS);
        
        digitalWrite(led_pin, HIGH);
        blink_delay_ms = getNewDelay(blink_delay_ms);
        vTaskDelay(blink_delay_ms / portTICK_PERIOD_MS);
        
        blinkState.counter++;
        
        if (0 == blinkState.counter % BLINK_NOTIFY_INTERVAL) {
            if ( ! xQueueSend(queue_2_blinked, (void*) &blinkState, 10) ) {
                // Serial.println("ERROR: couldn't notify about blink milestone, queue too full!");
            }
        }
    }
}
