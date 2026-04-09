#pragma once
#include "globals.hpp"
#include "./vehicles/car.hpp"
#include <memory>
#include <mutex>
#include <random>

class Incrocio; // forward declaration

class TrafficAlgorithm
{
    public:

    static void start_algorithm(std::unique_ptr<Incrocio>& Actual_Incrocio, std::unique_ptr<Incrocio>& Next_Incrocio, Veicolo* A);

    private:
    
    static void go_alone(std::unique_ptr<Incrocio>& Next_Incrocio, Veicolo* A, int tentativi);
    static void search_exit(std::unique_ptr<Incrocio>& Actual_Incrocio, std::unique_ptr<Incrocio>& Next_Incrocio, Veicolo* A);
};