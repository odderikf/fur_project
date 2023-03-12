//
// Created by odder on 10/03/2023.
//

#include "timeutils.h"
#include <chrono>


static auto prev = std::chrono::steady_clock::now();
double get_time_delta_seconds(){
    auto current = std::chrono::steady_clock::now();
    auto diff = current - prev;
    prev = current;
    return std::chrono::duration<double>(diff).count();
}