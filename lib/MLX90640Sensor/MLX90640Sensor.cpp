/**
 * @file MLX90640Sensor.cpp
 * @brief Implements the MLX90640Sensor wrapper class methods.
 */
#include "MLX90640Sensor.h"
#include <math.h> // Potentially needed for isnan, although MLX library might not return NaN.

// Constructor implementation - Initializes the TwoWire reference and zero-initializes the frame buffer.
MLX90640Sensor::MLX90640Sensor(TwoWire &wire) : _wire(wire), frame{} {
    // The Adafruit_MLX90640 'mlx' object is implicitly default-constructed here.
    // The 'frame' buffer is allocated (e.g., on stack if object is local, or in static memory if global).
    // Using frame{} ensures the buffer starts zeroed, which can be helpful for debugging.
}

// Initializes the sensor communication and sets operating parameters using the Adafruit library.
bool MLX90640Sensor::begin() {
    // Attempt to initialize the Adafruit_MLX90640 library object using the default
    // I2C address (0x33) and the provided TwoWire instance (e.g., Wire or Wire1).
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &_wire)) {
        // Optional: Log failure to the Serial monitor for debugging.
        // Serial.println("Failed to initialize MLX90640 sensor. Check wiring and I2C address.");
        return false; // Return false if library initialization fails.
    }
    // Optional: Log success for debugging.
    // Serial.println("MLX90640 Initialized Successfully");

    // Configure sensor parameters after successful initialization.
    mlx.setMode(MLX90640_CHESS);       // Set readout pattern (Chess recommended for faster readout).
    mlx.setResolution(MLX90640_ADC_18BIT); // Set ADC resolution (higher means less noise, slower).
    mlx.setRefreshRate(MLX90640_0_5_HZ); // Set refresh rate (lower rate means less noise).

    // IMPORTANT NOTE (documented in .h): A delay must be added *after* calling this begin()
    // function in the main sketch (e.g., in setup()) before the first readFrame() call
    // to allow the sensor time for its first measurement cycle (e.g., >1s for 0.5Hz).

    return true; // Return true indicating successful initialization and configuration.
}

// Reads a complete frame of thermal data (768 pixels) into the internal 'frame' buffer.
bool MLX90640Sensor::readFrame() {
    // Call the Adafruit library function to get a complete frame.
    // The library handles reading subpages and calculating temperatures internally.
    // It returns 0 on success, or a non-zero error code on failure.
    if (mlx.getFrame(frame) != 0) {
        // Optional: Log frame read failure for debugging.
        // Serial.println("Failed to read frame from MLX90640");
        return false; // Frame read failed.
    }
    // Optional: Add short delay if needed between reads, although library might handle timing.
    // delay(50);
    return true; // Frame read successfully into the 'frame' buffer.
}

// Returns a direct pointer to the internal buffer holding the last read frame data.
float* MLX90640Sensor::getThermalData() {
    // The caller receives a pointer to the internal 'frame' array.
    // The validity of the data depends on the success of the last 'readFrame()' call.
    return frame;
}

// Calculates the average temperature of all pixels in the current 'frame' buffer.
float MLX90640Sensor::getAverageTemperature() {
    float sum = 0.0f;
    const int totalPixels = 32 * 24; // 768 pixels total.

    // Sum up all pixel temperature values from the buffer.
    for (int i = 0; i < totalPixels; i++) {
        // Basic validity check - skip NaN or potentially invalid very low values if needed
        // if (!isnan(frame[i]) && frame[i] > -40.0f) { // Example check
           sum += frame[i];
        // } else { /* handle error or skip pixel */ }
    }

    // Return the average temperature. Handle division by zero if totalPixels were 0 (shouldn't happen here).
    if (totalPixels > 0) {
        return sum / totalPixels;
    } else {
        return 0.0f; // Or NAN or other error indicator
    }
}

// Finds the maximum temperature among all pixels in the current 'frame' buffer.
float MLX90640Sensor::getMaxTemperature() {
    const int totalPixels = 32 * 24;
    if (totalPixels == 0) return -INFINITY; // Or NAN

    float maxTemp = -INFINITY; // Start with a very low value or frame[0] after checking validity.
    // If using frame[0], ensure it's valid first. Safer to start with -INFINITY.

    // Iterate through all pixels to find the maximum valid temperature.
    for (int i = 0; i < totalPixels; i++) {
         // Basic validity check - skip NaN or potentially invalid values if needed
        // if (!isnan(frame[i])) {
            if (frame[i] > maxTemp) {
                maxTemp = frame[i]; // Update max if current pixel is hotter.
            }
        // }
    }
    return maxTemp;
}

// Finds the minimum temperature among all pixels in the current 'frame' buffer.
float MLX90640Sensor::getMinTemperature() {
    const int totalPixels = 32 * 24;
     if (totalPixels == 0) return INFINITY; // Or NAN

    float minTemp = INFINITY; // Start with a very high value or frame[0] after checking validity.
     // If using frame[0], ensure it's valid first. Safer to start with INFINITY.

    // Iterate through all pixels to find the minimum valid temperature.
    for (int i = 0; i < totalPixels; i++) {
         // Basic validity check - skip NaN or potentially invalid values if needed
         // if (!isnan(frame[i])) {
            if (frame[i] < minTemp) {
                minTemp = frame[i]; // Update min if current pixel is colder.
            }
        // }
    }
    return minTemp;
}