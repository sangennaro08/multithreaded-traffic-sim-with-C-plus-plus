#include "Intersection.hpp"
#include "Traffic_algorithm.hpp"

Intersection::Intersection() : enter(max_per_intersection),
    directions(
        {{"Right",   true},
        {"Left",     true},
        {"Back",     true},
        {"Forward",  true}}),

    entry_queues(
        {{"Right",  {}},
        {"Left",    {}},
        {"Back",    {}}})
{}

void Intersection::initialize_intersections(std::vector<std::unique_ptr<Intersection>>& intersections)
{
    intersections.reserve(total_intersections);

    for(int i = 0; i < total_intersections; i++)
        intersections.emplace_back(std::make_unique<Intersection>());
}

void Intersection::initialize_cars(std::vector<Vehicle*>& cars, std::vector<std::unique_ptr<Intersection>>& intersections)
{
    cars.reserve(total_cars);

    for(int i = 0; i < total_cars; i++)
    {
        std::string name = 'A' + std::to_string(i);
        int pos = pick_intersection(intersections);
        Vehicle* c = new Car(name, pos);

        cars.push_back(c);

        intersections.at(pos)->vehicles_in_intersection.push_back(c);
    }
}

void Intersection::create_ambulances(std::vector<Vehicle*>& ambulances)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> create{1, (((int)total_cars / 8) + 1)};

    int size = create(gen);

    ambulances.reserve(size);

    for(size_t i = 0; i < size; i++)
        ambulances.emplace_back(new Ambulance(i));
}

void Intersection::launch_cars(std::vector<Vehicle*>& cars, std::vector<std::unique_ptr<Intersection>>& intersections, std::vector<Vehicle*>& ambulances)
{
    for(size_t i = 0; i < cars.size(); i++)
        cars.at(i)->th = std::thread(&Intersection::simulate_crossing,
            intersections.at(cars.at(i)->start_intersection).get(),
            cars.at(i), std::ref(intersections), std::ref(ambulances));
}

void Intersection::join_threads(std::vector<Vehicle*>& cars, std::vector<Vehicle*>& ambulances)
{
    for(auto& car : cars)
        if(car->th.joinable()) car->th.join();

    for(auto& amb : ambulances)
        if(amb->th.joinable()) amb->th.join();
}

int Intersection::pick_intersection(std::vector<std::unique_ptr<Intersection>>& intersections)
{
    for( ; ; )
    {
        static std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
        static std::uniform_int_distribution<int> dist(0, total_intersections - 1);
        int pos = dist(rng);

        if(intersections.at(pos)->vehicles_in_intersection.size() < 3)
            return pos;
    }
}

void Intersection::arrive_at_intersection(const Vehicle* V, int idx)
{
    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "Vehicle " << V->name << " is arriving at intersection "
                  << idx << " in " << V->arrival_time << " seconds\n\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(V->arrival_time));
}

void Intersection::set_directions(std::unique_ptr<Intersection>& intersection, Vehicle* V)
{
    std::lock_guard<std::mutex> lock(intersection->direction_mutex);

    if(V->entry_direction.empty())
    {
        for(auto& [dir, available] : intersection->directions)
        {
            if(available)
            {
                V->exit_direction = dir;
                available = false;
                break;
            }
        }
    }
    else
    {
        V->exit_direction = V->entry_direction;
        V->entry_direction = "";
    }
}

void Intersection::leave_intersection(std::unique_ptr<Intersection>& intersection, const Vehicle* V)
{
    std::lock_guard<std::mutex> lock(intersection->direction_mutex);

    auto& vec = intersection->vehicles_in_intersection;
    auto it = find(vec.begin(), vec.end(), V);
    if(it != vec.end())
        vec.erase(it);

    intersection->directions.at(V->exit_direction) = true;

    intersection->exit_cv.notify_all();
}

