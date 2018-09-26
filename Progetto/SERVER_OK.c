#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#define fflush(stdin) while(getchar() != '\n')
#define file_utenti "user.dat"
#define file_messaggi "messages.dat"
 
/*	Codici errore (Corrispondono al valore di ritorno):
 * 	 1 | Operazione effettuata correttamente
 * 
 *      -1 | Accesso già effettuato
 *	-2 | Effettua prima log-in
 *	-3 | Nome utente e Password non trovati
 * 
 *	-5 | Errore nell'apertura del file
 *  -6 | Nessun messaggio presente
 * 	-7 | Errore nella scrittura del file
 * 	-8 | Il messaggio non è presente nel sistema
 * 	-9 | Non hai il permesso per eliminare il messaggio!
 *	
 *  Codice operazione:
 *	 0 | Effettua accesso -> Argomento1: nome_utente | Argomento2: password
 *   1 | Leggi tutti i messaggi -> Argomento1: NULL | Argomento2: NULL
 * 	 2 | Inserisci messaggio -> Argomento1: oggetto_messaggio | Argomento2: testo_messaggio
 * 	 3 | Rimuovi messaggio -> Argomento1: id_messaggio | Argomento2: NULL
 */ 
 
pthread_mutex_t *fileAccess;
 
struct comunicazione {
	int operazione;
	int valore_ritorno;
	char argomento1[512];
	char argomento2[512];
	};
 
int consistenza_sessione;

struct sessione {
	int id_utente_loggato;
	char nome_utente_loggato[64];
	};

struct messaggi {
	int id_utente;
	int id_messaggio;
	char messaggio[512];
	char oggetto[128];
	char mittente[64];
	};

struct utente {
	int id_utente;
	char nome_utente[64];
	char password_utente[64];
	};
	
__thread struct sessione utente_loggato;

void decode(char *buffer, struct comunicazione *struttura){

   struttura->operazione = ntohs(atoi(strtok(buffer, "|")));
   struttura->valore_ritorno = ntohs(atoi(strtok(NULL, "|")));
   strcpy(struttura->argomento1, strtok(NULL, "|"));
   strcpy(struttura->argomento2, strtok(NULL, "|"));
}

void *recupero_consistenza_informazioni(){
	
	FILE *infile;
	FILE *infile1;
    struct messaggi recupero;
    
    infile = fopen (file_messaggi, "r");
    if (infile == NULL)
    {
		consistenza_sessione = 0;
		return (int *)-5;
    }

    fseek(infile, -(sizeof(struct messaggi)), SEEK_END);
    fread(&recupero, sizeof(struct messaggi), 1, infile);
    consistenza_sessione = recupero.id_messaggio;
    fclose(infile);
    
    return (int *)1;
	}

int elimina_messaggio(int id_utente, int id_messaggio){
	
    FILE *leggo;
    FILE *scrivo;
    pthread_mutex_lock(fileAccess);
    struct messaggi controllo;
  
    leggo = fopen (file_messaggi, "r");
    if (leggo == NULL)
    {
		pthread_mutex_unlock(fileAccess);
		fclose(leggo);
        return -5; // Errore nell'apertura del file
    }
    
	scrivo = fopen ("passaggio.dat", "w+");
    if (scrivo == NULL)
    {
		pthread_mutex_unlock(fileAccess);
		fclose(scrivo);
        return -5; // Errore nell'apertura del file
    }
    
    fseek(leggo, 0, SEEK_END);
    if (ftell(leggo) == 0){
		pthread_mutex_unlock(fileAccess);
		fclose(leggo);
		fclose(scrivo);
		return(-6); // Nessun messaggio presente
	}
    fseek(leggo, 0, SEEK_SET);
    
    int occorrenze_messaggio = 0;
    int occorrenze_utente = 0;
    
	while(fread(&controllo, sizeof(struct messaggi), 1, leggo)){
		if(controllo.id_messaggio == id_messaggio){
			occorrenze_messaggio = 1;
		}
		
		if(controllo.id_utente == id_utente && controllo.id_messaggio == id_messaggio){
			occorrenze_utente = 1;
			consistenza_sessione = consistenza_sessione - 1;
			continue;
		}
		
		if(fwrite(&controllo, sizeof(struct messaggi), 1, scrivo) == 0){
			pthread_mutex_unlock(fileAccess);
			fclose(leggo);
			fclose(scrivo);
			return -7; // Errore nella scrittura su file
		}
	}
        
    fclose(leggo);
    unlink(file_messaggi);
    link("passaggio.dat", file_messaggi);
    fclose(scrivo);
	unlink("passaggio.dat");
	pthread_mutex_unlock(fileAccess);

	if (occorrenze_messaggio == 0){
		return -8;
		}
	if (occorrenze_messaggio == 1 && occorrenze_utente == 0){
		return -9;
		}
    return 1;
}

