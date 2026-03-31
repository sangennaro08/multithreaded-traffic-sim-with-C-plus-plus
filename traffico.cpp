#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>

using namespace std;

//TODO: fare controllo delle variabili per vedere se il programma è funzionale
// programma funzionale se auto <= tot_incroci*3

const int tot_incroci = 5;
atomic<int> Auto_completate = 0;
const int auto_presenti = 7;

//NON MODIFICARE
constexpr int tot_entrare_Incrocio = 3;

mutex scrittura_info;//scrivere cosa accade
mutex finisci_programma;

condition_variable continua;
condition_variable get_direzione;

class Incrocio;
class Auto;

/*
Simulatore di traffico
Più macchine (thread) percorrono strade con incroci condivisi. Ogni incrocio è una risorsa protetta da mutex — 
le macchine si accodano, aspettano il semaforo verde e attraversano.
Si vede chiaramente il concetto di contesa sulle risorse e deadlock se non gestito bene.

*/

/*
Aggiunta in più...le auto devono rispettare un certo ordine simile alle leggi stradali:

1)immaginiamo un incrocio del genere


            | A1 |
------------     -------------
A2                          
------------     -------------
            | A4 |


            |    |
------------     -------------
A1                            A3
------------     -------------
            | A4 |

Supponendo che gli incroci si susseguono uno dopo l'alto dal basso verso l'alto:
1)le auto di destra hanno la precedenza SEMPRE NB:abbiamo solo 3 auto MAX per ogni incrocio

2)chi esce a sinistra in base alla situazione,si troverà a sinistra del prossimo incrocio
chi si esce a destra si troverà a destra del prossimo incrocio
chi esce in basso ha la possibilità di andare a sinistra o destra del prossimo incrocio
chi sale andrà al prossimo incrocio salendo 

3)tener conto che ogni auto deve essere consapevole se il prossimo incrocio è libero,
per questo devono controllare se il prossimo incrocio è libero o meno
SE è LIBERO,proseguire,altrimenti aspettare

*/

/*
ALGORTIMO PER IL PASSAGGIO DELLE AUTO NEGLI INCROCI:

vincoli:
1)viene utilizzato SE E SOLO SE le auto negli incroci hanno l'etichetta che definiscono
la direzione d'entrata.
2)funziona solo se Cè ALMENO 1 POSTO VUOTO NELL'INCROCIO
3)DEVE comunicare con l'incrocio successivo per capire quale entrata è libera
4)DEVE essere rigido sulle leggi stradali imposte in precedenza

funzionalità:
1)le auto in uscita quando entrano al prossimo incrocio hanno un tot tempo di entrata,
visto che deve passare escono dal semaforo.
2)l'algortimo sceglie una singola auto da far uscire,la migliore nella situazione
vantaggi:
a)semafori più liberi

svantaggi:
a)creazione di code esterne agli incroci da mantenere
b)definire su quale uscita andare in base a quante auto escono da un uscita

FUNZIONAMENTO:
1)controllo degli spazi liberi del prossimo incrocio
2)controllo degli spazi liberi nello stesso incrocio 
3)in base alle regole stradali inserite far passare l'auto corretta

1a:copiare l'array delle direzioni libere del prossimo incrocio(i rimasti sono le posizioni libere)
2a:in base agli spazi liberi dell'incrocio 

*/
class Auto
{

    public:

    char nome;
    short int lunghezza;
    atomic<short int> incrocio_iniziale;
    int tempo_incrocio;
    int tempo_di_arrivo;
    string direzione = "";
    string direzione_entrata = "";

    mt19937 generatore;

    thread th;

    Auto(char nome,int inizio):nome(nome),
    generatore(chrono::system_clock::now().time_since_epoch().count())
    {
        uniform_int_distribution<int> distribution(0, 5);

        this->incrocio_iniziale = inizio;
        this->lunghezza = distribution(generatore) % 5 +3;
        this->tempo_incrocio = (distribution(generatore) % 5 +1) * lunghezza; 
        this->tempo_di_arrivo = (int)((distribution(generatore) % 5 +1) * lunghezza)/3; 
    }

    ~Auto(){}
};

class Incrocio
{

    public:

    short int auto_incrocio;
    bool decide = false;//variabile per decidere se applicare o meno l'algortimo
    counting_semaphore<tot_entrare_Incrocio> entrare;

    map<string, bool> direzioni = {
        {"Avanti", true},
        {"Sinistra", true},
        {"Dietro", true},
        {"Destra", true}
    };

    vector <Auto*> auto_in_incrocio;

    mutex direzione;//controllo per l'accesso al vettore delle direzioni
    mutex sincronizza_auto;//mutex per algortimo,così solo un auto per incrociolo applica
    //mutex decidi_uscita;//mutex per decidere l'uscita,così solo un auto per incrocio decide
    mutex find_patter;

