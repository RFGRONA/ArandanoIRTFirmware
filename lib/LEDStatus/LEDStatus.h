/**
 * @file LEDStatus.h
 * @brief Gestiona un LED NeoPixel (WS2812) individual para indicación de estado.
 *
 * Proporciona métodos para inicializar el LED y establecer su color
 * basándose en estados operativos predefinidos.
 * Asume un único NeoPixel conectado. Usa la librería Adafruit_NeoPixel.
 */
#ifndef LEDSTATUS_H
#define LEDSTATUS_H

#include <Adafruit_NeoPixel.h> 
#include <Arduino.h>           

/**
 * @brief Define los posibles estados operativos que el sistema
 * puede indicar a través del color del LED.
 */
enum LEDState {
    ALL_OK,          ///< Estado: Todo OK / Inactivo. Color: Blanco.
    OFF,             ///< Estado: LED explícitamente apagado. Color: Apagado.
    ERROR_AUTH,      ///< Estado: Error de autenticación. Color: Rojo.
    ERROR_SEND,      ///< Estado: Error enviando datos (HTTP). Color: Naranja.
    ERROR_SENSOR,    ///< Estado: Error al leer/inicializar sensor. Color: Púrpura.
    ERROR_DATA,      ///< Estado: Error procesando/capturando datos (ej. imagen). Color: Cian.
    TAKING_DATA,     ///< Estado: Tomando mediciones o captura. Color: Azul.
    SENDING_DATA,    ///< Estado: Enviando datos (HTTP). Color: Verde.
    CONNECTING_WIFI, ///< Estado: Intentando conectar a WiFi. Color: Amarillo.
    ERROR_WIFI,      ///< Estado: Fallo al conectar a WiFi. Color: Rosa.
    ERROR_TIMER,     ///< Estado: Fallo al sincronizar hora (NTP). Color: Rojo Oscuro.
    CONFIG_MODE_AP   ///< Estado: Modo configuración (Access Point). Color: Teal (Verde-azul).
};

/**
 * @class LEDStatus
 * @brief Gestiona un único NeoPixel (WS2812) para mostrar el estado del sistema.
 *
 * Encapsula la librería Adafruit_NeoPixel y mapea los estados lógicos
 * (definidos en `LEDState`) a colores específicos.
 */
class LEDStatus {
public:
    /**
     * @brief Constructor.
     * Inicializa la instancia de la librería NeoPixel con los parámetros
     * definidos en el .cpp (pin, número de píxeles, tipo).
     */
    LEDStatus();

    /**
     * @brief Inicializa la comunicación hardware con el NeoPixel.
     * Configura la librería y asegura que el LED inicie apagado.
     * Debe llamarse una vez en la función `setup()`.
     */
    void begin();

    /**
     * @brief Establece el color del LED según el estado del sistema especificado.
     * @param state El estado deseado (del enum `LEDState`).
     */
    void setState(LEDState state);

    /**
     * @brief Apaga completamente el LED.
     * Equivalente a llamar `setState(OFF)`.
     */
    void turnOffAll();

    /**
     * @brief Obtiene el estado lógico actual del LED.
     * @return El estado actual (del enum `LEDState`).
     */
    LEDState getCurrentState() const; 

private:
    Adafruit_NeoPixel pixels; ///< Instancia de la librería Adafruit NeoPixel.
    LEDState _currentState;   ///< Almacena el estado lógico (LEDState) actual. 

    /**
     * @brief Función interna (helper) para establecer el color RGB crudo del LED.
     * @param r Componente Rojo (0-255).
     * @param g Componente Verde (0-255).
     * @param b Componente Azul (0-255).
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b);
};

#endif // LEDSTATUS_H