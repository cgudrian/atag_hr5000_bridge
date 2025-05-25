#include <unity.h>
#include <stdint.h>
#include <stddef.h>
#include <chrono>

// Mock Arduino types for testing
using millis_t = decltype(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());

// Integrator class for testing
class Integrator {
public:
    float update(float value, millis_t currentTime) {
        if (!firstTime) {
            auto deltaT = (currentTime - lastT) / 1000.0;
            result += 0.5 * (lastValue + value) * deltaT;
        } else {
            firstTime = false;
        }

        lastValue = value;
        lastT = currentTime;

        return result;
    }

    float getResult() const { return result; }
    
    void reset() {
        firstTime = true;
        lastT = 0;
        lastValue = 0;
        result = 0;
    }

private:
    bool firstTime{true};
    millis_t lastT{};
    float lastValue{};
    float result{};
};

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

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Packet validation tests
    RUN_TEST(test_valid_packet_sizes);
    RUN_TEST(test_invalid_packet_sizes);
    RUN_TEST(test_unknown_packet_addresses);
    RUN_TEST(test_valid_data_indices);
    RUN_TEST(test_invalid_data_indices);
    
    // Integrator tests (from test_integrator.cpp)
    RUN_TEST(test_integrator_first_value);
    RUN_TEST(test_integrator_second_value);
    RUN_TEST(test_integrator_multiple_values);
    RUN_TEST(test_integrator_reset);
    RUN_TEST(test_integrator_power_to_energy_simulation);
    
    return UNITY_END();
}