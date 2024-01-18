#include <iostream>
#include <fstream>
#include "process.hpp"
#include "errors.hpp"
#include <vector>
#include <zlib.h>
#include <math.h>
//#include <stdlib.h>
 
using namespace std;

unsigned int arrayToInt(const vector<unsigned char>& array) {
    unsigned int number = (array[0] << 24) | (array[1] << 16) |
                          (array[2] << 8)  |  array[3];
    return number;
}

class PNG {
    public:
        unsigned int width, height;
        unsigned short int bit_depth, color_type, interlace, bytes_per_pixel;
        vector<unsigned short int*> pixels;

        // Constructor PNG class start 
        PNG(string &file_path) : image(file_path.c_str(), ios::binary) {
            if (!image.is_open()){ // Check if file is open
                throw invalid_argument("Error opening image");
            }

            {
                char buffer;
                // Check if file start with 8 bytes PNG signature
                unsigned short int png_signature[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
                for (short int position = 0; position < 8; position++) {
                    image.get(buffer);
                    if (png_signature[position] != int(static_cast<unsigned char>(buffer))){
                        throw invalid_PNG_file();
                    }
                }
            }

            read_IHDR();
            
            unsigned int chunk_lenght, crc;
            vector<unsigned char> chunk_data;

            // If color is Indexed, the PLTE chunk is the next after IHDR
            if (color_type == 3) {
                chunk_lenght = arrayToInt(read_bytes(4));
                chunk_data = read_bytes(4 + chunk_lenght);

                {   // Calculate CRC32 of chunk and compare if is equal
                    crc = arrayToInt(read_bytes(4));
                    uLong calculated_crc = crc32(0L, Z_NULL, 0);
                    calculated_crc = crc32(calculated_crc, reinterpret_cast<const Bytef*>(chunk_data.data()), static_cast<uInt>(chunk_data.size()));
                    
                    if (crc != calculated_crc) {
                        throw invalid_PNG_crc();
                    }
                }

                if (!equal(chunk_data.begin(), chunk_data.begin()+4, "PLTE")) {
                    throw invalid_PNG_chunk();
                }
                if ((chunk_lenght % 3) != 0) throw invalid_PNG_chunk();

                unsigned short int* buffer;
                for (unsigned int x = 0; x < chunk_lenght; x+=3) {
                    buffer = new unsigned short int[3];

                    buffer[0] = chunk_data[4+x];
                    buffer[1] = chunk_data[4+x+1];
                    buffer[2] = chunk_data[4+x+2];

                    color_palet.push_back(buffer);
                }
            }

            while (true) {
                chunk_lenght = arrayToInt(read_bytes(4));
                chunk_data = read_bytes(4+chunk_lenght); 

                //for (short int x = 0; x < 4; x++) cout << chunk_data[x];
                //cout << endl;

                {
                    crc = arrayToInt(read_bytes(4));
                    uLong calculated_crc = crc32(0L, Z_NULL, 0);
                    calculated_crc = crc32(calculated_crc, reinterpret_cast<const Bytef*>(chunk_data.data()), static_cast<uInt>(chunk_data.size()));
                    
                    if (crc != calculated_crc) {
                        throw invalid_PNG_crc();
                    }
                }

                // If image ends
                if (equal(chunk_data.begin(), chunk_data.begin()+4, "IEND")) {
                    read_IDAT();
                    break;
                }
                
                // If Chunk type is IDAT
                if (equal(chunk_data.begin(), chunk_data.begin()+4, "IDAT")) {
                    chunk_data.erase(chunk_data.begin(), chunk_data.begin()+4);

                    idat_chunk_data.insert(idat_chunk_data.end(), chunk_data.begin(), chunk_data.end());  
                }
                
            }

            image.close();
        } // End of constructor class function

        ~PNG() { // Destructor class function
            if (image.is_open()) {
                image.close();
            }
            for (auto& pixel : color_palet) {delete[] pixel;}
            for (auto& row : pixels) delete[] row;
            pixels.clear();
        } // End destructor

    private:
        ifstream image;
        vector<unsigned short int*> color_palet;
        vector<unsigned char> idat_chunk_data;
        unsigned int width_total_size;
        vector<unsigned char> vector_buffer;
        
        vector<unsigned char> read_bytes(const unsigned int lenght) {
            vector<unsigned char> buffer;
            buffer.reserve(lenght);

            for (unsigned int x = 0; x < lenght; x++) {
                buffer.push_back(image.get());
            }
            return buffer;
        }

        void read_IHDR(){
            {
                // All IHDR chunk must have 13 bytes of lenght
                vector<unsigned char> chunk_lenght = read_bytes(4);
                unsigned int x = arrayToInt(chunk_lenght);
                if (x != 13) {
                    throw invalid_PNG_chunk();
                }
            }

            vector<unsigned char> chunk_data = read_bytes(17);

            {
                unsigned int crc = arrayToInt(read_bytes(4));
                uLong calculated_crc = crc32(0L, Z_NULL, 0);
                calculated_crc = crc32(calculated_crc, reinterpret_cast<const Bytef*>(chunk_data.data()), static_cast<uInt>(chunk_data.size()));
                
                if (crc != calculated_crc) {
                    throw invalid_PNG_crc();
                }
            }

            // Check if chunk name is IHDR
            if (!equal(chunk_data.begin(), chunk_data.begin()+4, "IHDR")) {
                throw invalid_PNG_chunk();
            }

            width = arrayToInt({chunk_data.begin()+4, chunk_data.begin()+8});
            height = arrayToInt({chunk_data.begin()+8, chunk_data.begin()+12});

            bit_depth = chunk_data[12];
            color_type = chunk_data[13];
            interlace = chunk_data[16];
            
            printf("Largura:%u\nAltura:%u\nBit:%u\nColor_Type:%u\nInterlace:%u\n", width, height, bit_depth, color_type, interlace);
            
            switch (color_type) {
                case 0:
                    bytes_per_pixel = 1;
                    break;
                case 2:
                    bytes_per_pixel = 3;
                    break;
                case 3:
                    bytes_per_pixel = 1;
                    break;
                case 4:
                    bytes_per_pixel = 2;
                    break;
                case 6:
                    bytes_per_pixel = 4;
                    break;
            }
            width_total_size = width * bytes_per_pixel * bit_depth / 8;
        }

        unsigned short int Recon_a(const unsigned int heightPosition, const unsigned int widthPosition) {
            if (widthPosition >= (bytes_per_pixel * ((bit_depth > 8) ? 2 : 1))) {
                return vector_buffer[(heightPosition * width_total_size) + widthPosition - (bytes_per_pixel * ((bit_depth > 8) ? 2 : 1))];
            } else {
                return 0;
            }
        }
        unsigned short int Recon_b(const unsigned int heightPosition, const unsigned int widthPosition) {
            if (heightPosition > 0) {
                return vector_buffer[((heightPosition - 1) * width_total_size) + widthPosition];
            } else {
                return 0;
            }
        }
        unsigned short int Recon_c(const unsigned int heightPosition, const unsigned int widthPosition) {
            if (heightPosition > 0 && widthPosition >= (bytes_per_pixel * ((bit_depth > 8) ? 2 : 1))) {
                return vector_buffer[((heightPosition - 1) * width_total_size) + widthPosition - (bytes_per_pixel * ((bit_depth > 8) ? 2 : 1))];
            } else {
                return 0;
            }
        }
        unsigned short int Paeth(const unsigned int reconA, const unsigned int reconB, const unsigned int reconC) {
            short int base, diferenceA, diferenceB, diferenceC;

            base = reconA + reconB - reconC;

            diferenceA = base - reconA;
            diferenceA = (diferenceA >= 0) ? diferenceA : diferenceA * (-1);

            diferenceB = base - reconB;
            diferenceB = (diferenceB >= 0) ? diferenceB : diferenceB * (-1);

            diferenceC = base - reconC;
            diferenceC = (diferenceC >= 0) ? diferenceC : diferenceC * (-1);

            if (diferenceA <= diferenceB && diferenceA <= diferenceC) return diferenceA;
            else if (diferenceB <= diferenceC) return diferenceB;
            else return diferenceC;
        }

        void read_IDAT() {
            unsigned int expected_size;
            if (color_type != 3) {
                expected_size = height * ((width * bytes_per_pixel * bit_depth / 8) + 1);
            } else {
                expected_size = height * ((width * bit_depth / 8) + 1);
            }

            vector_buffer.reserve(height * width_total_size);

            { // Start reverse filter block
                idat_chunk_data = descomprimir(idat_chunk_data, expected_size);

                unsigned int index = 0;
                unsigned short int filter_method, pixel_x;
                for (unsigned int h = 0; h < height; h++) {
                    filter_method = idat_chunk_data[index];

                    index++;
                    for (unsigned int w = 0; w < width_total_size; w++) {       
                        switch (filter_method) {
                            case 0:
                                vector_buffer.push_back( idat_chunk_data[index] );
                                break;
                            case 1:
                                pixel_x = idat_chunk_data[index];
                                vector_buffer.push_back( static_cast<unsigned char>( pixel_x + Recon_a(h, w) ) );
                                break;
                            case 2:
                                pixel_x = idat_chunk_data[index];
                                vector_buffer.push_back( static_cast<unsigned char>( pixel_x + Recon_b(h, w) ) );
                                break;
                            case 3:
                                pixel_x = idat_chunk_data[index];
                                vector_buffer.push_back( static_cast<unsigned char>( pixel_x + floor(( Recon_a(h, w) + Recon_b(h, w)) / 2) ) );
                                break;
                            case 4:
                                pixel_x = idat_chunk_data[index];
                                vector_buffer.push_back( static_cast<unsigned char>( pixel_x + Paeth(Recon_a(h, w), Recon_b(h, w), Recon_c(h, w)) ) );
                                break;
                            default: 
                                throw invalid_PNG_chunk();
                        }
                        index++;
                    }
                };
                idat_chunk_data.clear();
            } // End reverse filter block

            { // Start Bit convert
                pixels.reserve(height * width);

                unsigned int index = 0;
                unsigned short int* buffer;
                unsigned short int max_bit_number = pow(2, bit_depth);
                unsigned char mask = (1u << bit_depth) - 1u;

                for (unsigned int h = 0; h < height; h++) {

                    vector<unsigned short int> multiple_bit;
                    multiple_bit.reserve(width);

                    for (unsigned int w = 0; w < (width * bytes_per_pixel); w++) {
                        if (bit_depth > 8) { // Images with 16 bit depth
                            buffer = new unsigned short int[bytes_per_pixel];

                            for (unsigned short int pixel_index = 0; pixel_index < bytes_per_pixel; pixel_index++) {
                                buffer[pixel_index] = (vector_buffer[index] << 8) | vector_buffer[index+1];
                                index+=2;
                            }
                            
                            pixels.push_back( buffer );

                        } else { // Images with 8 bit depth or less
                            
                            if (color_type == 3) { // Images with color palet
                                for (unsigned int bit_index = 0; bit_index < (8/bit_depth); bit_index++) {
                                    // Position of pixel in Color Palet (PLTE)
                                    unsigned short int palet_index = vector_buffer[index] & (mask << (bit_index * bit_depth));
                                    buffer = new unsigned short int[3];

                                    cout << palet_index << " " << static_cast<unsigned int>(vector_buffer[index]) << endl;

                                    buffer[0] = color_palet[palet_index][0]; // R
                                    buffer[1] = color_palet[palet_index][1]; // G
                                    buffer[2] = color_palet[palet_index][2]; // B

                                    pixels.push_back( buffer );
                                }
                            } else { // All other colors type

                                for (unsigned int bit_index = 0; bit_index < (8/bit_depth); bit_index++) {
                                    // The pixel in format of bit_depth
                                    unsigned short int bit_pixel = vector_buffer[index] & (mask << (bit_index * bit_depth));
                                    
                                    multiple_bit.push_back((bit_pixel + 1) * (256 / max_bit_number) - 1); // Convert any bit_depth pixel to 8 bit
                                }
                            }
                            index++;
                        }
                    }

                    if (bit_depth <= 8 && color_type != 3) {
                        
                        for (unsigned int bit_index = 0; bit_index < multiple_bit.size();) {

                            buffer = new unsigned short int[bytes_per_pixel];

                            for (unsigned short int pixel_index = 0; pixel_index < bytes_per_pixel; pixel_index++){

                                if (multiple_bit.size() <= bit_index) break;
                                
                                buffer[pixel_index] = multiple_bit[bit_index];

                                bit_index++;
                            }

                            pixels.push_back( buffer );
                        }
                    }
                    multiple_bit.clear();
                }
                
            } // End Bit convert

        } // End Read_IDAT function
}; // End of class PNG

