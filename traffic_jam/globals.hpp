#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>

void set_variables();
bool controll_variables();
int valid(int number);

inline int tot_incroci = 5;
inline std::atomic<int> Auto_completate = 0;
inline int auto_presenti = 15;
inline constexpr int tot_entrare_Incrocio = 3;

inline std::mutex scrittura_info;
inline std::mutex finisci_programma;
inline std::condition_variable continua;