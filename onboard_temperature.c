#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/adc.h"

// Buffer or no buffer - use this to compare the number of ticks taken using the buffer optimisation
// to the unoptimised method used in the workshop.
bool useBuffer = false;

/* Choose 'C' for Celsius or 'F' for Fahrenheit. */
#define TEMPERATURE_UNITS 'C'

// Define a buffer to store batched temperature data
#define BATCH_SIZE 10
float temperature_batch[BATCH_SIZE];
int batch_index = 0;

float convert_temperature(const char unit, uint16_t raw) {
    
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)raw * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C') {
        return tempC;
    } else if (unit == 'F') {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

// Formats the batched temperatures as a single string
void print_batched_temperatures(const char unit) {
    char buffer[100];
    int offset = 0;
    for (int i = 0; i < batch_index; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%.02f %c ", temperature_batch[i], unit);
    }
    printf("Batched temperature values: %s\n", buffer);
    
    batch_index = 0; // Reset batch index
}

// Handles code run during interrupt
void core1_interrupt_handler() {

    // Runs while there is data in the FIFO register
    while (multicore_fifo_rvalid()){
        if (useBuffer == false)
        {
            // Receive raw data and convert it to a number we understand
            uint32_t raw = multicore_fifo_pop_blocking();
            float temperature = convert_temperature(TEMPERATURE_UNITS, raw);

            // Gets system time before sending
            uint32_t ticks_before_send = time_us_32();

            // Outputs the temperature
            //printf("Onboard temperature = %.02f %c\n", temperature, TEMPERATURE_UNITS);
            printf("T: %.02f %c\n", temperature, TEMPERATURE_UNITS);

            // Gets system time after sending
            uint32_t ticks_after_send = time_us_32();

            // Outputs the ticks taken to send the temperature string
            printf("ticks taken to send data: %lu\n", ticks_after_send-ticks_before_send);

            // Get read time
            uint32_t readtime = multicore_fifo_pop_blocking();

            // Outputs the ticks taken to read the data
            printf("ticks taken to read data: %lu\n", readtime);
        } else
        {
            // Receive raw data and convert it to a temperature
            uint32_t raw = multicore_fifo_pop_blocking();
            float temperature = convert_temperature(TEMPERATURE_UNITS, raw);

            // Batch temperature
            temperature_batch[batch_index] = temperature;
            batch_index++;

            // Check if batch is full
            if (batch_index >= BATCH_SIZE) {

                // Outputs the formatted batch of temperatures and measures the ticks taken
                uint32_t ticks_before_send = time_us_32();
                print_batched_temperatures('C'); // 'C' for Celsius, 'F' for Fahrenheit
                uint32_t ticks_after_send = time_us_32();

                // Outputs the ticks taken to send the temperatures
                printf("ticks taken to send data: %lu\n", ticks_after_send-ticks_before_send);
            }

            // Get read time
            uint32_t readtime = multicore_fifo_pop_blocking();

            // Outputs the read time
            printf("ticks taken to read data: %lu\n", readtime);
        }
    }
    // Clears interrupt
    multicore_fifo_clear_irq();
}

// Core 1 code
void core1_entry() {
    // Clear interrupt flag to be safe
    multicore_fifo_clear_irq();

    // Set function that is executed when interruption occurs
    irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);

    // Enable interrupt
    irq_set_enabled(SIO_IRQ_PROC1, true);

    // Infinite While Loop to wait for interrupt
    while (1){
        tight_loop_contents();
    }
}

// Core 0 code
int main(void){
    stdio_init_all();

    multicore_launch_core1(core1_entry); // Start core 1 - Do this before any interrupt configuration

    // Configure the ADC
    adc_init();
    adc_set_temp_sensor_enabled(true); // Enable on board temp sensor
    adc_select_input(4);

    // Primary Core 0 Loop
    while (1) {
        // get the value of the Pico hardware timer before the ADC read operation (WS3)
        uint32_t ticks_before_read = time_us_32();

        uint32_t raw = adc_read();

        // get the value of the Pico hardware timer after the ADC read operation (WS3)
        uint32_t ticks_after_read = time_us_32();

        uint32_t ticks_to_read = ticks_after_read - ticks_before_read;

        // Send temperature data to fifo
        multicore_fifo_push_blocking(raw);

        // Send readtime data to fifo
        multicore_fifo_push_blocking(ticks_to_read);
    }
}