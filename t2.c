#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#define MAX_ROUTERS 100
#define INFINITE 300

typedef struct {
	int id_dst;
	int cost;
} link; //id_src não é necessário pois será sempre LOCAL_ROUTER

typedef struct {
	int cost;
	bool allocated;
	bool active;
} distance_vector;

typedef struct {
	distance_vector distance[MAX_ROUTERS][MAX_ROUTERS];
} dv_table;


unsigned int LOCAL_ROUTER;
unsigned int LOCAL_ROUTER_PORT;
link* neighborhood; 	//enlaces deste roteador
int qt_links;	 		//neighborhood.length()
dv_table dv_table_;		//tabela vetor-distância deste roteador

/**
*	Retorna o número da porta do roteador id_router.
*	Retorna 0 caso o id_router não esteja especificado no arquivo
*	roteador.config.
*/
int search_router_port() {
	int loop_router_id, loop_router_port;
	char loop_router_host[16];
	int port;

	//abre o arquivo com as informações do roteador
	FILE *f = fopen("roteador.config", "r");
	if (f == NULL) {
		printf("Erro ao abrir arquivo 'roteador.config'\n");
		exit(-2);
	}

	port = 0;
	while (1) {
		if (fscanf(f, "%d %d %s\n", 
			&loop_router_id, &loop_router_port, loop_router_host) != EOF) {
			if (loop_router_id == LOCAL_ROUTER) {
				port = loop_router_port;
				break;
			}
		} else {
			break;
		}
	}
	return port;
}

/**
	Cria o vetor de vizinhos do roteador local.
*/
void load_neighbors() {
	int router1, router2, cost;

	//abre o arquivo com os enlaces
	FILE *f = fopen("enlaces.config", "r");
	if (f == NULL) {
		printf("Erro ao abrir o arquivo 'enlaces.config'.\n");
		exit(-4);
	}

	qt_links = 0;
	while(1) {
		if (fscanf(f, "%d %d %d", &router1, &router2, &cost) != EOF) {
			if (router1 == LOCAL_ROUTER || router2 == LOCAL_ROUTER) {
				neighborhood = (link *)realloc(neighborhood, sizeof(link) * ++qt_links);
				neighborhood[qt_links-1].id_dst = (router1 == LOCAL_ROUTER) ? router2 : router1;
				neighborhood[qt_links-1].cost = cost;
			}
		} else {
			break;
		}
	}
}

void* receiver_func(void *data) {}
void* sender_func(void *data) {}
void* dv_sender_func(void *data) {}

/**
	Após as configurações e carregamentos iniciais, inicia as funcionalidades do roteador.
	Cria 3 threads, uma para receber, outra para enviar e outra para atualizar os vetores-distância
*/
void start_router() {
	pthread_t threads[3];
	pthread_create(&(threads[0]), NULL, receiver_func, NULL);
	pthread_create(&(threads[1]), NULL, sender_func, NULL);
	pthread_create(&(threads[2]), NULL, dv_sender_func, NULL);
	pthread_join(threads[2], NULL);
	pthread_join(threads[1], NULL);
	pthread_join(threads[0], NULL);
}

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
	for (i = 0; i < qt_links; i++) {
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].cost = neighborhood[i].cost;
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].active = true;
		dv_table_.distance[LOCAL_ROUTER][neighborhood[i].id_dst].allocated = true;
	}
}

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		printf("Atenção! Executar usando ./t2 id=ID_ROUTER.\n\n");
		exit(-1);
	}
	LOCAL_ROUTER = atoi(argv[1]);
	
	//procura a porta do roteador correspondente à ID instanciada
	LOCAL_ROUTER_PORT = search_router_port();
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