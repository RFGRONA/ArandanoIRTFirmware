/**
 * @file MLX90640Sensor.cpp
 * @brief Implementa los métodos de la clase wrapper MLX90640Sensor.
 */
#include "MLX90640Sensor.h"

// --- Configuración para Promediado Temporal (Temporal Averaging) ---

// Define el número de fotogramas a promediar para reducir el ruido.
// Poner en 1 para deshabilitar el promediado y realizar una lectura única.
#define NUM_SAMPLES_TO_AVERAGE 6

// Define el retardo (delay) entre muestras en milisegundos.
// Debe basarse en la tasa de refresco del sensor.
// Para 0.5Hz, el periodo es 2000ms. Se añade un pequeño margen.
#define INTER_SAMPLE_DELAY_MS 2500


// Constructor: Inicializa la referencia TwoWire y el buffer 'frame' (a ceros).
MLX90640Sensor::MLX90640Sensor(TwoWire &wire) : _wire(wire), frame{} {
    // El objeto 'mlx' se construye por defecto aquí.
}

// Inicializa la comunicación con el sensor y establece los parámetros operativos.
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

    // Configura los parámetros del sensor para el mejor rendimiento (reducción de ruido).
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx.setRefreshRate(MLX90640_0_5_HZ); // 0.5 Hz = 1 fotograma cada 2 segundos

    return true;
}

/**
 * @brief Lee un fotograma térmico, promediando múltiples muestras para mejor precisión.
 *
 * Esta función realiza un "promediado temporal" para reducir el ruido.
 * Captura un número configurable de fotogramas (NUM_SAMPLES_TO_AVERAGE),
 * esperando el ciclo de refresco del sensor entre cada captura, y calcula
 * el promedio de temperatura para cada píxel.
 * El resultado promediado final se almacena en el buffer interno 'frame'.
 *
 * @return True si el fotograma promediado se leyó exitosamente, false en caso contrario.
 */
bool MLX90640Sensor::readFrame() {
    // Si el promediado está deshabilitado (muestras <= 1), realiza una lectura única.
    if (NUM_SAMPLES_TO_AVERAGE <= 1) {
        // retorna 'true' si getFrame() devuelve 0 (éxito)
        return (mlx.getFrame(frame) == 0);
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MLX90640] Starting thermal frame averaging (%d samples)...\n", NUM_SAMPLES_TO_AVERAGE);
    #endif

    // Buffer temporal para almacenar cada lectura de fotograma individual.
    // Se aloja en el stack (pila). 768 * 4 bytes = 3KB, lo cual es seguro en ESP32.
    float tempFrame[768];

    // Paso 1: Limpia el buffer principal 'frame' para usarlo como acumulador.
    for (int i = 0; i < 768; i++) {
        frame[i] = 0.0f;
    }

    // Paso 2: Adquirir y acumular múltiples muestras.
    for (uint8_t s = 0; s < NUM_SAMPLES_TO_AVERAGE; s++) {
        // Para la primera muestra (s=0), no esperamos. Para las siguientes,
        // esperamos el periodo de refresco del sensor (INTER_SAMPLE_DELAY_MS)
        // para asegurar que estamos leyendo datos nuevos.
        if (s > 0) {
            delay(INTER_SAMPLE_DELAY_MS);
        }

        // Intenta leer un fotograma en el buffer temporal.
        if (mlx.getFrame(tempFrame) != 0) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MLX90640] ERROR: Failed to read sample %d/%d.\n", s + 1, NUM_SAMPLES_TO_AVERAGE);
            #endif
            return false; // Aborta todo el proceso si una sola lectura falla.
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MLX90640]   - Sample %d/%d read successfully.\n", s + 1, NUM_SAMPLES_TO_AVERAGE);
        #endif

        // Acumula los valores del fotograma temporal en nuestro buffer principal 'frame'.
        for (int i = 0; i < 768; i++) {
            frame[i] += tempFrame[i];
        }
    }

    // Paso 3: Calcula el promedio final para cada píxel.
    for (int i = 0; i < 768; i++) {
        frame[i] /= NUM_SAMPLES_TO_AVERAGE;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MLX90640] Frame averaging complete. Final data is ready."));
    #endif

    return true; // Retorna 'true' indicando éxito.
}

// Retorna un puntero directo al buffer interno que contiene los datos del último fotograma leído.
float* MLX90640Sensor::getThermalData() {
    // Los datos apuntados son el resultado de la última llamada exitosa a readFrame().
    return frame;
}