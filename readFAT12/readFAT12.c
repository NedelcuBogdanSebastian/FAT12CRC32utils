
/*
    TO DETERMINE FILE SIZE IN WINDOWS:
    >>> for %A in (WSCLI.htm) do @echo %~zA
    
    For testing you need to drag and drop the FLASH FAT12 partition
	on top of the exe and you will see the file list from the FAT12 partition.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "FAT12.h"

// Define constants for display formatting
#define NAME_WIDTH 15
#define SIZE_WIDTH 10
#define LOCATION_WIDTH 10

struct BPB bpb;

// Global file pointer to simulate SD card access
FILE *sdcard_file;

// Global buffer simulating SD card
char *FAT12_buffer;

// Size of the buffer representing the SD card
int32_t sdcard_size;


// There are needed by the FAT12 library
uint8_t NO_OF_FILES = 0;
struct FILE_ENTRY FILES[20];  // Array to store up to 20 files
uint16_t last_cluster = 0;
uint32_t bytes_read_so_far = 0;

// Buffer to store the file chunk (we will send HTML chunks only)
char fileChunkBuffer[4096];

char fileBuffer[FILEBUFFER_SIZE]; // 264kB +100 bytes


int loadDataspaceBuff(char *fname)
{
    // Open the file in binary mode (read binary)
    sdcard_file = fopen(fname, "rb");
    if (sdcard_file == NULL) {
        perror("Error opening UDISK file");
        return 1;
    }

    // Find the size of the file
    if (fseek(sdcard_file, 0, SEEK_END) != 0) {  // Move the file pointer to the end of the file
        perror("Error seeking to the end of the UDISK file");
        fclose(sdcard_file);
        return 1;
    }

    sdcard_size = ftell(sdcard_file);  // Get the current file pointer (this is the size of the file)
    if (sdcard_size == -1) {
        perror("Error getting the UDISK file size");
        fclose(sdcard_file);
        return 1;
    }

    rewind(sdcard_file);  // Move the file pointer back to the beginning

    // Allocate memory for the buffer based on file size
    FAT12_buffer = (char *)malloc(sdcard_size);
    if (FAT12_buffer == NULL) {
        perror("Memory allocation for UDISK file failed");
        fclose(sdcard_file);
        return 1;
    }

    // Read the entire file into the buffer
    size_t read_size = fread(FAT12_buffer, 1, sdcard_size, sdcard_file);
    if (read_size != sdcard_size) {
        perror("Error loading UDISK file");
        free(FAT12_buffer);
        fclose(sdcard_file);
        return 1;
    }

    // Print FAT12 buffer size
    printf("FAT12 file loaded successfully, size: %ld bytes\n", sdcard_size);

    return 0;  // Success
}


int main(int argc, char *argv[]) 
{
    // Clear the screen
    system("cls");
    
    // Check if file name is provided
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    // Open the file provided as the first argument
    char *fn = argv[1];
    
    // Use these if you want to run from the IDE not from command line
    //char *fn = "25Q32FLASH_last"; //argv[1];
    //char *fn = "25Q32FLASH_big"; //argv[1];
    //char *fn = "25Q32FLASH_with_files"; //argv[1];    
    //char *fn = "25Q32FLASH_MOD"; //argv[1];   
    //char *fn = "25Q32FLASH"; //argv[1];   
    
    printf("\n");
    
    // Load all data from the file to a buffer we will use 
    // as a FAT12 simultated dataspace
    if (loadDataspaceBuff(fn) != 0) {
        return 1;     
    }

    /* ONE SECTOR HAS 512 BYTES
    0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    000000000000
    */
	
    // Load BPB from the boot sector (first sector of the buffer)
    load_bpb(&bpb, FAT12_buffer);

    printf("\n");
 
     // Call the function to list the files
    NO_OF_FILES = get_files(&bpb, FAT12_buffer, FILES);

    // Print the table header
    printf("%-8s %-*s %-*s %-*s\n", "Index", NAME_WIDTH, "Name", SIZE_WIDTH, "Size", LOCATION_WIDTH, "Location");

    // Print the file details
    for (int i = 0; i < NO_OF_FILES; i++) {
        printf("%-8d %-*s %-*u 0x%-*X\n", 
               FILES[i].index,                   // Index
               NAME_WIDTH, FILES[i].name,        // Name
               SIZE_WIDTH, FILES[i].size,        // Size
               LOCATION_WIDTH, FILES[i].location // Location
        );
    }
	/*
	Index    Name            Size       Location
	0        SYSTEM~1.       0          0x7000
	1        AJAXCLI.HTM     1969       0xA000
	2        CONFIG.TXT      53         0xB000
	3        README.TXT      208        0xC000
	4        WSCLI.HTM       10054      0xD000
	*/


    printf("\n");

    // Display a file from the FAT12 buffer loading data directly, no logical parsing
    //uint8_t file_index = 4;
    //for (size_t i = FILES[file_index].location; i < FILES[file_index].location + FILES[file_index].size; i++) 
    //printf("%c", FAT12_buffer[i]);


    // Display a file from the FAT12 buffer by reading clusters
    int bytes_loaded = load_file_to_buffer(&bpb, FAT12_buffer, "WSCLI.HTM", fileBuffer, sizeof(fileBuffer));
    
    printf("%s\n", fileBuffer);
    printf("\nFile size %u bytes.\n", bytes_loaded);
                    
     
/*
    // Read a file by chunks of 512 byte each, until EOF is reached
    while (1) {
        // Read the next chunk from the file
        int bytes_read = load_file_chunk(&bpb, FAT12_buffer, &FILES[4], 
                                         fileChunkBuffer, sizeof(fileChunkBuffer), 
                                         bytes_read_so_far, CHUNK_SIZE, 
                                         &last_cluster, &bytes_read_so_far);
        // If no more bytes are read, break the loop
        if (bytes_read <= 0) {
            break;
        }

        // Print the chunk data (as hex or plain text)
        printf("Chunk at offset %u, bytes read: %d\n", bytes_read_so_far - bytes_read, bytes_read);
        for (int i = 0; i < bytes_read; i++) {
            printf("%02X ", (unsigned char)fileChunkBuffer[i]);
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
*/      
    
    // Free the buffer when you're done
    free(FAT12_buffer);

    // Close the file
    fclose(sdcard_file);

    printf("\nPress any key...\n");
    getchar();  // Waits for a keypress

    return 0;
}      
