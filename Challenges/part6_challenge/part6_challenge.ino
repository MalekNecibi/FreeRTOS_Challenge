/**
 * FreeRTOS Mutex Challenge
 * 
 * Pass a parameter to a task using a mutex.
 * 
 * Date: January 20, 2021
 * Author: Shawn Hymel
 * License: 0BSD
 */

// Modified by: Malek Necibi
//              2023/09/13

// You'll likely need this on vanilla FreeRTOS
//#include <semphr.h>

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Pins (change this if your Arduino board does not have LED_BUILTIN defined)
static const int led_pin = LED_BUILTIN;

static SemaphoreHandle_t setup_stack_mutex;


//*****************************************************************************
// Tasks

// Blink LED based on rate passed by parameter
void blinkLED(void *parameters) {

  // Copy the parameter into a local variable
  int num = *(int *)parameters;

  // Notify setup() that it can free up its memory
  xSemaphoreGive(setup_stack_mutex);

  // Print the parameter
  Serial.print("Received: ");
  Serial.println(num);
  
  // Configure the LED pin
  pinMode(led_pin, OUTPUT);
  
  // Blink forever and ever
  while (1) {
    digitalWrite(led_pin, HIGH);
    vTaskDelay(num / portTICK_PERIOD_MS);
    digitalWrite(led_pin, LOW);
    vTaskDelay(num / portTICK_PERIOD_MS);
  }
}

//*****************************************************************************
// Main (runs as its own task with priority 1 on core 1)

void setup() {
  long int delay_arg;

  // Notify when setup() stack can be free'd
  setup_stack_mutex = xSemaphoreCreateBinary();
  xSemaphoreGive(setup_stack_mutex);    // semaphore is created in the 'empty' state

  // Configure Serial
  Serial.begin(115200);

  // Wait a moment to start (so we don't miss Serial output)
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println();
  Serial.println("---FreeRTOS Mutex Challenge---");
  Serial.println("Enter a number for delay (milliseconds)");

  // Wait for input from Serial
  while (Serial.available() <= 0);

  // Read integer value
  delay_arg = Serial.parseInt();
  Serial.print("Sending: ");
  Serial.println(delay_arg);
  
  // Block function cleanup  while setup() stack still in use
  xSemaphoreTake(setup_stack_mutex, 0);

  // Start task 1
  xTaskCreatePinnedToCore(blinkLED,
                          "Blink LED",
                          4096,
                          (void *)&delay_arg,
                          1,
                          NULL,
                          app_cpu);

  // Wait for Task to use input data
  if ( ! xSemaphoreTake(setup_stack_mutex, portMAX_DELAY) ) {
      Serial.println("WARNING: never informed when safe to free setup memory (setup_stack_mutex)");
  } 
  
//   xSemaphoreGive(setup_stack_mutex);    // optional

  // Show that we accomplished our task of passing the stack-based argument
  Serial.println("Done!");
  
}

void loop() {
  
  // Do nothing but allow yielding to lower-priority tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
