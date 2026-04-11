#include "Traffic_algorithm.hpp"
#include "Intersection.hpp"

void TrafficAlgorithm::start_algorithm(std::unique_ptr<Intersection>& current, std::unique_ptr<Intersection>& next, Vehicle* V)
{
    int attempts = 0;
    V->choosed = false;

    do
    {
        {
            std::unique_lock<std::mutex> lock(current->find_pattern);

            if(current->can_decide && !V->choosed)
            {
                current->can_decide = false;
                search_exit(current, next, V);
                current->can_decide = true;

                lock.unlock();
                current->get_direction_cv.notify_all();
            }
            else
            {
                current->get_direction_cv.wait(lock, [&]{return V->choosed;});
            }
        }

        attempts++;
        go_alone(next, V, attempts);

    }while(!V->choosed);
}

void TrafficAlgorithm::go_alone(std::unique_ptr<Intersection>& next_intersection, Vehicle* V, int attempts)
{
    if(attempts >= 3 && !V->choosed)
    {
        std::lock_guard<std::mutex> lock(next_intersection->direction_mutex);

        std::uniform_int_distribution<int> dist(0, 3);

        auto it = next_intersection->directions.begin();

        do
        {
            it = next_intersection->directions.begin();
            std::advance(it, dist(V->generator));

        }while(it->first == "Forward");

        V->entry_direction = it->first;
        V->choosed = true;
    }
}

void TrafficAlgorithm::search_exit(std::unique_ptr<Intersection>& current, std::unique_ptr<Intersection>& next, Vehicle* V)
{
    std::lock_guard<std::mutex> lock(current->direction_mutex);
    std::lock_guard<std::mutex> lock_next(next->direction_mutex);

    int to_move = std::min(next->free_spots, (int)current->vehicles_in_intersection.size());

    for(int i = 0; i < to_move; i++)
    {
        for(auto& [dir, available] : next->directions)
        {
            if(available && current->vehicles_in_intersection.at(i)->entry_direction != dir && dir != "Forward")
            {
                current->vehicles_in_intersection.at(i)->entry_direction = dir;
                current->vehicles_in_intersection.at(i)->choosed = true;

                break;
            }
        }
    }
}

void TrafficAlgorithm::rush_to_hospital(int& idx, int& next, int last_intersection, std::vector<std::unique_ptr<Intersection>>& intersections, Vehicle* V)
{
    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "Ambulance " << V->name  << " is rushing to the hospital! Too much time has passed!\n\n";
    }

    while(idx != last_intersection)
    {
        next = (idx + 1) % total_intersections;

        Intersection::set_directions(intersections.at(idx), V);

        std::this_thread::sleep_for(std::chrono::seconds(V->crossing_time / 2));

        Intersection::leave_intersection(intersections.at(idx), V);

        std::this_thread::sleep_for(std::chrono::seconds(V->arrival_time / 2));

        {
            std::lock_guard<std::mutex> lock(intersections.at(next)->direction_mutex);
            intersections.at(next)->vehicles_in_intersection.push_back(V);
        }

        V->start_intersection = next;
        idx = next;
    }

    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "Ambulance " << V->name << " has reached the hospital!\n\n";
    }
}
