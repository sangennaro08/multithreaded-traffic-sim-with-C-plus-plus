#pragma once
#include "globals.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <random>

class Auto
{
    public:

    int nome;
    std::atomic<int> incrocio_iniziale;
    int tempo_incrocio;
    int tempo_di_arrivo;

    std::atomic<unsigned int> entered_queues = 0;

    unsigned long long time_in_simulation;

    std::string direzione = "";
    std::string direzione_entrata = "";

    bool choosed = false;

    std::mt19937 generatore;

    std::thread th;

    Auto(int nome, int inizio) : nome(nome),
    generatore(std::chrono::system_clock::now().time_since_epoch().count())
    {
        std::uniform_int_distribution<int> distribution(0, 5);
        
        int random = distribution(generatore) % 3 + 1;

        this->incrocio_iniziale = inizio;
        this->tempo_incrocio = (distribution(generatore) % 5 + 1) * random;
        this->tempo_di_arrivo = (int)(((distribution(generatore) % 5 + 2) * random) / 1.5);
    }

    static void print_info(const Auto *A)
    {
        std::cout<<"\n\n";
        std::cout<<"-------------------------------------------\n";
        std::cout<<"Auto numero:"<<A->nome<<"\n";
        std::cout<<"Ha inziato la simulazione dall'incrocio numero "<<A->incrocio_iniziale<<"\n";
        std::cout<<"Ha percorso tutti gli incroci con un tempo di "<<A->time_in_simulation<<"s \noppure "
                 <<A->time_in_simulation/60<<"minut\ni";
        std::cout<<"Ha dovuto entrare in una coda "<<A->entered_queues<<" volte\n\n";
    }

    virtual ~Auto(){}
};