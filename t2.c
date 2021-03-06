#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> 
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "t2.h"

//necessário mutex para proteger escrita concorrente na dv_table_
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t printf_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned int LOCAL_ROUTER;
unsigned int LOCAL_ROUTER_PORT;
unsigned int SEQ_NUMBER;
neighbor* neighborhood; 	//enlaces deste roteador
int qt_links;	 			//neighborhood.length()
dv_table dv_table_;			//tabela vetor-distância deste roteador

/** 
	Exibe o erro e encerra o processo.
*/
void die(char *s) {
    perror(s);
    exit(1);
}

/**
	Imprime um unico vetor distancia passado como argumento.
*/
void print_unique_dv(distance_vector* dv) {
	int i;
	bool print_comma = false;
	for (i = 0; i < MAX_ROUTERS; i++) {
		if (dv[i].allocated) {
			if (print_comma) printf(",");
			printf("%d->%d", i, dv[i].cost);
			print_comma = true;
		}
	}
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
	
	if(pck->type == TYPE_DATA && (
			dv_table_.distance[LOCAL_ROUTER][link->id_dst].allocated == false ||
			dv_table_.distance[LOCAL_ROUTER][link->id_dst].cost == INFINITE)) {
		pthread_mutex_lock(&printf_mutex);
		printf("-----\nImpossível alcançar o roteador %d.\n", link->id_dst);
		pthread_mutex_unlock(&printf_mutex);
		return;
	}
	
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
		pthread_mutex_lock(&printf_mutex);
		printf("-----\nRoteador %d inalcançável após %d tentativas com estouro de timeout.\n", link->id_dst, TRIES_UNTIL_DESCONNECT);
		
		if (dv_table_.distance[link->id_dst][LOCAL_ROUTER].allocated == true) {
			update_after_error(link->id_dst);
			printf("ERRO CALCULADO\n");
			print_dv_table();
		}
		printf("-----\n");
		pthread_mutex_unlock(&printf_mutex);
	}
}

void update_after_error(int id_dst) {
	int i;
	pthread_mutex_lock(&mutex);
	
	dv_table_.distance[LOCAL_ROUTER][id_dst].cost = INFINITE;
	for (i = 0; i < MAX_ROUTERS; i++)
		dv_table_.distance[id_dst][i].allocated = false;
	
	int min_cost = INFINITE;
	int neighbor_min_cost = LOCAL_ROUTER;
	for (i = 0; i < qt_links; i++) {
		int aux = neighborhood[i].id_dst;
		if (dv_table_.distance[aux][LOCAL_ROUTER].allocated == true) {
			int cost = dv_table_.distance[aux][id_dst].cost + dv_table_.distance[aux][LOCAL_ROUTER].cost;
			if (cost < min_cost) {
				min_cost = cost;
				neighbor_min_cost = aux;
			}
		}
	}
	dv_table_.distance[LOCAL_ROUTER][id_dst].cost = min_cost;
	dv_table_.distance[LOCAL_ROUTER][id_dst].id_neighbor = neighbor_min_cost;
	
	pthread_mutex_unlock(&mutex);	
}

/**
	Dado um id de roteador destino, encontra qual é o vizinho que leva
	até ele com o menor custo.
	Retorna o index do array de vizinhos em que ele está.
*/
unsigned int whos_the_next(int id_dst) {
	int i;
	
	if (id_dst == LOCAL_ROUTER)
		return -1;
	
	for (i = 0; i < qt_links; i++) {
		if (neighborhood[i].id_dst == dv_table_.distance[LOCAL_ROUTER][id_dst].id_neighbor) {
			return i;
		}
	}
	
	pthread_mutex_lock(&printf_mutex);
	printf("-----\nNão é possível alcançar o roteador %d.\n-----\n", id_dst);
	pthread_mutex_unlock(&printf_mutex);
	return -1;
}

