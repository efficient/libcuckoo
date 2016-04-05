/**
* @file chrono.h
* @author David Alberto Nogueira (dan)
* @brief std::chrono wrapper.
*
* USAGE:
* @code{.cpp}
* chronowrap::Chronometer chrono; //Declare a Chronometer
* chrono.GetTime(); //Start timer
* {
*   ... //do your code
* }
* chrono.StopTime(); //Stop timer
* std::cout << "Time: " << chrono.GetElapsedTime() 
*           << " sec." << std::endl; //Print duration

* @endcode
*
* @copyright Copyright (c) 2016, David Alberto Nogueira.
*            All rights reserved. See licence below.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* (1) Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* (2) Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in
* the documentation and/or other materials provided with the
* distribution.
*
* (3) The name of the author may not be used to
* endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CHRONO_H
#define CHRONO_H
#include <iostream>
#include <chrono>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace chronowrap {
class Chronometer {
public:
  Chronometer() {
    time_span = std::chrono::steady_clock::duration::zero();
  };
  virtual ~Chronometer() {};

  void GetTime() {
    clock_begin = std::chrono::steady_clock::now();
  }
  void StopTime() {
    std::chrono::steady_clock::time_point clock_end = 
    std::chrono::steady_clock::now();
    time_span += clock_end - clock_begin;
  }
  //Return elapsed time in seconds
  double GetElapsedTime() {
    return double(time_span.count()) * resolution;
  }
  void Reset() {
    time_span = std::chrono::steady_clock::duration::zero();
  }
  //in us
  double GetClockResolutionUS() {
    return resolution*1e6;
  }
  void PrintClockResolution() {
    std::cout << "clock::period: " << GetClockResolutionUS() << " us.\n";
  }
  bool IsClockSteady() {
    return std::chrono::steady_clock::is_steady;
  }
  void PrintClockSteady() {
    printf("clock::is_steady: %s\n", IsClockSteady() ? "yes" : "no");
  }
  
protected:
  std::chrono::steady_clock::time_point clock_begin;
  std::chrono::steady_clock::duration time_span;
  const double resolution = double(std::chrono::steady_clock::period::num) /
    double(std::chrono::steady_clock::period::den);
};
}

#endif // CHRONO_H
