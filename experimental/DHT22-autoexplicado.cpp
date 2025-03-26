#include <Arduino.h> // Incluye la biblioteca Arduino estándar, necesaria para la programación en Arduino.
#include <DHT.h> // Incluye la biblioteca DHT sensor library, necesaria para usar sensores DHT.

// Configuración del sensor
#define DHTPIN 38      // GPIO7 - Define el pin digital al que está conectado el sensor DHT22. 
#define DHTTYPE DHT22  // Tipo de sensor - Define el tipo de sensor DHT que se está utilizando, en este caso, DHT22.

DHT dht(DHTPIN, DHTTYPE); // Crea un objeto DHT llamado 'dht' para interactuar con el sensor, utilizando el pin y el tipo definidos.

void setup() { // Función setup - se ejecuta una sola vez al inicio del programa.
  Serial.begin(115200); // Inicializa la comunicación serial a 115200 baudios para enviar datos a la computadora.
  Serial.println("\nIniciando prueba del DHT22"); // Imprime un mensaje en el monitor serial indicando que la prueba del DHT22 está comenzando.
  dht.begin(); // Inicializa el sensor DHT22.
}

void loop() { // Función loop - se ejecuta continuamente después de la función setup.
  // Espera entre lecturas (mínimo 2 segundos para DHT22)
  delay(2000); // Pausa el programa por 2000 milisegundos (2 segundos). Esto es importante para dar tiempo al sensor DHT22 para realizar la lectura.

  // Lectura de temperatura y humedad
  float h = dht.readHumidity(); // Lee la humedad del sensor DHT22 y la guarda en la variable 'h' de tipo float.
  float t = dht.readTemperature(); // Lee la temperatura del sensor DHT22 y la guarda en la variable 't' de tipo float.

  // Verifica si la lectura falló
  if (isnan(h) || isnan(t)) { // Verifica si la lectura de humedad 'h' o temperatura 't' es 'Not a Number' (NaN), lo que indica un error en la lectura del sensor.
    Serial.println("Error leyendo el sensor!"); // Imprime un mensaje de error en el monitor serial si la lectura del sensor falla.
    return; // Sale de la función loop y vuelve a empezar desde el principio.
  }

  // Muestra los resultados
  Serial.print("Humedad: "); // Imprime la etiqueta "Humedad: " en el monitor serial.
  Serial.print(h); // Imprime el valor de la humedad leída.
  Serial.print("%\t"); // Imprime el símbolo de porcentaje y un tabulador para formatear la salida.
  Serial.print("Temperatura: "); // Imprime la etiqueta "Temperatura: " en el monitor serial.
  Serial.print(t); // Imprime el valor de la temperatura leída.
  Serial.println("°C"); // Imprime el símbolo de grados Celsius y un salto de línea para la siguiente lectura.
}