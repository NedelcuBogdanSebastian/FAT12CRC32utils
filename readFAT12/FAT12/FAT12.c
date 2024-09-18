
#include <stdint.h>
#include "FAT12.h"


extern uint8_t NO_OF_FILES;
extern struct FILE_ENTRY FILES[20];  // Array to store up to 20 files
extern uint16_t last_cluster;
extern uint32_t bytes_read_so_far;
    

// Function to read 16-bit values (little endian)
uint16_t read16(const uint8_t *buf, uint16_t offset) {
    uint32_t result = 0;
    result |= (buf[offset + 1] << 8);
    result |=  buf[offset];
    return result;    
}


// Function to read 32-bit values (little endian)
uint32_t read32(const uint8_t *buf, uint16_t offset) {
    uint32_t result = 0;
    result |= (buf[offset + 3] << 24);
    result |= (buf[offset + 2] << 16);
    result |= (buf[offset + 1] << 8);
    result |=  buf[offset];
    return result;
}


// Function to load BIOS Parameter Block from FAT12
void load_bpb(struct BPB *bpb, const char *buffer) {
    bpb->bytes_per_sector = read16((uint8_t*)buffer, 11);
    bpb->sectors_per_cluster = buffer[13];
    bpb->reserved_sectors = read16((uint8_t*)buffer, 14);
    bpb->num_fats = buffer[16];
    bpb->root_dir_entries = read16((uint8_t*)buffer, 17);
    bpb->total_sectors = read16((uint8_t*)buffer, 19);
    bpb->sectors_per_fat = read16((uint8_t*)buffer, 22);

    // Calculate root directory sector and size
    bpb->root_dir_sector = bpb->reserved_sectors + (bpb->num_fats * bpb->sectors_per_fat);
    bpb->root_dir_size = (bpb->root_dir_entries * FAT12_ENTRY_SIZE + bpb->bytes_per_sector - 1) / bpb->bytes_per_sector;

    // Calculate start of data region
    bpb->data_start_sector = bpb->root_dir_sector + bpb->root_dir_size;    
    
    printf("\n        FAT12 data\n");
    printf("=========================\n");
    printf("Bytes_per_sector: %d\n", bpb->bytes_per_sector);
    printf("Sectors_per_cluster: %d\n", bpb->sectors_per_cluster);
    printf("Reserved_sectors: %d\n", bpb->reserved_sectors);
    printf("Num_fats: %d\n", bpb->num_fats);
    printf("Root_dir_entries: %d\n", bpb->root_dir_entries);
    printf("Total_sectors: %d\n", bpb->total_sectors);
    printf("Sectors_per_fat: %d\n", bpb->sectors_per_fat);
    printf("Root_dir_sector: %d\n", bpb->root_dir_sector);
    printf("Root_dir_size: %d\n", bpb->root_dir_size);
    printf("Data_start_sector: %d\n", bpb->data_start_sector);
    printf("=========================\n");    
}


// Function to get file data location from starting cluster
uint32_t get_file_location(const struct BPB *bpb, uint16_t starting_cluster) {
    // In FAT12, cluster numbering starts from 2 (clusters 0 and 1 are reserved)
    uint32_t first_data_sector = bpb->data_start_sector;
    uint32_t sector = first_data_sector + (starting_cluster - 2); // * bpb->sectors_per_cluster; - only one sector per cluster
    return sector * 4096; // bpb->bytes_per_sector;  // Return byte offset in the buffer
}


/*
// Function to calculate file location in sectors
uint32_t get_file_location_in_sectors(const struct BPB *bpb, uint16_t starting_cluster) {
    // Calculate the sector number for the given starting cluster
    uint32_t sector_number = (starting_cluster - 2) * bpb->sectors_per_cluster + bpb->reserved_sectors;
    return sector_number;
}
*/


// Get count of the files
uint8_t count_files(struct BPB *bpb, const char *buffer) {    
    uint8_t no_of_files = 0;
    
    // Root directory starts after reserved sectors + FAT areas
    uint32_t root_dir_offset = bpb->root_dir_sector * bpb->bytes_per_sector;

    for (uint16_t i = 0; i < bpb->root_dir_entries; i++) {
        const char *entry = buffer + root_dir_offset + i * FAT12_ENTRY_SIZE;

        // First byte 0x00 indicates no more entries
        if (entry[0] == 0x00) break;

        // Check if it's a valid file (skip deleted/unused entries)
        if ((uint8_t)entry[0] == 0xE5 || (entry[11] & 0x08)) continue;
        
        // Incrment file counter
        no_of_files++;
    }
    
    return no_of_files;
}