int controllo_accesso(char *nome_utente, char *password){
	
	pthread_mutex_lock(fileAccess);

	if(utente_loggato.id_utente_loggato != 0){
		pthread_mutex_unlock(fileAccess);
		return -1; // Utente già loggato
	}

    FILE *infile;
    struct utente input;
     
    infile = fopen ("user.dat", "r");
    if (infile == NULL)
    {
		fclose(infile);
		pthread_mutex_unlock(fileAccess);
        return -5; // Errore nell'apertura del file
    }
     
    while(fread(&input, sizeof(struct utente), 1, infile)){
		if(strcmp((const char*)&(input.nome_utente),(const char *)nome_utente) == 0 & strcmp((const char *)&(input.password_utente), (const char *)password) == 0){
			utente_loggato.id_utente_loggato = input.id_utente;
			strcpy(utente_loggato.nome_utente_loggato, input.nome_utente);
			fclose(infile);
			pthread_mutex_unlock(fileAccess);
			return input.id_utente;
		}
	}
	pthread_mutex_unlock(fileAccess);
    fclose(infile);
    return -3; // Username o password errati
}

int inserisci_nuovo_messaggio(char *messaggio, char *oggetto, char *mittente, int id_utente_loggato){
	
	if(utente_loggato.id_utente_loggato == 0){
		return -2; // Utente non loggato
	}
	
	// Inserisco il nuovo messaggio
	struct messaggi nuovo_messaggio;
	nuovo_messaggio.id_utente = id_utente_loggato;
	nuovo_messaggio.id_messaggio = consistenza_sessione + 1;
	strcpy(nuovo_messaggio.messaggio, messaggio);
	strcpy(nuovo_messaggio.oggetto, oggetto);
	strcpy(nuovo_messaggio.mittente, mittente);
	
	pthread_mutex_lock(fileAccess);
	
	FILE *outfile;
     
    outfile = fopen (file_messaggi, "a+");
    if (outfile == NULL){   
		 
		pthread_mutex_unlock(fileAccess);
        return -5; // Errore nell'apertura del file
    }
     
    if (fwrite(&nuovo_messaggio, sizeof(struct messaggi), 1, outfile) < 0){
		pthread_mutex_unlock(fileAccess);
		fclose(outfile);
		return -7; // Errore nella scrittura su file
	}  
    fclose(outfile);
    pthread_mutex_unlock(fileAccess);
    consistenza_sessione = consistenza_sessione + 1;
    return 1;
} 
 
int leggi_tutti_messaggi(void *socket){
	
    pthread_mutex_lock(fileAccess);
    FILE *infile;
    struct messaggi input;
    int valore_ritorno;
    infile = fopen (file_messaggi, "r");
    if (infile == NULL)
    {
		pthread_mutex_unlock(fileAccess);
		return(-5); // Errore nell'apertura del file
    }
    fseek(infile, 0, SEEK_END);
    int size = ftell(infile);
    send(*((int*)socket), &size, sizeof(int) , 0);
    if (ftell(infile) == 0){
		fclose(infile);
		pthread_mutex_unlock(fileAccess);
		return(-6); // Nessun messaggio presente
	}
	fseek(infile, 0, SEEK_SET);
    while(fread(&input, sizeof(struct messaggi), 1, infile)){
		if(send(*((int*)socket), &input, sizeof(struct messaggi) , 0) > 0){
			if (recv(*((int*)socket) , &valore_ritorno , sizeof(int) , 0) >0 ){
				if (valore_ritorno != 1){
					fclose(infile);
					pthread_mutex_unlock(fileAccess);
					return -100;
				}else{
					continue;
				}		
			}else{
				pthread_mutex_unlock(fileAccess);
				pthread_exit(-1);
				}
		}else{
			pthread_mutex_unlock(fileAccess);
			pthread_exit(-1);
			}
	}
    fclose(infile);
	pthread_mutex_unlock(fileAccess);
    return 0;
}
	
