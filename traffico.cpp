#include <iostream>

//librerie strutture dati
#include <vector>
#include <queue>
#include <unordered_map>

//librerie sincronizzazione concorrenza
#include <thread>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>

//librerie in più come waiting,deallocazione automatica della memoria e 
//random number generator
#include <chrono>
#include <memory>
#include <random>
#include <limits>

//TODO vedere perchè è presente una assenza di auto andando avanti nel programma
//causa:code in conflitto con il settaggio della posizione libera

using namespace std;

bool controll_variables();
void set_variables();
int valid(int number);

int tot_incroci = 5;
atomic<int> Auto_completate = 0;
int auto_presenti = 15;

//NON MODIFICARE
constexpr int tot_entrare_Incrocio = 3;

mutex scrittura_info;//scrivere cosa accade
mutex finisci_programma;

condition_variable continua;

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
1)le auto di destra hanno la precedenza  NB:abbiamo solo 3 auto MAX per ogni incrocio
e può essere che in base alla priorità di altre auto esso può anche passare come secondo 
in base alla situazione

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

    int nome;

    atomic<int> incrocio_iniziale;
    int tempo_incrocio;
    int tempo_di_arrivo;

    string direzione = "";
    string direzione_entrata = "";

    bool choosed = false;

    mt19937 generatore;

    thread th;

    Auto(int nome,int inizio):nome(nome),
    generatore(chrono::system_clock::now().time_since_epoch().count())
    {
        uniform_int_distribution<int> distribution(0, 5);
        int random = distribution(generatore) % 3 + 1;

        this->incrocio_iniziale = inizio;
        this->tempo_incrocio = (distribution(generatore) % 5 + 1) * random; 
        this->tempo_di_arrivo = (int) (((distribution(generatore) % 5 + 2) * random)/ 1.5); 
    }

    ~Auto(){}
};

class Incrocio
{

    public:

    bool decide = true;//variabile per decidere se applicare o meno l'algortimo
    int posti_liberi = 3;

    counting_semaphore <tot_entrare_Incrocio> entrare;

    unordered_map <string, bool> direzioni = {
        {"Destra"  , true},
        {"Sinistra", true},
        {"Dietro"  , true},
        {"Avanti"  , true}
    };

    //code prima di entrare nell'effettivo incrocio...
    //perchè servono?
    //1)se lo spazio è gia occupato in quel posto allora l'auto deve aspettare in una coda
    //COME UN TRAFFICO!!!
    //il primo del traffico andrà avanti al prossimo incrocio,per questo loro devono aspettare 
    unordered_map <string, queue <Auto*>> code_entrata{
        {"Destra"   , {}},
        {"Sinistra" , {}},
        {"Dietro"   , {}}

    };

    vector <Auto*> auto_in_incrocio;
    atomic <int>   tot_auto = 0;
    //queue  <Auto*> coda_entrata;
    
    //shared_mutex direzione;//controllo per l'accesso al vettore delle direzioni
    mutex direzione;
    //mutex sincronizza_auto;//mutex per algortimo,così solo un auto per incrociolo applica
    //mutex decidi_uscita;//mutex per decidere l'uscita,così solo un auto per incrocio decide
    mutex find_patter;
    //shared_mutex read_queues;

    condition_variable get_direzione;
    condition_variable esci;

    Incrocio():entrare(tot_entrare_Incrocio)    
    {}

    ~Incrocio(){}

    static void inizializza_Incroci(vector<unique_ptr<Incrocio>>& Incroci)
    {
        Incroci.reserve(tot_incroci);

        for(int i = 0; i < tot_incroci; i++)
            Incroci.emplace_back(make_unique<Incrocio>());
    }
    
    static void inizializza_auto(vector<Auto*>& Auto_in_gioco, vector <unique_ptr<Incrocio>>& Incroci)
    {
       Auto_in_gioco.reserve(auto_presenti);

        for(int i = 0;i <auto_presenti; i++)
        {
            int pos = decide_interception(Incroci);

            Auto* a = new Auto(i, pos);

            Auto_in_gioco.push_back(a);
            Incroci.at(pos)->auto_in_incrocio.push_back(a);
            Incroci.at(pos)->tot_auto++;
        }
    }

    static void lancia_auto(vector<Auto*>& Auto_in_gioco,vector<unique_ptr<Incrocio>>& Incroci)
    {
        for(int i = 0; i < (int)Auto_in_gioco.size(); i++)
            Auto_in_gioco.at(i)->th = thread(&Incrocio::simula_passaggio,Incroci.at(Auto_in_gioco.at(i)->incrocio_iniziale).get(),
                                            Auto_in_gioco.at(i),ref(Incroci));                                   
    }

