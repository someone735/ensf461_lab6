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

// Created variables
#define PROCESSLIMIT 4
int TLBEntries = 8;
u_int32_t* PhysMemory;
int pageLimit = 256;
int alreadyDefined = FALSE;
int r1 = 0, r2 = 0;  // Registers
int process_registers[PROCESSLIMIT][2];  // Each process has its own r1, r2


// Define variables to store bits from 'define' command
int OFFBits = 0;
int PFNBits = 0;
int VPNBits = 0;

// TLB entry structure
typedef struct {
    int valid;
    int vpn;
    int pfn;
    int pid;
    int timestamp;
    int time_used;
} TLBEntry;

// Page and Process structures
typedef struct {
    int validBit;
    int VPNBits;
    int PFNBits;
} Page;

typedef struct {
    Page* pageTable;
} Process;

// Process list, TLB, and global variables
Process processList[PROCESSLIMIT];
int processID = 0;
TLBEntry TLB[8];
int timestampCounter = 0; 
int least_recently_used;

// Function to tokenize input
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

// Function to find a TLB entry for a given VPN and PID
int find_TLB_entry(int vpn, int pid) {
    for (int i = 0; i < TLBEntries; i++) {
        if (TLB[i].valid && TLB[i].vpn == vpn && TLB[i].pid == pid) {
            TLB[i].time_used = timestampCounter;
            return i;
        }
    }
    return -1;
}

// Function to add a new entry to the TLB or update an existing one
void add_TLB_entry(int vpn, int pfn) {
    int existing_index = find_TLB_entry(vpn, processID);

    // Update the existing entry if it already exists
    if (existing_index != -1) {
        TLB[existing_index].pfn = pfn;
        TLB[existing_index].timestamp = timestampCounter;
        TLB[existing_index].time_used = timestampCounter;
        return;
    }

    // Otherwise, find a replacement entry
    int replace_index = -1;

    // Determine the TLB entry to replace based on strategy
    if (strcmp(strategy, "FIFO") == 0) {
        int oldest_timestamp = timestampCounter;
        for (int i = 0; i < TLBEntries; i++) {
            if (!TLB[i].valid) {
                replace_index = i;
                break;
            } else if (TLB[i].timestamp < oldest_timestamp) {
                replace_index = i;
                oldest_timestamp = TLB[i].timestamp;
            }
        }
    } else if (strcmp(strategy, "LRU") == 0) {
        for (int i = 0; i < TLBEntries; i++) {
            if (!TLB[i].valid) {
                replace_index = i;
                break;
            } else if (TLB[i].time_used < TLB[replace_index].time_used) {
                replace_index = i;
                // least_recently_used = TLB[i].timestamp;
            }
        }
    }

    // If all entries are valid, replace the entry determined by FIFO or LRU
    if (replace_index == -1) replace_index = 0; // default to the first entry if no replacement is found

    // Update the TLB entry at the replace_index
    TLB[replace_index].valid = TRUE;
    TLB[replace_index].vpn = vpn;
    TLB[replace_index].pfn = pfn;
    TLB[replace_index].pid = processID;
    //if (strcmp(strategy, "FIFO") == 0) {
    //     TLB[replace_index].timestamp = timestampCounter;
    // } else{
    //     TLB[replace_index].timestamp = least_recently_used;
    // } 
    TLB[replace_index].timestamp = timestampCounter;
    TLB[replace_index].time_used = timestampCounter;
}

// Function to translate a virtual address to a physical address
int translate_address(int virtual_address, int* physical_address) {
    // Calculate VPN and offset based on OFFBits
    int vpn = virtual_address >> OFFBits;
    int offset_mask = (1 << OFFBits) - 1;
    int offset = virtual_address & offset_mask;

    int tlb_index = find_TLB_entry(vpn, processID);
    if (tlb_index != -1) {
        // TLB hit
        *physical_address = (TLB[tlb_index].pfn << OFFBits) | offset;
        fprintf(output_file, "Current PID: %d. Translating. Lookup for VPN %d hit in TLB entry %d. PFN is %d\n", processID, vpn, tlb_index, TLB[tlb_index].pfn);
        fflush(output_file);
        return TRUE;
    } else {
        // TLB miss
        fprintf(output_file, "Current PID: %d. Translating. Lookup for VPN %d caused a TLB miss\n", processID, vpn);
        fflush(output_file);

        // Check if the VPN is mapped in the page table
        if (processList[processID].pageTable[vpn].validBit) {
            int pfn = processList[processID].pageTable[vpn].PFNBits;
            *physical_address = (pfn << OFFBits) | offset;
            fprintf(output_file, "Current PID: %d. Translating. Successfully mapped VPN %d to PFN %d\n", processID, vpn, pfn);
            fflush(output_file);

            // Add to TLB
            add_TLB_entry(vpn, pfn);
            return TRUE;
        } else {
            // Page is unmapped
            fprintf(output_file, "Current PID: %d. Translating. Translation for VPN %d not found in page table\n", processID, vpn);
            fflush(output_file);
            return FALSE;  // Indicate that translation failed due to unmapped memory
        }
    }
}

