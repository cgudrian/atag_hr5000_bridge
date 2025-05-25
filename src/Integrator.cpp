#include "Integrator.h"

#ifdef UNIT_TEST
millis_t millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
#endif

float Integrator::update(float value, millis_t currentTime) {
    if (!firstTime) {
        auto deltaT = (currentTime - lastT) / 1000.0;
        // Integration with linear interpolation (trapezoidal rule)
        // This calculates the area under the power curve
        result += 0.5 * (lastValue + value) * deltaT;
    } else {
        firstTime = false;
    }

    lastValue = value;
    lastT = currentTime;

    return result;
}

float Integrator::update(float value) {
    return update(value, millis());
}

void Integrator::reset() {
    firstTime = true;
    lastT = 0;
    lastValue = 0;
    result = 0;
}