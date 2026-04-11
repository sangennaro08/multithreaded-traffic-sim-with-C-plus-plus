#pragma once
#include "globals.hpp"

#include "./vehicles/car.hpp"
#include "./vehicles/ambulance.hpp"

#include "Traffic_algorithm.hpp"

#include <semaphore>
#include <unordered_map>
#include <queue>
#include <vector>
#include <mutex>
#include <memory>
#include <condition_variable>

class Intersection
{
    public:

    bool can_decide = true;
    int free_spots = 3;

    std::counting_semaphore<max_per_intersection> enter;

    std::unordered_map<std::string, bool> directions;
    std::unordered_map<std::string, std::queue<Vehicle*>> entry_queues;

    std::vector<Vehicle*> vehicles_in_intersection;

    std::mutex direction_mutex;
    std::mutex find_pattern;

    std::condition_variable get_direction_cv;
    std::condition_variable exit_cv;

    Intersection();
    ~Intersection(){}

    static void initialize_intersections(std::vector<std::unique_ptr<Intersection>>& intersections);
    static void initialize_cars(std::vector<Vehicle*>& cars, std::vector<std::unique_ptr<Intersection>>& intersections);
    static void create_ambulances(std::vector<Vehicle*>& ambulances);
    static void launch_cars(std::vector<Vehicle*>& cars, std::vector<std::unique_ptr<Intersection>>& intersections, std::vector<Vehicle*>& ambulances);
    static void join_threads(std::vector<Vehicle*>& cars, std::vector<Vehicle*>& ambulances);

    static void leave_intersection(std::unique_ptr<Intersection>& intersection, const Vehicle* V);
    static void set_directions(std::unique_ptr<Intersection>& intersection, Vehicle* V);

    private:

    static int pick_intersection(std::vector<std::unique_ptr<Intersection>>& intersections);
    void simulate_crossing(Vehicle* V, std::vector<std::unique_ptr<Intersection>>& intersections, std::vector<Vehicle*>& ambulances);
    void arrive_at_intersection(const Vehicle* V, int idx);
    void wait_in_queue(std::unique_ptr<Intersection>& next_intersection, Vehicle* V, const int next);
    void launch_ambulance(std::vector<std::unique_ptr<Intersection>>& intersections, std::vector<Vehicle*>& ambulances);
};