    Incrocio():auto_incrocio(tot_entrare_Incrocio),entrare(tot_entrare_Incrocio)    
    {}

    ~Incrocio(){}

    void inizializza_Incroci(vector<unique_ptr<Incrocio>>& Incroci)
    {
        Incroci.reserve(auto_presenti);

        for(int i = 0; i < tot_incroci; i++)
            Incroci.emplace_back(make_unique<Incrocio>());
    }
    
    void inizializza_auto(vector<Auto*>& Auto_in_gioco, vector <unique_ptr<Incrocio>>& Incroci)
    {
       Auto_in_gioco.reserve(auto_presenti);

        char nome = 'A';
        int incrocio_idx = 0;
        int auto_in_questo_incrocio = 0;

        for(int i = 0; i < auto_presenti; i++)
        {
            // incrocio_idx è l'indice corretto (0-based)
            Auto* a = new Auto(nome++, incrocio_idx);
            Auto_in_gioco.push_back(a);
            Incroci.at(incrocio_idx)->auto_in_incrocio.push_back(a);

            auto_in_questo_incrocio++;

            // Passa al prossimo incrocio dopo 3 auto
            if(auto_in_questo_incrocio == tot_entrare_Incrocio)
            {
                auto_in_questo_incrocio = 0;
                if(incrocio_idx + 1 < (int)tot_incroci)
                    incrocio_idx++;
            }
        }
        /*for(int i = 0; i < auto_presenti; i++)
            for(int j = 0 ; j < tot_entrare_Incrocio; j++)
                Auto_in_gioco.emplace_back(new Auto(i,j));*/

    }

    void lancia_auto(vector<Auto*>& Auto_in_gioco,vector<unique_ptr<Incrocio>>& Incroci)
    {
        for(int i = 0; i < Auto_in_gioco.size(); i++)
            Auto_in_gioco.at(i)->th = thread(&Incrocio::simula_passaggio,Incroci.at(Auto_in_gioco.at(i)->incrocio_iniziale).get(),
                                            Auto_in_gioco.at(i),ref(Incroci),ref(Auto_in_gioco));                                   
    }

    void Arrivo_a_incrocio(Auto* A)
    {
        {
            lock_guard<mutex> lock(scrittura_info);
            cout<<"L'auto "<<A->nome<<" sta arrivando all'incrocio numero "<<A->incrocio_iniziale
            <<"in un tempo di "<<A->tempo_di_arrivo<<" secondi\n\n"<<flush;
        }

        this_thread::sleep_for(chrono::seconds(A->tempo_di_arrivo));
    }

