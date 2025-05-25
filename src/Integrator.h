#pragma once

#ifdef UNIT_TEST
#include <chrono>
using millis_t = decltype(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());
#else
#include <Arduino.h>
using millis_t = decltype(millis());
#endif

/**
 * @brief Power integrator class for calculating cumulative energy consumption
 * 
 * This class implements numerical integration using linear interpolation
 * to calculate energy (kWh) from power (kW) measurements over time.
 * 
 * The integration formula used is:
 * energy += 0.5 * (lastPower + currentPower) * deltaTime
 * 
 * This represents the area under the power curve using the trapezoidal rule.
 */
class Integrator {
public:
    /**
     * @brief Update the integrator with a new power value
     * 
     * @param value Power value in kW
     * @param currentTime Current timestamp in milliseconds
     * @return Current integrated energy in kW⋅s
     */
    float update(float value, millis_t currentTime);
    
    /**
     * @brief Update the integrator with a new power value (uses millis() for time)
     * 
     * @param value Power value in kW
     * @return Current integrated energy in kW⋅s
     */
    float update(float value);
    
    /**
     * @brief Get the current integrated result
     * 
     * @return Integrated energy in kW⋅s
     */
    float getResult() const { return result; }
    
    /**
     * @brief Reset the integrator to initial state
     */
    void reset();

private:
    bool firstTime{true};        ///< Flag to track first measurement
    millis_t lastT{};           ///< Last timestamp in milliseconds
    float lastValue{};          ///< Last power value in kW
    float result{};             ///< Integrated energy in kW⋅s
};