/**
	Recebe um vetor distância do vizinho e aplica as alterações se necessário.
*/
int update_dv_table(distance_vector* dv, int id_src) {
	// Temos que alterar primeiro uma cópia da tabela de vetor distancia,
	// pois podem existir mais threads querendo alterá-la concorrentemente.
	// Para não segurar muito tempo os locks, faremos uma cópia e só no fim
	// adquirimos o lock através do mutex para alterar a dv_table_ original. 

	pthread_mutex_lock(&mutex);
	int retorno = 0;
	int i, j;
    

	for (i = 0; i < MAX_ROUTERS; i++) {
		if (dv[i].allocated == false)
			continue;
			
		dv_table_.distance[LOCAL_ROUTER][i].allocated = true;

		if (dv_table_.distance[id_src][i].allocated == false) {
			retorno = PRINT_AND_SEND_TABLE;

			for (j = 0; j < qt_links; j++) {
				int aux = neighborhood[j].id_dst;
	            dv_table_.distance[aux][i].allocated = true;
	            dv_table_.distance[aux][i].cost = INFINITE;
	        }
		}
		
		if (dv_table_.distance[id_src][i].cost != dv[i].cost)
			retorno = PRINT_TABLE;
			
		dv_table_.distance[id_src][i].cost = dv[i].cost;
		dv_table_.distance[id_src][i].id_neighbor = dv[i].id_neighbor;
	}

	for (i = 0; i < MAX_ROUTERS; i++) {
		if (dv_table_.distance[LOCAL_ROUTER][i].allocated == false)
			continue;
		
		if (i == LOCAL_ROUTER) {
			dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].cost = 0;
			continue;
		}

		int min_cost = INFINITE;
		int neighbor_min_cost = i;
		for (j = 0; j < qt_links; j++) {
			int aux = neighborhood[j].id_dst; 
			int cost = dv_table_.distance[aux][i].cost + dv_table_.distance[aux][LOCAL_ROUTER].cost;
			if (dv_table_.distance[aux][LOCAL_ROUTER].allocated == true && cost < min_cost) {
				min_cost = cost;
				neighbor_min_cost = aux;
			}
		}
		
		for (j = 0; j < qt_links; j++) {
			int aux = neighborhood[j].id_dst;
			if (aux == i && dv_table_.distance[aux][LOCAL_ROUTER].allocated == true) {
				if (neighborhood[j].cost < min_cost) {
					min_cost = neighborhood[j].cost;
					neighbor_min_cost = aux;
				}
			}
		}

		//if (min_cost < dv_table_.distance[LOCAL_ROUTER][i].cost) {
		if (min_cost != dv_table_.distance[LOCAL_ROUTER][i].cost) {
			retorno = PRINT_AND_SEND_TABLE;
			dv_table_.distance[LOCAL_ROUTER][i].cost = min_cost;
			dv_table_.distance[LOCAL_ROUTER][i].id_neighbor = neighbor_min_cost;
		}
	}
	pthread_mutex_unlock(&mutex);
	return retorno;
}

/**
	Recebe o vetor-distância do vizinho, aplica as atualizações e 
	envia novamente aos vizinhos em caso de alteração.
*/
void* update_dv(void* data) {
	dv_payload* dv_msg = (dv_payload *)data;
	int changes = update_dv_table(dv_msg->dv, dv_msg->id_src);
	
	if (changes > 0) { 
		pthread_mutex_lock(&printf_mutex);
		printf("-----\nAlteração da tabela após receber DV do roteador %d:\n", dv_msg->id_src);
		print_unique_dv(dv_msg->dv);
		printf("\n");
		print_dv_table();
		printf("-----\n");
		pthread_mutex_unlock(&printf_mutex);
	}
	pthread_exit(NULL);
}

