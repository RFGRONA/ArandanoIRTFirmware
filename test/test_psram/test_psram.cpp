#include <Arduino.h>
#include <unity.h>
#include <esp_heap_caps.h> 

void setUp(void) {}
void tearDown(void) {}

// Test para verificar si la PSRAM está presente y habilitada
void test_psram_is_available() {
    TEST_ASSERT_TRUE_MESSAGE(psramFound(), "PSRAM no encontrada o no habilitada en la configuración");
}

// Test para verificar el tamaño reportado de la PSRAM
void test_psram_size() {
    TEST_ASSERT_TRUE_MESSAGE(psramFound(), "Skipping test: PSRAM no encontrada");
    size_t psramSize = ESP.getPsramSize();
    // Esperamos 8MB para un N16R8, pero comprobamos un mínimo razonable (>4MB)
    // ya que el tamaño exacto usable puede variar ligeramente.
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(4 * 1024 * 1024, psramSize, "Tamaño de PSRAM inesperadamente bajo");
}

// Test para asignar y liberar memoria específicamente en PSRAM
void test_psram_allocation_and_free() {
    TEST_ASSERT_TRUE_MESSAGE(psramFound(), "Skipping test: PSRAM no encontrada");

    size_t allocSize = 1024 * 100; // Intentar asignar 100KB
    void* psramPtr = nullptr;

    // Usar heap_caps_malloc para asegurar asignación en PSRAM
    psramPtr = heap_caps_malloc(allocSize, MALLOC_CAP_SPIRAM);

    TEST_ASSERT_NOT_NULL_MESSAGE(psramPtr, "heap_caps_malloc falló al asignar en PSRAM");

    // Si la asignación funcionó, intentar escribir/leer (simple) y liberar
    if (psramPtr != nullptr) {
        // Prueba simple de escritura/lectura
        memset(psramPtr, 0xA5, allocSize); // Llenar con un patrón
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5, *((uint8_t*)psramPtr), "Fallo al leer/escribir en memoria PSRAM asignada");

        // Liberar la memoria
        heap_caps_free(psramPtr);

        //Intentar asignar/liberar con ps_malloc si lo usas en tu código
        void* psPtr2 = ps_malloc(1024);
        TEST_ASSERT_NOT_NULL(psPtr2);
        if (psPtr2) free(psPtr2);
    }
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_psram_is_available);
    RUN_TEST(test_psram_size);
    RUN_TEST(test_psram_allocation_and_free);
    UNITY_END();
}

void loop() {}