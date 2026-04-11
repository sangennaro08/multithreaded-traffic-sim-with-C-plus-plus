#pragma once

#include "../globals.hpp"
#include "vehicle.hpp"

class Ambulance: public Vehicle
{

    public:

    std::atomic<float> percentage = 0.0f;

    unsigned int patients;

    bool siren = false;

    Ambulance(size_t number)
    {
        generator.seed(static_cast<unsigned>(
                        std::chrono::steady_clock::now().time_since_epoch().count()
                    ));

        this->name = "Ambulance" + std::to_string(number);
        this->entry_direction = "Back";

        this->arrival_time = (distribution(generator)) + 2;
        this->crossing_time = (distribution(generator)) + 2;

        this->patients = distribution(generator);
    }

    ~Ambulance() override {}

    bool run() override
    {
        this->end = std::chrono::steady_clock::now();
        this->time_in_simulation = std::chrono::duration_cast<std::chrono::seconds>(this->end - this->start).count();

        int threshold = 120 / this->patients;

        if(this->time_in_simulation > threshold)
            return true;

        return false;
    }

    void start_time() override
    {
        this->start = std::chrono::steady_clock::now();
    }

    void add_percentage() override
    {
        std::lock_guard<std::mutex> lock(ambulance_mutex);
        this->percentage += 0.1f;
    }

    void reset_ambulance() override
    {
        std::lock_guard<std::mutex> lock(ambulance_mutex);
        this->percentage = 0.0f;
    }

    void exit_simulation() override
    {
        ambulances_active--;

        {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "Ambulance " << this->name << " has finished and exited the last intersection\n\n";
        }

        if(cars_completed >= total_cars && ambulances_active == 0)
            cv_continue.notify_all();
    }
};
