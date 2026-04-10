#pragma once
#include "globals.hpp"
#include "car.hpp"
#include "Traffic_algorithm.hpp"

#include <semaphore>
#include <unordered_map>
#include <queue>
#include <vector>
#include <mutex>
#include <memory>
#include <condition_variable>

class Incrocio
{
    public:

    bool decide = true;
    int posti_liberi = 3;

    std::counting_semaphore<tot_entrare_Incrocio> entrare;

    std::unordered_map<std::string, bool> direzioni;
    std::unordered_map<std::string, std::queue<Auto*>> code_entrata;

    std::vector<Auto*> auto_in_incrocio;

    std::atomic<int> tot_auto = 0;

    std::mutex direzione;
    std::mutex find_patter;

    std::condition_variable get_direzione;
    std::condition_variable esci;

    Incrocio();
    ~Incrocio(){}

    static void inizializza_Incroci(std::vector<std::unique_ptr<Incrocio>>& Incroci);
    static void inizializza_auto(std::vector<Auto*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci);
    static void lancia_auto(std::vector<Auto*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci);
    static void join_threads(std::vector<Auto*>& Auto_in_gioco);

    private:

    static int decide_interception(std::vector<std::unique_ptr<Incrocio>>& Incroci);
    void Arrivo_a_incrocio(const Auto* A, int idx);
    void simula_passaggio(Auto* A, std::vector<std::unique_ptr<Incrocio>>& Incroci);
    void waiting_in_queues(std::unique_ptr<Incrocio>& Next_Incrocio, Auto* A, const int next);
    void set_directions(std::unique_ptr<Incrocio>& Actual_Incrocio, Auto* A);
    void get_out_intersection(std::unique_ptr<Incrocio>& Actual_Incrocio, const Auto* A);
};