// Function to get files from the root directory and their size and locations
uint8_t get_files(struct BPB *bpb, const char *buffer, struct FILE_ENTRY *files) {
    uint8_t file_counter = 0;
    
    // Root directory starts after reserved sectors + FAT areas
    uint32_t root_dir_offset = bpb->root_dir_sector * bpb->bytes_per_sector;

    for (uint16_t i = 0; i < bpb->root_dir_entries; i++) {
        const char *entry = buffer + root_dir_offset + i * FAT12_ENTRY_SIZE;

        // First byte 0x00 indicates no more entries
        if (entry[0] == 0x00) break;

        // Check if it's a valid file (skip deleted/unused entries)
        if ((uint8_t)entry[0] == 0xE5 || (entry[11] & 0x08)) continue;

        // Extract filename (8 chars) and extension (3 chars)
        char filename[9] = {0};  // 8 characters + null terminator
        char ext[4] = {0};       // 3 characters + null terminator
        strncpy(filename, entry, 8);
        strncpy(ext, entry + 8, 3);
        
        // Remove trailing spaces from filename and extension
        for (int j = 7; j >= 0 && filename[j] == ' '; j--) {
            filename[j] = '\0';
        }
        for (int j = 2; j >= 0 && ext[j] == ' '; j--) {
            ext[j] = '\0';
        }
        
        // Extract the file size (little endian, 4 bytes at offset 28)
        files[file_counter].size = read32((uint8_t*)entry, 28);

        // Store the current file index
        files[file_counter].index = file_counter;

        // Combine the filename and extension, and copy it to file->name
        snprintf(files[file_counter].name, sizeof(files[file_counter].name), "%s.%s", filename, ext);      

        // Extract the starting cluster (little endian)
        uint16_t starting_cluster = read16((uint8_t*)entry, 26);

        // Calculate the file's location in the buffer
        files[file_counter].location = get_file_location(bpb, starting_cluster);

        // Increment file counter
        file_counter++;
    }
    return file_counter;
}


/********************************************************************************************************************
*********************************************************************************************************************
*********************************************************************************************************************
*********************************************************************************************************************
*********************************************************************************************************************/


/*
// VARIANTA 1 --- GOOD
uint32_t get_next_cluster(const struct BPB *bpb, uint16_t current_cluster, const char *buffer) {    
    
    // FAT12 stores 12-bit entries. Calculate the FAT offset based on the current cluster.
    uint32_t fat_offset = (current_cluster * 3) / 2;  // 12 bits per entry, so 3 bytes represent 2 clusters

    // FAT table starts after reserved sectors
    const char *fat_start = buffer + (bpb->reserved_sectors * bpb->bytes_per_sector);

    uint16_t next_cluster;
    
    // If current cluster is even, we take the lower 12 bits
    if (current_cluster % 2 == 0) {
        next_cluster = (*(uint16_t *)(fat_start + fat_offset)) & 0x0FFF;  // Lower 12 bits
    } else {
        next_cluster = (*(uint16_t *)(fat_start + fat_offset) >> 4) & 0x0FFF;  // Upper 12 bits
    }

    printf("\n=======================================================================\n");
    printf("Current cluster: %u, Next cluster: %u\n", current_cluster, next_cluster);
    printf("=======================================================================\n");
    
    return next_cluster;
}
*/



// VARIANTA 2 --- GOOD
uint32_t get_next_cluster(const struct BPB *bpb, uint16_t current_cluster, const char *buffer) {

    // For Cluster 0: fat_offset = (0 * 3) / 2 = 0 (The first cluster entry starts at byte 0)
    // For Cluster 1: fat_offset = (1 * 3) / 2 = 1 (The entry starts at byte 1)
    // For Cluster 2: fat_offset = (2 * 3) / 2 = 3 (The entry starts at byte 3)
    
    // Calculate FAT offset based on the current cluster
    uint32_t fat_offset = (current_cluster * 3) / 2;  // 12 bits per entry, so 3 bytes represent 2 clusters

    // FAT table starts after reserved sectors
    const char *fat_start = buffer + 4096; // 4096 = (bpb->reserved_sectors * bpb->bytes_per_sector);
    
    uint16_t next_cluster;

    // Use read16 to read 2 bytes from the FAT table
    // In the micro we will read 2 bytes by SPI from the flash memory
    uint16_t entry_value = read16((const uint8_t *)fat_start, fat_offset);

    // If current cluster is even, take the lower 12 bits
    if ((current_cluster & 1) == 0) { // if (current_cluster % 2 == 0)
        next_cluster = entry_value & 0x0FFF;  // Lower 12 bits   // Even index: take the first byte and the lower 4 bits of the second byte
    } else {
        next_cluster = (entry_value >> 4) & 0x0FFF;  // Upper 12 bits     // Odd index: take the upper 4 bits of the second byte and the third byte
    }

    printf("\n=======================================================================\n");
    printf("Current cluster: %u, Next cluster: %u\n", current_cluster, next_cluster);
    printf("=======================================================================\n");
    
    return next_cluster;
}

