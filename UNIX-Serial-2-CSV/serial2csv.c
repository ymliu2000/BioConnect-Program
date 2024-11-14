/*
 *  Title: Serial Port to CSV
 *  Description: This Programm will read out a serial port, filter the samples countinously and saves the output into a CSV-File.
 *  Author: Frederic Waldmann
 *  Date: 10.09.2024
 *  Version: 1.0
 */

// C library headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

// Constants
#define BUFFER_SIZE 1024          // Size of buffer for storing each complete value
#define CHUNK_SIZE 256            // Number of bytes to read in each call

int setup_serial_port(const char* port_name){
    
    // Open the serial port
    int serial_port = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);

    // Check for errors
    if (serial_port < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
    } else {
        printf("Serial Port opened.\n");
    }
    
    // Configure the serial port
    struct termios tty;
    
    // Read in existing settings, and handle any error
    if(tcgetattr(serial_port, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
    }
    
    // 8N1 (8 bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;  // No parity
    tty.c_cflag &= ~CSTOPB;  // 1 stop bit
    tty.c_cflag &= ~CSIZE;   // Clear the current data size setting
    tty.c_cflag |= CS8;      // 8 data bits

    tty.c_cflag &= ~CRTSCTS;  // Disable hardware flow control
    tty.c_cflag |= CREAD | CLOCAL;  // Turn on READ & ignore modem control lines
    tty.c_lflag &= ~ICANON;  // Disable canonical mode (raw input)
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // Turn off software flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    
    // Set the minimum number of characters to read and the read timeout
    tty.c_cc[VMIN] = 1;    // Wait for at least 1 character
    tty.c_cc[VTIME] = 0;   // No timeout (blocking read)
    
    // Set the baud rates to 115200
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    
    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    }
    return serial_port;
}


int main(int argc, const char * argv[]) {
    
    char port_name[] = "/dev/tty.usbmodem1103";  // Change this to your serial port !!!
    int serial_port = setup_serial_port(port_name);
    
    // Open the CSV file for appending
    char export_file_name[] = "../Export/data.csv"; // Export Filenames
    FILE *csvFile = fopen(export_file_name, "w");
    if (csvFile == NULL) {
        perror("Unable to open data.csv");
        close(serial_port);
        return 1;
    }
    

    //  ----------------------- START DSP Initialization -----------------------
    //  In this section you can initialize varialbes for your algorithm/filter.
    
    float scaling_factor = 1.5;
    float offset = 2024;

    
    //  ----------------------- END DSP Initialization -------------------------


    // Continuously read from the serial port
    
    char buffer[BUFFER_SIZE];  // Buffer to store the received value
    int buffer_index = 0;      // Index to track position in buffer
    char chunk[CHUNK_SIZE];    // Temporary buffer to read multiple bytes
    int raw_val;               // Integer value of the full buffer
    float proc_val;            // Float value to write out
    ssize_t n_bytes;           // Number of bytes read
    
    printf("Press CTRL+C to terminate...");
    
    while (1) {
        // Read one byte
        n_bytes = read(serial_port, chunk, CHUNK_SIZE);
        
        if (n_bytes > 0) {
            
            // Process each byte in the chunk
            for (int i = 0; i < n_bytes; ++i) {
                if (chunk[i] == '\n') {
                    
                    // End of a value (newline detected)
                    buffer[buffer_index] = '\0';  // Null-terminate the string
                    raw_val = atof(buffer);       // Convert string to float
                    
                    //  ----------------------- START Processing -------------------------
                    /*
                       Here you can implement your own algorithms or digital filter on the sensor data.
					   The new measurement is provided in the variable raw_val (float). The processed value
					   must be stored to the variable proc_val (float), which then will be appended to the 
					   CSV file. 
					*/  
                    
                    // Dummy code removing signal offset and scaling the signal by an arbitrary scale factor.
                    raw_val -= offset;
                    raw_val *= scaling_factor;
					// Dumm code END
					
                    proc_val = raw_val;
                    
                    printf("Processed value: %f\n", proc_val);  // Print the value
                    //  ----------------------- END Processing -------------------------
                    
                    // saves the processed value to the end of the csv file.
                    fprintf(csvFile, "%f\n", proc_val);
                    fflush(csvFile);
                    
                    buffer_index = 0;  // Reset buffer for the next value
                    
                } else {
                    // Accumulate characters until newline is detected
                    if (buffer_index < BUFFER_SIZE - 1) {
                        buffer[buffer_index++] = chunk[i];
                        
                        // Handle buffer overflow
                    } else {
                        fprintf(stderr, "Buffer overflow, discarding data\n");
                        buffer_index = 0;  // Reset buffer if overflow occurs
                    }
                }
            }
        } else {
            if (n_bytes < 0) {
                perror("Error reading from the serial port");
                continue;
            }
        }
        // Pause Loop to receive data
        usleep(10000);
    }

    // when while loop get terminated properly close port and file
    if (close(serial_port) == 0 && fclose(csvFile) == 0) {
        printf("Serial Port and CSV file closed.\n");
    }
    
    return 0;
}
