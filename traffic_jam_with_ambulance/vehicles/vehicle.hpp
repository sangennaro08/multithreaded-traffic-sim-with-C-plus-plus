#pragma once

#include <atomic>
#include <thread>
#include <random>
#include <string>

class Vehicle
{

    public:

    std::string name;

    std::atomic<int> start_intersection = 0;
    int crossing_time;
    int arrival_time;

    float time_in_simulation;

    bool choosed = false;

    std::string exit_direction = "";
    std::string entry_direction = "";

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;

    std::mt19937 generator;
    std::uniform_int_distribution<int> distribution{1, 5};

    std::thread th;

    Vehicle(){}

    virtual ~Vehicle(){}

    // Car functions
    virtual void add_queue(){};
    virtual void get_time(){};

    // Ambulance functions
    virtual void add_percentage(){};
    virtual void reset_ambulance(){};
    virtual bool run(){return false;};

    // Both vehicles
    virtual void exit_simulation() = 0;
    virtual void start_time() = 0;
};