/********************************************************************************************************************
*********************************************************************************************************************
*********************************************************************************************************************
*********************************************************************************************************************
*********************************************************************************************************************/


// ==== GOOD ====
// Function to load a file into the buffer
int load_file_to_buffer(struct BPB *bpb, const char *buffer, const char *filename_to_find, char *fileBuffer, uint32_t buffer_size) {
      
    
    uint32_t root_dir_offset = bpb->root_dir_sector * bpb->bytes_per_sector;


    // Iterate through the root directory entries
    for (uint16_t i = 0; i < bpb->root_dir_entries; i++) 
    {
                    
            const char *entry = buffer + root_dir_offset + i * FAT12_ENTRY_SIZE;
    
    
            // First byte 0x00 indicates no more entries
            if (entry[0] == 0x00) break;
    
            // Check if it's a valid file (skip deleted/unused entries)
            if ((uint8_t)entry[0] == 0xE5 || (entry[11] & 0x08)) continue;
    
    
            // Extract filename (8 chars) and extension (3 chars)
            char filename[9] = {0};
            char ext[4] = {0};
            strncpy(filename, entry, 8);
            strncpy(ext, entry + 8, 3);
    
            // Remove trailing spaces from filename and extension
            for (int j = 7; j >= 0 && filename[j] == ' '; j--) {
                filename[j] = '\0';
            }
            for (int j = 2; j >= 0 && ext[j] == ' '; j--) {
                ext[j] = '\0';
            }
            // Combine the filename and extension to compare with the target
            char full_filename[FAT12_FILENAME_LENGTH + 2] = {0};  // 8.3 format + dot
            snprintf(full_filename, sizeof(full_filename), "%.8s.%.3s", filename, ext);
    
    
            // Compare with the target file name (case-sensitive)
            if (strcmp(full_filename, filename_to_find) == 0) 
            {
            
                    // Extract the file size (little endian, 4 bytes at offset 28)
                    uint32_t file_size = read32((uint8_t*)entry, 28);
                    
                                 
                    if (file_size > buffer_size) {
                        printf("Error: Buffer too small for file %s (size: %u bytes)\n", filename_to_find, file_size);
                        return -1;  // File size exceeds buffer
                    }
        
        
                    // Extract the starting cluster (little endian)
                    uint16_t current_cluster = read16((uint8_t*)entry, 26);
        
         
                    // Read the file data cluster by cluster
                    uint32_t bytes_read = 0;
                    
                    
                    while (bytes_read < file_size) 
                    {
                            // Get file cluster location
                            // In FAT12, cluster numbering starts from 2 (clusters 0 and 1 are reserved)
                            //uint32_t sector = bpb->data_start_sector + (current_cluster - 2); // * bpb->sectors_per_cluster; // only one sector per cluster
                            //uint32_t cluster_location = sector * 4096; // bpb->bytes_per_sector;  // Return byte offset in the buffer   
                            
                            uint32_t cluster_location = get_file_location(bpb, current_cluster);
                            
                            
                                    printf("bpb->data_start_sector = %X\n", bpb->data_start_sector); 
                                                                       
                                    printf("file_size = %u\n", file_size); 

                            
                            uint32_t cluster_size = 4096; // 4096 = bpb->sectors_per_cluster * bpb->bytes_per_sector;
                            
                                    //printf("cluster_size = %u\n", cluster_size);
                            
                            uint32_t remaining_bytes = file_size - bytes_read;
                                    
                                    printf("bytes_read = %u\n", bytes_read);
                            
                            uint32_t bytes_to_copy = (remaining_bytes < cluster_size) ? remaining_bytes : cluster_size;
                        
                                    printf("bytes_to_copy = %u\n", bytes_to_copy);
                        
                            // Ensure buffer has enough space
                            if (bytes_read + bytes_to_copy > FILEBUFFER_SIZE) {
                                fprintf(stderr, "Error: Buffer overflow. fileBuffer size: %u, bytes to copy: %u\n", FILEBUFFER_SIZE, bytes_to_copy);
                                //break;
                            }
                        
                            // Copy data from the cluster to the file buffer
                            //memcpy(fileBuffer + bytes_read, buffer + cluster_location, bytes_to_copy);
                            
                            // Easier to implement this in micro using SPI :)
                            for (size_t i = 0; i < bytes_to_copy; i++) {
                                fileBuffer[bytes_read + i] = buffer[cluster_location + i];
                            }
                           
                            // Update byte to copy counter
                            bytes_read += bytes_to_copy;
                        
                            // Get the next cluster from the FAT
                            current_cluster = get_next_cluster(bpb, current_cluster, buffer);
                        
                        
                            // If we've read all the bytes needed, we are done
                            if (bytes_read >= file_size) { 
                                printf("\n================== All bytes were copied...\n\n");
                                break;
                            }            
                        
                            if (current_cluster >= 0xFF8){
                                printf("\n================== Current cluster is the end of file...\n\n");
                                break;
                            }  
                        
                    }
        
                    //printf("==== File %s loaded into buffer (size: %u bytes)\n", filename_to_find, bytes_read);
                    return bytes_read;  // Return the actual number of bytes read
                                        
            }
                    
    }

    printf("File %s not found\n", filename_to_find);
    return -1;  // File not found
                
}

