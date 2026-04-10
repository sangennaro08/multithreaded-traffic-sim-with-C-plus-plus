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

void Incrocio::inizializza_auto(std::vector<Veicolo*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci)
{
    Auto_in_gioco.reserve(auto_presenti);

    for(int i = 0; i < auto_presenti; i++)
    {
        std::string nome = 'A' + std::to_string(i);
        int pos = decide_interception(Incroci);
        Veicolo* a = new Auto(nome, pos);

        Auto_in_gioco.push_back(a);

        Incroci.at(pos)->auto_in_incrocio.push_back(a);
        //Incroci.at(pos)->tot_auto++;
    }
}

void Incrocio::lancia_auto(std::vector<Veicolo*>& Auto_in_gioco, std::vector<std::unique_ptr<Incrocio>>& Incroci, std::vector<Veicolo*>& Ambulanze)
{
    for(size_t i = 0; i < Auto_in_gioco.size(); i++)
        Auto_in_gioco.at(i)->th = std::thread(&Incrocio::simula_passaggio,
            Incroci.at(Auto_in_gioco.at(i)->incrocio_iniziale).get(),
            Auto_in_gioco.at(i), std::ref(Incroci), std::ref(Ambulanze));
}

void Incrocio::create_ambulances(std::vector<Veicolo*>& Ambulanze)
{

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> create{1, (((int)auto_presenti / 8) + 1)};

    int size = create(gen);

    Ambulanze.reserve(size);

    for(size_t i = 0; i < size; i++)
        Ambulanze.emplace_back(new Ambulanza(i));
}

void Incrocio::launch_ambulance(std::vector<std::unique_ptr<Incrocio>>& Incroci, std::vector<Veicolo*>& Ambulanze)
{   

    if(Ambulance_launched == Ambulanze.size())
        return;

    std::scoped_lock lock(access_Ambulance, Incroci.at(0)->direzione);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution <float> floats{0.8f, 1.0f};

    if(Ambulanza* a = dynamic_cast<Ambulanza*>(Ambulanze.at(Ambulance_launched)))
        if(a->percentage >= floats(gen) && Incroci.at(Ambulance_launched)->direzioni.at("Dietro"))
        {

            Incroci.at(0)->auto_in_incrocio.push_back(Ambulanze.at(Ambulance_launched));
            Ambulance_here++;
            Ambulance_launched++;

            a->th = std::thread(&Incrocio::simula_passaggio, Incroci.at(0).get(), a, std::ref(Incroci), std::ref(Ambulanze));

            //Ambulanze.erase(Ambulanze.begin());
        }
}

void Incrocio::join_threads(std::vector<Veicolo*>& Auto_in_gioco, std::vector<Veicolo*>& Ambulances)
{
    for(auto& Auto : Auto_in_gioco)
        if(Auto->th.joinable()) Auto->th.join();

    for(auto& Amb : Ambulances)
        if(Amb->th.joinable()) Amb->th.join();    
}

int Incrocio::decide_interception(std::vector<std::unique_ptr<Incrocio>>& Incroci)
{
    for( ; ; )
    {
        static std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
        static std::uniform_int_distribution<int> dist(0, tot_incroci - 1);
        int pos = dist(rng);

        if(Incroci.at(pos)->auto_in_incrocio.size() < 3)
            return pos;
    }
}

void Incrocio::Arrivo_a_incrocio(const Veicolo* A, int idx)
{
    {
        std::lock_guard<std::mutex> lock(scrittura_info);
        std::cout << "L'auto " << A->nome << " sta arrivando all'incrocio numero "
                  << idx << " in un tempo di "
                  << A->tempo_di_arrivo << " secondi\n\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(A->tempo_di_arrivo));
}

void Incrocio::set_directions(std::unique_ptr<Incrocio>& Actual_Incrocio, Veicolo* A)
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

void Incrocio::get_out_intersection(std::unique_ptr<Incrocio>& Actual_Incrocio, const Veicolo* A)
{
    std::lock_guard<std::mutex> lock(Actual_Incrocio->direzione);

    auto& vec_corrente = Actual_Incrocio->auto_in_incrocio;
    auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
    if(it != vec_corrente.end())
        vec_corrente.erase(it);

    //Actual_Incrocio->tot_auto--;
    Actual_Incrocio->direzioni.at(A->direzione) = true;

    Actual_Incrocio->esci.notify_all();

    return;
}

void Incrocio::waiting_in_queues(std::unique_ptr<Incrocio>& Next_Incrocio,Veicolo* A, const int next)
{
    std::unique_lock<std::mutex> lock(Next_Incrocio->direzione);

    if(!(Next_Incrocio->direzioni.at(A->direzione_entrata)))
    {
        Next_Incrocio->code_entrata.at(A->direzione_entrata).push(A);

        A->add_queue();

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

void Incrocio::simula_passaggio(Veicolo* A, std::vector<std::unique_ptr<Incrocio>>& Incroci, std::vector<Veicolo*>& Ambulanze)
{

    int idx = A->incrocio_iniziale;
    int finish_last_interception = idx % tot_incroci;
    const int last_intercetion = tot_incroci - 1;

    A->start_time();

    Arrivo_a_incrocio(A, idx);

    for(size_t i = 0; i < Incroci.size() || finish_last_interception < last_intercetion; i++)
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
                          << idx << " adesso si deciderà in quale direzione del prossimo incrocio andrà\n\n";
            }

            int next = (idx + 1) % tot_incroci;

            TrafficAlgorithm::start_algorithm(Incroci.at(idx), Incroci.at(next), A);

            {
                std::lock_guard<std::mutex> lock(scrittura_info);

                std::cout << "L'auto " << A->nome << " sta percorrendo l'incrocio "
                          << idx << " e ci mette " << A->tempo_incrocio << " secondi\n\n";
            }

            Incroci.at(idx)->entrare.release();

            {
                std::lock_guard<std::mutex> lock(Incroci.at(idx)->direzione);

                Incroci.at(idx)->posti_liberi++;
            }

            std::this_thread::sleep_for(std::chrono::seconds(A->tempo_incrocio));
 
            get_out_intersection(Incroci.at(idx), A);
            Arrivo_a_incrocio(A, next);

            if(next != last_intercetion || i < Incroci.size()) 
                waiting_in_queues(Incroci.at(next), A, next); 

            A->incrocio_iniziale = next;
            idx = next;
            finish_last_interception = idx % tot_incroci;

            //se l'ambulanza è troppo in simulazione fa un rush all' ospedale
            if(A->run())
            {
                TrafficAlgorithm::rush_to_hospital(idx, next, last_intercetion, Incroci, A);
                break;
            }
                
        }
    }

    get_out_intersection(Incroci.at(tot_incroci - 1), A);

    A->reset_ambulance();

    if(Ambulance_launched != Ambulanze.size())
        Ambulanze.at(Ambulance_launched)->add_percentage();

    launch_ambulance(Incroci,Ambulanze);

    A->exit_simulation();
    A->get_time();
}