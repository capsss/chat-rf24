#include <Arduino.h>
#include <RF24.h>

// 1 - FEITO - implementar checksum
// 1,5 - FAZER - enquanto nao receber resposta, enviar de novo
// 2 - FAZER - particao de mensagem
// 3 - FAZER - cliente servidor

#define TAMANHO_PAYLOAD 64

#define MINHA_PLACA 39

#define ENDERECO_BROADCAST 999

#if MINHA_PLACA == 39
#define ENDERECO_ORIGEM MINHA_PLACA
#define ENDERECO_DESTINO_1 1
#define ENDERECO_DESTINO_2 13
#define ENDERECO_DESTINO_3 6
#define FUNCIONALIDADE 0
#endif

#if MINHA_PLACA == 6
#define ENDERECO_ORIGEM MINHA_PLACA
#define ENDERECO_DESTINO_1 39
#define FUNCIONALIDADE 1
#endif

#if MINHA_PLACA == 1
#define ENDERECO_ORIGEM MINHA_PLACA
#define ENDERECO_DESTINO_1 39
#define FUNCIONALIDADE 1
#endif

#if MINHA_PLACA == 13
#define ENDERECO_ORIGEM MINHA_PLACA
#define ENDERECO_DESTINO_1 39
#define FUNCIONALIDADE 1
#endif



#define TAMANHO_DA_LISTA_DE_PACOTES 10

typedef struct { 
  uint8_t endereco_destino;
  uint8_t endereco_origem;
  uint8_t checksum;
  uint8_t id_mensagem;
  // uint8_t numero_da_mensagem_atual; //soh se precisar usar
  // uint8_t numero_de_mensagens;      //soh se precisar usar
  char ack;
  uint8_t tamanho_payload;
  char payload[26];
} Pacote;

typedef struct{
  Pacote pacote;
  unsigned long hora_do_envio;
  unsigned long hora_do_recebimento;
  bool respondido;
} PacoteEnviado;

PacoteEnviado lista_de_pacotes_enviados[TAMANHO_DA_LISTA_DE_PACOTES];
int posicao_na_lista_de_pacotes_enviados = 0;

RF24 radio(7, 8);
const uint64_t pipes[2] = {0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL};
int cont_id = 0;

Pacote criarPacote(int ed, int eo, int id, char ack, char *payload, int tp){
  Pacote pacote;
  if (!(tp > TAMANHO_PAYLOAD)){
    int i = 0;
    for (i = 0; i < tp; i++){
      pacote.payload[i] = payload[i];
    }
    pacote.payload[i] = '\0';

    pacote.endereco_destino = ed;
    pacote.endereco_origem = eo;
    pacote.ack = ack;
    pacote.id_mensagem = id;
    

    int cont = 0;
    for(int i=0; i<TAMANHO_PAYLOAD; i++){
     if(payload[i+1] == '\0'){
      break;
     }
     cont++;
    }
    
    pacote.checksum = pacote.endereco_destino + pacote.endereco_origem + cont;
    pacote.tamanho_payload = cont;
    
  } else{
    pacote.id_mensagem = -1;
  }

  return pacote;
}

PacoteEnviado criarPacoteEnviado(Pacote pacote, unsigned long hora_do_envio){
  PacoteEnviado pacote_enviado;
  pacote_enviado.pacote = pacote;
  pacote_enviado.hora_do_envio = hora_do_envio;
  pacote_enviado.respondido = false;

  return pacote_enviado;
}

void adicionarPacoteNaLista(PacoteEnviado pacote){
  lista_de_pacotes_enviados[posicao_na_lista_de_pacotes_enviados++] = pacote;
  if (posicao_na_lista_de_pacotes_enviados == TAMANHO_DA_LISTA_DE_PACOTES){
    posicao_na_lista_de_pacotes_enviados = 0;
  }
}

void removerPacoteDaLista(Pacote pacote, unsigned long hora_do_recebimento){
  for (int i = 0; i < TAMANHO_DA_LISTA_DE_PACOTES; i++){
    if (lista_de_pacotes_enviados[i].pacote.endereco_destino == pacote.endereco_origem){
      if (lista_de_pacotes_enviados[i].pacote.id_mensagem == pacote.id_mensagem){
        if (!lista_de_pacotes_enviados[i].respondido){
          lista_de_pacotes_enviados[i].respondido = true;
          lista_de_pacotes_enviados[i].hora_do_recebimento = hora_do_recebimento;
          Serial.print("confirmacao em: ");
          Serial.print(lista_de_pacotes_enviados[i].hora_do_recebimento - lista_de_pacotes_enviados[i].hora_do_envio);
          Serial.println(" milisegundos");
        }
      }
    }
  }
}

void setup(){
  Serial.begin(115200);
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_2MBPS);
  radio.setChannel(0x34);
  // radio.setChannel(110);
  if (FUNCIONALIDADE){
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1, pipes[0]);
  } else {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);
  }
  radio.powerUp();
  radio.startListening();

  radio.printDetails();
}

void loop(){
  //enviar
  if (Serial.available()){
    String entrada_via_serial = Serial.readString();

    char payload[64];
    int i;
    for (i = 0; i < 64; i++){
      payload[i] = entrada_via_serial[i];
    }
    payload[--i] = '\0'; //uma posicao pra tras para ignora o enter "\n" da serial

    radio.stopListening();
    Pacote pacote = criarPacote(ENDERECO_DESTINO_1, ENDERECO_ORIGEM, cont_id++, 's', payload, 26);
    while(true){
      if(radio.write(&pacote, sizeof(Pacote))){
        PacoteEnviado pacote_enviado = criarPacoteEnviado(pacote, micros());
        adicionarPacoteNaLista(pacote_enviado);
        break;
      } else {
        Serial.println("pacote nao enviado");
      }
    }

    radio.startListening();
  }

  //receber
  if (radio.available()){
    Pacote pacote2;
    while (radio.available()){
      radio.read(&pacote2, sizeof(Pacote));
    }

    if (pacote2.ack == 'r'){
      removerPacoteDaLista(pacote2, micros());
    } else {
      if (pacote2.endereco_destino == ENDERECO_ORIGEM || pacote2.endereco_destino == ENDERECO_BROADCAST){
        if( (pacote2.endereco_destino + pacote2.endereco_origem + strlen(pacote2.payload) - 1) == pacote2.checksum) {
          char payload_lixo[1] = {'\0'};
          radio.stopListening();
          Pacote pacote_resposta = criarPacote(pacote2.endereco_origem, pacote2.endereco_destino, pacote2.id_mensagem, 'r', payload_lixo, 0);
          
          radio.write(&pacote_resposta, sizeof(Pacote));
          radio.startListening();

          Serial.println("recebido: "); Serial.print(pacote2.payload);
        }
      } else {
        Serial.println("chegou mensagem para esse destino mas o checksum nao bateu");
      }
    }
  }

  delay(1000);
}