/*
// ==== GOOD ====
// Function to load a file into the buffer
int load_file_to_buffer(struct BPB *bpb, const char *buffer, const char *filename_to_find, char *fileBuffer, uint32_t buffer_size) {
    
    
    
    uint32_t root_dir_offset = bpb->root_dir_sector * bpb->bytes_per_sector;



    // Iterate through the root directory entries
    for (uint16_t i = 0; i < bpb->root_dir_entries; i++) 
    {
                    
            const char *entry = buffer + root_dir_offset + i * FAT12_ENTRY_SIZE;
    
   
    
    
    
            // First byte 0x00 indicates no more entries
            if (entry[0] == 0x00) break;
    
            // Check if it's a valid file (skip deleted/unused entries)
            if ((uint8_t)entry[0] == 0xE5 || (entry[11] & 0x08)) continue;
    
    
    
    
    
    
            // Extract filename (8 chars) and extension (3 chars)
            char filename[9] = {0};
            char ext[4] = {0};
            strncpy(filename, entry, 8);
            strncpy(ext, entry + 8, 3);
    
            // Remove trailing spaces from filename and extension
            for (int j = 7; j >= 0 && filename[j] == ' '; j--) {
                filename[j] = '\0';
            }
            for (int j = 2; j >= 0 && ext[j] == ' '; j--) {
                ext[j] = '\0';
            }
            // Combine the filename and extension to compare with the target
            char full_filename[FAT12_FILENAME_LENGTH + 2] = {0};  // 8.3 format + dot
            snprintf(full_filename, sizeof(full_filename), "%.8s.%.3s", filename, ext);
    
    
    
    
    
            // Compare with the target file name (case-sensitive)
            if (strcmp(full_filename, filename_to_find) == 0) {
                
                        
                        // Extract the file size (little endian, 4 bytes at offset 28)
                        uint32_t file_size = read32((uint8_t*)entry, 28);
                                     
                                    
                        if (file_size > buffer_size) {
                            printf("Error: Buffer too small for file %s (size: %u bytes)\n", filename_to_find, file_size);
                            return -1;  // File size exceeds buffer
                        }
            
             
                        // Extract the starting cluster (little endian)
                        uint16_t starting_cluster = read16((uint8_t*)entry, 26);
                        uint16_t current_cluster = starting_cluster;
            
             
                        // Read the file data cluster by cluster
                        uint32_t bytes_read = 0;
                                                                
                        
                        
                        while (bytes_read < file_size) {
                            uint32_t cluster_location = get_file_location(bpb, current_cluster);
                            
                            //#define INVALID_CLUSTER_LOCATION 0xFFFF
                            //if (cluster_location == INVALID_CLUSTER_LOCATION) {
                            //    fprintf(stderr, "Error: Invalid cluster location for cluster %u\n", current_cluster);
                            //    break;
                            //}
                        
                            uint32_t cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
                            uint32_t remaining_bytes = file_size - bytes_read;
                            uint32_t bytes_to_copy = (remaining_bytes < cluster_size) ? remaining_bytes : cluster_size;
                        
                            // Ensure buffer has enough space
                            if (bytes_read + bytes_to_copy > FILEBUFFER_SIZE) {
                                fprintf(stderr, "Error: Buffer overflow. fileBuffer size: %u, bytes to copy: %u\n", FILEBUFFER_SIZE, bytes_to_copy);
                                break;
                            }
                        
                            // Copy data from the cluster to the file buffer
                            memcpy(fileBuffer + bytes_read, buffer + cluster_location, bytes_to_copy);
                            bytes_read += bytes_to_copy;
                        
                            // Get the next cluster from the FAT
                            current_cluster = get_next_cluster(bpb, current_cluster, buffer);
                        
                            if (current_cluster >= 0xFF8){
                                printf(" ================== Current cluster end of file...\n");
                                break;
                            }            
                            
                        
                            // If we've read all the bytes needed, we are done
                            if (bytes_read >= file_size) { 
                                printf(" ================== We have reach the end of file...\n");
                                break;
                            }            
                        
                        
                            // Check if we have reached the end of the file
                            if (current_cluster >= 0xFF8) {  // End-of-file marker
                                if (bytes_read < file_size) {
                                    // Calculate how many bytes are still needed
                                    uint32_t bytes_needed = file_size - bytes_read;
                                    // Copy the remaining bytes from the current cluster
                                    memcpy(fileBuffer + bytes_read, buffer + cluster_location, bytes_needed);
                                    bytes_read += bytes_needed;
                                    fprintf(stderr, "Warning: Reached end-of-file marker, but copied remaining %u bytes from the last cluster\n", bytes_needed);
                                }
                                break;
                            }
                        }
            
            
                        //printf("==== File %s loaded into buffer (size: %u bytes)\n", filename_to_find, bytes_read);
                        return bytes_read;  // Return the actual number of bytes read
           
                        
            }
         
                    
    }

    printf("File %s not found\n", filename_to_find);
    return -1;  // File not found
                
}

*/






