#pragma once

#include "../globals.hpp"
#include "vehicle.hpp"

#include <iostream>
#include <chrono>

class Car: public Vehicle
{
    public:

    std::atomic<unsigned int> queues_entered = 0;

    Car(std::string name, int start)
    {
        generator.seed(static_cast<unsigned>(
                        std::chrono::steady_clock::now().time_since_epoch().count()
                    ));

        this->name = name;
        this->start_intersection = start;
        this->crossing_time = (distribution(generator)) * (distribution(generator));
        this->arrival_time = (distribution(generator) * 3);
    }

    static void print_info(Vehicle* V)
    {
        Car* c = dynamic_cast<Car*>(V);

        std::cout << "\n\n";
        std::cout << "-------------------------------------------\n";
        std::cout << "Car: " << V->name << "\n";
        std::cout << "Started simulation from intersection number " << V->start_intersection << "\n";
        std::cout << "Completed all intersections in " << c->time_in_simulation << "s\nor "
                  << c->time_in_simulation / 60 << " minutes\n";
        std::cout << "Had to enter a queue " << c->queues_entered << " times\n\n";
    }

    ~Car() override {}

    void add_queue() override
    {
        this->queues_entered++;
    }

    void start_time() override
    {
        this->start = std::chrono::steady_clock::now();
    }

    void get_time() override
    {
        this->end = std::chrono::steady_clock::now();
        this->time_in_simulation = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    }

    void exit_simulation() override
    {
        {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "Car " << this->name << " has finished and exited the last intersection\n\n";
        }

        if(++cars_completed >= total_cars && ambulances_active == 0)
            cv_continue.notify_all();
    }
};
