#ifndef MLX90640Sensor_h
#define MLX90640Sensor_h

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>

class MLX90640Sensor {
public:
    MLX90640Sensor(TwoWire &wire);
    
    // Inicializa el sensor MLX90640
    bool begin();
    
    // Captura una nueva trama de datos térmicos
    bool readFrame();
    
    // Retorna un puntero al array de datos térmicos (32x24)
    float* getThermalData();
    
    void printRawData(Stream &output);
    void printSimulatedHeatmap(Stream &output);
    
private:
    Adafruit_MLX90640 mlx;
    float frame[32 * 24];
    TwoWire &_wire;  
    char temperaturaACaracter(float temp);
};

#endif