/**
	Função a ser executada na thread que recebe as mensagens,
	identifica-as e toma uma decisão.
*/
void* receiver_func(void *data) {
	struct sockaddr_in si_me, si_other;
	unsigned int slen = sizeof(si_other), recv_len, neighbor_index;
	int s, ack, i;
	
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
					pthread_mutex_lock(&printf_mutex);
					printf("-----\n\nMensagem de %d:                                 \n", pck->id_src);
					printf("%s\n\n-----\n", pck->data);
					pthread_mutex_unlock(&printf_mutex);
				} else {
					//se já estourou a quantidade de saltos máxima, para de encaminhar.
					int qt_routers = 0;
					for (i = 0; i < MAX_ROUTERS; i++) {
						if (dv_table_.distance[LOCAL_ROUTER][i].allocated == true) 
							qt_routers++;
					}
					
					if (pck->jump > qt_routers) break;
					(pck->jump)++;
					//Se o roteador não é o destino, encaminha ao próximo roteador.
					pthread_mutex_lock(&printf_mutex);
					printf("-----\nRoteador %d encaminhando mensagem de %d com # sequência %d para o destino %d.\n-----\n", LOCAL_ROUTER, pck->id_src, pck->seq_number, pck->id_dst);
					pthread_mutex_unlock(&printf_mutex);
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
				/*pthread_mutex_lock(&printf_mutex);
				printf("Recebido vetor distância do roteador %d. Conteúdo: \n", pck->id_src);
				print_unique_dv(pck->dv.dv);
				printf("Vetor atual:\n");
				print_unique_dv(dv_table_.distance[pck->id_src]);
				pthread_mutex_unlock(&printf_mutex);*/
				
				/* 
					cria thread para aplicar a atualização de vetor-distância,
					pois pode ter concorrência
				*/
				pthread_t thread;
				pthread_create(&thread, NULL, update_dv, &pck->dv);
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
	pck.seq_number = SEQ_NUMBER++;
	pck.jump = 0;
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
		scanf("%d", &id_dst);
		getchar();
		if (id_dst == LOCAL_ROUTER) {
			pthread_mutex_lock(&printf_mutex);
			printf("----\n !!! O roteador %d é você mesmo! !!!\n-----\n", id_dst);
			pthread_mutex_unlock(&printf_mutex);
			continue;
		}
		
		pthread_mutex_lock(&printf_mutex);
		printf("Escreva a mensagem (limite de 100 caracteres): ");
		pthread_mutex_unlock(&printf_mutex);
		fflush(stdout);
		fflush(stdin);
		fgets(message, MSGLEN, stdin);
		message[strlen(message)-1] = '\0';
		
		//cria o pacote, encontra o vizinho que leva ao destino e envia a mensagem
		packet pck = create_pck(LOCAL_ROUTER, id_dst, TYPE_DATA, message);
		neighbor_index = whos_the_next(id_dst);
		if (neighbor_index < 0)
			continue;
		next_link = &neighborhood[neighbor_index];
		send_message(&pck, next_link);
	}
	pthread_exit(NULL);
}

/**
	Envia o vetor distância para os vizinhos.
*/
void send_dv(dv_table table) {
	int i;
	packet pck;
	pck.id_src = LOCAL_ROUTER;
	pck.type = TYPE_DV;
	pck.dv.id_src = LOCAL_ROUTER;
	memcpy(pck.dv.dv, table.distance[LOCAL_ROUTER], sizeof(distance_vector) * MAX_ROUTERS);
	
	for (i = 0; i < qt_links; i++) {
		send_message(&pck, &neighborhood[i]);
	}
}

/**
	Função a ser executada na thread que reenvia o vetor-distância.
*/
void* dv_sender_func(void *data) {
	
	while (1) {
		pthread_mutex_lock(&printf_mutex);
		printf("-----\nEnvio periódico de Vetor Distância\n");		
		printf("Enviando o DV: ");
		print_unique_dv(dv_table_.distance[LOCAL_ROUTER]);
		printf("\n");
		pthread_mutex_unlock(&printf_mutex);
		
		send_dv(dv_table_);
		
		pthread_mutex_lock(&printf_mutex);
		printf("-----\nTabela enviada a todos os vizinhos, esperando %d segundos para reenviar...\n-----\n", DV_RESEND_INTERVAL);
		pthread_mutex_unlock(&printf_mutex);
		
		sleep (DV_RESEND_INTERVAL);
	}
	
	pthread_exit(NULL);
}

