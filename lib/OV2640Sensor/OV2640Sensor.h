#ifndef OV2640SENSOR_H
#define OV2640SENSOR_H

#include <Arduino.h>
#include "esp_camera.h"
#include "esp_heap_caps.h"

class OV2640Sensor {
public:
  OV2640Sensor();
  
  // Inicializa la cámara; devuelve true si la inicialización fue exitosa.
  bool begin();
  
  // Captura una imagen y retorna un buffer con los datos JPEG.
  // El parámetro "length" contendrá el tamaño de la imagen.
  // Se debe liberar la memoria retornada usando free() cuando ya no se necesite.
  uint8_t* captureJPEG(size_t &length);
  
  // Desinicializa la cámara.
  void end();

private:
  // Puedes agregar funciones privadas si es necesario.
};

#endif