/*
// ==== GOOD ====
// Function to load a file into the buffer
int load_file_to_buffer(struct BPB *bpb, const char *buffer, const char *filename_to_find, char *fileBuffer, uint32_t buffer_size) {
    uint32_t root_dir_offset = bpb->root_dir_sector * bpb->bytes_per_sector;

    // Iterate through the root directory entries
    for (uint16_t i = 0; i < bpb->root_dir_entries; i++) {
        const char *entry = buffer + root_dir_offset + i * FAT12_ENTRY_SIZE;

        // First byte 0x00 indicates no more entries
        if (entry[0] == 0x00) break;

        // Check if it's a valid file (skip deleted/unused entries)
        if ((uint8_t)entry[0] == 0xE5 || (entry[11] & 0x08)) continue;

        // Extract filename (8 chars) and extension (3 chars)
        char filename[9] = {0};
        char ext[4] = {0};
        strncpy(filename, entry, 8);
        strncpy(ext, entry + 8, 3);

        // Remove trailing spaces from filename and extension
        for (int j = 7; j >= 0 && filename[j] == ' '; j--) {
            filename[j] = '\0';
        }
        for (int j = 2; j >= 0 && ext[j] == ' '; j--) {
            ext[j] = '\0';
        }
        // Combine the filename and extension to compare with the target
        char full_filename[FAT12_FILENAME_LENGTH + 2] = {0};  // 8.3 format + dot
        snprintf(full_filename, sizeof(full_filename), "%.8s.%.3s", filename, ext);

        // Compare with the target file name (case-sensitive)
        if (strcmp(full_filename, filename_to_find) == 0) {
            
            
            // Extract the file size (little endian, 4 bytes at offset 28)
            uint32_t file_size = read32((uint8_t*)entry, 28);
                        
            if (file_size > buffer_size) {
                printf("Error: Buffer too small for file %s (size: %u bytes)\n", filename_to_find, file_size);
                return -1;  // File size exceeds buffer
            }

            // Extract the starting cluster (little endian)
            uint16_t starting_cluster = read16((uint8_t*)entry, 26);
            uint16_t current_cluster = starting_cluster;

            // Read the file data cluster by cluster
            uint32_t bytes_read = 0;
            
            while (bytes_read < file_size) {
                uint32_t cluster_location = get_file_location(bpb, current_cluster);
                
                //#define INVALID_CLUSTER_LOCATION 0xFFFF
                //if (cluster_location == INVALID_CLUSTER_LOCATION) {
                //    fprintf(stderr, "Error: Invalid cluster location for cluster %u\n", current_cluster);
                //    break;
                //}
            
                uint32_t cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
                uint32_t remaining_bytes = file_size - bytes_read;
                uint32_t bytes_to_copy = (remaining_bytes < cluster_size) ? remaining_bytes : cluster_size;
            
                // Ensure buffer has enough space
                if (bytes_read + bytes_to_copy > FILEBUFFER_SIZE) {
                    fprintf(stderr, "Error: Buffer overflow. fileBuffer size: %u, bytes to copy: %u\n", FILEBUFFER_SIZE, bytes_to_copy);
                    break;
                }
            
                // Copy data from the cluster to the file buffer
                memcpy(fileBuffer + bytes_read, buffer + cluster_location, bytes_to_copy);
                bytes_read += bytes_to_copy;
            
                // Get the next cluster from the FAT
                current_cluster = get_next_cluster(bpb, current_cluster, buffer);
            
                if (current_cluster >= 0xFF8){
                    printf(" ================== Current cluster end of file...\n");
                    break;
                }            
                
            
                // If we've read all the bytes needed, we are done
                if (bytes_read >= file_size) { 
                    printf(" ================== We have reach the end of file...\n");
                    break;
                }            
            
            
                // Check if we have reached the end of the file
                if (current_cluster >= 0xFF8) {  // End-of-file marker
                    if (bytes_read < file_size) {
                        // Calculate how many bytes are still needed
                        uint32_t bytes_needed = file_size - bytes_read;
                        // Copy the remaining bytes from the current cluster
                        memcpy(fileBuffer + bytes_read, buffer + cluster_location, bytes_needed);
                        bytes_read += bytes_needed;
                        fprintf(stderr, "Warning: Reached end-of-file marker, but copied remaining %u bytes from the last cluster\n", bytes_needed);
                    }
                    break;
                }
            }

            //printf("==== File %s loaded into buffer (size: %u bytes)\n", filename_to_find, bytes_read);
            return bytes_read;  // Return the actual number of bytes read
        }
    }

    printf("File %s not found\n", filename_to_find);
    return -1;  // File not found
}
*/


