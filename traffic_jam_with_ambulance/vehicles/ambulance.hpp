#pragma once

#include "../globals.hpp"
#include "vehicle.hpp"

class Ambulanza: public Veicolo
{

    public:

    std::atomic<float> percentage = 0.0f;

    unsigned int pazienti;

    bool sirena = false;

    Ambulanza(size_t numbers)
    {

        generatore.seed(static_cast<unsigned>(
                        std::chrono::steady_clock::now().time_since_epoch().count()
                    ));

        this->nome = "Ambulanza" + std::to_string(numbers);

        this->tempo_di_arrivo = (distribution(generatore)) + 2;
        this->tempo_incrocio = (distribution(generatore)) + 2;

        this->pazienti = distribution(generatore);
    }

    ~Ambulanza() override {}

    bool run() override
    {
        this->end = std::chrono::steady_clock::now();
        this->time_in_simulation = std::chrono::duration_cast<std::chrono::seconds>(this->end - this->start).count();

        int soglia = 120 / this->pazienti;

        if(this->time_in_simulation > soglia)
            return true;

        return false;
    }

    void start_time() override 
    {
        this->start = std::chrono::steady_clock::now();
    }

    void add_percentage() override
    {
        std::lock_guard <std::mutex> lock(access_Ambulance);

        this->percentage += 0.1f;
    }

    void reset_ambulance()
    {
        std::lock_guard <std::mutex> lock(access_Ambulance);

        this->percentage = 0.0f;
    }

    void exit_simulation() override
    {

        Ambulance_here--;

        {    
            std::lock_guard<std::mutex> lock(scrittura_info);

            std::cout << "L'auto " << this->nome << " ha finito ed è uscito all'ultimo incrocio adesso andrà via\n\n";
        }

        if(Auto_completate >= auto_presenti && Ambulance_here == 0)
            continua.notify_all(); 


    }
};