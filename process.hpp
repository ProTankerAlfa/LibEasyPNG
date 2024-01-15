#include <vector>
#include <libdeflate.h>
#include <zlib.h>
#include <string>

/*std::s tring comprimir(const std::string& dados) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        return "";
    }

    zs.next_in = (Bytef*)dados.data();
    zs.avail_in = dados.size();

    int ret;
    char buffer[4096];

    std::string comprimido;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(buffer);
        zs.avail_out = sizeof(buffer);

        ret = deflate(&zs, Z_FINISH);

        comprimido.append(buffer, sizeof(buffer) - zs.avail_out);
    } while (zs.avail_out == 0);

    deflateEnd(&zs);

    return comprimido;
} */

std::vector<unsigned char> descomprimir(std::vector<unsigned char>& comprimido, size_t espected_size) {
    struct libdeflate_decompressor *descompressor = libdeflate_alloc_decompressor();
    
    std::vector<unsigned char> buffer(espected_size);
    unsigned long long int anything = 1;
    //cout << comprimido.data();

    auto x = libdeflate_zlib_decompress(descompressor, comprimido.data(), comprimido.size(), buffer.data(), espected_size, NULL);

    if (x == LIBDEFLATE_SUCCESS) std::cout << "ok" << std::endl;
    if (x == LIBDEFLATE_BAD_DATA) std::cout << "bad data" << std::endl;
    if (x == LIBDEFLATE_INSUFFICIENT_SPACE) std::cout << "space" << std::endl;
    if (x == LIBDEFLATE_SHORT_OUTPUT) std::cout << "output" << std::endl;

    libdeflate_free_decompressor(descompressor);

    return buffer;
}


std::vector<unsigned char> descomprimirZlib(const std::vector<unsigned char>& compressed_data, unsigned int expected_size) {
    // Estruturas de controle do zlib
    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    // Inicializar o zlib para descompressão
    if (inflateInit(&stream) != Z_OK) {
        std::cerr << "Erro ao inicializar o zlib para descompressão.\n";
        return {};
    }

    // Configurar a entrada e a saída
    stream.next_in = const_cast<Bytef*>(compressed_data.data());
    stream.avail_in = static_cast<uInt>(compressed_data.size());

    std::vector<unsigned char> uncompressed_data;

    // Tamanho inicial do buffer de saída (pode ser ajustado conforme necessário)
    uncompressed_data.resize(expected_size);

    do {
        stream.next_out = uncompressed_data.data() + stream.total_out;
        stream.avail_out = static_cast<uInt>(uncompressed_data.size() - stream.total_out);

        // Descomprimir os dados
        int result = inflate(&stream, Z_NO_FLUSH);

        if (result == Z_NEED_DICT || result == Z_DATA_ERROR || result == Z_MEM_ERROR) {
            std::cerr << "Erro durante a descompressão do zlib.\n";
            std::cout << result;
            inflateEnd(&stream);
            return {};
        }

        // Se o buffer de saída estiver cheio, redimensione
        if (stream.avail_out == 0) {
            size_t newSize = uncompressed_data.size() * 2;
            uncompressed_data.resize(newSize);
        }

    } while (stream.avail_in > 0 || stream.avail_out == 0);

    // Definalizar o zlib
    inflateEnd(&stream);

    // Redimensionar para o tamanho real dos dados descomprimidos
    uncompressed_data.resize(stream.total_out);

    //printf("Capacity: %u | Real: %u", buffer.capacity(), buffer.size());

    return uncompressed_data;
}