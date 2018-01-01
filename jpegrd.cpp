#include <fstream>
#include <ios>
#include <vector>
#include <cstdint>


#define die(str, ...) \
    do { printf(str, __VA_ARGS__); exit(1); } while(0)

void read_bytes(std::ifstream &f, uint8_t *buffer, std::streamsize sz)
{
    if(!f.read(reinterpret_cast<char*>(buffer), sz))
    {
        die("Expected to read %zd bytes\n", sz);
    }
}

uint32_t read_size(std::ifstream &f)
{
    uint8_t buffer[2];
    read_bytes(f, buffer, 2);
    uint32_t size = buffer[0] << 8 | buffer[1];
    return size;
}

void skip_size(std::ifstream &f, std::streamsize to_skip)
{
    f.seekg(to_skip - 2, std::ios_base::cur);
}

void check_buffer(uint8_t *buffer, const std::vector<uint8_t> &val)
{
    uint8_t *b = buffer;
    for(auto v : val)
    {
        if (*b != v)
        {
            die("Mismatch! Expected %02x, got %02x\n", v, *b);
        }
        b++;
    }
}

uint32_t find_next_header(std::ifstream &f)
{
    uint8_t b;
    bool found = false;
    uint32_t count = 0;
    do
    {
        read_bytes(f, &b, 1);
        if (b == 0xFF)
        {
            if (f.peek() == 0x00)
            {
                read_bytes(f, &b, 1);
                count+= 2;
            }
            else
            {
                f.unget();
                found = true;
            }
        }
        else
        {
            count++;
        }
    } while(!found);

    return count;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Expected filename as argument\n");
        exit(1);
    }

    std::ifstream f(argv[1], std::ios_base::in|std::ios_base::binary);
    if (!f)
    {
        die("Couldn't open the file %s\n", argv[1]);
    }

    uint8_t buffer[2];
    uint32_t total = 0;

    read_bytes(f, buffer, 2);
    check_buffer(buffer, {0xFF, 0xd8});
    total += 2;

    bool eoi = false;
    do
    {
        uint32_t size;
        read_bytes(f, buffer, 2);
        if (buffer[0] != 0xff)
        {
            die("Expected 0xFF byte, got %02x at offset %zu\n", 
                buffer[0], (size_t)f.tellg());
        }
        total += 2;
        switch(buffer[1])
        {
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
            size = read_size(f);
            total += size;
            printf("APP Data Type %02x: %u bytes of application data\n",
                   buffer[1], size);
            skip_size(f, size);
            break;

        case 0xDB:
            size = read_size(f);
            total += size;
            printf("DQT: %u bytes of quantization data\n", size);
            skip_size(f, size);
            break;

        case 0xC0:
        case 0xC2:
            size = read_size(f);
            total += size;
            printf("SOF: %u bytes of frame data\n", size);
            skip_size(f, size);
            break;

        case 0xC4:
            size = read_size(f);
            total += size;
            printf("DHT: %u bytes of huffman tables\n", size);
            skip_size(f, size);
            break;

        case 0xDA:
            size = read_size(f);
            skip_size(f, size);
            size += find_next_header(f);
            total += size;
            printf("SOS: %u bytes of scan data\n", size);
            break;

        case 0xD9:
            printf("EOI: end of image\n");
            eoi = true;
            break;

        case 0xFE:
            size = read_size(f);
            skip_size(f, size);
            total += size;
            printf("COM: comment %u bytes\n", size);
            break;

        default:
            die("Expected known encoding byte, got %02x\n", buffer[1]);
            break;
        }
    } while(!eoi);
    printf("Total size = %u\n", total);
}