// Define function to initialize memory and page tables
void define_memory(int OFFBits_input, int PFNBits_input, int VPNBits_input) {
    if (alreadyDefined) {
        fprintf(output_file, "Current PID: %d. Error: multiple calls to define in the same trace\n", processID);
        fflush(output_file);
        exit(-1);
    }

    // Store the bits globally
    OFFBits = OFFBits_input;
    PFNBits = PFNBits_input;
    VPNBits = VPNBits_input;

    int length = 1 << (OFFBits + PFNBits);
    PhysMemory = (u_int32_t*)calloc(length, sizeof(u_int32_t));

    for (int i = 0; i < PROCESSLIMIT; i++) {
        processList[i].pageTable = (Page*)malloc(pageLimit * sizeof(Page));
        for (int x = 0; x < pageLimit; x++) {
            processList[i].pageTable[x].validBit = FALSE;
        }
    }
    fprintf(output_file, "Current PID: %d. Memory instantiation complete. OFF bits: %d. PFN bits: %d. VPN bits: %d\n", processID, OFFBits, PFNBits, VPNBits);
    fflush(output_file);
    alreadyDefined = TRUE;
}

// Map function for setting a VPN to PFN mapping
void map_page(int vpn, int pfn) {
    processList[processID].pageTable[vpn].validBit = TRUE;
    processList[processID].pageTable[vpn].PFNBits = pfn;
    add_TLB_entry(vpn, pfn);
    fprintf(output_file, "Current PID: %d. Mapped virtual page number %d to physical frame number %d\n", processID, vpn, pfn);
    fflush(output_file);
}

// Unmap function to remove a VPN mapping
void unmap_page(int vpn) {
    if (processList[processID].pageTable[vpn].validBit) {
        // Invalidate the page table entry
        processList[processID].pageTable[vpn].validBit = FALSE;
        processList[processID].pageTable[vpn].PFNBits = 0;

        // Invalidate the corresponding TLB entry
        for (int i = 0; i < TLBEntries; i++) {
            if (TLB[i].valid && TLB[i].vpn == vpn && TLB[i].pid == processID) {
                TLB[i].valid = FALSE;
                TLB[i].pfn = 0;
                break;
            }
        }

        fprintf(output_file, "Current PID: %d. Unmapped virtual page number %d\n", processID, vpn);
        fflush(output_file);
    }
}

// Context switch function to change process context
void context_switch(int newProcessID) {
    if (newProcessID < 0 || newProcessID >= PROCESSLIMIT) {
        fprintf(output_file, "Current PID: %d. Invalid context switch to process %d\n", processID, newProcessID);
        fflush(output_file);
        exit(-1);
    }

    processID = newProcessID;

    // Output context switch
    fprintf(output_file, "Current PID: %d. Switched execution context to process: %d\n", processID, newProcessID);
    fflush(output_file);
}

void load(char* dst, char* src) {
    int value;
    if (src[0] == '#') {  // Immediate value
        value = atoi(src + 1);
        if (strcmp(dst, "r1") == 0) process_registers[processID][0] = value;
        else if (strcmp(dst, "r2") == 0) process_registers[processID][1] = value;
        else {
            fprintf(output_file, "Current PID: %d. Error: invalid register operand %s\n", processID, dst);
            exit(-1);
        }
        fprintf(output_file, "Current PID: %d. Loaded immediate %d into register %s\n", processID, value, dst);
    } else {  // Memory address
        int virtual_address = atoi(src);
        int physical_address;
        if (translate_address(virtual_address, &physical_address)) {
            value = PhysMemory[physical_address];
            if (strcmp(dst, "r1") == 0) process_registers[processID][0] = value;
            else if (strcmp(dst, "r2") == 0) process_registers[processID][1] = value;
            else {
                fprintf(output_file, "Current PID: %d. Error: invalid register operand %s\n", processID, dst);
                exit(-1);
            }
            fprintf(output_file, "Current PID: %d. Loaded value of location %d (%d) into register %s\n", processID, virtual_address, value, dst);
        } else {
            fflush(output_file);
            exit(-1);  // Terminate due to unmapped memory access
        }
    }
    fflush(output_file);
}

