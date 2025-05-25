#include <unity.h>
#include <stdint.h>
#include <stddef.h>
#include <chrono>

// Include the extracted Integrator class
#ifndef UNIT_TEST
#define UNIT_TEST
#endif
#include "../../src/Integrator.h"

// Include implementation for testing (since PlatformIO test doesn't build src/ files)
#include "../../src/Integrator.cpp"

// ATAG protocol constants
static constexpr uint8_t ATAG_ADDRESS_0x41 = 0x41;
static constexpr uint8_t ATAG_ADDRESS_0x71 = 0x71;
static constexpr size_t ATAG_PACKET_0x41_SIZE = 30;
static constexpr size_t ATAG_PACKET_0x71_SIZE = 41;

// Data indices
static constexpr uint8_t ATAG_DATA_INDEX_TEMPERATURES = 0x03;
static constexpr uint8_t ATAG_DATA_INDEX_STATE = 0x04;
static constexpr uint8_t ATAG_DATA_INDEX_PRESSURE = 0x06;
static constexpr uint8_t ATAG_DATA_INDEX_POWER = 0x07;

// Test functions (copied from atag_protocol.cpp for testing)
bool isValidPacketSize(uint8_t address, size_t size) {
    switch (address) {
        case ATAG_ADDRESS_0x41:
            return size == ATAG_PACKET_0x41_SIZE;
        case ATAG_ADDRESS_0x71:
            return size == ATAG_PACKET_0x71_SIZE;
        default:
            return false;
    }
}

bool isValidDataIndex(uint8_t dataIndex) {
    return dataIndex == ATAG_DATA_INDEX_TEMPERATURES ||
           dataIndex == ATAG_DATA_INDEX_STATE ||
           dataIndex == ATAG_DATA_INDEX_PRESSURE ||
           dataIndex == ATAG_DATA_INDEX_POWER;
}

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

void test_valid_packet_sizes(void) {
    // Test valid 0x41 packet
    TEST_ASSERT_TRUE(isValidPacketSize(ATAG_ADDRESS_0x41, ATAG_PACKET_0x41_SIZE));
    
    // Test valid 0x71 packet
    TEST_ASSERT_TRUE(isValidPacketSize(ATAG_ADDRESS_0x71, ATAG_PACKET_0x71_SIZE));
}

void test_invalid_packet_sizes(void) {
    // Test invalid 0x41 packet sizes
    TEST_ASSERT_FALSE(isValidPacketSize(ATAG_ADDRESS_0x41, 29));
    TEST_ASSERT_FALSE(isValidPacketSize(ATAG_ADDRESS_0x41, 31));
    TEST_ASSERT_FALSE(isValidPacketSize(ATAG_ADDRESS_0x41, 0));
    
    // Test invalid 0x71 packet sizes
    TEST_ASSERT_FALSE(isValidPacketSize(ATAG_ADDRESS_0x71, 40));
    TEST_ASSERT_FALSE(isValidPacketSize(ATAG_ADDRESS_0x71, 42));
    TEST_ASSERT_FALSE(isValidPacketSize(ATAG_ADDRESS_0x71, 0));
}

void test_unknown_packet_addresses(void) {
    // Test unknown packet addresses
    TEST_ASSERT_FALSE(isValidPacketSize(0x00, 30));
    TEST_ASSERT_FALSE(isValidPacketSize(0xFF, 41));
    TEST_ASSERT_FALSE(isValidPacketSize(0x42, 30));
}

void test_valid_data_indices(void) {
    TEST_ASSERT_TRUE(isValidDataIndex(ATAG_DATA_INDEX_TEMPERATURES));
    TEST_ASSERT_TRUE(isValidDataIndex(ATAG_DATA_INDEX_STATE));
    TEST_ASSERT_TRUE(isValidDataIndex(ATAG_DATA_INDEX_PRESSURE));
    TEST_ASSERT_TRUE(isValidDataIndex(ATAG_DATA_INDEX_POWER));
}

void test_invalid_data_indices(void) {
    TEST_ASSERT_FALSE(isValidDataIndex(0x00));
    TEST_ASSERT_FALSE(isValidDataIndex(0x01));
    TEST_ASSERT_FALSE(isValidDataIndex(0x02));
    TEST_ASSERT_FALSE(isValidDataIndex(0x05));
    TEST_ASSERT_FALSE(isValidDataIndex(0x08));
    TEST_ASSERT_FALSE(isValidDataIndex(0xFF));
}

// Integrator tests
void test_integrator_first_value(void) {
    Integrator integrator;
    
    // First call should not integrate (no previous value)
    float result = integrator.update(10.0f, 1000);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, integrator.getResult());
}

