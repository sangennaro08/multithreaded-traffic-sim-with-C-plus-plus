#include <iostream>
#include <vector>
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

size_t tot_incroci = 5;
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
vantaggi:
a)semafori più liberi

svantaggi:
a)creazione di code esterne agli incroci da mantenere
b)definire su quale uscita andare in base a quante auto escono da un uscita


*/
class Auto
{

    public:

    char nome;
    short int lunghezza;
    short int incrocio_iniziale;
    int tempo_incrocio;
    int tempo_di_arrivo;
    string direzione = "";

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
    counting_semaphore<tot_entrare_Incrocio> entrare;

    vector <string> direzioni = {"Avanti","Sotto","Destra","Sinistra"};

    mutex direzione;//controllo per l'accesso al vettore delle direzioni
    mutex sincronizza_auto;//mutex per algortimo,così solo un auto per incrociolo applica

    Incrocio():auto_incrocio(tot_entrare_Incrocio),entrare(tot_entrare_Incrocio)    
    {}

    ~Incrocio(){}

    void inizializza_Incroci(vector<unique_ptr<Incrocio>>& Incroci)
    {
        Incroci.reserve(auto_presenti);

        for(int i = 0; i < tot_incroci; i++)
            Incroci.emplace_back(make_unique<Incrocio>());
    }
    
    void inizializza_auto(vector<Auto*>& Auto_in_gioco)
    {
        Auto_in_gioco.reserve(auto_presenti);

        char nome = 'A';

        for(int i = 0; i < tot_incroci; i++)
            for(int j = 0;j < tot_entrare_Incrocio; j++)
                Auto_in_gioco.emplace_back(new Auto(nome++,i));

        /*for(int i = 0; i < auto_presenti; i++)
            for(int j = 0 ; j < tot_entrare_Incrocio; j++)
                Auto_in_gioco.emplace_back(new Auto(i,j));*/

    }

    void lancia_auto(vector<Auto*>& Auto_in_gioco,vector<unique_ptr<Incrocio>>& Incroci)
    {
        for(int i = 0; i < auto_presenti; i++)
            Auto_in_gioco.at(i)->th = thread(&Incrocio::simula_passaggio,Incroci.at(Auto_in_gioco.at(i)->incrocio_iniziale).get(),
                                            Auto_in_gioco.at(i),ref(Incroci));                                   
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

    void simula_passaggio(Auto* A,vector<unique_ptr<Incrocio>>& Incroci)
    {

        for(int i = 0; i < Incroci.size(); i++)
        {
            Arrivo_a_incrocio(A);

            //sincronizza_auto è un mutex dell'incrocio,così soo 1 auto può fare l'algortmo
            //e fa decidere chi va avanti o meno

            if(Incroci.at(i)->entrare.try_acquire())
            {
                {
                    lock_guard<mutex> lock(sincronizza_auto);
                
                    if(A->direzione == "")
                    {
                        {
                            //ottengo il mutex dell'incrocio in cui sono dentro
                            lock_guard<mutex> lock(Incroci.at(A->incrocio_iniziale)->direzione);

                            A->direzione = Incroci.at(A->incrocio_iniziale)->direzioni.back();
                            Incroci.at(A->incrocio_iniziale)->direzioni.pop_back();
                        }  
                    }
                }

                //Algortimo per trovare l'uscita migliore nella situazione attuale
                search_exit(A,Incroci);
                
                Incroci.at(i)->entrare.release();
            }
        }

        if(++A->incrocio_iniziale == 5)
            A->incrocio_iniziale = 0;

        if(++Auto_completate >= auto_presenti)
            continua.notify_one();
    }

    //Passo il vector di incroci così che possa comunicare con l'incrocio successivo
    //e se stesso così da poter fare scelte adatte al suo incrocio in base alla sua situazione
    void search_exit(Auto* A,vector <unique_ptr<Incrocio>>& Incroci)
    {
        
    }

    void pop_element(Incrocio& I,Auto* A)
    {
        I.direzioni.erase(remove(I.direzioni.begin(),I.direzioni.end(),A->direzione),
                    I.direzioni.end());
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
    I.inizializza_auto(Auto_in_gioco);
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