    void simula_passaggio(Auto* A,vector<unique_ptr<Incrocio>>& Incroci,vector<Auto*>& Auto_in_gioco)
    {

        int idx = A->incrocio_iniziale;
        //int start;

        //{
        //    lock_guard <mutex> lock(Incroci.at(A->incrocio_iniziale)->sincronizza_auto); 
        //    start = A->incrocio_iniziale;
        //}
        //Così ogni auto percorre tutti gli incroci
        for(int i = 0; i < Incroci.size(); i++)
        {
            //{
            //    lock_guard <mutex> lock(Incroci.at(start)->sincronizza_auto);
            //idx = (start + i) % (int)tot_incroci;
            //}
            //
            //Fase di arrivo all incrocio
            //
            Arrivo_a_incrocio(A);

            //Fase di entrata all'incrocio
            //entrata,algortimo 
            if(Incroci.at(idx)->entrare.try_acquire())
            {
                {
                    //sincronizza_auto è un mutex dell'incrocio,così soo 1 auto può fare l'algortmo
                    //e fa decidere chi va avanti o meno
                    lock_guard<mutex> lock1(Incroci.at(idx)->sincronizza_auto);
                
                    if(A->direzione == "" && A->direzione_entrata == "")
                    {
                        {
                            //ottengo il mutex dell'incrocio in cui sono dentro
                            lock_guard<mutex> lock(Incroci.at(idx)->direzione);

                            //A->direzione = Incroci.at(idx)->direzioni.back();
                            //Incroci.at(idx)->direzioni.pop_back();
                            for(auto& [dir, dispo] : Incroci.at(idx)->direzioni)
                            {
                                if(dispo)
                                {
                                    dispo = false;
                                    A->direzione = dir;
                                    break;
                                }
                            }
                                
                            // Rimuovi dal vettore dell'incrocio corrente
                            //auto& vec_corrente = Incroci.at(idx)->auto_in_incrocio;
                            //auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
                            //if(it != vec_corrente.end())
                            //    vec_corrente.erase(it);
                            
                            // Aggiungi al vettore dell'incrocio successivo
                            //int next = (idx + 1) % tot_incroci;
                            //Incroci.at(next)->auto_in_incrocio.push_back(A);
                            
                            // Aggiorna l'incrocio corrente dell'auto
                            //A->incrocio_iniziale = next;
                            //auto& vettore = Incroci.at(idx)->auto_in_incrocio;
                            //int it = find(vettore.begin(),vettore.end(),A) - vettore.begin();
                            //vettore.at(it)->direzione = A->direzione;

                            //auto it = find(Auto_in_gioco.begin(), Auto_in_gioco.end(), A);

                            //if(it != Auto_in_gioco.end())
                            //    (*it)->direzione = A->direzione;
                        } 

                    }else{

                        {
                            //ottengo il mutex dell'incrocio in cui sono dentro
                            lock_guard<mutex> lock(Incroci.at(idx)->direzione);

                            A->direzione = A->direzione_entrata;
                            A->direzione_entrata = "";

                                
                            // Rimuovi dal vettore dell'incrocio corrente
                            //auto& vec_corrente = Incroci.at(idx)->auto_in_incrocio;
                            //auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
                            //if(it != vec_corrente.end())
                            //    vec_corrente.erase(it);
                            
                            // Aggiungi al vettore dell'incrocio successivo
                            //int next = (idx + 1) % tot_incroci;
                            //Incroci.at(next)->auto_in_incrocio.push_back(A);
                            
                            // Aggiorna l'incrocio corrente dell'auto
                            //A->incrocio_iniziale = next;
                            
                            //auto& vettore = Incroci.at(idx)->auto_in_incrocio;
                            //int it = find(vettore.begin(),vettore.end(),A) - vettore.begin();
                            //vettore.at(it)->direzione = A->direzione;

                            //auto it = find(Auto_in_gioco.begin(), Auto_in_gioco.end(), A);
                            
                            //if(it != Auto_in_gioco.end())
                            //(*it)->direzione = A->direzione;                            
                        }
                    }

                    {
                        unique_lock<mutex> lock(Incroci.at(idx)->find_patter);

                        if(!Incroci.at(idx)->decide)
                        {
                            Incroci.at(idx)->decide = true;
                            lock.unlock();

                            //Algoritmo per trovare l'uscita migliore nella situazione attuale
                            //search_exit(A,Incroci);

                            //get_direzione.notify_all();

                        }else{
                            //get_direzione.wait(lock,[&]{return !Incroci.at(idx)->decide;});
                        }  
                    }

                    Incroci.at(idx)->entrare.release(); 
                }

                int next = (idx + 1) % tot_incroci;

                {
                    lock_guard <mutex> lock(Incroci.at(idx)->direzione);

                    auto& vec_corrente = Incroci.at(idx)->auto_in_incrocio;
                    auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
                    if(it != vec_corrente.end())
                        vec_corrente.erase(it);
                }
                            
                {
                    lock_guard <mutex> lock(Incroci.at(next)->direzione);
                    // Aggiungi al vettore dell'incrocio successivo
                    Incroci.at(next)->auto_in_incrocio.push_back(A);
                }
                            
                // Aggiorna l'incrocio corrente dell'auto
                A->incrocio_iniziale = next;
                idx = next;
            }

            //if(++idx == 5)
            //    A->incrocio_iniziale = 0;
        }

        if(++Auto_completate >= auto_presenti)
            continua.notify_all();
    }

    //Passo il vector di incroci così che possa comunicare con l'incrocio successivo
    //e se stesso così da poter fare scelte adatte al suo incrocio in base alla sua situazione
    void search_exit(Auto* A,vector <unique_ptr<Incrocio>>& Incroci)
    {
        //COSA FA?
        //1)trakking del prossimo incrocio per capire quale entrata è libera
        //2)decidere quale auto passare al prossimo incrocio
        //3)in base alla situazione portare l'alto ad un uscita per evitare ingorghi
        int next_incrocio = (A->incrocio_iniziale + 1 == tot_incroci) ? 0 : A->incrocio_iniziale + 1;
        vector <string> direzioni_libere;

        //{
        //    lock_guard<mutex> lock(Incroci.at(next_incrocio)->direzione);
        //    direzioni_libere = Incroci.at(next_incrocio)->direzioni;
        //}
        

        for(int i = 0; i < auto_in_incrocio.size(); i++)
        {
            
        }

    }

    void join_threads(vector<Auto*>& Auto_in_gioco)
    {
        for(auto& Auto : Auto_in_gioco)
            if(Auto->th.joinable()) Auto->th.join();
    }
};

int main()
{

    vector <unique_ptr<Incrocio>> Incroci;
    vector <Auto*> Auto_in_gioco;

    Incrocio I;

    I.inizializza_Incroci(Incroci);
    I.inizializza_auto(Auto_in_gioco,Incroci);
    I.lancia_auto(Auto_in_gioco,Incroci);

    {
        unique_lock<mutex> lock(finisci_programma);

        continua.wait(lock,[&]{return Auto_completate >= auto_presenti;});
    }

    I.join_threads(Auto_in_gioco);

    for(auto& auti : Auto_in_gioco)
        delete auti;

    return 0;
}
