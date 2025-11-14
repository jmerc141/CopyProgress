/*
    Optimized file copy with progress bar in C
    Compile with: gcc -O3 -o copy copy.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define PATH_SEP '\\'
    #define mkdir(path) _mkdir(path)
    #define stat _stat
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
    #include <unistd.h>
    #include <sys/time.h>
    #define PATH_SEP '/'
#endif

// For linux utf-8 output
#ifndef CP_UTF8
    #define CP_UTF8 65001
#endif

#define UPDATE_INTERVAL 0.25

// On termnials that support 256 colors
static const char* RED  = "\033[38;5;160m";
static const char* BRED = "\033[48;5;52m";

static const char* GRN  = "\033[38;5;34m";
static const char* BGRN = "\033[48;2;0;30;0m";

static const char* BLU  = "\033[38;5;26m";
static const char* BBLU = "\033[48;2;0;0;30m";

static const char* YEL  = "\033[38;5;220m";
static const char* BYEL = "\033[48;5;3m";

static const char* PURP = "\033[38;5;91m";
static const char* BPUR = "\033[48;5;53m";

static const char* GREY = "\033[38;5;230m";
static const char* BGRY = "\033[48;5;235m";

static const char* END  = "\033[0m";

//static const char* BLOCKS[] = {"░", "▒", "▓", "█"};
static const char* BLOCKS[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
//static const char* BLOCKS[] = {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"}
//static const char* BLOCKS[] = {" ", "╸", "━"}
static const int NUM_BLOCKS = sizeof(BLOCKS) / sizeof(BLOCKS[0]) - 1;
static unsigned int CHUNK_SIZE = 1024*1024*8;

// Get terminal width
int get_terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    return 80; // Default fallback
#endif
}

// Get current time in seconds with high precision
double get_time() {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}

void progress_bar(double percent, double speed, int bar_width, int m, int s) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    
    int total_steps = bar_width * NUM_BLOCKS;
    int filled_steps = (int)(percent / 100.0 * total_steps);
    int full_blocks = filled_steps / NUM_BLOCKS;
    int remainder = filled_steps % NUM_BLOCKS;
    
    printf("\033[K\r%s%s", BBLU, BLU);

    // Print full blocks
    for (int i = 0; i < full_blocks; i++) {
        printf("%s", BLOCKS[NUM_BLOCKS]);
    }
    
    // Print partial block
    if (remainder > 0 && full_blocks < bar_width) {
        printf("%s", BLOCKS[remainder]);
        full_blocks++;
    }
    
    // Print empty blocks
    for (int i = full_blocks; i < bar_width; i++) {
        printf("%s", BLOCKS[0]);
    }
    
    printf("%s %s%6.2f%% %s%4.1fMB/s %s%dm%ds %s", END, YEL, percent, RED, speed, PURP, m, s, END);
    fflush(stdout);
}

int copy_file_with_progress(const char* src, const char* dst) {
    FILE* fsrc = fopen(src, "rb");
    if (!fsrc) {
        fprintf(stderr, "Error: Cannot open source file: %s\n", src);
        return -1;
    }
    
    FILE* fdst = fopen(dst, "wb");
    if (!fdst) {
        fprintf(stderr, "Error: Cannot create destination file: %s\n", dst);
        fclose(fsrc);
        return -1;
    }
    
    // Get file size
    unsigned long long total;
    #ifdef _WIN32
        _fseeki64(fsrc, 0, SEEK_END);
        total = _ftelli64(fsrc);
        _fseeki64(fsrc, 0, SEEK_SET);
    #else
        fseeko(fsrc, 0, SEEK_END);
        total = ftello(fsrc);
        fseeko(fsrc, 0, SEEK_SET);
    #endif
    
    // Allocate buffer
    char* buffer = (char*)malloc(CHUNK_SIZE);
    if (!buffer) {
        fprintf(stderr, "Error: Cannot allocate memory\n");
        fclose(fsrc);
        fclose(fdst);
        return -1;
    }
    
    // Set larger buffer sizes for better performance
    setvbuf(fsrc, NULL, _IOFBF, CHUNK_SIZE);
    setvbuf(fdst, NULL, _IOFBF, CHUNK_SIZE);
    
    unsigned long long copied = 0;
    long long last_copied = 0;
    double start = get_time();
    double last_update = 0;
    double last_update2= 0;
    double speed = 0.0;
    int m = 0;
    int s = 0;
    int bar_width = get_terminal_width() / 4;
    
    printf("Copying %s -> %s\n", src, dst);
    
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, fsrc)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_read, fdst);
        if (bytes_written != bytes_read) {
            fprintf(stderr, "\nError: Write failed\n");
            free(buffer);
            fclose(fsrc);
            fclose(fdst);
            return -1;
        }
        
        copied += bytes_written;
        
        double current_time = get_time();
        double elapsed = current_time - start;
        
        // Update progress every UPDATE_INTERVAL seconds
        if (elapsed - last_update >= UPDATE_INTERVAL) {
            unsigned long long bytes_since_update = copied - last_copied;
            double time_since_update = elapsed - last_update;
            speed = bytes_since_update / time_since_update;
            
            progress_bar(100.0 * copied / total, speed/1048576.0, bar_width, m, s);
            
            last_update = elapsed;
            last_copied = copied;
        }
        if (elapsed - last_update2 >= 1){
            unsigned long eta = (total - copied) / speed;
            m = eta / 60;
            s = eta % 60;
            progress_bar(100.0 * copied / total, speed/1048576.0, bar_width, m, s);
            last_update2 = elapsed;
        }
    }
    
    // Final progress update
    progress_bar(100.0, speed, bar_width, m, s);
    double elapsed = get_time() - start;
    printf("\n%.2fMB copied in %.3fs\n", total / 1048576.0, elapsed);
    
    free(buffer);
    fclose(fsrc);
    fclose(fdst);
    
    // Copy file permissions
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(src);
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesA(dst, attrs);
    }
#else
    struct stat st;
    if (stat(src, &st) == 0) {
        chmod(dst, st.st_mode);
    }
#endif
    return 0;
}

int is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}

void get_basename(const char* path, char* basename) {
    const char* last_sep = strrchr(path, PATH_SEP);
    if (last_sep) {
        strcpy(basename, last_sep + 1);
    } else {
        strcpy(basename, path);
    }
}

int main(int argc, char* argv[]) {
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #else
        
    #endif
    
    if (argc < 2 || argc > 4) {
        printf("Usage: %s <source> <destination> [-n] (network transfer)\n", argv[0]);
        printf("  Source and destination can be files or directories\n");
        return 1;
    }

    const char* src = argv[1];
    const char* dst = argv[2];
    
    if (argc == 4){
        const char* n_flag = "-n";
        const char* ntwrk = argv[3];
        // 8MB chunk optimal for disk performance
        // 64MB chunk for optimal network performance
        if (strcmp(n_flag, ntwrk) == 0) {
            CHUNK_SIZE = 1024*1024*64;
        } else {
            printf("Usage: %s <source> <destination> [-n] (network transfer)\n", argv[0]);
            printf("  Source and destination can be files or directories\n");
            return 1;
        }
    }
    
    
    // Check if source exists
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) {
        fprintf(stderr, "Error: Source '%s' does not exist\n", src);
        return 1;
    }
    
    // Handle file copy
    if (!S_ISDIR(src_stat.st_mode)) {
        // Source is a file
        if (is_directory(dst)) {
            // Destination is directory - append filename
            char dest_path[1024];
            char basename[256];
            get_basename(src, basename);
            snprintf(dest_path, sizeof(dest_path), "%s%c%s", dst, PATH_SEP, basename);
            
            struct stat dst_stat;
            if (stat(dest_path, &dst_stat) == 0) {
                printf("File %s already exists\n", dest_path);
                return 1;
            }
            
            return copy_file_with_progress(src, dest_path);
        } else {
            // Destination is a file
            return copy_file_with_progress(src, dst);
        }
    } else {
        // Directory copy not implemented in this basic version
        fprintf(stderr, "Error: Directory copying not implemented yet\n");
        fprintf(stderr, "Please copy files individually\n");
        return 1;
    }
    
    return 0;
}
