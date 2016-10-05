#ifndef MODULE_ACTUATOR_PROPULSION_H
#define MODULE_ACTUATOR_PROPULSION_H

#include <nuclear>
#include <utility/io/uart.h>
#include "TorqeedoHAL.h"
#include "Stepper.h"

namespace module {
namespace actuator {

    class Propulsion : public NUClear::Reactor {

        struct Thruster
        {
            utility::io::uart torqeedo_uart, stepper_uart;
            std::unique_ptr<TorqeedoHAL> torqeedo;
            std::unique_ptr<Stepper> stepper;
        };

        Thruster port, starboard;

        int i;
        bool started;

    public:
        /// @brief Called by the powerplant to build and setup the Propulsion reactor.
        explicit Propulsion(std::unique_ptr<NUClear::Environment> environment);
    };

}
}

#endif  // MODULE_ACTUATOR_PROPULSION_H