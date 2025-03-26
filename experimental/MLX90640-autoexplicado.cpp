#include <Arduino.h> // Librería base de Arduino para funciones generales de programación en Arduino.
#include <Wire.h>    // Librería para comunicación I2C, necesaria para interactuar con el sensor MLX90640.
#include <Adafruit_MLX90640.h> // Librería específica de Adafruit para el sensor MLX90640. Esta librería facilita la interfaz con el sensor.

Adafruit_MLX90640 mlx; // Crea un objeto 'mlx' de la clase Adafruit_MLX90640. Este objeto será utilizado para controlar el sensor MLX90640.
float frame[32 * 24];  // Declara un array de tipo float llamado 'frame' de tamaño 32x24 (768 elementos). Este array se utilizará como buffer para almacenar los datos de temperatura leídos del sensor MLX90640. Cada elemento del array corresponderá a la temperatura de un píxel del sensor.

// Función para mapear la temperatura a un carácter de densidad para escala de grises simulada.
// Esta función toma un valor de temperatura como entrada y devuelve un carácter que representa visualmente la temperatura en una escala de "grises" simulada con caracteres.
char temperaturaACaracter(float temp) {
  if (temp < 5.0) {
    return ' '; // Si la temperatura es menor a 5°C, retorna un espacio en blanco ' '. Esto simula el color negro, representando temperaturas muy frías fuera del rango de interés.
  } else if (temp > 40.0) {
    return '#'; // Si la temperatura es mayor a 40°C, retorna el carácter '#'. Esto simula el color blanco, representando temperaturas muy cálidas fuera del rango de interés.
  } else {
    // Si la temperatura está entre 5°C y 40°C, se realiza un mapeo lineal para asignar un carácter que represente la temperatura dentro de este rango.
    float escala = (temp - 5.0) / (40.0 - 5.0); // Calcula una escala normalizada entre 0 y 1.
          // Se resta 5.0 (el mínimo del rango) a la temperatura y se divide por el rango total (40.0 - 5.0 = 35.0).
          // 'escala' ahora tendrá un valor entre 0 (para 5°C) y 1 (para 40°C).
    if (escala < 0.2) return '.'; // Si 'escala' está entre 0 y 0.2, retorna el carácter '.'.  Representa las temperaturas más bajas dentro del rango de 5-40°C.
    else if (escala < 0.4) return ','; // Si 'escala' está entre 0.2 y 0.4, retorna el carácter ','.
    else if (escala < 0.6) return '-'; // Si 'escala' está entre 0.4 y 0.6, retorna el carácter '-'.
    else if (escala < 0.8) return '+'; // Si 'escala' está entre 0.6 y 0.8, retorna el carácter '+'.
    else return '*'; // Si 'escala' está entre 0.8 y 1, retorna el carácter '*'. Representa las temperaturas más altas dentro del rango de 5-40°C.
          // Los caracteres '.', ',', '-', '+', '*' se eligen para simular diferentes niveles de "densidad" o "gris" en el monitor serie,
          // creando una representación visual aproximada de la temperatura.
  }
}

void setup() {
  Serial.begin(115200); // Inicializa la comunicación serial a 115200 baudios. Esta velocidad debe coincidir con la configurada en el monitor serie de Arduino IDE para poder ver la salida.
  Wire.begin(16, 15);  // Inicializa la comunicación I2C en los pines GPIO 16 (SDA) y GPIO 15 (SCL) del ESP32.
      // Estos pines son específicos para el ESP32 y pueden variar en otras placas Arduino.
  Wire.setClock(400000); // Configura la velocidad del reloj I2C a 400kHz. Una velocidad más alta puede mejorar la velocidad de comunicación si el sensor y el microcontrolador lo soportan.
      // Empieza con 100kHz por defecto, pero se aumenta a 400kHz para potencialmente mejorar la velocidad.

  // Sección de Diagnóstico Inicial:
  Serial.println("\n\n=== Test MLX90640 ==="); // Imprime un encabezado en el monitor serie para indicar el inicio de las pruebas del sensor MLX90640.
  Serial.println("Realizando escaneo I2C..."); // Informa que se va a realizar un escaneo del bus I2C para detectar dispositivos.

  byte count = 0; // Inicializa un contador 'count' a 0. Este contador se utilizará para contar el número de dispositivos I2C encontrados.
  for (byte i = 8; i < 120; i++) { // Bucle for que itera a través de posibles direcciones I2C (direcciones I2C válidas generalmente están entre 8 y 119).
    Wire.beginTransmission(i); // Inicia una transmisión I2C a la dirección 'i'. Esto es parte del proceso de escaneo I2C para verificar si un dispositivo responde en esta dirección.
    if (Wire.endTransmission() == 0) { // Intenta finalizar la transmisión (sin enviar datos). Si 'endTransmission' retorna 0, significa que un dispositivo ACK (acknowledges) en la dirección 'i', por lo tanto, hay un dispositivo presente.
      Serial.print("Dispositivo encontrado en: 0x"); // Si se encuentra un dispositivo, imprime un mensaje indicando que se encontró un dispositivo.
      Serial.println(i, HEX); // Imprime la dirección I2C 'i' en formato hexadecimal.
      count++; // Incrementa el contador de dispositivos encontrados.
      delay(1); // Pequeña pausa de 1ms. Puede ser útil para la estabilidad del bus I2C en algunos casos o para asegurar que el monitor serie pueda procesar la salida.
    }
  }
  Serial.print(count); // Después de escanear todas las direcciones, imprime el número total de dispositivos I2C encontrados.
  Serial.println(" dispositivos encontrados"); // Completa el mensaje indicando que se ha terminado de buscar dispositivos.

  // Sección de Inicialización del Sensor MLX90640:
  Serial.println("Inicializando MLX90640..."); // Informa que se va a inicializar el sensor MLX90640.
  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) { // Inicializa el sensor MLX90640 utilizando la dirección I2C por defecto (MLX90640_I2CADDR_DEFAULT) y el objeto Wire para la comunicación I2C.
          // 'mlx.begin' retorna 'true' si la inicialización es exitosa y 'false' si falla. El '!' niega el resultado, por lo que el 'if' se ejecuta si la inicialización falla.
    Serial.println("¡Error de comunicación con el sensor!"); // Si la inicialización falla, imprime un mensaje de error en el monitor serie.
    while (1) delay(10); // Entra en un bucle infinito que imprime un mensaje de error y hace una pausa de 10ms continuamente. Detiene la ejecución del programa para indicar que hay un problema crítico.
  }

  // Configuración del sensor MLX90640:
  mlx.setMode(MLX90640_CHESS); // Configura el modo de operación del sensor a 'CHESS MODE'. En este modo, las subpáginas del sensor se leen de forma entrelazada, lo que puede ser útil para ciertas aplicaciones.
  mlx.setResolution(MLX90640_ADC_18BIT); // Establece la resolución del ADC (Convertidor Analógico-Digital) del sensor a 18 bits. Una resolución más alta proporciona lecturas de temperatura más precisas pero puede aumentar el tiempo de lectura.
  mlx.setRefreshRate(MLX90640_4_HZ); // Configura la tasa de refresco del sensor a 4Hz. Esto significa que el sensor tomará 4 mediciones de temperatura por segundo. Se pueden elegir otras tasas según la necesidad de la aplicación.

  Serial.println("Configuración exitosa"); // Imprime un mensaje de éxito si la configuración del sensor se ha realizado correctamente.
  Serial.println("Resolución: 18 bits"); // Confirma la resolución configurada.
  Serial.println("Modo: Chess"); // Confirma el modo de operación configurado.
  Serial.println("Tasa de refresco: 4Hz"); // Confirma la tasa de refresco configurada.
}