/********************************************************************************************************************
*********************************************************************************************************************
********************************             WORK IN PROGRESS               *****************************************
*********************************************************************************************************************
*********************************************************************************************************************/


int load_file_chunk(struct BPB *bpb, const char *buffer, const struct FILE_ENTRY *file_entry,
                    char *fileBuffer, uint32_t buffer_size, 
                    uint32_t offset, uint32_t chunk_size, uint16_t *last_cluster, uint32_t *bytes_read_so_far) {

    // Check if the file entry is valid
    if (!file_entry) {
        printf("Error: Invalid file entry\n");
        return -1;
    }

    // Extract file size and starting cluster from file_entry
    uint32_t file_size = file_entry->size;
    uint16_t starting_cluster = file_entry->location;
        
    uint16_t current_cluster;
    if (last_cluster != NULL && *last_cluster != 0) {
        current_cluster = *last_cluster;
    } else {
        current_cluster = starting_cluster;
    }    

    printf("\n");
    printf("File name: %s\n", file_entry->name);
    printf("Starting cluster: 0x%X\n", starting_cluster);
    printf("File size: %d\n", file_size);
    
    // If the offset exceeds the file size, return 0 (nothing more to read)
    if (offset >= file_size) {
        printf("Nothing more to read\n");
        return 0;
    }

    // Calculate starting position (skip clusters if offset is beyond the first cluster)
    uint32_t bytes_read = (bytes_read_so_far) ? *bytes_read_so_far : 0;
    uint32_t cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
    uint32_t chunk_read = 0;

    // Print the calculated values in hexadecimal
    printf("Bytes read so far: %d\n", bytes_read);
    printf("Cluster size: 0x%X\n", cluster_size);
    
    // Skip clusters to reach the starting offset
    while (bytes_read < offset) {
        current_cluster = get_next_cluster(bpb, current_cluster, buffer);
        if (current_cluster >= 0xFF8 || current_cluster == 0xFFFF) {
            printf("Error: Reached end of file before reaching offset\n");
            return -1;
        }
        bytes_read += cluster_size;
    }

    printf("Bytes read so far: %d\n", bytes_read);
    printf(" ===\n");

    // Now start reading chunks from the offset
    while (chunk_read < chunk_size && bytes_read < file_size) {
        
        uint32_t cluster_location = get_file_location(bpb, current_cluster);
        if (cluster_location == UINT32_MAX) { // Error retrieving location
            printf("Error: Could not retrieve cluster location\n");
            return -1;
        }

        uint32_t remaining_bytes = file_size - bytes_read;
        uint32_t bytes_to_copy = (remaining_bytes < cluster_size) ? remaining_bytes : cluster_size;

        // Adjust bytes_to_copy if it exceeds the chunk size
        if (bytes_to_copy > chunk_size - chunk_read) {
            bytes_to_copy = chunk_size - chunk_read;
        }

        // Ensure buffer has enough space
        if (chunk_read + bytes_to_copy > buffer_size) {
            printf("Error: Buffer overflow. fileBuffer size: %u, bytes to copy: %u\n", buffer_size, bytes_to_copy);
            return -1;
        }

        // Copy the data from the cluster to the fileBuffer
        memcpy(fileBuffer + chunk_read, buffer + cluster_location, bytes_to_copy);
        chunk_read += bytes_to_copy;
        bytes_read += bytes_to_copy;

        // If we have read enough for this chunk, return
        if (chunk_read >= chunk_size || bytes_read >= file_size) {
            break;
        }

        // Move to the next cluster
        current_cluster = get_next_cluster(bpb, current_cluster, buffer);

        // Check if we have reached the end of the file
        if (current_cluster >= 0xFF8) {
            break;
        }
    }

    // Save the current cluster and bytes_read position for subsequent calls
    if (last_cluster) {
        *last_cluster = current_cluster;
    }
    if (bytes_read_so_far) {
        *bytes_read_so_far = bytes_read;
    }

    return chunk_read;  // Return the number of bytes read in this chunk
}



