#ifdef _WIN32
    #include <windows.h>

    #ifndef WIN_GETOPT_H
    #define WIN_GETOPT_H

    #include <string.h>
    #include <stdio.h>

    // Global variables
    char* optarg = NULL;    // Current option argument
    int optind = 1;         // Index of next argument to process
    int opterr = 1;         // Enable error messages (default: 1)
    int optopt = 0;         // Current option character

    // Reset getopt internal state
    void win_getopt_reset() {
        optarg = NULL;
        optind = 1;
        opterr = 1;
        optopt = 0;
    }

    int getopt(int argc, char* const argv[], const char* optstring) {
        static int optpos = 1;  // Position within current argument
        const char* colon = strchr(optstring, ':');

        // Reset for new scan
        optarg = NULL;

        // Argument boundary checks
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }

        // Handle "--" end of options
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }

        // Get current option character
        optopt = argv[optind][optpos++];

        // Find option in optstring
        char* optptr = strchr(optstring, optopt);

        // Unknown option
        if (!optptr) {
            if (opterr && *optstring != ':')
                fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], optopt);
            return '?';
        }

        // Handle required argument
        if (optptr[1] == ':') {
            // Argument in current token
            if (argv[optind][optpos] != '\0') {
                optarg = &argv[optind][optpos];
                optpos = 1;
                optind++;
            }
            // Argument in next token
            else if (argc > optind + 1) {
                optarg = argv[optind + 1];
                optind += 2;
                optpos = 1;
            }
            // Missing argument
            else {
                if (opterr && *optstring != ':') {
                    fprintf(stderr, "%s: option requires an argument -- '%c'\n",
                            argv[0], optopt);
                }
                optpos = 1;
                optind++;
                return (colon && colon[1] == ':') ? ':' : '?';
            }
        }
        // No argument required
        else {
            if (argv[optind][optpos] == '\0') {
                optpos = 1;
                optind++;
            }
        }

        return optopt;
    }

    #endif // WIN_GETOPT_H
#elif defined(__linux__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <getopt.h>
#else
    #error "Unsupported platform"
#endif
