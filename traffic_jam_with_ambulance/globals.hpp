#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>

void set_variables();
bool check_variables();
int read_int(int number);

inline int total_intersections = 5;
inline std::atomic<int> cars_completed = 0;
inline std::atomic<int> ambulances_active = 0;
inline std::atomic<int> ambulances_launched = 0;
inline int total_cars = 15;
inline constexpr int max_per_intersection = 3;

inline std::mutex print_mutex;
inline std::mutex end_program;
inline std::mutex ambulance_mutex;

inline std::condition_variable cv_continue;