    static void join_threads(vector<Auto*>& Auto_in_gioco)
    {
        for(auto& Auto : Auto_in_gioco)
            if(Auto->th.joinable()) Auto->th.join();
    }

    private:

    static int decide_interception(vector <unique_ptr<Incrocio>>& Incroci)
    {
       for( ; ; )
       {
            int pos = (int) rand()% tot_incroci;
            if(Incroci.at(pos)->auto_in_incrocio.size() < 3)
                return pos;
       }
    }

    void Arrivo_a_incrocio(Auto* A)
    {
        {
            lock_guard <mutex> lock(scrittura_info);
            cout<<"L'auto "<<A->nome<<" sta arrivando all'incrocio numero "<<A->incrocio_iniziale
            <<"in un tempo di "<<A->tempo_di_arrivo<<" secondi\n\n"<<flush;
        }

        this_thread::sleep_for(chrono::seconds(A->tempo_di_arrivo));
    }

    void simula_passaggio(Auto* A,vector<unique_ptr<Incrocio>>& Incroci)
    {

        int idx = A->incrocio_iniziale;
        int finish_last_interception = idx % tot_incroci;
        const int last_intercetion = tot_incroci - 1;
        //
        //Fase di arrivo all incrocio
        //
        Arrivo_a_incrocio(A);

        //Così ogni auto percorre tutti gli incroci
        //la condizione idx % Incroci.size() != 4 può anche essere tolta...
        //è solo per realismo ed evitare che un auto finisca il programma in mezzo ad un incrocio
        //e non a quello finale(sembra che scompaia e non va via fisicamente)
        for(int i = 0; i < Incroci.size() || (finish_last_interception != last_intercetion); i++)
        {

            //Fase di entrata all'incrocio
            //entrata,algoritmo 
            if(Incroci.at(idx)->entrare.try_acquire())
            {
                {
                    lock_guard <mutex> lock(Incroci.at(idx)->direzione);

                    Incroci.at(idx)->posti_liberi--;
                }
                
                /*if(A->direzione == "" && A->direzione_entrata == "")
                {
                    {
                        //ottengo il mutex dell'incrocio in cui sono dentro
                        unique_lock<mutex> lock(Incroci.at(idx)->direzione);

                        //A->direzione = Incroci.at(idx)->direzioni.back();
                        //Incroci.at(idx)->direzioni.pop_back();

                        for(auto& [dir, dispo] : Incroci.at(idx)->direzioni)
                        {
                            if(dispo)
                            {
                                A->direzione = dir;
                                dispo = false;
                                break;
                            }
                        }
                    }

                }else{
                    
                     {
                        //ottengo il mutex dell'incrocio in cui sono dentro
                        lock_guard <mutex> lock(Incroci.at(idx)->direzione);

                        A->direzione = A->direzione_entrata;
                        A->direzione_entrata = "";                          
                    }
                }*/

                {
                    lock_guard<mutex> lock(Incroci.at(idx)->direzione);

                    // Caso 1: prima volta in assoluto → cerca una direzione libera
                    if (A->direzione_entrata.empty())
                    {
                        for (auto& [dir, dispo] : Incroci.at(idx)->direzioni)
                        {
                            if (dispo)
                            {
                                A->direzione = dir;
                                dispo = false;
                                break;
                            }
                        }

                    }else{

                        // Caso 2: arriva da un incrocio precedente → usa la direzione prenotata
                        // La direzione_entrata è già stata "prenotata" in search_exit
                        // quindi NON serve settare dispo = false (già fatto)
                        A->direzione = A->direzione_entrata;
                        A->direzione_entrata = "";
                    }
                }

                if (A->direzione.empty())
                {
                    Incroci.at(idx)->entrare.release();
                    Incroci.at(idx)->posti_liberi++;
                    i--;
                    continue; // riprova al prossimo giro del for
                }

                {
                    lock_guard <mutex> lock(scrittura_info);

                    cout<<"L'auto "<<A->nome<<" è arrivato all'incrocio "<<A->incrocio_iniziale
                    <<" adesso si deciderà in quale direzione del prossimo incrocio andrà\n\n";
                }

                    //
                    //fase di scelta dell'auto
                    //se l'auto non è scelta sta nell'incrocio e continua
                    //la procedura del do-while cioè 
                    //scelta di chi va al prossimo incrocio, SE POSSIBILE
                    //    
                A->choosed = false;

                int tentativi = 0;
                do
                {
                    {
                        unique_lock<mutex> lock(Incroci.at(idx)->find_patter);

                        if(Incroci.at(idx)->decide && !A->choosed)
                        {
                            Incroci.at(idx)->decide = false;

                            search_exit(A, Incroci, idx);

                            Incroci.at(idx)->decide = true;

                            lock.unlock();

                            Incroci.at(idx)->get_direzione.notify_all();
                        }else{
                            Incroci.at(idx)->get_direzione.wait(lock,[&]{return A->choosed;});   
                        }
                    }

                    tentativi++; // incrementa sempre, dentro o fuori dal ramo if

                    if(tentativi >= 3 && !A->choosed)
                    {
                        lock_guard<mutex> lock(Incroci.at((idx+1)%tot_incroci)->direzione);

                        uniform_int_distribution<int> dist(0, 3);

                        auto it = Incroci.at((idx+1)%tot_incroci)->direzioni.begin();
                        do
                        {
                            it = Incroci.at((idx+1)%tot_incroci)->direzioni.begin();
                            advance(it, dist(A->generatore));

                        }while(it->first == "Avanti");

                        A->direzione_entrata = it->first;
                        A->choosed = true;
                    }

                }while(!A->choosed);
                    //scelta delle auto
                {
                    lock_guard <mutex> lock(scrittura_info);
                        
                    cout<<"L'auto "<<A->nome<<" sta percorrendo l'incrocio "
                    <<A->incrocio_iniziale<<" e ci mette "<<A->tempo_incrocio<<" secondi\n\n";
                }

                Incroci.at(idx)->entrare.release(); 

                {
                    lock_guard <mutex> lock(Incroci.at(idx)->direzione);

                    Incroci.at(idx)->posti_liberi++;
                }

                int next = (idx + 1) % tot_incroci;

                {
                    lock_guard <mutex> lock(Incroci.at(idx)->direzione);

                    /*for(auto& [dir, pos] : Incroci.at(idx)->direzioni)
                    {
                        if(A->direzione == dir)
                        {
                            A->direzione = "";
                            pos = true;                                
                            break;
                        }        
                    }*/

                    Incroci.at(idx)->direzioni.at(A->direzione) = true;
                    //A->direzione = "";

                    auto& vec_corrente = Incroci.at(idx)->auto_in_incrocio;
                    auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
                    if(it != vec_corrente.end())
                        vec_corrente.erase(it);

                    Incroci.at(idx)->tot_auto--;

                    Incroci.at(idx)->esci.notify_all();
                }

                this_thread::sleep_for(chrono::seconds(A->tempo_incrocio));

                /*int next = (idx + 1) % tot_incroci;

                {
                    lock_guard <mutex> lock(Incroci.at(idx)->direzione);

                    /*for(auto& [dir, pos] : Incroci.at(idx)->direzioni)
                    {
                        if(A->direzione == dir)
                        {
                            A->direzione = "";
                            pos = true;                                
                            break;
                        }        
                    }

                    Incroci.at(idx)->direzioni.at(A->direzione) = true;
                    //A->direzione = "";

                    auto& vec_corrente = Incroci.at(idx)->auto_in_incrocio;
                    auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
                    if(it != vec_corrente.end())
                        vec_corrente.erase(it);

                    Incroci.at(idx)->tot_auto--;

                    Incroci.at(idx)->esci.notify_all();
                }*/

                    //
                    //fase di uscita dall'incrocio
                    //si setta la direzione = "" in quanto stiamo andando fuori dall'incrocio e lo inseriamo nella queue
                    //e settermo true la posizione dentro la hash map 


                //Incroci.at(idx)->entrare.release(); 

                //Incroci.at(idx)->posti_liberi++;

                //QUA L'AUTO è CERTA CHE ARRIVA AL PROSSIMO INCROCIO ADESSO CHE LI HO INSERITI DEVONO ASPETTARE
                //
                Arrivo_a_incrocio(A);

                //ATTENTZIONE LA POSIZIONE LA PRENDE IN SEARCH_EXIT...DEADLOCK ASPETTANDO RISPOSTA DA NESSUNO!!!
                bool in_coda = false;
                {
                    unique_lock <mutex> lock(Incroci.at(next)->direzione);

                    if(!(Incroci.at(next)->direzioni.at(A->direzione_entrata)))
                    {
                        // Aggiungi al vettore dell'incrocio successivo
                        Incroci.at(next)->code_entrata.at(A->direzione_entrata).push(A);

                        {
                            lock_guard <mutex> lock1(scrittura_info);

                            cout<<"L'auto "<<A->nome<<" aspetterà nella coda "<<A->direzione_entrata
                            <<" dell'incrocio "<<next<<"\n\n";
                        }

                        Incroci.at(next)->esci.wait(lock,[&]
                        {
                        return Incroci.at(next)->code_entrata.at(A->direzione_entrata).front() == A && 
                        Incroci.at(next)->direzioni.at(A->direzione_entrata);
                        });

                        Incroci.at(next)->code_entrata.at(A->direzione_entrata).pop();

                        {
                            lock_guard <mutex> lock1(scrittura_info);

                            cout<<"L'auto "<<A->nome<<" è uscito dalla coda "<<A->direzione_entrata
                            <<" dell'incrocio "<<next<<"\n\n";
                        }   
                    }
                //}

                /*{
                    unique_lock<mutex> lock1(Incroci.at(next)->direzione);
                    if(!(Incroci.at(next)->direzioni.at(A->direzione_entrata)))
                    {
                        Incroci.at(next)->code_entrata.at(A->direzione_entrata).push(A);
                        in_coda = true;
                        Incroci.at(next)->esci.wait(lock1, [&]{

                            return Incroci.at(next)->code_entrata.at(A->direzione_entrata).front() == A && 
                            Incroci.at(next)->direzioni.at(A->direzione_entrata);

                        });
                        Incroci.at(next)->code_entrata.at(A->direzione_entrata).pop();
                    }
                }

                if(in_coda)
                {
                    lock_guard<mutex> lock(scrittura_info);
                    cout << "L'auto " << A->nome << " è uscito dalla coda...\n";
                }*/

                //{
                    //lock_guard <mutex> lock(Incroci.at(next)->direzione);
                    Incroci.at(next)->auto_in_incrocio.push_back(A);

                    //TODO FORSE QUA FUNZIONA
                    Incroci.at(next)->direzioni.at(A->direzione_entrata) = false;
                }
                // Aggiorna l'incrocio corrente dell'auto
                A->incrocio_iniziale = next;
                idx = next;

                finish_last_interception = idx % tot_incroci;

                //Arrivo_a_incrocio(A);
            }else
            {
                cout<<"non ho preso il try_acquire!!! sono auto "<<A->nome<<" incrocio"<<A->incrocio_iniziale<<"\n\n";
            }
        }

        {
            lock_guard <mutex> lock(Incroci.at(/*A->incrocio_iniziale*/tot_incroci - 1)->direzione);

            Incroci.at(/*A->incrocio_iniziale*/tot_incroci - 1)->direzioni.at(A->direzione) = true;
            A->direzione = "";
            auto& vec_corrente = Incroci.at(/*A->incrocio_iniziale*/tot_incroci - 1)->auto_in_incrocio;
            auto it = find(vec_corrente.begin(), vec_corrente.end(), A);
            if(it != vec_corrente.end())
                vec_corrente.erase(it);

            Incroci.at(/*A->incrocio_iniziale*/tot_incroci - 1)->tot_auto--;

            Incroci.at(/*A->incrocio_iniziale*/tot_incroci - 1)->esci.notify_all();
        }

        if(++Auto_completate >= auto_presenti)
            continua.notify_all();
            
        {
            lock_guard <mutex> lock(scrittura_info);
            cout<<"L\'auto "<<A->nome<<" ha finito ed è uscito all'ultimo incrocio adesso andrà via\n\n";
        }

        return;    
    }

