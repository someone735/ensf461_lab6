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
int pageLimit = 256;
int alreadyDefined = FALSE;


//needed structs
typedef struct{
    int validBit;
    int VPNBits;
    int PFNBits;
} Page;
typedef struct{
    Page* pageTable;
} Process;


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
            //Current PID: 0. Error: multiple calls to define in the same trace
            if (alreadyDefined == TRUE)
            {
                fprintf(output_file,"Current PID: %d. Error: multiple calls to define in the same trace\n",processID);
                fflush(output_file);
                return -1;
            }
            
            int OFFBits = atoi(tokens[1]);
            int PFNBits = atoi(tokens[2]);
            int VPNBits = atoi(tokens[3]);
            
            int length = 1 << (OFFBits + PFNBits);
            PhysMemory = (u_int32_t*)calloc(length, sizeof(u_int32_t));

            for (int i = 0; i < 4; i++)
            {
                processList[i].pageTable = (Page*)malloc(pageLimit*sizeof(Page));
                
                for (int x = 0; x < pageLimit; x++)
                {
                    processList[i].pageTable[x].validBit = FALSE;
                    processList[i].pageTable[x].VPNBits = i;
                    processList[i].pageTable[x].PFNBits = 0;
                    
                }
            }
            fprintf(output_file, "Current PID: %d. Memory instantiation complete. OFF bits: %d. PFN bits: %d. VPN bits: %d\n", processID, OFFBits, PFNBits, VPNBits);
            fflush(output_file);
            alreadyDefined = TRUE;
        } else if (strcmp(tokens[0], "ctxswitch") == 0)
        {
            int newProcessID = atoi(tokens[1]);

            if (newProcessID <0 || PROCESSLIMIT <= newProcessID)
            {
                //Current PID: 3. Invalid context switch to process 5
                fprintf(output_file, "Current PID: %d. Invalid context switch to process %d\n", processID, newProcessID);
                fflush(output_file);
                
            } else {
                processID = newProcessID;
                fprintf(output_file, "Current PID: %d. Switched execution context to process: %d\n", processID, newProcessID);
                fflush(output_file);
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