/*int load_file_chunk(struct BPB *bpb, const char *buffer, const char *filename_to_find, 
                    char *fileBuffer, uint32_t buffer_size, 
                    uint32_t offset, uint32_t chunk_size, uint16_t *last_cluster, uint32_t *bytes_read_so_far) {

    uint32_t root_dir_offset = bpb->root_dir_sector * bpb->bytes_per_sector;

    // Iterate through the root directory entries
    for (uint16_t i = 0; i < bpb->root_dir_entries; i++) {
        const char *entry = buffer + root_dir_offset + i * FAT12_ENTRY_SIZE;

        // First byte 0x00 indicates no more entries
        if (entry[0] == 0x00) break;

        // Check if it's a valid file (skip deleted/unused entries)
        if ((uint8_t)entry[0] == 0xE5 || (entry[11] & 0x08)) continue;

        // Extract filename (8 chars) and extension (3 chars)
        char filename[9] = {0};
        char ext[4] = {0};
        strncpy(filename, entry, 8);
        strncpy(ext, entry + 8, 3);

        // Combine the filename and extension to compare with the target
        char full_filename[FAT12_FILENAME_LENGTH + 2] = {0};  // 8.3 format + dot
        snprintf(full_filename, sizeof(full_filename), "%.8s.%.3s", filename, ext);

        // Remove trailing spaces from filename and extension
        for (int j = 7; j >= 0 && filename[j] == ' '; j--) {
            filename[j] = '\0';
        }
        for (int j = 2; j >= 0 && ext[j] == ' '; j--) {
            ext[j] = '\0';
        }

        // Compare with the target file name (case-sensitive)
        if (strcmp(full_filename, filename_to_find) == 0) {
            // Extract the file size (little endian, 4 bytes at offset 28)
            uint32_t file_size = read32((uint8_t*)entry, 28);
            
            // If the offset exceeds the file size, return 0 (nothing more to read)
            if (offset >= file_size) {
                return 0;
            }

            // Extract the starting cluster (little endian)
            uint16_t starting_cluster = read16((uint8_t*)entry, 26);
            uint16_t current_cluster = (last_cluster && *last_cluster != 0) ? *last_cluster : starting_cluster;

            // Calculate starting position (skip clusters if offset is beyond the first cluster)
            uint32_t bytes_read = (bytes_read_so_far) ? *bytes_read_so_far : 0;
            uint32_t cluster_size = bpb->sectors_per_cluster * bpb->bytes_per_sector;
            uint32_t chunk_read = 0;

            // Skip clusters to reach the starting offset
            while (bytes_read < offset) {
                current_cluster = get_next_cluster(bpb, current_cluster, buffer);
                bytes_read += cluster_size;

                if (current_cluster >= 0xFF8 || current_cluster == 0xFFFF) {
                    fprintf(stderr, "Error: Reached end of file before reaching offset\n");
                    return -1;
                }
            }

            // Now start reading chunks from the offset
            while (chunk_read < chunk_size && bytes_read < file_size) {
                uint32_t cluster_location = get_file_location(bpb, current_cluster);
                uint32_t remaining_bytes = file_size - bytes_read;
                uint32_t bytes_to_copy = (remaining_bytes < cluster_size) ? remaining_bytes : cluster_size;

                // Adjust bytes_to_copy if it exceeds the chunk size
                if (bytes_to_copy > chunk_size - chunk_read) {
                    bytes_to_copy = chunk_size - chunk_read;
                }

                // Ensure buffer has enough space
                if (chunk_read + bytes_to_copy > buffer_size) {
                    fprintf(stderr, "Error: Buffer overflow. fileBuffer size: %u, bytes to copy: %u\n", buffer_size, bytes_to_copy);
                    return -1;
                }

                // Copy the data from the cluster to the fileBuffer
                memcpy(fileBuffer + chunk_read, buffer + cluster_location, bytes_to_copy);
                chunk_read += bytes_to_copy;
                bytes_read += bytes_to_copy;

                // If we have read enough for this chunk, return
                if (chunk_read >= chunk_size || bytes_read >= file_size) {
                    break;
                }

                // Move to the next cluster
                current_cluster = get_next_cluster(bpb, current_cluster, buffer);

                // Check if we have reached the end of the file
                if (current_cluster >= 0xFF8) {
                    break;
                }
            }

            // Save the current cluster and bytes_read position for subsequent calls
            if (last_cluster) {
                *last_cluster = current_cluster;
            }
            if (bytes_read_so_far) {
                *bytes_read_so_far = bytes_read;
            }

            return chunk_read;  // Return the number of bytes read in this chunk
        }
    }

    printf("File %s not found\n", filename_to_find);
    return -1;  // File not found
}
*/



