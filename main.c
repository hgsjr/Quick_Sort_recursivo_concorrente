#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "timer.h"

int * vetor; //vetor que será ordenado

int tam; //tamanho do vetor que será ordenado

int nthreads; //número de threads escolhido pelo usuário

pthread_mutex_t lock; //variável especial para sincronização de exclusão mútua

int threadsOciosas = 0; //número de threads ociosas no sistema

/*
 * Struct criada com intuito de armazenar os argumentos que serão
 * passados para as threads durante a execução do programa. Diante
 * da lógica de execução escolhida (tentar deixar o mínimo de threads
 * de modo ocioso possível) foi necessário criar uma fila onde serão
 * depositados os argumentos que ficarão "esperando" alguma thread estar
 * disponível para utilizá-los.
 */
typedef struct Arg {

    int esq, dir; //variáveis auxiliadoras do Quick Sort
    struct Arg * proximo; //próximo elemento da lista encadeada (fila de argumentos das threads)
} ArgFila;

ArgFila * primeiroDaFila = NULL;
ArgFila * ultimoDaFila = NULL;

/*
 * Função que adiciona um elemento na fila encadeada.
 * o antigo último elemento da fila passa a ter seu campo
 * "proximo" apontando para o novo elemento e o ponteiro
 * para o último elemento da lista passa a apontar para
 * o novo elemento adicionado. Obrigado, professora, pois
 * havia um erro aqui antes!
 */
void addFila(ArgFila * arg) {

    if(ultimoDaFila == NULL || primeiroDaFila == NULL) {

        primeiroDaFila = arg;
    }
    else {

        ultimoDaFila -> proximo = arg;
    }
    ultimoDaFila = arg;
}

/*
 * Função que retira o primeiro elemento da fila.
 * essa função certifica-se de que o primeiro
 * elemento da fila seja atualizado de forma devida
 * antes de retornar o elemento que está saindo.
 * Se não houver ninguém na fila retorna NULL.
 */
ArgFila * retiraDaFila() {

    ArgFila * arg = primeiroDaFila;
    if(arg == NULL)
        return NULL;
    primeiroDaFila = primeiroDaFila -> proximo;
    return arg;
}

/*
 * Função que cria um argumento novo, alocando
 * memória para esse e inicializando os atributos
 * do mesmo, como um "construtor". No fim, retorna
 * um ponteiro para ele.
 */
ArgFila * criaArg(int esq, int dir) {

    ArgFila * arg = (ArgFila *) malloc(sizeof(ArgFila));
    //inicializa os atributos do argumento
    arg -> esq = esq;
    arg -> dir = dir;
    arg -> proximo = NULL;

    return arg;
}

//função que troca a posição dos elementos do vetor
void troca(int *a, int *b) {

    int t = *a;
    *a = *b;
    *b = t;
}

//função que particiona o vetor com base no pivô
int particiona(int *vet, int esq, int dir) {

    //seleciona o elemento que será o pivô
    int pivo = vet[dir];
    int i = (esq - 1);

    //coloca os elementos menores que o pivô à sua esquerda
    //e os elementos maiores que o pivô à direita do mesmo
    for(int j = esq; j < dir; j++) {

        if (vet[j] <= pivo) {

            i++;
            troca(&vet[i], &vet[j]);
        }
    }

    troca(&vet[i + 1], &vet[dir]);
    return (i + 1);
}

/*
 * algoritmo quick sort implementado para rodar de forma concorrente
 */
