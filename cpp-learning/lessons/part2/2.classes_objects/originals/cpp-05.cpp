class Logger {
public:
    // C++17 inline static: initialized inside the header file
    inline static int log_count = 0; 
    
    Logger() { log_count++; }
};