void Intersection::wait_in_queue(std::unique_ptr<Intersection>& next_intersection, Vehicle* V, const int next)
{
    std::unique_lock<std::mutex> lock(next_intersection->direction_mutex);

    if(!(next_intersection->directions.at(V->entry_direction)))
    {
        next_intersection->entry_queues.at(V->entry_direction).push(V);

        V->add_queue();

        {
            std::lock_guard<std::mutex> lock1(print_mutex);
            std::cout << "Vehicle " << V->name << " is waiting in queue "
                      << V->entry_direction << " at intersection " << next << "\n\n";
        }

        next_intersection->exit_cv.wait(lock, [&]{
            return next_intersection->entry_queues.at(V->entry_direction).front() == V &&
                   next_intersection->directions.at(V->entry_direction);
        });

        next_intersection->entry_queues.at(V->entry_direction).pop();

        {
            std::lock_guard<std::mutex> lock1(print_mutex);
            std::cout << "Vehicle " << V->name << " left the queue "
                      << V->entry_direction << " at intersection " << next << "\n\n";
        }
    }

    next_intersection->vehicles_in_intersection.push_back(V);
    next_intersection->directions.at(V->entry_direction) = false;
}

void Intersection::launch_ambulance(std::vector<std::unique_ptr<Intersection>>& intersections, std::vector<Vehicle*>& ambulances)
{
    if(ambulances_launched == ambulances.size())
        return;

    std::scoped_lock lock(ambulance_mutex, intersections.at(0)->direction_mutex);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> threshold_dist{0.8f, 1.0f};

    if(Ambulance* a = dynamic_cast<Ambulance*>(ambulances.at(ambulances_launched)))
        if(a->percentage >= threshold_dist(gen) && intersections.at(0)->directions.at("Back"))
        {
            intersections.at(0)->vehicles_in_intersection.push_back(ambulances.at(ambulances_launched));
            ambulances_active++;
            ambulances_launched++;

            a->th = std::thread(&Intersection::simulate_crossing, intersections.at(0).get(),
                                a, std::ref(intersections), std::ref(ambulances));
        }
}

void Intersection::simulate_crossing(Vehicle* V, std::vector<std::unique_ptr<Intersection>>& intersections, std::vector<Vehicle*>& ambulances)
{
    int idx = V->start_intersection;
    int finish_last = idx % total_intersections;
    const int last_intersection = total_intersections - 1;
    
    V->start_time();

    arrive_at_intersection(V, idx);

    for(size_t i = 0; i < last_intersection || (finish_last != last_intersection); i++)
    {
        if(intersections.at(idx)->enter.try_acquire())
        {
            {
                std::lock_guard<std::mutex> lock(intersections.at(idx)->direction_mutex);
                intersections.at(idx)->free_spots--;
            }

            set_directions(intersections.at(idx), V);

            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::cout << "Vehicle " << V->name << " arrived at intersection "
                          << idx << " and is deciding direction for next intersection\n\n";
            }

            int next = (idx + 1) % total_intersections;

            TrafficAlgorithm::start_algorithm(intersections.at(idx), intersections.at(next), V);

            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::cout << "Vehicle " << V->name << " is crossing intersection "
                          << idx << " in " << V->crossing_time << " seconds\n\n";
            }

            intersections.at(idx)->enter.release();

            {
                std::lock_guard<std::mutex> lock(intersections.at(idx)->direction_mutex);
                intersections.at(idx)->free_spots++;
            }

            std::this_thread::sleep_for(std::chrono::seconds(V->crossing_time));

            leave_intersection(intersections.at(idx), V);
            arrive_at_intersection(V, next);

            if(next != last_intersection || i < intersections.size())
                wait_in_queue(intersections.at(next), V, next);

            V->start_intersection = next;
            idx = next;
            finish_last = idx % total_intersections;

            if(V->run())
            {
                TrafficAlgorithm::rush_to_hospital(idx, next, last_intersection, intersections, V);
                break;
            }
        }
    }

    leave_intersection(intersections.at(total_intersections - 1), V);

    V->reset_ambulance();

    if(ambulances_launched != ambulances.size())
        ambulances.at(ambulances_launched)->add_percentage();

    launch_ambulance(intersections, ambulances);

    V->exit_simulation();
    V->get_time();
}
