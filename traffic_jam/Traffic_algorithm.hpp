#pragma once
#include "globals.hpp"
#include "car.hpp"
#include <memory>
#include <mutex>
#include <random>

class Incrocio; // forward declaration

class TrafficAlgorithm
{
    public:

    static void start_algorithm(std::unique_ptr<Incrocio>& Actual_Incrocio, std::unique_ptr<Incrocio>& Next_Incrocio, Auto* A);

    private:
    
    static void go_alone(std::unique_ptr<Incrocio>& Next_Incrocio, Auto* A, int tentativi);
    static void search_exit(std::unique_ptr<Incrocio>& Actual_Incrocio, std::unique_ptr<Incrocio>& Next_Incrocio, Auto* A);
};