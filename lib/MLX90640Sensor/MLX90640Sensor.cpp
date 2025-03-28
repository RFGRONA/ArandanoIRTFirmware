#include "MLX90640Sensor.h"

MLX90640Sensor::MLX90640Sensor(TwoWire &wire) : _wire(wire) {}

bool MLX90640Sensor::begin() {
    // Inicializa el sensor con la dirección I2C por defecto
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &_wire)) {
        return false; 
    }

    // Configura el modo, la resolución y la tasa de refresco del sensor
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx.setRefreshRate(MLX90640_0_5_HZ);
    return true; 
}

bool MLX90640Sensor::readFrame() {
    // Captura una nueva trama de datos térmicos
    if (mlx.getFrame(frame) != 0) {
        return false; 
    }
    return true; 
}

float* MLX90640Sensor::getThermalData() {
    // Retorna el puntero al array de datos térmicos
    return frame;
}

float MLX90640Sensor::getAverageTemperature() {
    float sum = 0;
    const int totalPixels = 32 * 24;
    for (int i = 0; i < totalPixels; i++) {
        sum += frame[i];
    }
    return sum / totalPixels;
}

float MLX90640Sensor::getMaxTemperature() {
    const int totalPixels = 32 * 24;
    float maxTemp = frame[0];
    for (int i = 1; i < totalPixels; i++) {
        if (frame[i] > maxTemp) {
            maxTemp = frame[i];
        }
    }
    return maxTemp;
}

float MLX90640Sensor::getMinTemperature() {
    const int totalPixels = 32 * 24;
    float minTemp = frame[0];
    for (int i = 1; i < totalPixels; i++) {
        if (frame[i] < minTemp) {
            minTemp = frame[i];
        }
    }
    return minTemp;
}
