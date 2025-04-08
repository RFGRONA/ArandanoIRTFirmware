#include "MLX90640Sensor.h"

// Constructor implementation - Initializes the TwoWire reference.
MLX90640Sensor::MLX90640Sensor(TwoWire &wire) : _wire(wire) {
    // The Adafruit_MLX90640 'mlx' object is implicitly constructed here.
    // The 'frame' buffer is allocated on the stack or globally depending on instantiation context.
}

// Initializes the sensor using the Adafruit library.
bool MLX90640Sensor::begin() {
    // Attempt to initialize the Adafruit_MLX90640 library object
    // using the default I2C address and the provided TwoWire instance.
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &_wire)) {
        // Serial.println("Failed to init MLX90640"); // Optional debug
        return false; // Return false if library initialization fails
    }
    // Serial.println("MLX90640 Initialized"); // Optional debug

    // Set sensor parameters after successful initialization
    mlx.setMode(MLX90640_CHESS);          // Set readout pattern (Chess or Interleaved)
    mlx.setResolution(MLX90640_ADC_18BIT); // Set ADC resolution
    mlx.setRefreshRate(MLX90640_0_5_HZ);  // Set refresh rate (lower rate means less noise)

    // NOTE: A delay should be added *after* calling this begin() function
    // in the main sketch (~1.1s for 0.5Hz) before the first readFrame() call.

    return true; // Return true if initialization and configuration were successful
}

// Reads a complete frame of thermal data into the internal 'frame' buffer.
bool MLX90640Sensor::readFrame() {
    // Call the library function to get a frame.
    // The library handles reading subpages and calculating temperatures.
    // It returns 0 on success, non-zero on error.
    if (mlx.getFrame(frame) != 0) {
        // Serial.println("Failed to read frame from MLX90640"); // Optional debug
        return false; // Frame read failed
    }
    return true; // Frame read successfully
}

// Returns a pointer to the internal buffer holding the last read frame data.
float* MLX90640Sensor::getThermalData() {
    // The caller receives a pointer to the internal 'frame' array.
    // The data is valid only after a successful call to readFrame().
    return frame;
}

// Calculates the average temperature of all pixels in the current 'frame' buffer.
float MLX90640Sensor::getAverageTemperature() {
    float sum = 0;
    const int totalPixels = 32 * 24; // 768 pixels total
    // Sum up all pixel values
    for (int i = 0; i < totalPixels; i++) {
        sum += frame[i];
    }
    // Return the average
    return sum / totalPixels;
}

// Finds the maximum temperature among all pixels in the current 'frame' buffer.
float MLX90640Sensor::getMaxTemperature() {
    const int totalPixels = 32 * 24;
    float maxTemp = frame[0]; // Assume first pixel is max initially
    // Iterate through the rest of the pixels
    for (int i = 1; i < totalPixels; i++) {
        if (frame[i] > maxTemp) {
            maxTemp = frame[i]; // Update max if current pixel is hotter
        }
    }
    return maxTemp;
}

// Finds the minimum temperature among all pixels in the current 'frame' buffer.
float MLX90640Sensor::getMinTemperature() {
    const int totalPixels = 32 * 24;
    float minTemp = frame[0]; // Assume first pixel is min initially
    // Iterate through the rest of the pixels
    for (int i = 1; i < totalPixels; i++) {
        if (frame[i] < minTemp) {
            minTemp = frame[i]; // Update min if current pixel is colder
        }
    }
    return minTemp;
}