/**
 * @file MLX90640Sensor.cpp
 * @brief Implements the MLX90640Sensor wrapper class methods.
 */
#include "MLX90640Sensor.h"

// --- Configuration for Temporal Averaging ---
// Define the number of frames to average for noise reduction.
// Set to 1 to disable averaging and perform a single read.
#define NUM_SAMPLES_TO_AVERAGE 6

// Define the delay between samples in milliseconds.
// This should be based on the sensor's refresh rate.
// For 0.5Hz, the period is 2000ms. A small margin is added.
#define INTER_SAMPLE_DELAY_MS 2500


// Constructor: Initializes the TwoWire reference.
MLX90640Sensor::MLX90640Sensor(TwoWire &wire) : _wire(wire), frame{} {
    // The Adafruit_MLX90640 'mlx' object is implicitly default-constructed here.
}

// Initializes the sensor communication and sets operating parameters.
bool MLX90640Sensor::begin() {
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &_wire)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MLX90640] Failed to initialize sensor. Check wiring and I2C address."));
        #endif
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MLX90640] Initialized Successfully."));
    #endif

    // Configure sensor parameters for best noise performance.
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx.setRefreshRate(MLX90640_0_5_HZ);

    return true;
}

/**
 * @brief Reads a thermal data frame, averaging multiple samples for better accuracy.
 *
 * This function performs temporal averaging to reduce noise. It captures a configurable
 * number of frames (NUM_SAMPLES_TO_AVERAGE), waiting for the sensor's refresh cycle
 * between each capture, and calculates the average temperature for each pixel.
 * The final averaged result is stored in the internal 'frame' buffer.
 *
 * @return True if the averaged frame was read successfully, false otherwise.
 */
bool MLX90640Sensor::readFrame() {
    // If averaging is disabled (samples <= 1), perform a single, standard read.
    if (NUM_SAMPLES_TO_AVERAGE <= 1) {
        return (mlx.getFrame(frame) == 0);
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MLX90640] Starting thermal frame averaging (%d samples)...\n", NUM_SAMPLES_TO_AVERAGE);
    #endif

    // Temporary buffer to store each individual frame reading.
    // This is allocated on the stack. 768 * 4 bytes = 3KB, which is safe for ESP32.
    float tempFrame[768];

    // Step 1: Clear the main 'frame' buffer to use it as an accumulator.
    for (int i = 0; i < 768; i++) {
        frame[i] = 0.0f;
    }

    // Step 2: Acquire and accumulate multiple samples.
    for (uint8_t s = 0; s < NUM_SAMPLES_TO_AVERAGE; s++) {
        // For the first sample (s=0), we don't wait. For subsequent samples,
        // we wait for the sensor's refresh period to get new data.
        if (s > 0) {
            delay(INTER_SAMPLE_DELAY_MS);
        }

        // Attempt to read a single frame into the temporary buffer.
        if (mlx.getFrame(tempFrame) != 0) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MLX90640] ERROR: Failed to read sample %d/%d.\n", s + 1, NUM_SAMPLES_TO_AVERAGE);
            #endif
            return false; // Abort the entire process if any single read fails.
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MLX90640]   - Sample %d/%d read successfully.\n", s + 1, NUM_SAMPLES_TO_AVERAGE);
        #endif

        // Accumulate the values from the temporary frame into our main buffer.
        for (int i = 0; i < 768; i++) {
            frame[i] += tempFrame[i];
        }
    }

    // Step 3: Calculate the final average for each pixel.
    for (int i = 0; i < 768; i++) {
        frame[i] /= NUM_SAMPLES_TO_AVERAGE;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MLX90640] Frame averaging complete. Final data is ready."));
    #endif

    return true; // Return true indicating success.
}

// Returns a direct pointer to the internal buffer holding the last read frame data.
float* MLX90640Sensor::getThermalData() {
    // The data pointed to is the result of the last successful readFrame() call.
    return frame;
}