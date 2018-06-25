#define MAX_ROUTERS 100
#define INFINITE 300

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
	int id_neighbor;
	int cost;
	bool allocated;
	bool active;
} distance_vector;

typedef struct {
	distance_vector distance[MAX_ROUTERS][MAX_ROUTERS];
} dv_table;

typedef struct {
	short type; //2B
	int id_src; //4B
	int id_dst; //4B
	unsigned int seq_number; //4B
	char data[100]; //100B
	distance_vector dv[MAX_ROUTERS];
} packet;
