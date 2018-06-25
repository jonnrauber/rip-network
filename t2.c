#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> 
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include "t2.h"

unsigned int LOCAL_ROUTER;
unsigned int LOCAL_ROUTER_PORT;
neighbor* neighborhood; 	//enlaces deste roteador
int qt_links;	 		//neighborhood.length()
dv_table dv_table_;		//tabela vetor-distância deste roteador

/** 
	Exibe o erro e encerra o processo.
*/
void die(char *s) {
    perror(s);
    exit(1);
}

/**
*	Retorna o número da porta do roteador id_router.
*	Retorna 0 caso o id_router não esteja especificado no arquivo
*	roteador.config.
*/
int search_router_port(int id_router) {
	int router_id, router_port;
	char router_host[16];
	int port;

	//abre o arquivo com as informações do roteador
	FILE *f = fopen("roteador.config", "r");
	if (f == NULL) {
		printf("Erro ao abrir arquivo 'roteador.config'\n");
		exit(-2);
	}

	port = 0;
	while (1) {
		if (fscanf(f, "%d %d %s\n", &router_id, &router_port, router_host) != EOF) {
			if (router_id == id_router) {
				port = router_port;
				break;
			}
		} else {
			break;
		}
	}
	fclose(f);
	return port;
}

/**
	Cria o vetor de vizinhos do roteador local.
*/
void load_neighbors() {
	int router1, router2, cost, i, router_id, router_port;
	char router_host[16];

	//abre o arquivo com os enlaces
	FILE *f = fopen("enlaces.config", "r");
	if (f == NULL) {
		printf("Erro ao abrir o arquivo 'enlaces.config'.\n");
		exit(-4);
	}

	//carrega os vizinhos no vetor neighborhood
	qt_links = 0;
	while (1) {
		if (fscanf(f, "%d %d %d", &router1, &router2, &cost) != EOF) {
			if (router1 == LOCAL_ROUTER || router2 == LOCAL_ROUTER) {
				neighborhood = (neighbor *)realloc(neighborhood, sizeof(neighbor) * ++qt_links);
				neighborhood[qt_links-1].id_dst = (router1 == LOCAL_ROUTER) ? router2 : router1;
				neighborhood[qt_links-1].cost = cost;
			}
		} else {
			break;
		}
	}
	fclose(f);
	
	//complementa as informações dos vizinhos 
	f = fopen("roteador.config", "r");
	while (1) {
		if (fscanf(f, "%d %d %s\n", &router_id, &router_port, router_host) != EOF) {
			for (i = 0; i < qt_links; i++) {
				if (neighborhood[i].id_dst == router_id) {
					strcpy(neighborhood[i].host_dst, router_host);
					neighborhood[i].port_dst = router_port;
					break;
				}
			}
		} else {
			break;
		}
	}
	fclose(f);	
}