void store(char* dst, char* src) {
    int value;
    if (src[0] == 'r') {  // Register source
        if (strcmp(src, "r1") == 0) value = process_registers[processID][0];
        else if (strcmp(src, "r2") == 0) value = process_registers[processID][1];
        else {
            fprintf(output_file, "Error: invalid register operand %s\n", src);
            exit(-1);
        }
    } else if (src[0] == '#') {  // Immediate value
        value = atoi(src + 1);
    } else {
        fprintf(output_file, "Error: invalid source operand %s\n", src);
        exit(-1);
    }

    int virtual_address = atoi(dst);
    int physical_address;
    if (translate_address(virtual_address, &physical_address)) {
        PhysMemory[physical_address] = value;
        if (src[0] == 'r') {
            fprintf(output_file, "Current PID: %d. Stored value of register %s (%d) into location %d\n", processID, src, value, virtual_address);
        } else {
            fprintf(output_file, "Current PID: %d. Stored immediate %d into location %d\n", processID, value, virtual_address);
        }
        fflush(output_file);
    }
}

void add() {
    int result = process_registers[processID][0] + process_registers[processID][1];
    fprintf(output_file, "Current PID: %d. Added contents of registers r1 (%d) and r2 (%d). Result: %d\n", processID, process_registers[processID][0], process_registers[processID][1], result);
    process_registers[processID][0] = result;
    fflush(output_file);
}

void pinspect(int vpn){
    //Page* currentProcessPageTable = processList[processID].pageTable;
    //for (int i = 0; i < PROCESSLIMIT; i++){
        //if (processList[i].pageTable->VPNBits == vpn){
            //Current PID: 0. Inspected page table entry 0. Physical frame number: 31. Valid: 1
    fprintf(output_file, "Current PID: %d. Inspected page table entry %d. Physical frame number: %d. Valid: %d\n", processID, vpn, processList[processID].pageTable[vpn].PFNBits, processList[processID].pageTable[vpn].validBit);
    fflush(output_file);
            //break;
        //}
    //}
}
//     int existingTLBEntry = FALSE;
        //     for (int i = 0; i < TLBSize; i++){
        //         if (TLBEntries[i].valid && TLBEntries[i].VPN == vpn){
        //             TLBEntries[i].PFN = pfn;
        //             existingTLBEntry = TRUE;
        //             break;
        //         }
        //     }
        //     if (!existingTLBEntry){
        //         for (int i = 0; i < TLBSize; i++){
        //             if (!TLBEntries[i].valid){
        //                 TLBEntries[i].PFN = pfn;
        //                 TLBEntries[i].VPN = vpn;
        //                 TLBEntries[i].valid = TRUE;
        //                 break;
        //             }

void tinspect(int tlbEntry){
    if (strcmp(strategy, "FIFO") == 0){
        fprintf(output_file, "Current PID: %d. Inspected TLB entry %d. VPN: %d. PFN: %d. Valid: %d. PID: %d. Timestamp: %d\n", processID, tlbEntry, TLB[tlbEntry].vpn, TLB[tlbEntry].pfn, TLB[tlbEntry].valid, TLB[tlbEntry].pid, TLB[tlbEntry].timestamp);
        fflush(output_file);  
    } else if (strcmp(strategy, "LRU") == 0){
        fprintf(output_file, "Current PID: %d. Inspected TLB entry %d. VPN: %d. PFN: %d. Valid: %d. PID: %d. Timestamp: %d\n", processID, tlbEntry, TLB[tlbEntry].vpn, TLB[tlbEntry].pfn, TLB[tlbEntry].valid, TLB[tlbEntry].pid, TLB[tlbEntry].time_used);
        fflush(output_file); 
    }
    //for (int i = 0; i < TLBEntries; i++){
        //if (TLB[i].vpn == tlbEntry){
            //Current PID: 2. Inspected TLB entry 0. VPN: 0. PFN: 10. Valid: 1. PID: 2. Timestamp: 3
            //fprintf(output_file, "Current PID: %d. Inspected TLB Entry %d. VPN: %d. PFN: %d. Valid: %d. PID: %d. Timestamp: %d\n", processID, tlbEntry, TLB[i].vpn, TLB[i].pfn, TLB[i].valid, TLB[i].pid, TLB[i].timestamp);
            //fflush(output_file);
            //break;
        //}
    //}
}

