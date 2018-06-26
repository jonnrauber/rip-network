#define MAX_ROUTERS 100
#define INFINITE 999

#define MSGLEN 100
#define BUFLEN sizeof(packet)

#define TYPE_DATA 1
#define TYPE_DV 2

#define TRIES_UNTIL_DESCONNECT 3
#define TIMEOUT 5

#define DV_RESEND_INTERVAL 10

typedef struct {
	int id_dst;
	char host_dst[16];
	int port_dst;
	int cost;
} neighbor; //id_src não é necessário pois será sempre LOCAL_ROUTER

typedef struct {
	int id_neighbor; 	//qual enlace de saída para chegar ao id_dst
	int cost;			//custo do caminho
	int prev_cost;		//usado para calcular a atualização dos custos
	bool allocated;		
} distance_vector;

typedef struct {
	int id_src;
	distance_vector dv[MAX_ROUTERS];
} dv_payload;

typedef struct {
	distance_vector distance[MAX_ROUTERS][MAX_ROUTERS];
} dv_table;

typedef struct {
	short type; //2B
	int id_src; //4B
	int id_dst; //4B
	unsigned int seq_number; //4B
	char data[100]; //100B
	dv_payload dv;
} packet;

packet create_pck(int id_src, int id_dst, int type, void* message);
void* dv_sender_func(void *data);
void load_neighbors();
void print_dv_table();
void print_neighbors();
void print_unique_dv(distance_vector* dv);
void* receiver_func(void *data);
int search_router_port(int id_router);
void send_message(packet* pck, neighbor* link);
void* sender_func(void *data);
void start_distance_vector();
void start_router();
void* update_dv(void* data);
bool update_dv_table(distance_vector* dv, int id_neighbor);
unsigned int whos_the_next(int id_dst);
void send_dv();
