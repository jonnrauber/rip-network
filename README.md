# T2 - Redes de Computadores
## Implementação de um simulador do RIP (Routing Information Protocol) usando o algoritmo Bellman-Ford distribuído para trocar as tabelas de roteamento dos nós da rede.

### Autores
- Jefferson A. Coppini (jeffecoppini1@gmail.com)
- Jonathan T. Rauber (jonathan.rauber@uffs.edu.br)

### Comandos de Execução
Para compilar/executar os roteadores um a um, pode-se usar o comando make abaixo:

	make id=#

onde # é o número do roteador que se quer instanciar.

Também disponibilizamos um shell script que automaticamente inicia 4 instâncias de roteadores.
Para "rodá-lo" basta digitar no terminal:

	./run.sh

### Funcionamento 
- O programa executa uma instância de um roteador. Este carrega as informações dos nós vizinhos,
descritas pelos arquivos routers.config e enlaces.config.

- Conhecidos os seus vizinhos, o roteador inicia a tabela de roteamento, 
preenchendo seu vetor distância com os custos dos enlaces.

- São criadas três threads a partir da thread principal.

- A primeira aguarda a entrada de um ID de roteador destino da entrada padrão. 
	- Ela é a encarregada por enviar as mensagens de dados para os outros roteadores;
	- O usuário deve entrar com o ID do roteador para o qual deseja enviar a mensagem;
	- Em seguida, se for um identificador válido, será apresentado na tela o texto "Escreva a 
mensagem (limite de 100 caracteres):", onde o usuário deve digitar o que deseja mandar para
o roteador de destino.

- A segunda é responsável por receber as mensagens enviadas pelos outros roteadores.
	- Se a mensagem recebida tiver outro destino, é encaminhada adiante;
	- Se o nó atual for o destino, exibe a mensagem na tela;
	- O pacote recebido também pode ser um vetor distância de um vizinho. Nesse caso, atualizará a sua tabela de roteamento com os novos dados.

- A terceira thread possui um daemon que manda o vetor distância do roteador para os vizinhos a cada período de tempo.
	- Também possui a função de detectar queda de enlaces/nós;
	- Caso os vizinhos recebam o vetor distância, está tudo certo e o daemon espera n segundos para reenviar.
	- Caso algum deles não responda após T tentativas, considera o enlace morto e atualiza a tabela de roteamento.
	- A informação irá se propagando e os custos para o nó tenderão ao infinito;
	- Limitamos a contagem ao infinito definindo um valor teto para esta contagem, que fará a tabela parar de ser atualizada.

### Enlaces da rede
Os enlaces podem ser consultados/alterados no arquivo "enlaces.config".

### Constantes definidas
As constantes à serem definidas/ajustadas para a execução do programa são:
- MAX_ROUTERS: número máximo de roteadores na rede;
- INFINITE: um valor estipulado para ser o infinito, que limita o incremento do custo no vetor distância;
- TRIES_UNTIL_DESCONNECT: número de tentativas que serão feitas ao transmitir um pacote a outro roteador;
- TIMEOUT: tempo de espera entre cada tentativa de transmissão;
- DV_RESEND_INTERVAL: tempo de espera do Daemon para reenviar os vetores distância aos vizinhos.

### Atributos do pacote
- type: assume os valores TYPE_DV (contém um vetor distância) ou TYPE_DATA (contém uma mensagem);
- id_src: ID do roteador remetente;
- id_dst: ID do roteador destinatário;
- seq_number: número de sequência da mensagem;
- data: conteúdo da mensagem, string limitada em 100 caracteres (consta se type = TYPE_DV);
- dv: vetor distância (consta se type = TYPE_DATA);
- jump: contador de saltos (evita problemas em loops ao haver queda de enlaces). Se em um certo momento jump for maior que a quantidade de roteadores na rede, a retransmissão é cancelada.
