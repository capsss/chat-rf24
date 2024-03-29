#include <Arduino.h>
#include <RF24.h>

#define TAMANHO_PAYLOAD 64

#define MINHA_PLACA 39

#define ENDERECO_BROADCAST 999

#if MINHA_PLACA == 39
  #define ENDERECO_ORIGEM MINHA_PLACA
  #define ENDERECO_DESTINO_1 1
  #define ENDERECO_DESTINO_2 1
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

#define TAMANHO_DA_LISTA_DE_PACOTES 20


//estrutura do pacote
typedef struct{
  uint8_t endereco_destino;
  uint8_t endereco_origem;
  uint8_t checksum;
  uint8_t id_mensagem;
  char ack;
  uint8_t tamanho_payload;
  char payload[26];
}Pacote;

//estrutura para lista de pacotes
typedef struct{
  Pacote pacote;
  unsigned long hora_do_envio;
  unsigned long hora_do_recebimento;
  bool esperando_resposta;
}PacoteEnviado;

PacoteEnviado lista_de_pacotes_enviados[TAMANHO_DA_LISTA_DE_PACOTES];
int posicao_na_lista_de_pacotes_enviados = 0;

RF24 radio(7,8);
const uint64_t pipes[2] = {0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL};
int cont_id = 0;

Pacote criarPacote(int ed, int eo, int id, char ack, char *payload, int tp){
  Pacote pacote;
  if(!(tp > TAMANHO_PAYLOAD)){
    int i=0;
    for(i=0; i<tp; i++){
      pacote.payload[i] = payload[i];
    }
    pacote.payload[i] = '\0';

    pacote.endereco_destino = ed;
    pacote.endereco_origem = eo;
    pacote.id_mensagem = id;
    pacote.ack = ack;

    int cont = 0;
    for(int i=0; i<TAMANHO_PAYLOAD; i++){
     if(payload[i+1] == '\0'){
      break;
     }
     cont++;
    }
    pacote.checksum = pacote.endereco_destino + pacote.endereco_origem + cont;
    pacote.tamanho_payload = cont;

  } else {
    pacote.id_mensagem = -1;
  }
  
  return pacote;
}

PacoteEnviado criarPacoteEnviado(Pacote pacote, unsigned long hora_do_envio){
  PacoteEnviado pacote_enviado;
  pacote_enviado.pacote = pacote;
  pacote_enviado.hora_do_envio = hora_do_envio;
  pacote_enviado.esperando_resposta = true;

  return pacote_enviado;
}

void adicionarPacoteNaLista(PacoteEnviado pacote){
  lista_de_pacotes_enviados[posicao_na_lista_de_pacotes_enviados++] = pacote;
  if(posicao_na_lista_de_pacotes_enviados == TAMANHO_DA_LISTA_DE_PACOTES){
    posicao_na_lista_de_pacotes_enviados = 0;
  }
}

void removerPacoteDaLista(Pacote pacote, unsigned long hora_do_recebimento){
  for(int i=0; i<TAMANHO_DA_LISTA_DE_PACOTES; i++){
    if(lista_de_pacotes_enviados[i].pacote.endereco_destino == pacote.endereco_origem){
      if(lista_de_pacotes_enviados[i].pacote.id_mensagem == pacote.id_mensagem){
        if(lista_de_pacotes_enviados[i].esperando_resposta){
          lista_de_pacotes_enviados[i].esperando_resposta = false;
          posicao_na_lista_de_pacotes_enviados--;
        }
      }
    }
  }
}









void setup() {
  Serial.begin(115200);
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_2MBPS);
  radio.setChannel(96);
  if(FUNCIONALIDADE){
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  }else{
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  radio.powerUp();
  radio.startListening();

  radio.printDetails();
}





void loop() {
  //rotina para reenvio de mensagens sem confirmacao de resposta
  int i=posicao_na_lista_de_pacotes_enviados;
  if( lista_de_pacotes_enviados[i].esperando_resposta == true && ((micros()-lista_de_pacotes_enviados[i].hora_do_recebimento)> 2000000) ){
    radio.write(&lista_de_pacotes_enviados[i].pacote, sizeof(lista_de_pacotes_enviados[i].pacote));
  }


  //enviar
  if(Serial.available()){
    String entrada_via_serial = Serial.readString();
    uint8_t endereco_destino;

    int i=0;
    #if MINHA_PLACA == 39
      i=3;
      char lixo[4];
      lixo[0] = entrada_via_serial[0];
      lixo[1] = entrada_via_serial[1];
      lixo[2] = '\0';

      endereco_destino = atoi(lixo);
    #endif

    char payload[64];
    for(i; i<64; i++){
      #if MINHA_PLACA == 39
        payload[i-3] = entrada_via_serial[i];
      #endif
      #if MINHA_PLACA != 39
        payload[i] = entrada_via_serial[i];
      #endif
    }
    payload[--i] = '\0';

    radio.stopListening();

    #if MINHA_PLACA == 39
    Pacote pacote = criarPacote(endereco_destino, ENDERECO_ORIGEM, cont_id++, 's', payload, 26);
    #endif

    #if MINHA_PLACA != 39
    Pacote pacote = criarPacote(ENDERECO_DESTINO_1, ENDERECO_ORIGEM, cont_id++, 's', payload, 26);
    #endif

    radio.write(&pacote, sizeof(Pacote));

    PacoteEnviado pacote_enviado = criarPacoteEnviado(pacote,micros());
    adicionarPacoteNaLista(pacote_enviado);

    radio.startListening();
  }



  //receber
  if(radio.available()){
    Pacote pacote2;
    while (radio.available()) {
      radio.read(&pacote2, sizeof(Pacote));
    }

    if(pacote2.ack == 'r'){
      removerPacoteDaLista(pacote2,micros());
    } else {
      if( (pacote2.endereco_destino + pacote2.endereco_origem + strlen(pacote2.payload) - 1) == pacote2.checksum) {
        if(pacote2.endereco_destino == ENDERECO_ORIGEM || pacote2.endereco_destino == ENDERECO_BROADCAST){
          char payload_lixo[1] = {'\0'};
          radio.stopListening();
          Pacote pacote_resposta = criarPacote(pacote2.endereco_origem, pacote2.endereco_destino, pacote2.id_mensagem, 'r', payload_lixo, 0);
          radio.write(&pacote_resposta, sizeof(Pacote));   
          radio.startListening();

          Serial.print("RECEBIDO: ");
          Serial.print(pacote2.payload);
          Serial.print("DA PLACA: ");
          Serial.print(pacote2.endereco_origem);
          Serial.println();
        }
      } else {
        // Serial.println("chegou mensagem para esse destino mas o checksum nao bateu");
      }
    }
  }

  delay(1000);
}