    //TODO controllare poi se la funzione search_exit è corretta

    void search_exit(Auto* A,vector <unique_ptr<Incrocio>>& Incroci,int idx)
    {
        //COSA FA?


        //TODO applicare questa funzionalità
        //far passare tante auto tanti quanti sono gli spazi vuoti del prossimo incrocio,
        //l'altra auto rimarrà all'incrocio.
        //fare controlli come
        //1)se sei entrato a destra,non esci a destra per andare al prossimo incrocio(vale per tutte le altre posizioni)
        //2)se il posto libero è inacessibile ci saranno più auto che rimarranno all'incrocio

        int next_incrocio = (A->incrocio_iniziale + 1) % tot_incroci;

        //usato per controlli futuri per guardare il prossimo incrocio
        //è all'inizio

        //QUA SI SETTA LA STRADA D'USCITA
        {
            lock_guard <mutex> lock(Incroci.at(idx)->direzione);
            lock_guard <mutex> lock_next(Incroci.at(next_incrocio)->direzione);

            int da_muovere = min(Incroci.at(next_incrocio)->posti_liberi, 
                     (int)Incroci.at(idx)->auto_in_incrocio.size());

            for(int i = 0; i < da_muovere; i++)
            {
                for(auto& [dir, pos] : Incroci.at(next_incrocio)->direzioni)
                {
                    if( pos && Incroci.at(idx)->auto_in_incrocio.at(i)->direzione_entrata != dir && dir != "Avanti")
                    {
                        Incroci.at(idx)->auto_in_incrocio.at(i)->direzione_entrata = dir;
                        Incroci.at(idx)->auto_in_incrocio.at(i)->choosed = true;
                        //pos = false;
                        break;
                    }
                }
            }

            /*if(Incroci.at(idx)->auto_in_incrocio.size() == 1)
            {
                A->choosed = true;

                for(auto& [dir, pos] : Incroci.at(next_incrocio)->direzioni)
                {
                    if(dir != A->direzione && pos)
                    {
                        A->direzione_entrata = dir;
                        break;
                    }
                }
                return;

            }else{*/

                /*for(int i = 0; i < Incroci.at(idx)->auto_in_incrocio.size(); i++)
                {
                    int pos;
                    auto it = Incroci.at(next_incrocio)->direzioni.begin();
                    do
                    {
                        pos = rand() % 3;
                        it = Incroci.at(next_incrocio)->direzioni.begin();
                        advance(it, pos);

                    }while(it->first == "Dietro");
                                 
                    Incroci.at(idx)->auto_in_incrocio.at(i)->choosed = true;
                    Incroci.at(idx)->auto_in_incrocio.at(i)->direzione_entrata = it->first;
                }
                for(int i = 0; i < Incroci.at(idx)->auto_in_incrocio.size(); i++)
                {
                    vector <string> positions = {"Dietro","Sinistra","Destra"};

                    //creare set per generatore random
                    shuffle(positions.begin(), positions.end(), Incroci.at(idx)->auto_in_incrocio.at(i)->generatore);
                    uniform_int_distribution <int> dist(0, positions.size() - 1);

                    int scelta = dist(Incroci.at(idx)->auto_in_incrocio.at(i)->generatore);

                    Incroci.at(idx)->auto_in_incrocio.at(i)->choosed = true;
                    Incroci.at(idx)->auto_in_incrocio.at(i)->direzione_entrata = positions.at(scelta);
                    //A->choosed = true;
                    //A->direzione_entrata = positions.at(scelta);
                }*/
                
            //}
        }  
    }
    
};

