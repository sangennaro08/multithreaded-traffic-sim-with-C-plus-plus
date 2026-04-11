#include "Intersection.hpp"

#include <limits>

/*
Traffic Simulator
Multiple cars (threads) travel through roads with shared intersections.
Each intersection is a shared resource protected by a mutex —
cars queue up, wait for the green light, and cross.
This clearly demonstrates resource contention and deadlock prevention.

Road rules:
1) Max 3 cars per intersection
2) Cars must check if the next intersection is free before proceeding

Intersection crossing algorithm:
1) Check free spots in the next intersection
2) Check free spots in the current intersection
3) Move the correct car based on road rules
*/

int main()
{
    set_variables();

    std::vector<std::unique_ptr<Intersection>> intersections;
    std::vector<Vehicle*> cars;
    std::vector<Vehicle*> ambulances;

    Intersection::initialize_intersections(intersections);
    Intersection::initialize_cars(cars, intersections);
    Intersection::create_ambulances(ambulances);

    std::cout << "Created " << ambulances.size() << " ambulances that may appear during simulation...\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    Intersection::launch_cars(cars, intersections, ambulances);

    {
        std::unique_lock<std::mutex> lock(end_program);
        cv_continue.wait(lock, [&]{return cars_completed >= total_cars && ambulances_active == 0;});
    }

    Intersection::join_threads(cars, ambulances);

    for(int i = 0; i < cars.size(); i++)
        Car::print_info(cars.at(i));

    for(auto& car : cars)
        delete car;

    for(auto& amb : ambulances)
        delete amb;

    return 0;
}

void set_variables()
{
    std::cout << "Set simulation variables:\n";
    do
    {
        std::cout << "Enter number of intersections (minimum 2)\n";
        total_intersections = read_int(total_intersections);

        std::cout << "Maximum cars allowed: " << total_intersections * 3 << "\n\n";

        std::cout << "Enter number of cars\n";
        total_cars = read_int(total_cars);

    }while(check_variables());

    std::cout << "Setup complete. Simulation will start in 5 seconds...\n\n";

    std::this_thread::sleep_for(std::chrono::seconds(5));
}

bool check_variables()
{
    if(total_intersections * 3 >= total_cars && total_intersections >= 2)
        return false;

    return true;
}

int read_int(int number)
{
    while(!(std::cin >> number))
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    return number;
}
