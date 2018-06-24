# T1 - Redes de Computadores
## Implementação de um programa de envio de mensagens entre roteadores com entrega confiável, sobre sockets UDP, com confirmação por ACK, timeout e retransmissão.

### Autores
- Jefferson A. Coppini (jeffecoppini1@gmail.com)
- Jonathan T. Rauber (jonathan.rauber@uffs.edu.br)

### Comandos de Execução
Comando para compilar/executar:

	make id=#

onde # é o número do roteador que se quer instanciar.

### Funcionamento 
O programa executa uma instância de um roteador. Este carrega as informações 
descritas pelos arquivos routers.config e enlaces.config para construir a topologia
da rede.

- O processo cria uma thread que espera receber mensagens.
	- Se a mensagem recebida tiver outro destino, é encaminhada adiante;
	- Se o nó atual for o destino, exibe a mensagem na tela e manda de volta um ACK.

- A thread principal espera da entrada padrão um identificador de um roteador destino, 
para o qual será enviada uma mensagem digitada.

- Quando o terminal apresentar a mensagem "Escolha o roteador de destino:", o usuário deve
entrar com o ID do roteador para o qual deseja enviar a mensagem.

- Em seguida, se for um identificador válido, será apresentado na tela o texto "Escreva a 
mensagem (limite de 100 caracteres):", onde o usuário deve digitar o que deseja mandar para
o roteador de destino.

- Após estes passos, o roteador local tentará enviar a mensagem através do caminho calculado,
esperará a confirmação e poderá retransmitir após o timeout em casos de insucesso.


### Tamanho dos pacotes
   ACK flag (1 byte)
 + ID roteador origem (4 bytes)
 + ID roteador destino (4 bytes)
 + Numero de sequência (#) do pacote (4 bytes)
 + Dados (100 bytes)
 ---------------------
 = Total: 113 bytes


### Entrega confiável
Além do recebimento ser confirmado pelo roteador destino, que envia de volta à origem
um ACK com o número de sequência do pacote, o programa faz a retransmissão do pacote
após um tempo definido pela constante TIMEOUT (em segundos). Implementa um Stop-And-Wait,
ou seja, percebendo a não confirmação de um pacote, retransmite-o até o recebimento
de um ACK.


### Caminho de entrega dos pacotes (Roteamento)
Os caminhos para o envio dos pacotes da origem até um destino são calculados em 
cada roteador, onde o mesmo guarda em uma tabela somente o nodo de saída para cada
destino. Por padrão o programa executa o algoritmo de Dijkstra, mas também há na 
função bellman_ford() a implementação do algoritmo de Bellman-Ford-Moore, ambos
para calcular os caminhos de custo mínimo.