void linspect(int physicalLocation){
    //Current PID: 1. Inspected physical location 64. Value: 0
    fprintf(output_file, "Current PID: %d. Inspected physical location %d. Value: %d\n", processID, physicalLocation, PhysMemory[physicalLocation]);
    fflush(output_file);
}

void rinspect(char* reg){
    //Current PID: 2. Inspected register r1. Content: 0
    if (strcmp(reg, "r1") == 0){
        fprintf(output_file, "Current PID: %d. Inspected register r1. Content: %d\n", processID, process_registers[processID][0]);
        fflush(output_file);
        //exit(-1);
    }
    else if (strcmp(reg, "r2") == 0){
        fprintf(output_file, "Current PID: %d. Inspected register r2. Content: %d\n", processID, process_registers[processID][1]);
        fflush(output_file);
        //exit(-1);
    }
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
    if (!input_file) {
        perror("Error opening input file");
        return 1;
    }
    output_file = fopen(output_trace, "w");
    if (!output_file) {
        perror("Error opening output file");
        fclose(input_file);
        return 1;
    }

    while (fgets(buffer, sizeof(buffer), input_file)) {
        // Remove newline character
        timestampCounter++;
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }

        if (buffer[0] == '%'){
            timestampCounter--;
            continue;
        } // Skip comments

        char** tokens = tokenize_input(buffer);

        if (strcmp(tokens[0], "define") == 0) {
            // Process the define command without checking 'alreadyDefined'
            if (tokens[1] && tokens[2] && tokens[3]) {
                define_memory(atoi(tokens[1]), atoi(tokens[2]), atoi(tokens[3]));
            } else {
                fprintf(output_file, "Current PID: %d. Error: invalid define instruction format\n", processID);
                fflush(output_file);
                exit(-1);
            }
        } else {
            // Check if 'define_memory' was called before executing any other instruction
            if (!alreadyDefined) {
                fprintf(output_file, "Current PID: %d. Error: attempt to execute instruction before define\n", processID);
                fflush(output_file);
                exit(-1);
            }

            // Process other instructions
            if (strcmp(tokens[0], "map") == 0) {
                if (tokens[1] && tokens[2]) {
                    map_page(atoi(tokens[1]), atoi(tokens[2]));
                } else {
                    fprintf(output_file, "Current PID: %d. Error: invalid map instruction format\n", processID);
                    fflush(output_file);
                    exit(-1);
                }
            } else if (strcmp(tokens[0], "unmap") == 0) {
                if (tokens[1]) {
                    unmap_page(atoi(tokens[1]));
                } else {
                    fprintf(output_file, "Current PID: %d. Error: invalid unmap instruction format\n", processID);
                    fflush(output_file);
                    exit(-1);
                }
            } else if (strcmp(tokens[0], "ctxswitch") == 0) {
                if (tokens[1]) {
                    context_switch(atoi(tokens[1]));
                } else {
                    fprintf(output_file, "Current PID: %d. Error: invalid ctxswitch instruction format\n", processID);
                    fflush(output_file);
                    exit(-1);
                }
            } else if (strcmp(tokens[0], "load") == 0) {
                if (tokens[1] && tokens[2]) {
                    load(tokens[1], tokens[2]);
                } else {
                    fprintf(output_file, "Current PID: %d. Error: invalid load instruction format\n", processID);
                    fflush(output_file);
                    exit(-1);
                }
            } else if (strcmp(tokens[0], "store") == 0) {
                if (tokens[1] && tokens[2]) {
                    store(tokens[1], tokens[2]);
                } else {
                    fprintf(output_file, "Current PID: %d. Error: invalid store instruction format\n", processID);
                    fflush(output_file);
                    exit(-1);
                }
            } else if (strcmp(tokens[0], "add") == 0) {
                add();
            } else if (strcmp(tokens[0], "pinspect") == 0){
                pinspect(atoi(tokens[1]));
            } else if (strcmp(tokens[0], "tinspect") == 0){
                tinspect(atoi(tokens[1]));
            } else if (strcmp(tokens[0], "linspect") == 0){
                linspect(atoi(tokens[1]));
            } else if (strcmp(tokens[0], "rinspect") == 0){
                rinspect(tokens[1]);
            }
            
            else {
                fprintf(output_file, "Current PID: %d. Error: Unknown instruction %s\n", processID, tokens[0]);
                fflush(output_file);
                exit(-1);
            }
            
        }
        //timestampCounter++;
        // Deallocate tokens
        for (int i = 0; tokens[i] != NULL; i++) free(tokens[i]);
        free(tokens);
    }


    fclose(input_file);
    fclose(output_file);
    return 0;
}
