#pragma once

#include "../globals.hpp"
#include "vehicle.hpp"

class Ambulanza: public Veicolo
{

    public:

    float percentage = 0.0f;
    float percentage_sirene = 0.0f;

    unsigned int pazienti;

    bool sirena = false;

    Ambulanza()
    {

        generatore.seed(static_cast<unsigned>(
                        std::chrono::steady_clock::now().time_since_epoch().count()
                    ));

        this->nome = "Ambulanza";
        this->direzione = "Dietro";

        this->pazienti = distribution(generatore);
    }

    ~Ambulanza() override {}

    void add_percentage() override
    {
        this->percentage += 0.1f;
    }

    void add_percentage_sirene() override 
    {
        this->percentage_sirene += 0.1f;
    }

    void reset_ambulance()
    {
        this->percentage = 0.0f;
        this->percentage_sirene = 0.0f;
    }
};