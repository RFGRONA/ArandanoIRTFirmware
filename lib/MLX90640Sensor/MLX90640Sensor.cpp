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
    mlx.setRefreshRate(MLX90640_4_HZ);
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

void MLX90640Sensor::printRawData(Stream &output) {
    for (uint8_t y = 0; y < 24; y++) {
        for (uint8_t x = 0; x < 32; x++) {
            output.printf("%4.1f ", frame[y * 32 + x]);
        }
        output.println();
    }
}

void MLX90640Sensor::printSimulatedHeatmap(Stream &output) {
    for (uint8_t y = 0; y < 24; y++) {
        for (uint8_t x = 0; x < 32; x++) {
            output.print(temperaturaACaracter(frame[y * 32 + x]));
            output.print(" ");
        }
        output.println();
    }
}

char MLX90640Sensor::temperaturaACaracter(float temp) {
    if (temp < 5.0) {
        return ' ';
    } else if (temp > 40.0) {
        return '#';
    } else {
        float escala = (temp - 5.0) / (40.0 - 5.0);
        if (escala < 0.2) return '.';
        else if (escala < 0.4) return ',';
        else if (escala < 0.6) return '-';
        else if (escala < 0.8) return '+';
        else return '*';
    }
}
