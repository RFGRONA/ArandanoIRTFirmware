/**
 * @file LEDStatus.cpp
 * @brief Implementa la clase LEDStatus para controlar el LED de estado (WS2812).
 */
#include "LEDStatus.h"

// --- Configuración Hardware ---
#define LED_PIN 48   ///< Pin GPIO donde se conecta la línea de datos del NeoPixel.
#define NUMPIXELS 1  ///< Número de NeoPixels (debe ser 1 para esta clase).
// ----------------------------

/**
 * @brief Implementación del constructor.
 * Configura el objeto NeoPixel usando la lista de inicialización
 * y establece el estado interno inicial en OFF.
 */
LEDStatus::LEDStatus() : 
    pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800),
    _currentState(OFF) // Inicializa el estado interno
    {
    // (Vacío intencionalmente)
}

/**
 * @brief Inicializa la comunicación de la librería NeoPixel.
 * Prepara la librería y se asegura que el LED esté físicamente apagado.
 */
void LEDStatus::begin() {
    pixels.begin();       // Inicializa la librería
    pixels.clear();       // Establece el buffer a (0,0,0)
    pixels.show();        // Envía el estado 'apagado' al píxel
    _currentState = OFF;  // Sincroniza el estado interno
    delay(50);            // Pausa corta post-inicialización (opcional).
}

/**
 * @brief Apaga el LED.
 * Establece el estado en OFF y actualiza el píxel físico.
 */
void LEDStatus::turnOffAll() {
    pixels.clear(); // Pone el buffer a (0,0,0)
    pixels.show();  // Envía el estado 'apagado'
    _currentState = OFF;
}

/**
 * @brief Función interna (helper) para establecer el color del LED.
 * Actualiza el buffer del píxel y lo envía (show).
 */
void LEDStatus::setColor(uint8_t r, uint8_t g, uint8_t b) {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
}

/**
 * @brief Establece el LED a un estado (y color) específico.
 * Actualiza el estado interno y cambia el color físico del LED.
 * @param state El estado deseado (LEDState).
 */
void LEDStatus::setState(LEDState state) {
    _currentState = state; // Actualiza el estado interno

    switch(state) {
        case ERROR_AUTH:
            setColor(255, 0, 0);   // Rojo
            break;
        case ERROR_SEND:
            setColor(255, 165, 0); // Naranja 
            break;
        case ERROR_SENSOR:
            setColor(255, 0, 255); // Púrpura/Magenta
            break;
        case ERROR_DATA:
            setColor(0, 255, 255); // Cian
            break;
        case TAKING_DATA:
            setColor(0, 0, 255);   // Azul
            break;
        case SENDING_DATA:
            setColor(0, 255, 0);   // Verde
            break;
        case ALL_OK:
            setColor(255, 255, 255); // Blanco 
            break;
        case OFF:
            pixels.clear();
            pixels.show();
            break;
        case CONNECTING_WIFI:
             setColor(255, 223, 0); // Amarillo 
            break;
        case ERROR_WIFI:
             setColor(255, 105, 180); // Rosa
            break;
        case ERROR_TIMER: // (Error en sincronización NTP)
            setColor(139, 0, 0);   // Rojo Oscuro 
            break;
        case CONFIG_MODE_AP:
            setColor(0, 128, 128); // Teal (Verde-Azul)
            break;
        default: // Maneja estados indefinidos (apaga el LED)
            pixels.clear();
            pixels.show();
            _currentState = OFF; // Asegura un estado interno definido
            break;
    }
}

/**
 * @brief Obtiene el estado lógico actual del LED.
 * @return El estado actual (del enum `LEDState`).
 */
LEDState LEDStatus::getCurrentState() const { 
    return _currentState;
}