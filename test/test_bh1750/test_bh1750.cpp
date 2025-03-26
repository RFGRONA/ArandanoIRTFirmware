#include <Arduino.h>
#include <unity.h>
#include "BH1750Sensor.h"

TwoWire testWire(0);
BH1750Sensor testSensor(testWire, 42, 41); // SDA y SCL

void test_sensor_initialization() {
  TEST_ASSERT_TRUE(testSensor.begin());
}

void test_light_reading() {
  float lux = testSensor.readLightLevel();
  TEST_ASSERT_GREATER_OR_EQUAL(0.0, lux);
}

void setup() {
  delay(2000);
  testWire.begin(42, 41); // SDA y SCL
  
  UNITY_BEGIN();
  RUN_TEST(test_sensor_initialization);
  RUN_TEST(test_light_reading);
  UNITY_END();
}

void loop() {}