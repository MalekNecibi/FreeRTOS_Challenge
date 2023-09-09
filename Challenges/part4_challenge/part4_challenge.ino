#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#define SERIAL_RELAY_MAX 255

// static bool new_message = 0;
static char* _newMessage = NULL;

void serialListener(void* parameter);
void serialResponder(void* parameter);

void setup() {
    Serial.begin(115200);
    vTaskDelay( 1000 / portTICK_PERIOD_MS );    // Serial initialization needs time
    
    // Start Listener Task
    xTaskCreatePinnedToCore(
        serialListener,
        "Serial Listener Task",
        256 + configMINIMAL_STACK_SIZE,
        &_newMessage,
        1,
        NULL,
        app_cpu
    );

    // Start Responder Task
    xTaskCreatePinnedToCore(
        serialResponder,
        "Serial Responder",
        256 + SERIAL_RELAY_MAX + configMINIMAL_STACK_SIZE,
        &_newMessage,
        1,
        NULL,
        app_cpu
    );
    
    // vTaskStartScheduler();   // needed on Vanilla FreeRTOS
    vTaskDelete(NULL);
}

void loop() {}

void serialListener(void* parameter) {
    if (NULL == parameter) {
        return;
    }
    char** pNewMessage = (char**)parameter;
    
    char stack_buffer[SERIAL_RELAY_MAX] = {0};
    
    
    while(1) {
        // wait for new Serial input
        if (Serial.available() > 0) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            // Read Serial input into stack buffer
            size_t buffer_index = 0;
            while(Serial.peek() != '\n' && buffer_index < SERIAL_RELAY_MAX-1) {
                stack_buffer[buffer_index] = Serial.read();
                buffer_index++;
            }
            if ('\n' == Serial.peek()) {
                Serial.read();
            }
            
            // last index MUST be a newline            
            stack_buffer[buffer_index] = '\n';
            size_t buffer_size = buffer_index + 1;
            
            // Copy message into heap buffer
            char* heap_buffer = (char*) pvPortMalloc((buffer_size) * sizeof(char));
            if (NULL == heap_buffer) {
                // ERROR : allocation failed
                Serial.println("ERROR: Failed to allocate sufficient heap memory");
                return;
            }
            
            // copy characters into buffer
            memcpy(heap_buffer, stack_buffer, (buffer_size) * sizeof(char));
            memset(stack_buffer, 0, SERIAL_RELAY_MAX*sizeof(char));

            // notify Responder
            *pNewMessage = heap_buffer;

            // Serial.print("Heap Remaining (bytes): ")
            // Serial.println(xPortGetFreeHeapSize());
        }

        taskYIELD();
    }
}

void serialResponder(void* parameter) {
    if (NULL == parameter) {
        return;
    }
    char** pNewMessage = (char**)parameter;

    while(1) {
        if (NULL != *pNewMessage) {
            size_t index = 0;
            // Intentionally printing manually to identify/test for scaling issues
            while ((*pNewMessage)[index] != '\n') {
                Serial.print((*pNewMessage)[index]);
                index++;
            }
            Serial.print('\n');
            
            // free memory
            vPortFree(*pNewMessage);
            
            // clear notification of new message
            *pNewMessage = NULL;
        }
        taskYIELD();
    }
}