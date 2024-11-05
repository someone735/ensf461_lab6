#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define TRUE 1
#define FALSE 0

// Output file
FILE* output_file;

// TLB replacement strategy (FIFO or LRU)
char* strategy;

//created variables
#define PROCESSLIMIT 4
int TLBEntries = 8;
u_int32_t* PhysMemory;


//needed structs
typedef struct{
    Page* pageTable;
} Process;

typedef struct{
    int validBit;
    int VPNBits;
    int PFNBits;
} Page;

Process processList[PROCESSLIMIT];
int processID = 0;

char** tokenize_input(char* input) {
    char** tokens = NULL;
    char* token = strtok(input, " ");
    int num_tokens = 0;

    while (token != NULL) {
        num_tokens++;
        tokens = realloc(tokens, num_tokens * sizeof(char*));
        tokens[num_tokens - 1] = malloc(strlen(token) + 1);
        strcpy(tokens[num_tokens - 1], token);
        token = strtok(NULL, " ");
    }

    num_tokens++;
    tokens = realloc(tokens, num_tokens * sizeof(char*));
    tokens[num_tokens - 1] = NULL;

    return tokens;
}

int main(int argc, char* argv[]) {
    const char usage[] = "Usage: memsym.out <strategy> <input trace> <output trace>\n";
    char* input_trace;
    char* output_trace;
    char buffer[1024];

    // Parse command line arguments
    if (argc != 4) {
        printf("%s", usage);
        return 1;
    }
    strategy = argv[1];
    input_trace = argv[2];
    output_trace = argv[3];

    // Open input and output files
    FILE* input_file = fopen(input_trace, "r");
    output_file = fopen(output_trace, "w");  

    while ( !feof(input_file) ) {
        // Read input file line by line
        char *rez = fgets(buffer, sizeof(buffer), input_file);
        if ( !rez ) {
            fprintf(stderr, "Reached end of trace. Exiting...\n");
            return -1;
        } else {
            // Remove endline character
            buffer[strlen(buffer) - 1] = '\0';
        }
        char** tokens = tokenize_input(buffer);

        // TODO: Implement your memory simulator
        if(strcmp(tokens[0], "%") == 0){
            continue;
        } else if (strcmp(tokens[0], "define") == 0){
            //Memory instantiation complete. OFF bits: <OFF>. PFN bits: <PFN>. VPN bits: <VPN>
            int OFFBits = atoi(tokens[1]);
            int PFNBits = atoi(tokens[2]);
            int VPNBits = atoi(tokens[3]);
            
            int length = 1 << (OFFBits + PFNBits);
            PhysMemory = (u_int32_t*)calloc(length, sizeof(u_int32_t));

            for (int i = 0; i < 4; i++)
            {
                
            }
            
        }        
        
        
        // Deallocate tokens
        for (int i = 0; tokens[i] != NULL; i++)
            free(tokens[i]);
        free(tokens);
    }

    // Close input and output files
    fclose(input_file);
    fclose(output_file);

    return 0;
}