/**
	Recebe um 'packet' e encaminha ao roteador na tabela de roteamento
	que possua o menor custo.
*/
void send_message(packet* pck, neighbor* link) {
	struct sockaddr_in si_other;
	unsigned int slen = sizeof(si_other);
	int tries_until_desconnect = TRIES_UNTIL_DESCONNECT;
	int ack_received = 0;
	int s;
	
	memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(link->port_dst);
    
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		die("socket()");
    
    if (inet_aton(link->host_dst, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    
	while (tries_until_desconnect--) {
		if (sendto(s, pck, BUFLEN, 0, (struct sockaddr *) &si_other, slen) == -1)
			die("sendto()");
		
		struct timeval timeout;
		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = TIMEOUT*2;
	
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		recvfrom(s, &ack_received, sizeof(ack_received), 0, (struct sockaddr *) &si_other, &slen);
		
		if (ack_received)
			break;
	}
	
	/* 
		Se a entrega não deu certo, assume-se que o enlace caiu.
		Roteador precisa atualizar o vetor-distância.
	*/
	if (!ack_received) {
		printf("Roteador %d inalcançável após %d tentativas com estouro de timeout.\n", link->id_dst, TRIES_UNTIL_DESCONNECT);
		int changes;
		
	}
}

/**
	Dado um id de roteador destino, encontra qual é o vizinho que leva
	até ele com o menor custo.
	Retorna o index do array de vizinhos em que ele está.
*/
unsigned int whos_the_next(int id_dst) {
	int i;
	
	for (i = 0; i < qt_links; i++) {
		if (neighborhood[i].id_dst == dv_table_.distance[LOCAL_ROUTER][id_dst].id_neighbor) {
			return i;
		}
	}
	
	printf("Não é possível alcançar o roteador %d.\n", id_dst);
	return -1;
}

/**
	Recebe o vetor-distância do vizinho, aplica as atualizações e 
	envia novamente aos vizinhos em caso de alteração.
*/
void* updateDV(void* data) {
	distance_vector* dv = (distance_vector *)data;
	int changes;
	
	printf("teste\n");
	
	//updateMyTable(dv_table_, dv, &changes);
	//updateTable(newTable);
	//print_dv_table();
	
	//if(updated){
		//int id = arg->router->id;
		//list_t* routers = arg->routers;
		//list_t* neighboors = arg->neighboors;
		//resend(routers, neighboors, id);
	//}
}

/**
	Função a ser executada na thread que recebe as mensagens,
	identifica-as e toma uma decisão.
*/
void* receiver_func(void *data) {
	struct sockaddr_in si_me, si_other;
	unsigned int slen = sizeof(si_other), recv_len, neighbor_index;
	int s, ack;
	
	packet* pck;
	neighbor* link;
	
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		die("socket()");
	
	memset((char *) &si_me, 0, sizeof(si_me));
	
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(LOCAL_ROUTER_PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(s, (struct sockaddr *) &si_me, sizeof(si_me)) == -1)
		die("bind()");

	while(1) {
		pck = (packet *)malloc(sizeof(packet));
		fflush(stdout);

		//tenta receber os dados --> chamada bloqueante
		if ((recv_len = recvfrom(s, pck, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
			die("recvfrom()");

		switch (pck->type) {
			case TYPE_DATA: // mensagem de dados, original, a ser imprimida ou repassada.
				
				if (pck->id_dst == LOCAL_ROUTER) {
					//Se este roteador for o destino, imprime a msg
					printf("\rMensagem de %d:                                 \n", pck->id_src);
					printf("%s\n\n", pck->data);

				} else {
					//Se o roteador não é o destino, encaminha ao próximo roteador.
					printf("\rRoteador %d encaminhando mensagem com # sequência %d para o destino %d.\n", LOCAL_ROUTER, pck->seq_number, pck->id_dst);
					//Verifica qual é o vizinho que leva ao destino da mensagem
					neighbor_index = whos_the_next(pck->id_dst);
					
					if (neighbor_index < 0)
						continue;
					
					link = &neighborhood[neighbor_index];
					//Manda para o vizinho
					send_message(pck, link);
				}
				
			break;
			case TYPE_DV: ;
				/* 
					cria thread para aplicar a atualização de vetor-distância,
					pois pode ter concorrência
				*/
				pthread_t thread;
				pthread_create(&thread, NULL, updateDV, pck->dv);
			break;
			default:
			break;
		}
		
		ack=1;
		if (sendto(s, &ack, sizeof(ack), 0, (struct sockaddr *) &si_other, slen) == -1)
			die("sendto()");
		
		free(pck);
	}

	pthread_exit(NULL);
}

/**
	Construtor do tipo 'packet'.
*/
packet create_pck(int id_src, int id_dst, int type, void* message) {
	packet pck;
	pck.id_src = id_src;
	pck.id_dst = id_dst;
	pck.type = type;
	strcpy(pck.data, message);
	return pck;
}

/**
	Função a ser executada na thread que lê e envia as mensagens da entrada padrão.
*/
void* sender_func(void *data) {
	int id_dst, neighbor_index;
	char message[MSGLEN];
	neighbor* next_link;
	
	// loop infinito, sempre espera por novos envios
	while(1) {
		printf("\nEscolha o roteador de destino: "); 
		scanf("%d", &id_dst);
		getchar();
		
		printf("Escreva a mensagem (limite de 100 caracteres): ");
		fflush(stdout);
		fflush(stdin);
		fgets(message, MSGLEN, stdin);
		message[strlen(message)-1] = '\0';
		
		//cria o pacote, encontra o vizinho que leva ao destino e envia a mensagem
		packet pck = create_pck(LOCAL_ROUTER, id_dst, TYPE_DATA, message);
		neighbor_index = whos_the_next(id_dst);
		next_link = &neighborhood[neighbor_index];
		send_message(&pck, next_link);
		
	}
}

/**
	Função a ser executada na thread que reenvia o vetor-distância.
*/
void* dv_sender_func(void *data) {
	int i;
	packet pck;
	pck.id_src = LOCAL_ROUTER;
	pck.type = TYPE_DV;
	
	while (1) {
		memmove(pck.dv, dv_table_.distance[LOCAL_ROUTER], sizeof(distance_vector) * LOCAL_ROUTER);
		
		for (i = 0; i < qt_links; i++) {
			send_message(&pck, &neighborhood[i]);
		}
	}
	sleep (DV_RESEND_INTERVAL);
	
}

/**
	Após as configurações e carregamentos iniciais, inicia as funcionalidades do roteador.
	Cria 3 threads, uma para receber, outra para enviar e outra para atualizar os vetores-distância
*/
void start_router() {
	pthread_t threads[3];
	pthread_create(&(threads[0]), NULL, receiver_func, NULL);
	pthread_create(&(threads[1]), NULL, sender_func, NULL);
	//pthread_create(&(threads[2]), NULL, dv_sender_func, NULL);
	//pthread_join(threads[2], NULL);
	pthread_join(threads[1], NULL);
	pthread_join(threads[0], NULL);
}

/**
	Imprime o id dos roteadores ligados a este.
*/
void print_neighbors() {
	int i;

	printf("----- Vizinhos do roteador %d -----\n", LOCAL_ROUTER);
	for (i = 0; i < qt_links; i++) {
		printf("router %d cost %d\n", neighborhood[i].id_dst, neighborhood[i].cost);
	}
}

/**
	Imprime a tabela vetor-distância.
*/
void print_dv_table() {
	int i, j;

	//cabeçalho da tabela
	printf("\t|");
	for (i = 0; i < MAX_ROUTERS; i++) {
		if (!dv_table_.distance[LOCAL_ROUTER][i].allocated)
			continue;
		printf("\t%d|", i);
	}
	printf("\n");

	//linhas do vetor distancia
	for (i = 0; i < MAX_ROUTERS; i++) {
		if (!dv_table_.distance[LOCAL_ROUTER][i].allocated)
			continue;

		printf("%d\t|", i);
		for (j = 0; j < MAX_ROUTERS; j++) {
			if (!dv_table_.distance[i][j].allocated)
				continue;

			printf("\t%d|", dv_table_.distance[i][j].cost);
		}
		printf("\n");
	}
}

/**
	Inicia o vetor-distância.
*/
void start_distance_vector() {
	int i;

	//distance_vector.distance[LOCAL_ROUTER] refere-se às distancias deste roteador para os demais
	dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].cost = 0;
	dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].active = true;
	dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].allocated = true;
	dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].id_neighbor = 0;
	for (i = 0; i < qt_links; i++) {
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].cost = neighborhood[i].cost;
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].active = true;
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].allocated = true;
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].id_neighbor = neighborhood[i].id_dst;
	}
}

/**
	Função main.
*/
int main(int argc, char const *argv[]) {
	if (argc != 2) {
		printf("Atenção! Executar usando ./t2 id=ID_ROUTER.\n\n");
		exit(-1);
	}
	LOCAL_ROUTER = atoi(argv[1]);
	
	//procura a porta do roteador correspondente à ID instanciada
	LOCAL_ROUTER_PORT = search_router_port(LOCAL_ROUTER);
	if (LOCAL_ROUTER_PORT == 0) {
		printf("ID de roteador passado por parâmetro não especificado no arquivo roteador.config.\n");
		exit(-3);
	}

	//cria um vetor com os vizinhos do roteador
	load_neighbors();

	//inicia a estrutura vetor-distancia
	start_distance_vector();

	print_neighbors();
	print_dv_table();

	//inicia a operação do roteador
	start_router();
}
