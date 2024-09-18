#ifndef __FAT12_H__
#define __FAT12_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

//#define FILEBUFFER_SIZE 512
#define CHUNK_SIZE 512

#define BYTES_PER_SECTOR 512
#define FAT12_ENTRY_SIZE 32
#define FAT12_FILENAME_LENGTH 11

#define FILEBUFFER_SIZE  270920+100 // 264kB +100 bytes

// BIOS Parameter Block (BPB) for FAT12 structure to store disk layout
struct BPB {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_dir_entries;
    uint16_t total_sectors;
    uint16_t sectors_per_fat;
    uint32_t root_dir_sector;
    uint32_t root_dir_size;
    uint32_t data_start_sector; // New field to store the start of data region
};

// Structure to store file information
struct FILE_ENTRY {
    uint8_t index;          // File index
    char name[13];          // Filename in 8.3 format (8 chars + dot + 3 chars + null terminator)
    uint32_t size;          // File size
    uint32_t location;      // File location (starting cluster/sector)
};


void load_bpb(struct BPB *bpb, const char *buffer);
uint32_t get_file_location(const struct BPB *bpb, uint16_t starting_cluster);
//uint32_t get_file_location_in_sectors(const struct BPB *bpb, uint16_t starting_cluster);

uint8_t get_files(struct BPB *bpb, const char *buffer, struct FILE_ENTRY *files); // Return how many files
    
uint32_t get_file_size(struct BPB *bpb, const char *buffer, const char *filename_to_find);
void list_files(struct BPB *bpb, const char *buffer);
uint32_t get_next_cluster(const struct BPB *bpb, uint16_t current_cluster, const char *buffer);

int load_file_to_buffer(struct BPB *bpb, const char *buffer, const char *filename_to_find, char *fileBuffer, uint32_t buffer_size);

// int load_file_to_buffer(struct BPB *bpb, const char *buffer, const char *filename_to_find, char *fileBuffer, uint32_t buffer_size);

// In the microcontroller we will use a small 512 byte byffer to load chunks of file
// and send by HTML CHUNKED TRANSFER, that's why we don't need the above load_file_to_buffer function
// We don't load the entire file at once. Why waste memory??!!!
int load_file_chunk(struct BPB *bpb, const char *buffer, const struct FILE_ENTRY *file_entry,
                    char *fileBuffer, uint32_t buffer_size, 
                    uint32_t offset, uint32_t chunk_size, 
                    uint16_t *last_cluster, uint32_t *bytes_read_so_far);

#endif // FAT12_H
