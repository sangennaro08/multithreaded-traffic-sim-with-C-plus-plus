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

    bool choosed = false;

    std::string direzione = "";
    std::string direzione_entrata = "";

    std::mt19937 generatore;
    std::uniform_int_distribution<int> distribution{1, 5};

    std::thread th;

    Veicolo(){}

    virtual ~Veicolo(){}

    //funzioni auto
    virtual void add_queue(){};
    virtual void start_time(){};
    virtual void get_time(){};
    virtual void exit_simulation(){};


    //funzioni Ambulanza
    virtual void add_percentage(){};
    virtual void add_percentage_sirene(){};
    virtual void reset_ambulance(){};

};