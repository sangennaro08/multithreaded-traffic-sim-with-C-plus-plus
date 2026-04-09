#pragma once

#include "../globals.hpp"
#include "vehicle.hpp"

#include <iostream>
#include <chrono>

class Auto: public Veicolo
{
    public:

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;

    std::atomic<unsigned int> entered_queues = 0;

    float time_in_simulation;

    Auto(std::string nome, int inizio)//:
    //generatore(std::chrono::steady_clock::now().time_since_epoch().count())
    {

        generatore.seed(static_cast<unsigned>(
                        std::chrono::steady_clock::now().time_since_epoch().count()
                    ));

        this->nome = nome;
        this->incrocio_iniziale = inizio;
        this->tempo_incrocio = (distribution(generatore)) * (distribution(generatore));
        this->tempo_di_arrivo = (distribution(generatore) * 3);
    }

    static void print_info(Veicolo* A)
    {
        Auto* a = dynamic_cast<Auto*>(A);

        std::cout<< "\n\n";
        std::cout<< "-------------------------------------------\n";
        std::cout<< "Auto:"<<A->nome<<"\n";
        std::cout<< "Ha inziato la simulazione dall'incrocio numero "<< A->incrocio_iniziale <<"\n";
        std::cout<< "Ha percorso tutti gli incroci con un tempo di "<< a->time_in_simulation <<"s \noppure "
                 << a->time_in_simulation / 60 << " minuti\n";
        std::cout<<"Ha dovuto entrare in una coda "<< a->entered_queues <<" volte\n\n";
        
    }

    ~Auto() override {}

    void add_queue() override 
    {
        this->entered_queues++;
    }

    void start_time() override 
    {
        this->start = std::chrono::steady_clock::now();
    }

    void get_time() override
    {
        this->end = std::chrono::steady_clock::now();
        this->time_in_simulation = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    }

    void exit_simulation() override
    {

        if(++Auto_completate >= auto_presenti)
        {
            delete Amb;
            continua.notify_all();
        }

        {
            std::lock_guard<std::mutex> lock(scrittura_info);
            std::cout << "L'auto " << this->nome << " ha finito ed è uscito all'ultimo incrocio adesso andrà via\n\n";
        }
    }
};