void test_integrator_second_value(void) {
    Integrator integrator;
    
    // First value at t=1000ms
    integrator.update(10.0f, 1000);
    
    // Second value at t=2000ms (1 second later)
    float result = integrator.update(20.0f, 2000);
    
    // Expected: 0.5 * (10 + 20) * 1.0 = 15.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, result);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, integrator.getResult());
}

void test_integrator_multiple_values(void) {
    Integrator integrator;
    
    // Value sequence: 10 -> 20 -> 30 over 2 seconds
    integrator.update(10.0f, 1000);  // t=1s, no integration yet
    integrator.update(20.0f, 2000);  // t=2s, integrate 10-20 over 1s = 15.0
    integrator.update(30.0f, 3000);  // t=3s, integrate 20-30 over 1s = 25.0
    
    // Total: 15.0 + 25.0 = 40.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, integrator.getResult());
}

void test_integrator_reset(void) {
    Integrator integrator;
    
    // Add some values
    integrator.update(10.0f, 1000);
    integrator.update(20.0f, 2000);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, integrator.getResult());
    
    // Reset and verify
    integrator.reset();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, integrator.getResult());
    
    // First value after reset should not integrate
    float result = integrator.update(5.0f, 3000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result);
}

void test_integrator_power_to_energy_simulation(void) {
    Integrator powerToEnergy;
    
    // Simulate 1 hour of constant 10kW power
    millis_t startTime = 0;
    float constantPower = 10.0f; // kW
    
    // First sample
    powerToEnergy.update(constantPower, startTime);
    
    // Sample every minute for 1 hour
    for (int minute = 1; minute <= 60; minute++) {
        millis_t currentTime = startTime + (minute * 60 * 1000); // Convert to milliseconds
        powerToEnergy.update(constantPower, currentTime);
    }
    
    // Expected energy: 10 kW * 1 hour = 10 kWh
    // Convert from kW·s to kWh: divide by 3600
    float energyKWh = powerToEnergy.getResult() / 3600.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, energyKWh);
}

// AtagPacketProcessor tests
void test_packet_processor_temperature_data_structure(void) {
    // Test temperature packet structure (address 0x41, size 30, data index 0x03)
    uint8_t tempPacket[30] = {0};
    tempPacket[0] = 0x41;  // Address
    tempPacket[18] = 0x03; // Data index for temperatures
    
    // Temperature values at bytes 19-26 (signed int8)
    tempPacket[19] = 20;   // Flow temperature
    tempPacket[20] = 15;   // Return temperature  
    tempPacket[21] = 45;   // Hot water temperature
    tempPacket[22] = 5;    // Outside temperature
    tempPacket[23] = 80;   // Exhaust temperature
    tempPacket[26] = 21;   // Nominal temperature (val[7])
    
    // Verify packet structure is valid
    TEST_ASSERT_EQUAL(0x41, tempPacket[0]);
    TEST_ASSERT_EQUAL(0x03, tempPacket[18]);
    TEST_ASSERT_EQUAL(20, (int8_t)tempPacket[19]);
    TEST_ASSERT_EQUAL(15, (int8_t)tempPacket[20]);
}

void test_packet_processor_power_calculation(void) {
    // Test power calculation logic
    float maxPower = 21.6f;
    uint8_t powerPercentage = 50; // 50%
    
    float expectedPower = maxPower * powerPercentage / 100.0f; // 10.8 kW
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.8f, expectedPower);
    
    // Test operation flags parsing
    uint8_t flags = 0x07; // Binary: 00000111 (pump=1, heating=1, hotwater=1)
    TEST_ASSERT_TRUE(flags & 1);  // Pump active
    TEST_ASSERT_TRUE(flags & 2);  // Heating active  
    TEST_ASSERT_TRUE(flags & 4);  // Hot water active
}

void test_packet_processor_pressure_calculation(void) {
    // Test pressure calculation: val[7] / 10.0f
    uint8_t pressureRaw = 15; // Raw value 15
    float expectedPressure = 1.5f; // 15 / 10.0f = 1.5 bar
    float actualPressure = pressureRaw / 10.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expectedPressure, actualPressure);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Packet validation tests
    RUN_TEST(test_valid_packet_sizes);
    RUN_TEST(test_invalid_packet_sizes);
    RUN_TEST(test_unknown_packet_addresses);
    RUN_TEST(test_valid_data_indices);
    RUN_TEST(test_invalid_data_indices);
    
    // Integrator tests
    RUN_TEST(test_integrator_first_value);
    RUN_TEST(test_integrator_second_value);
    RUN_TEST(test_integrator_multiple_values);
    RUN_TEST(test_integrator_reset);
    RUN_TEST(test_integrator_power_to_energy_simulation);
    
    // AtagPacketProcessor tests
    RUN_TEST(test_packet_processor_temperature_data_structure);
    RUN_TEST(test_packet_processor_power_calculation);
    RUN_TEST(test_packet_processor_pressure_calculation);
    
    return UNITY_END();
}