void * quickSort() {

    int status = 0; //variável que controla internamente se a thread está ociosa ou não

    while(threadsOciosas < nthreads) { //enquanto houver alguma thread executando, pode ser que essa volte a executar também

        pthread_mutex_lock(&lock); //entra numa seção crítica

        ArgFila * arg = retiraDaFila(); //retira da fila um argumento que estava na espera (ou vê que não tinha nenhum)

        if(arg == NULL) { //se a fila estava vazia

            if(status != 1) { //se não estava ociosa antes, passa a ficar (obrigado, professora!)

                status = 1; //atualiza o status interno da thread para ociosa
                threadsOciosas++; //sinaliza globalmente que esta thread ficou ociosa
            }
        }

        pthread_mutex_unlock(&lock); //sai da seção crítica

        //foi utilizado "if", pois "else" não seria possível uma vez que acima estamos dando "unlock", o que quebra o bloco if-else
        if(arg != NULL) { //se havia elemento na fila, trabalhamos com ele

            if(status == 1) { //se estava ociosa, atualiza o controle interno e o controle global

                pthread_mutex_lock(&lock); //entra numa seção crítica
                status = 0; //volta para o estado ativo
                threadsOciosas--; //atualiza o número de threads ociosas
                pthread_mutex_unlock(&lock); //sai da seção crítica
            }

            int esq = arg -> esq;
            int dir = arg -> dir;

            free(arg); //libera o espaço de memória ocupado pelo argumento que não será mais utilizado

            if (esq < dir) {

                //seleciona a posição do pivô e coloca todos os elementos
                //menores que o pivô à sua esquerda e maior à sua direita
                int pivo = particiona(vetor, esq, dir);

                //cria os novos argumentos que serão colocados na fila para esperar a execução
                ArgFila * arg2 = criaArg(esq, pivo - 1);
                if(arg2 == NULL) {

                    fprintf(stderr, "Erro ao alocar memória para arg.\n");
                }
                ArgFila * arg3 = criaArg(pivo + 1, dir);
                if(arg3 == NULL) {

                    fprintf(stderr, "Erro ao alocar memória para arg.\n");
                }

                pthread_mutex_lock(&lock); //entra numa seção crítica

                //adiciona na fila os argumentos para a execução que ordenará os elementos que estão à esquerda do pivô
                addFila(arg2);

                //adiciona na fila os argumentos para a execução que ordenará os elementos que estão à direita do pivô
                addFila(arg3);

                pthread_mutex_unlock(&lock); //sai da seção crítica
            }
        }

    }

    //como todas as threads ficaram ociosas, significa que a execução do algoritmo quick sort terminou
    pthread_exit(NULL);
}

//declarações necessárias de funções:
int * geraVetor(); //função que gera o vetor que será ordenado
void exibeVetor(); //função que exibe o vetor

int main(int argc, char *argv[]) {

    //bloco que valida os parâmetros de entrada (tamanho do vetor que será ordenado, número de threads)
    if(argc < 3) {

        fprintf(stderr, "Digite: %s <número de elementos do vetor> <número de threads>", argv[0]);
        return 1;
    }
    tam = atoi(argv[1]);
    if(tam <= 0) {

        fprintf(stderr, "Digite um número positivo e diferente de zero para o tamanho do vetor.");
        return 1;
    }
    nthreads = atoi(argv[2]);
    if(nthreads <= 0) {

        fprintf(stderr, "Digite um número positivo e diferente de zero para o número de threads.");
        return 1;
    }

    double ini, fim; //variáveis de tomada de tempo

    //gera o vetor que será ordenado
    vetor = geraVetor();
    if(vetor == NULL) {

        fprintf(stderr, "Erro ao alocar memória para o vetor.\n");
        return 2;
    }

    //exibe o vetor antes da ordenação
    printf("Vetor gerado:\n");
    exibeVetor();

    GET_TIME(ini);
    //realiza a ordenação pelo algoritmo Quick Sort
    pthread_t tid[nthreads]; //vetor de identificadores das threads

    addFila(criaArg(0, tam - 1));

    if(primeiroDaFila == NULL) {

        fprintf(stderr, "Erro ao alocar memória para arg.\n");
        return 3;
    }

    pthread_mutex_init(&lock, NULL); //inicializa a variável de exclusão mútua

    for(int thread = 0; thread < nthreads; thread++) { //cria o número de threads que o usuário informou

        if(pthread_create(&tid[thread], NULL, quickSort, NULL)) {

            fprintf(stderr, "Erro ao criar uma thread.\n");
            return 4;
        }
    }

    for(int thread = 0; thread <nthreads; thread++) { //espera as threads criadas terminarem

        if(pthread_join(tid[thread], NULL)) {

            fprintf(stderr, "Erro ao executar pthread_join().\n");
            return 5;
        }
    }

    GET_TIME(fim);

    //exibe o vetor após a ordenação
    printf("Vetor ordenado:\n");
    exibeVetor();

    //tempo de execução
    printf("Tempo de execução: %lf\n", fim - ini);

    return 0;
}

/*
 * Função que aloca memória para o vetor que será ordenado e preenche
 * o mesmo com valores pseudo aleatórios pelo uso da rand(). Caso haja
 * erro na alocação de memória é retornado o valor NULL e o erro será
 * tratado na função main.
 */
int * geraVetor() {

    int * vet = (int *) malloc(sizeof(int) * tam);

    for(int i = 0; i < tam; i++)

        vet[i] = rand() % 100;

    return vet;
}

/*
 * Função que exibe o vetor.
 */
void exibeVetor() {

    printf("[");
    for(int i = 0; i < tam; i++) {

        printf("%d ", vetor[i]);
    }
    printf("\b]\n\n");
}