/*
Explanation of the Working
==========================
    Offset and Chunk Size:
        The function now accepts an offset and chunk_size as parameters, allowing you to start reading 
        from a specific position in the file and limit the number of bytes read in one call.

    Persistent Cluster and Byte Read Tracking:
        The function uses the `last_cluster` and `bytes_read_so_far` parameters to keep track of the current 
        file position across multiple calls. This allows the function to continue reading from where it 
        left off in the previous chunk.
        You can pass NULL to last_cluster and bytes_read_so_far on the first call to start from the 
        beginning of the file.

    Chunk-based Reading:
        The function reads data in chunks rather than loading the entire file. It skips over clusters to 
        reach the starting offset, and then reads only the requested chunk size. This makes the function 
        more memory-efficient.

    End of File:
        The function checks for the end-of-file marker (0xFF8) during the cluster traversal and ensures 
        that it doesn’t read beyond the end of the file.
        
EXAMPLE
========
        
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CHUNK_SIZE 512


int main() {
    uint16_t last_cluster = 0;
    uint32_t bytes_read_so_far = 0;
    char fileBuffer[CHUNK_SIZE];  // Buffer to store the file chunk
    const char *filename_to_find = "FILE.TXT";  // Change the file name as necessary

    struct BPB *bpb;
    const char *buffer;

    // Read file chunks until EOF is reached
    while (1) {
        // Read the next chunk from the file
        int bytes_read = load_file_chunk(bpb, buffer, filename_to_find, fileBuffer, sizeof(fileBuffer),
                                         bytes_read_so_far, CHUNK_SIZE, &last_cluster, &bytes_read_so_far);

        // If no more bytes are read, break the loop
        if (bytes_read <= 0) {
            break;
        }

        // Print the chunk data (as hex or plain text)
        printf("Chunk at offset %u, bytes read: %d\n", bytes_read_so_far - bytes_read, bytes_read);
        for (int i = 0; i < bytes_read; i++) {
            printf("%02X ", (unsigned char)fileBuffer[i]);
            if ((i + 1) % 16 == 0) {
                printf("\n");  // Newline for every 16 bytes
            }
        }
        printf("\n");

        // If the bytes read were less than CHUNK_SIZE, we've reached the last chunk
        if (bytes_read < CHUNK_SIZE) {
            break;
        }
    }

    return 0;
}

Explanation
===========
    Buffer Setup: A 512-byte buffer fileBuffer is used to store the file chunks.
    Reading Loop: The load_file_chunk function is called repeatedly, starting at the bytes_read_so_far position in the file. It reads chunks of up to 512 bytes until the end of the file.
    Chunk Printing: After each chunk is read, its contents are printed as hexadecimal values. For readability, the code prints 16 bytes per line.
    End of File Handling: If fewer than 512 bytes are read in a chunk, the loop exits since that means the end of the file is reached.

Important
==========
    File Size Handling: Whether the file size is a multiple of 512 or not, this approach ensures the program stops at the exact end of the file.
    Byte-by-Byte Printing: The bytes in the chunk are printed in hexadecimal. You can modify this to print ASCII characters or another format if needed.
*/    
    