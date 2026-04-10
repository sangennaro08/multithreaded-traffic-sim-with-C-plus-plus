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

class Incrocio
{
    public:

    bool decide = true;
    int posti_liberi = 3;

    std::counting_semaphore<tot_entrare_Incrocio> entrare;

    std::unordered_map<std::string, bool> direzioni;
    std::unordered_map<std::string, std::queue<Veicolo*>> code_entrata;

    std::vector<Veicolo*> auto_in_incrocio;

    //std::atomic<int> tot_auto = 0;

    std::mutex direzione;
    std::mutex find_patter;

    std::condition_variable get_direzione;
    std::condition_variable esci;

    Incrocio();
    ~Incrocio(){}

    static void inizializza_Incroci(std::vector<std::unique_ptr<Incrocio>>& Incroci);
    static void inizializza_auto(std::vector<Veicolo*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci);
    static void create_ambulances(std::vector<Veicolo*>& Ambulanze);
    static void lancia_auto(std::vector<Veicolo*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci, std::vector<Veicolo*>& );
    static void join_threads(std::vector<Veicolo*>& Auto_in_gioco, std::vector<Veicolo*>& Ambulances);

    static void get_out_intersection(std::unique_ptr<Incrocio>& Actual_Incrocio, const Veicolo* A);
    static void set_directions(std::unique_ptr<Incrocio>& Actual_Incrocio, Veicolo* A);

    private:

    static int decide_interception(std::vector<std::unique_ptr<Incrocio>>& Incroci);
    void simula_passaggio(Veicolo* A, std::vector<std::unique_ptr<Incrocio>>& Incroci, std::vector<Veicolo*>& Ambulanze);
    void Arrivo_a_incrocio(const Veicolo* A, int idx);
    void waiting_in_queues(std::unique_ptr<Incrocio>& Next_Incrocio, Veicolo* A, const int next);
    void launch_ambulance(std::vector<std::unique_ptr<Incrocio>>& Incroci, std::vector<Veicolo*>& Ambulanze);
};