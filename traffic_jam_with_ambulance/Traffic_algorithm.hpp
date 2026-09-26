#pragma once
#include "globals.hpp"
#include "./vehicles/car.hpp"
#include <memory>
#include <mutex>
#include <random>

class Intersection; // forward declaration

namespace TrafficAlgorithm
{
    void start_algorithm(std::unique_ptr<Intersection>& current, std::unique_ptr<Intersection>& next, Vehicle* V);
    void rush_to_hospital(int& idx, int& next, int last_intersection, std::vector<std::unique_ptr<Intersection>>& intersections, Vehicle* V);

    void go_alone(std::unique_ptr<Intersection>& next_intersection, Vehicle* V, int attempts);
    void search_exit(std::unique_ptr<Intersection>& current, std::unique_ptr<Intersection>& next, Vehicle* V);
}
