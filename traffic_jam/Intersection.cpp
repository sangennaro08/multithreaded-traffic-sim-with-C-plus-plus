#include "Intersection.hpp"
#include "Traffic_algorithm.hpp"

Incrocio::Incrocio() : entrare(tot_entrare_Incrocio),
    direzioni(
        {{"Destra", true},
        {"Sinistra",true},
        {"Dietro",  true},
        {"Avanti",  true}}),

    code_entrata(
        {{"Destra"  ,{}}
        ,{"Sinistra",{}},
        {"Dietro"   ,{}}})
{}

void Incrocio::inizializza_Incroci(std::vector<std::unique_ptr<Incrocio>>& Incroci)
{
    Incroci.reserve(tot_incroci);

    for(int i = 0; i < tot_incroci; i++)
        Incroci.emplace_back(std::make_unique<Incrocio>());
}

void Incrocio::inizializza_auto(std::vector<Auto*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci)
{
    Auto_in_gioco.reserve(auto_presenti);

    for(int i = 0; i < auto_presenti; i++)
    {
        int pos = decide_interception(Incroci);
        Auto* a = new Auto(i, pos);

        Auto_in_gioco.push_back(a);

        Incroci.at(pos)->auto_in_incrocio.push_back(a);
        Incroci.at(pos)->tot_auto++;
    }
}

void Incrocio::lancia_auto(std::vector<Auto*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci)
{
    for(int i = 0; i < (int)Auto_in_gioco.size(); i++)
        Auto_in_gioco.at(i)->th = std::thread(&Incrocio::simula_passaggio,
            Incroci.at(Auto_in_gioco.at(i)->incrocio_iniziale).get(),
            Auto_in_gioco.at(i), std::ref(Incroci));
}

void Incrocio::join_threads(std::vector<Auto*>& Auto_in_gioco)
{
    for(auto& Auto : Auto_in_gioco)
        if(Auto->th.joinable()) Auto->th.join();
}

int Incrocio::decide_interception(std::vector<std::unique_ptr<Incrocio>>& Incroci)
{
    for(;;)
    {
        static std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
        static std::uniform_int_distribution<int> dist(0, tot_incroci - 1);
        int pos = dist(rng);

        if(Incroci.at(pos)->auto_in_incrocio.size() < 3)
            return pos;
    }
}

