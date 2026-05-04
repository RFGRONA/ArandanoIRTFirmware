/**
 * @file MLX90640Sensor.h
 * @brief Define una clase "wrapper" (envoltorio) para el sensor de Cámara Térmica MLX90640.
 *
 * Proporciona una interfaz para inicializar y leer fotogramas (frames) de datos
 * térmicos (32x24 píxeles) desde un sensor MLX90640 usando una instancia I2C
 * específica y la librería Adafruit MLX90640.
 */
#ifndef MLX90640SENSOR_H
#define MLX90640SENSOR_H

#include <Arduino.h>           
#include <Wire.h>              // Requerido para la comunicación I2C (clase TwoWire)
#include <Adafruit_MLX90640.h> // Incluye la librería base de Adafruit MLX90640

/**
 * @class MLX90640Sensor
 * @brief Clase wrapper que simplifica el uso de la cámara térmica MLX90640.
 *
 * Encapsula el objeto de la librería Adafruit, proporcionando métodos para
 * inicializar el sensor, leer fotogramas térmicos completos (32x24 = 768 píxeles)
 * y acceder al buffer de datos crudos.
 */
class MLX90640Sensor {
public:
    /**
     * @brief Constructor para el wrapper del MLX90640.
     * @param wire Referencia a la instancia TwoWire (ej. Wire, Wire1) a la que está conectado el sensor.
     */
    MLX90640Sensor(TwoWire &wire);

    /**
     * @brief Inicializa la comunicación y configuración del sensor MLX90640.
     *
     * Intenta inicializar la comunicación usando la dirección I2C por defecto (0x33).
     * Configura los parámetros operativos del sensor:
     * - Modo: Lectura en patrón 'Chess' (`MLX90640_CHESS`)
     * - Resolución ADC: 18-bit (`MLX90640_ADC_18BIT`)
     * - Tasa de Refresco: 0.5 Hz (`MLX90640_0_5_HZ`) para menor ruido.
     * Este método debe ser llamado una vez en `setup()`.
     *
     * @note IMPORTANTE: Se DEBE añadir un retardo (`delay`) en el sketch *después*
     * de llamar a `begin()` y *antes* de la primera llamada a `readFrame()`.
     * El retardo requerido depende de la tasa de refresco (ej. > 2 segundos para 0.5 Hz).
     * Esto da tiempo al sensor para realizar sus mediciones iniciales.
     *
     * @return `true` si la inicialización y configuración fueron exitosas, `false` en caso contrario.
     */
    bool begin();

    /**
     * @brief Lee un nuevo fotograma de datos térmicos en el buffer interno.
     *
     * Llama a la función de la librería (`getFrame`) para obtener los 768 valores
     * de temperatura. (Ver implementación en .cpp para la lógica de promediado).
     *
     * @return `true` si se leyó un fotograma completo (o promediado) exitosamente,
     * `false` si ocurrió un error.
     */
    bool readFrame();

    /**
     * @brief Obtiene un puntero directo al buffer interno de datos térmicos.
     *
     * Proporciona acceso a los datos crudos de temperatura de la última llamada
     * exitosa a `readFrame()`. El buffer es un array plano de 768 (`32 * 24`)
     * valores flotantes que representan las temperaturas.
     *
     * @return Puntero al array interno `frame` (float) que contiene las temperaturas en grados Celsius ($^{\circ}C$).
     *
     * @note Los datos apuntados solo son válidos después de que `readFrame()`
     * retorne `true`. Llamar a esto antes de una lectura exitosa o después de
     * una fallida retornará un puntero a datos desactualizados o no inicializados.
     */
    float* getThermalData();

private:
    Adafruit_MLX90640 mlx;    ///< Instancia de la librería Adafruit MLX90640 subyacente.
    float frame[32 * 24];     ///< Buffer interno para almacenar 768 temp. (Celsius, $^{\circ}C$).
    TwoWire &_wire;           ///< Referencia al bus I2C (ej. Wire o Wire1) a utilizar.
};

#endif // MLX90640SENSOR_H