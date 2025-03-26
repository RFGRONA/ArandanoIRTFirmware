#include <Wire.h> // Incluye la librería Wire para comunicación I2C. Esta librería es esencial para comunicarse con dispositivos I2C como el sensor BH1750.
#include <BH1750.h> // Incluye la librería BH1750 para interactuar con el sensor de luz BH1750. Esta librería proporciona funciones para configurar y leer datos del sensor.

BH1750 lightMeter; // Crea un objeto llamado 'lightMeter' de la clase BH1750. Este objeto será utilizado para controlar y acceder a las funciones del sensor BH1750.

void setup() {
// Inicializar la comunicación serial
    Serial.begin(115200); // Inicializa la comunicación serial a una velocidad de 115200 baudios. Esto permite enviar datos desde la placa Arduino a la computadora para ser visualizados en el monitor serial.

// Inicializar I2C (SDA = Pin 42, SCL = Pin 41)
 Wire.begin(42, 41); // Inicializa la comunicación I2C utilizando los pines 42 (SDA - Datos) y 41 (SCL - Reloj) de la placa Arduino.  I2C es un protocolo de comunicación que permite la comunicación entre múltiples dispositivos utilizando solo dos cables. En este caso, se configura para comunicarse con el sensor BH1750.

// Inicializar el sensor BH1750
    if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) { // Inicializa el sensor BH1750 en modo de alta resolución continua. 'lightMeter.begin()' intenta iniciar la comunicación con el sensor. 'BH1750::CONTINUOUS_HIGH_RES_MODE' es un modo de operación que configura el sensor para realizar mediciones de luz de alta precisión de forma continua. La función 'begin()' devuelve 'true' si la inicialización es exitosa y 'false' si falla.
        Serial.println(F("BH1750 inicializado correctamente")); // Si 'lightMeter.begin()' devuelve 'true', este mensaje se imprime en el monitor serial, indicando que el sensor BH1750 se ha inicializado correctamente. 'F()' se usa para guardar la cadena en la memoria Flash, ahorrando memoria RAM.
    } else {
        Serial.println(F("Error al inicializar BH1750")); // Si 'lightMeter.begin()' devuelve 'false', este mensaje de error se imprime en el monitor serial, indicando que hubo un problema al inicializar el sensor BH1750.
        while (1); // Detener el programa si el sensor no se inicializa. Este bucle infinito detiene la ejecución del programa si el sensor no se inicializa correctamente. Esto evita que el programa continúe ejecutándose sin poder leer los datos del sensor.
    }
}

void loop() {
// Leer el valor de luz en lux
    float lux = lightMeter.readLightLevel(); // Lee el nivel de luz actual del sensor BH1750 y lo guarda en la variable 'lux' de tipo 'float'. La función 'readLightLevel()' retorna el valor de la intensidad de la luz en lux.

// Mostrar el valor de luz en el monitor serial
    Serial.print("Luz: "); // Imprime la etiqueta "Luz: " en el monitor serial.
    Serial.print(lux); // Imprime el valor de la variable 'lux' (la lectura de luz) en el monitor serial.
    Serial.println(" lx"); // Imprime la unidad " lx" (lux) y un salto de línea en el monitor serial. 'println()' añade un salto de línea al final del mensaje.

// Esperar 1 segundo antes de la siguiente lectura
    delay(1000); // Pausa la ejecución del programa por 1000 milisegundos (1 segundo). Esto controla la frecuencia con la que se toman las lecturas del sensor.
}