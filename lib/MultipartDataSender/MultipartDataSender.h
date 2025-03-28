#ifndef MULTIPART_DATA_SENDER_H
#define MULTIPART_DATA_SENDER_H

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

class MultipartDataSender {
public:
    /**
     * Envía datos térmicos y una imagen JPEG en formato multipart/form-data.
     * @param apiUrl URL del endpoint para enviar datos.
     * @param thermalData Array de datos térmicos (32x24).
     * @param jpegImage Puntero a la imagen JPEG.
     * @param jpegLength Tamaño de la imagen JPEG.
     * @return true si el envío fue exitoso, false en caso contrario.
     */
    static bool IOThermalAndImageData(
        const String& apiUrl, 
        float* thermalData, 
        uint8_t* jpegImage, 
        size_t jpegLength
    );
};

#endif // MULTIPART_DATA_SENDER_H
