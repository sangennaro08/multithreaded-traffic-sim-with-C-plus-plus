#pragma once

#include <atomic>
#include <thread>

#include <random>
#include <string>

class Veicolo
{

    public:

    std::string nome;

    std::atomic<int> incrocio_iniziale = 0;
    int tempo_incrocio;
    int tempo_di_arrivo;

    float time_in_simulation;

    bool choosed = false;

    std::string direzione = "";
    std::string direzione_entrata = "";

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;

    std::mt19937 generatore;
    std::uniform_int_distribution<int> distribution{1, 5};

    std::thread th;

    Veicolo(){}

    virtual ~Veicolo(){}

    //funzioni auto
    virtual void add_queue(){};
    virtual void get_time(){};


    //funzioni Ambulanza
    virtual void add_percentage(){};
    virtual void reset_ambulance(){};
    virtual bool run(){return false;};
    //funzioni ambi veicoli
    virtual void exit_simulation() = 0;
    virtual void start_time() = 0;
};