int main()
{

    set_variables();

    vector <unique_ptr<Incrocio>> Incroci;
    vector <Auto*> Auto_in_gioco;

    //Incrocio I;

    //I.inizializza_Incroci(Incroci);
    //I.inizializza_auto(Auto_in_gioco,Incroci);
    //I.lancia_auto(Auto_in_gioco,Incroci);

    Incrocio::inizializza_Incroci(Incroci);
    Incrocio::inizializza_auto(Auto_in_gioco,Incroci);
    Incrocio::lancia_auto(Auto_in_gioco,Incroci);

    {
        unique_lock<mutex> lock(finisci_programma);

        continua.wait(lock,[&]{return Auto_completate >= auto_presenti;});
    }

    Incrocio::join_threads(Auto_in_gioco);
    //I.join_threads(Auto_in_gioco);

    for(auto& auti : Auto_in_gioco)
        delete auti;

    return 0;
}

void set_variables()
{
    cout<<"Settare le variabili:\n";
    do
    {
        cout<<"Inserire quanti incroci voler avere\n";
        tot_incroci = valid(tot_incroci);

        cout<<"Non puoi inserire più di "<<tot_incroci * 3<<" auto\n\n";

        cout<<"Inserire quante auto voler avere\n\n";
        auto_presenti = valid(auto_presenti);

    }while(controll_variables());

    cout<<"Perfetto si può partire con la simulazione aspettare 5 secondi prima di partire\n\n";
    
    this_thread::sleep_for(chrono::seconds(5));

    return;
}

bool controll_variables()
{
    
    if(tot_incroci * 3 >= auto_presenti)
        return false;

    return true;
}

int valid(int number)
{
    while(!(cin>>number))
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    return number;
}