void Incrocio::Arrivo_a_incrocio(const Auto* A, int idx)
{
    {
        std::lock_guard<std::mutex> lock(scrittura_info);
        std::cout << "L'auto " << A->nome << " sta arrivando all'incrocio numero "
                  << idx << " in un tempo di "
                  << A->tempo_di_arrivo << " secondi\n\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(A->tempo_di_arrivo));
}

void Incrocio::set_directions(std::unique_ptr<Incrocio>& Actual_Incrocio, Auto* A)
{
    std::lock_guard<std::mutex> lock(Actual_Incrocio->direzione);

    if(A->direzione_entrata.empty())
    {
        for(auto& [dir, dispo] : Actual_Incrocio->direzioni)
        {
            if(dispo)
            {
                A->direzione = dir;
                dispo = false;
                
                break;
            }
        }
    }
    else
    {
        A->direzione = A->direzione_entrata;
        A->direzione_entrata = "";
    }

    return;
}

void Incrocio::get_out_intersection(std::unique_ptr<Incrocio>& Actual_Incrocio, const Auto* A)
{
    std::lock_guard<std::mutex> lock(Actual_Incrocio->direzione);

    auto& vec_corrente = Actual_Incrocio->auto_in_incrocio;
    auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
    if(it != vec_corrente.end())
        vec_corrente.erase(it);

    Actual_Incrocio->tot_auto--;
    Actual_Incrocio->direzioni.at(A->direzione) = true;

    Actual_Incrocio->esci.notify_all();

    return;
}

void Incrocio::waiting_in_queues(std::unique_ptr<Incrocio>& Next_Incrocio,Auto* A, const int next)
{
    std::unique_lock<std::mutex> lock(Next_Incrocio->direzione);

    if(!(Next_Incrocio->direzioni.at(A->direzione_entrata)))
    {
        Next_Incrocio->code_entrata.at(A->direzione_entrata).push(A);
        A->entered_queues++;

        {
            std::lock_guard<std::mutex> lock1(scrittura_info);
            std::cout << "L'auto " << A->nome << " aspetterà nella coda "
                      << A->direzione_entrata << " dell'incrocio " << next << "\n\n";
        }

        Next_Incrocio->esci.wait(lock, [&]{
            return Next_Incrocio->code_entrata.at(A->direzione_entrata).front() == A &&
                   Next_Incrocio->direzioni.at(A->direzione_entrata);
        });

        Next_Incrocio->code_entrata.at(A->direzione_entrata).pop();

        {
            std::lock_guard<std::mutex> lock1(scrittura_info);
            std::cout << "L'auto " << A->nome << " è uscito dalla coda "
                      << A->direzione_entrata << " dell'incrocio " << next << "\n\n";
        }
    }

    Next_Incrocio->auto_in_incrocio.push_back(A);
    Next_Incrocio->direzioni.at(A->direzione_entrata) = false;

    return;
}

void Incrocio::simula_passaggio(Auto* A, std::vector<std::unique_ptr<Incrocio>>& Incroci)
{
    int idx = A->incrocio_iniziale;
    int finish_last_interception = idx % tot_incroci;
    const int last_intercetion = tot_incroci - 1;

    auto start = std::chrono::high_resolution_clock::now();

    Arrivo_a_incrocio(A, idx);

    for(size_t i = 0; i < Incroci.size() || finish_last_interception != last_intercetion; i++)
    {
        if(Incroci.at(idx)->entrare.try_acquire())
        {
            {
                std::lock_guard<std::mutex> lock(Incroci.at(idx)->direzione);
                Incroci.at(idx)->posti_liberi--;
            }

            set_directions(Incroci.at(idx), A);

            {
                std::lock_guard<std::mutex> lock(scrittura_info);

                std::cout << "L'auto " << A->nome << " è arrivato all'incrocio "
                          << /*A->incrocio_iniziale*/idx << " adesso si deciderà in quale direzione del prossimo incrocio andrà\n\n";
            }

            TrafficAlgorithm::start_algorithm(Incroci.at(idx), Incroci.at((idx + 1) % tot_incroci), A);

            {
                std::lock_guard<std::mutex> lock(scrittura_info);

                std::cout << "L'auto " << A->nome << " sta percorrendo l'incrocio "
                          << /*A->incrocio_iniziale*/idx << " e ci mette " << A->tempo_incrocio << " secondi\n\n";
            }

            Incroci.at(idx)->entrare.release();

            {
                std::lock_guard<std::mutex> lock(Incroci.at(idx)->direzione);

                Incroci.at(idx)->posti_liberi++;
            }

            int next = (idx + 1) % tot_incroci;

            std::this_thread::sleep_for(std::chrono::seconds(A->tempo_incrocio));
 
            get_out_intersection(Incroci.at(idx), A);
            Arrivo_a_incrocio(A, next);

            if(next != last_intercetion || i < Incroci.size()) 
                waiting_in_queues(Incroci.at(next), A, next);

            A->incrocio_iniziale = next;
            idx = next;
            finish_last_interception = idx;
        }
    }

    get_out_intersection(Incroci.at(tot_incroci - 1), A);

    if(++Auto_completate >= auto_presenti)
        continua.notify_all();

    {
        std::lock_guard<std::mutex> lock(scrittura_info);
        std::cout << "L'auto " << A->nome << " ha finito ed è uscito all'ultimo incrocio adesso andrà via\n\n";
    }

    auto end = std::chrono::high_resolution_clock::now();

    A->time_in_simulation = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

}