void loop() {
  long start = millis(); // Almacena el tiempo actual en milisegundos al inicio del bucle 'loop'. Se utiliza para medir el tiempo que tarda en leer y procesar los datos del sensor.

  if (mlx.getFrame(frame) != 0) { // Llama a la función 'mlx.getFrame(frame)' para leer un frame de datos del sensor MLX90640 y almacenarlos en el array 'frame'.
         // 'mlx.getFrame' retorna 0 si la lectura es exitosa y un valor diferente de 0 si hay un error.
    Serial.println("Error al leer frame"); // Si 'mlx.getFrame' no retorna 0, imprime un mensaje de error indicando que hubo un problema al leer los datos del sensor.
    return; // Sale de la función 'loop'. En la siguiente iteración de 'loop', intentará leer un nuevo frame.
  }

  Serial.println("\nDatos térmicos:"); // Imprime un encabezado para indicar que a continuación se mostrarán los datos térmicos numéricos.
  // Imprime datos numéricos en un array de float de forma matricial (32x24)
  for (uint8_t y = 0; y < 24; y++) { // Bucle externo que itera sobre las filas (eje Y) del array de datos del sensor (24 filas).
    for (uint8_t x = 0; x < 32; x++) { // Bucle interno que itera sobre las columnas (eje X) de cada fila (32 columnas).
      Serial.printf("%4.1f ", frame[y * 32 + x]); // Imprime el valor de temperatura almacenado en 'frame[y * 32 + x]'.
          // '%4.1f' es un formato de impresión que indica:
          // - %f: Imprimir como un número de punto flotante (float).
          // - .1: Imprimir con 1 decimal.
          // - 4: Reservar un ancho de al menos 4 caracteres para el número, incluyendo el signo y el punto decimal. Esto ayuda a alinear la salida en columnas.
          // 'frame[y * 32 + x]' accede al elemento correcto en el array unidimensional 'frame' que representa la posición (x, y) en la matriz 24x32.
    }
    Serial.println(); // Después de imprimir los 32 valores de una fila (columnas), imprime un salto de línea para pasar a la siguiente fila en el monitor serie.
  }

  Serial.println("\nMapa de calor simulado:"); // Imprime un encabezado para indicar que a continuación se mostrará el mapa de calor simulado con caracteres.
  // Imprime mapa de calor simulado con caracteres
  for (uint8_t y = 0; y < 24; y++) { // Bucle externo para iterar sobre las filas.
    for (uint8_t x = 0; x < 32; x++) { // Bucle interno para iterar sobre las columnas.
      Serial.print(temperaturaACaracter(frame[y * 32 + x])); // Llama a la función 'temperaturaACaracter' con el valor de temperatura actual para obtener el carácter correspondiente a esa temperatura según la escala definida.
          // Imprime el carácter retornado por 'temperaturaACaracter'.
      Serial.print(" "); // Imprime un espacio después de cada carácter para separar visualmente los caracteres en la rejilla del mapa de calor simulado, mejorando la legibilidad.
    }
    Serial.println(); // Después de imprimir los 32 caracteres de una fila, imprime un salto de línea para empezar una nueva fila en el monitor serie.
  }

  Serial.printf("Tiempo de lectura: %d ms\n", millis() - start); // Calcula el tiempo transcurrido desde el inicio del bucle 'loop' hasta este punto (tiempo de lectura y procesamiento) restando 'start' del tiempo actual 'millis()'.
                                                                   // Imprime el tiempo de lectura en milisegundos.
  delay(5000); // Pausa la ejecución del programa durante 5000 milisegundos (5 segundos). Esto establece la frecuencia con la que se toman y muestran nuevas lecturas del sensor.
}