void *gestore_utente(void *socket){
	
	int scelta, valore_ritorno, uscita_thread, read_size;
	int *soc = socket;
	struct comunicazione ricezione;
	char *prova = malloc(1024*sizeof(char));
	utente_loggato.id_utente_loggato = 0;
	char *buffer = malloc(sizeof(struct comunicazione)+3*sizeof(char));
	while(recv(*((int*)socket) , buffer, sizeof(struct comunicazione)+3*sizeof(char), 0)> 0 )
    {
		puts("Arrivo?");
        decode(buffer, &ricezione);
        puts("Decodifico?");
        scelta = ricezione.operazione;
        sprintf(buffer, "Ho immesso questo: %d", scelta);
        puts(buffer);
		switch (scelta) {
		
	case 0: // Login
		if(utente_loggato.id_utente_loggato == 0){
		valore_ritorno = controllo_accesso(ricezione.argomento1, ricezione.argomento2);
		write(*((int*)socket), &valore_ritorno, sizeof(int));
		break;
		}else{
		valore_ritorno = -1;
		write(*((int*)socket), &valore_ritorno, sizeof(int));
		break;
		}
	
	case 1: // Leggi tutti i messaggi
		leggi_tutti_messaggi(socket);
		break;
	
	case 2: // Inserisci nuovo messaggio
		if(utente_loggato.id_utente_loggato != 0){
			valore_ritorno = inserisci_nuovo_messaggio(ricezione.argomento1, ricezione.argomento2, utente_loggato.nome_utente_loggato, utente_loggato.id_utente_loggato);
			write(*((int*)socket), &valore_ritorno, sizeof(int));
			break;
		}
		else
		{
			valore_ritorno = -2;
			write(*((int*)socket), &valore_ritorno, sizeof(int));
			break;
		}
	
	case 3:	// Elimina messaggio
		if(utente_loggato.id_utente_loggato != 0){
			valore_ritorno = elimina_messaggio(utente_loggato.id_utente_loggato, ricezione.valore_ritorno);
			write(*((int*)socket), &valore_ritorno, sizeof(int));
			break;
		}
		else
		{
			valore_ritorno = -2;
			write(*((int*)socket), &valore_ritorno, sizeof(int));
			break;
		}
			
		}
		}
	free(buffer);
	pthread_exit(-1);
}

int inserisci_nuovo_utente(int id_utente, char *nome_utente, char *password_utente){
	
	struct utente nuovo_utente;
	
	nuovo_utente.id_utente = id_utente;
	strcpy(nuovo_utente.nome_utente, nome_utente);
	strcpy(nuovo_utente.password_utente, password_utente);
	
	FILE *outfile;
     
    outfile = fopen ("user.dat", "a+");
    if (outfile == NULL)
    {
        printf("\nErrore nell'apertura del file\n");
        return -5; // Errore nell'apertura del file
    }
     
    if(fwrite(&nuovo_utente, sizeof(struct utente), 1, outfile) == 0){
		printf("\nErrore nella scrittura su file\n");
		return -7; // Errore nella scrittura su file
		}  
    fclose(outfile);
}

int visualizza_utenti(){
	
	FILE *infile;
    struct utente input;
    
    infile = fopen (file_utenti, "r");
    if (infile == NULL)
    {
        printf("\nErrore nell'apertura del file\n");
		return(-5);
    }
    printf("\nID Utente | Nome utente | Password utente\n");
    while(fread(&input, sizeof(struct utente), 1, infile)){
        printf ("\n%d | %s | %s\n", input.id_utente, input.nome_utente, input.password_utente);
    }
    fclose(infile);
    return 1;
}
	
void help(){
	printf("\nUtilizzo: server [opzioni]\nOpzioni:\n  --seeuser                                       Permette di visualizzare la lista degli utenti presenti nel sistema\n  --insertuser id_utente nome_utente password     Permette di inserire un nuovo utente al sistema\n\n");
}

int main(int argc , char *argv[]){
	
	if (argc > 1){
		if (strcmp(argv[1], "--seeuser") == 0){
		visualizza_utenti();
		return 0;
		}
	
	if(strcmp(argv[1], "--insertuser") == 0){
		inserisci_nuovo_utente(atoi(argv[2]), argv[3], argv[4]);
		return 0;
		}
		
	if(strcmp(argv[1], "--help") == 0){
		help();
		return 0;
		}
	}
	
	fileAccess = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(fileAccess, NULL);
	
	signal(SIGPIPE, SIG_IGN);

    int socket_desc , socket_cliente , c , read_size;
    struct sockaddr_in server , client;
    char client_message[2000];
    pthread_t thread, thread1;
    pthread_create(&thread1, NULL, recupero_consistenza_informazioni, NULL);
	pthread_join(thread1, NULL);
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8000 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
     
    //accept connection from an incoming client
    while(1){
    socket_cliente = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
    if (socket_cliente < 0)
    {
        perror("accept failed");
        return 1;
    }
    puts("Connection accepted");
    pthread_create(&thread, NULL, gestore_utente, (void *)&socket_cliente);
    }
    return 0;
}
