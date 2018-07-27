#pragma once
/*
 * Tools for debuging construction, destruction, copying and moving.
 * THe idea for noise class is from here: 
 * https://stackoverflow.com/questions/47026590/unable-to-construct-runtime-boost-spsc-queue-with-runtime-size-parameter-in-shar?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
 */

#include <iostream>

namespace tools {
    namespace noisy {

        /// noisy - traces special members
        struct noisy {
            noisy& operator=(noisy&&) noexcept { std::cout << "operator=(noisy&&)\n"; return *this;      } 
            noisy& operator=(const noisy&)     { std::cout << "operator=(const noisy&)\n"; return *this; } 
            noisy(const noisy&)                { std::cout << "noisy(const noisy&)\n";                   } 
            noisy(noisy&&) noexcept            { std::cout << "noisy(noisy&&)\n";                        } 
            ~noisy()                           { std::cout << "~noisy()\n";                              } 
            noisy()                            { std::cout << "noisy()\n";                               } 
        };

        /*
         * Noisy string for debugging. 
         * Usage: just use tools::noisy::string as a replacement for std::string
         */
        struct string : std::string, tools::noisy::noisy { using std::string::string; };
    }
}
