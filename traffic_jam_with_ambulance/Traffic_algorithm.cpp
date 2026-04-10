#include "Traffic_algorithm.hpp"
#include "Intersection.hpp"

void TrafficAlgorithm::start_algorithm(std::unique_ptr<Incrocio>& Actual_Incrocio, std::unique_ptr<Incrocio>& Next_Incrocio, Veicolo* A)
{
    int tentativi = 0;
    A->choosed = false;

    do
    {
        {
            std::unique_lock<std::mutex> lock(Actual_Incrocio->find_patter);

            if(Actual_Incrocio->decide && !A->choosed)
            {
                Actual_Incrocio->decide = false;
                search_exit(Actual_Incrocio, Next_Incrocio, A);
                Actual_Incrocio->decide = true;

                lock.unlock();
                Actual_Incrocio->get_direzione.notify_all();
            }
            else
            {
                Actual_Incrocio->get_direzione.wait(lock, [&]{return A->choosed;});
            }
        }

        tentativi++;
        go_alone(Next_Incrocio, A, tentativi);

    }while(!A->choosed);
}

void TrafficAlgorithm::go_alone(std::unique_ptr<Incrocio>& Next_Incrocio, Veicolo* A, int tentativi)
{
    if(tentativi >= 3 && !A->choosed)
    {
        std::lock_guard<std::mutex> lock(Next_Incrocio->direzione);

        std::uniform_int_distribution<int> dist(0, 3);
        
        auto it = Next_Incrocio->direzioni.begin();

        do
        {
            it = Next_Incrocio->direzioni.begin();
            std::advance(it, dist(A->generatore));

        }while(it->first == "Avanti");

        A->direzione_entrata = it->first;
        A->choosed = true;
    }
}

void TrafficAlgorithm::search_exit(std::unique_ptr<Incrocio>& Actual_Incrocio, std::unique_ptr<Incrocio>& Next_Incrocio, Veicolo* A)
{
    std::lock_guard<std::mutex> lock(Actual_Incrocio->direzione);
    std::lock_guard<std::mutex> lock_next(Next_Incrocio->direzione);

    int da_muovere = std::min(Next_Incrocio->posti_liberi, (int)Actual_Incrocio->auto_in_incrocio.size());

    for(int i = 0; i < da_muovere; i++)
    {
        for(auto& [dir, pos] : Next_Incrocio->direzioni)
        {
            if(pos && Actual_Incrocio->auto_in_incrocio.at(i)->direzione_entrata != dir && dir != "Avanti")
            {
                Actual_Incrocio->auto_in_incrocio.at(i)->direzione_entrata = dir;
                Actual_Incrocio->auto_in_incrocio.at(i)->choosed = true;
                
                break;
            }
        }
    }
}

void TrafficAlgorithm::rush_to_hospital(int& idx, int& next, int last_intercetion,std::vector<std::unique_ptr<Incrocio>>& I, Veicolo* A)
{
    {

        std::lock_guard <std::mutex> lock(I.at(idx)->direzione);

        std::cout<<"L'ambulanza è entrato in rush...è passato troppo tempo deve correre all'ospedale!!!\n\n";
    }

    while(idx != last_intercetion)
    {
        
        next = (idx + 1) % tot_incroci;

        Incrocio::set_directions(I.at(idx), A);

        std::this_thread::sleep_for(std::chrono::seconds(A->tempo_incrocio / 2));

        Incrocio::get_out_intersection(I.at(idx), A);
 
        std::this_thread::sleep_for(std::chrono::seconds(A->tempo_di_arrivo / 2));
        
        // entra direttamente senza waiting_in_queues
        {
            std::lock_guard<std::mutex> lock(I.at(next)->direzione);

            I.at(next)->auto_in_incrocio.push_back(A);
        }

        A->incrocio_iniziale = next;
        idx = next;

    }

    {
        std::lock_guard <std::mutex> lock(scrittura_info);

        std::cout<<"L'ambulanza è arrivata all'ospedale!\n\n";
    }
}