/*#include "Intersection.hpp"

#include <limits>

using namespace std;

bool controll_variables();
void set_variables();
int valid(int number);
*/

#include "Intersection.hpp"

#include <limits>
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

int main()
{

    set_variables();

    std::vector <std::unique_ptr<Incrocio>> Incroci;
    
    std::vector <Veicolo*> Auto_in_gioco;

    Incrocio::inizializza_Incroci(Incroci);
    Incrocio::inizializza_auto(Auto_in_gioco,Incroci);
    Incrocio::lancia_auto(Auto_in_gioco,Incroci);

    {
        std::unique_lock <std::mutex> lock(finisci_programma);

        continua.wait(lock,[&]{return Auto_completate >= auto_presenti;});
    }

    Incrocio::join_threads(Auto_in_gioco);

    for(int i = 0; i < Auto_in_gioco.size(); i++)
        Auto::print_info(Auto_in_gioco.at(i));

    for(auto& auti : Auto_in_gioco)
        delete auti;

    return 0;
}

void set_variables()
{
    std::cout<<"Settare le variabili:\n";
    do
    {  
        std::cout<<"Inserire quanti incroci voler avere,almeno 2\n";
        tot_incroci = valid(tot_incroci);

        std::cout<<"Non puoi inserire più di "<<tot_incroci * 3<<" auto\n\n";

        std::cout<<"Inserire quante auto voler avere\n\n";
        auto_presenti = valid(auto_presenti);

    }while(controll_variables());

    std::cout<<"Perfetto si può partire con la simulazione aspettare 5 secondi prima di partire\n\n";
    
    std::this_thread::sleep_for(std::chrono::seconds(5));

    return;
}

bool controll_variables()
{
    
    if(tot_incroci * 3 >= auto_presenti && tot_incroci >= 2)
        return false;

    return true;
}

int valid(int number)
{
    while(!(std::cin>>number))
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    return number;
}