/**
	Após as configurações e carregamentos iniciais, inicia as funcionalidades do roteador.
	Cria 3 threads, uma para receber, outra para enviar e outra para atualizar os vetores-distância
*/
void start_router() {
	pthread_t threads[3];
	
	printf("\n##############\n");
	printf("DIGITE O ID DO ROTEADOR QUE QUISER\nENVIAR A MENSAGEM A QUALQUER MOMENTO!\n");
	printf("##############\n\n");
	SEQ_NUMBER = 1;
	
	pthread_create(&(threads[0]), NULL, receiver_func, NULL);
	pthread_create(&(threads[1]), NULL, sender_func, NULL);
	pthread_create(&(threads[2]), NULL, dv_sender_func, NULL);
	pthread_join(threads[2], NULL);
	pthread_join(threads[1], NULL);
	pthread_join(threads[0], NULL);
}

/**
	Imprime o id dos roteadores ligados a este.
*/
void print_neighbors() {
	int i;
	
	printf("----- VIZINHOS DO ROTEADOR %d -----\n", LOCAL_ROUTER);
	for (i = 0; i < qt_links; i++) {
		printf("* ID: %d CUSTO DO ENLACE: %d\n", neighborhood[i].id_dst, neighborhood[i].cost);
	}
	printf("-----------------------------------\n\n");
}

/**
	Imprime a tabela vetor-distância.
*/
void print_dv_table() {
	int i, j;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	
	printf("Roteador %d, timestamp %lu\n", LOCAL_ROUTER, tv.tv_sec);

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
		if (!dv_table_.distance[i][LOCAL_ROUTER].allocated)
			continue;

		printf("%d\t|", i);
		for (j = 0; j < MAX_ROUTERS; j++) {
			if (! dv_table_.distance[i][j].allocated && ! dv_table_.distance[LOCAL_ROUTER][j].allocated)
				continue;
				
			if (dv_table_.distance[i][j].cost == INFINITE)
				printf("\t-|");
			else 
				printf("\t%d|", dv_table_.distance[i][j].cost);
		}
		printf("\n");
	}
}

/**
	Inicia o vetor-distância.
*/
void start_distance_vector() {
	int i, j;

	for (i = 0; i < MAX_ROUTERS; i++) { 
		for (j = 0; j < MAX_ROUTERS; j++) {
			dv_table_.distance[i][j].allocated = false; //inicializa vazia
			dv_table_.distance[i][j].cost = INFINITE;		
		}
	}
	
	dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].cost = 0;
	dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].allocated = true;
	dv_table_.distance[LOCAL_ROUTER][LOCAL_ROUTER].id_neighbor = LOCAL_ROUTER;
	
	for (i = 0; i < qt_links; i++) {
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].cost = neighborhood[i].cost;
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].allocated = true;
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].id_neighbor = neighborhood[i].id_dst;
		
		dv_table_.distance[neighborhood[i].id_dst][LOCAL_ROUTER].cost = INFINITE;
		dv_table_.distance[neighborhood[i].id_dst][LOCAL_ROUTER].allocated = true;
		dv_table_.distance[neighborhood[i].id_dst][LOCAL_ROUTER].id_neighbor = LOCAL_ROUTER;
		
		for (j = 0; j < qt_links; j++) {
			dv_table_.distance[neighborhood[i].id_dst][neighborhood[j].id_dst].cost = INFINITE;
			dv_table_.distance[neighborhood[i].id_dst][neighborhood[j].id_dst].allocated = true;
			dv_table_.distance[neighborhood[i].id_dst][neighborhood[j].id_dst].id_neighbor = neighborhood[j].id_dst;
		}
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

	printf("%%%% INICIANDO ROTEADOR %d....... %%%%\n\n", LOCAL_ROUTER);

	//cria um vetor com os vizinhos do roteador
	load_neighbors();

	//inicia a estrutura vetor-distancia
	start_distance_vector();
	
	print_neighbors();
	print_dv_table();
	
	//inicia a operação do roteador
	start_router();
}
