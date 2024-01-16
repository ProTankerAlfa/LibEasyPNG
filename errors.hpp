#include <stdexcept>

class invalid_PNG_file : public std::exception {
    public:
        const char* what() const noexcept override {
            return "File don't have PNG signature.";
        }
};
class invalid_PNG_chunk : public std::exception {
    public:
        const char* what() const noexcept override {
            return "Chunk is not in standard format.";
        }
};
class invalid_PNG_crc : public std::exception {
    /* Error when PNG is not in default format */
    public:
        const char* what() const noexcept override {
            return "Invalid crc chunk, file can be corrupted.";
        }
};
