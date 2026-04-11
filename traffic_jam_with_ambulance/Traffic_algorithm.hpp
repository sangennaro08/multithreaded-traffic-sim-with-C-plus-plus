#pragma once
#include "globals.hpp"
#include "./vehicles/car.hpp"
#include <memory>
#include <mutex>
#include <random>

class Intersection; // forward declaration

class TrafficAlgorithm
{
    public:

    static void start_algorithm(std::unique_ptr<Intersection>& current, std::unique_ptr<Intersection>& next, Vehicle* V);
    static void rush_to_hospital(int& idx, int& next, int last_intersection, std::vector<std::unique_ptr<Intersection>>& intersections, Vehicle* V);

    private:

    static void go_alone(std::unique_ptr<Intersection>& next_intersection, Vehicle* V, int attempts);
    static void search_exit(std::unique_ptr<Intersection>& current, std::unique_ptr<Intersection>& next, Vehicle* V);
};
