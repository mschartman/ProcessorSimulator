//
//  simulator.cpp
//  simulator
//
//  By Jonathan Go, Mark Hamlin, Matt Schartman
//

#include <iostream>

#include <assert.h>

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <list>
using std::list;

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    RFormat,
    IFormat,
    JFormat,
} MIPSFormat;

#define UNUSED_CTRL_BIT 0

typedef struct {
    uint32_t op;
    uint32_t func;
    
    MIPSFormat format;
    
    struct {
        uint32_t rd;
        uint32_t rs;
        uint32_t rt;
        uint32_t imm;
        uint32_t targ_address;
    } fields;
    
    struct {
        uint32_t alu;
        uint32_t mw;
        uint32_t mtr;
        uint32_t mr;
        uint32_t asrc;
        uint32_t bt;
        uint32_t rdst;
        uint32_t rw;
    } control_bits;
} DecoderOp;

static void usage()
{
    fprintf(stderr, "Usage: driver -file filename blocksize(bytes power of 2) cachesize1(bytes) cachesize2(bytes) L1accesstime(cycles) L2accesstime(cycles) L1associativity L2associativity misspenalty(cycles)\n");
}

#define OP_ADD_SLT_SUB 0
#define OP_ADDI 14
#define OP_XORI 20
#define OP_LW 3
#define OP_SW 7
#define OP_BNE 44
#define OP_BGE 34
#define OP_JAL 36

#define FUNC_ADD 10
#define FUNC_SLT 12
#define FUNC_SUB 17

#define SIX_BITS ((1|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)))
#define FIVE_BITS ((1|(1<<1)|(1<<2)|(1<<3)|(1<<4)))
#define FOUR_BITS ((1|(1<<1)|(1<<2)|(1<<3)))
#define THREE_BITS ((1|(1<<1)|(1<<2)))
#define TWO_BITS ((1|(1<<1)))
#define ONE_BIT 1

static string bits_to_string(uint32_t field, unsigned int nbits)
{
    string result;
    
    for (unsigned int i = 0; i < nbits; ++i) {
        if (field&0x1) {
            result.insert(result.begin(), 1, '1');
        }
        else {
            result.insert(result.begin(), 1, '0');
        }
        
        field >>= 1;
    }
    
    return result;
}

static void decode_control_bits(uint32_t op, uint32_t func, DecoderOp *decoded)
{
#define SET_CTRL_BITS(aluv, mwv, mtrv, mrv, asrcv, btv, rdstv, rwv) \
decoded->control_bits.alu = aluv;\
decoded->control_bits.mw = mwv;\
decoded->control_bits.mtr = mtrv;\
decoded->control_bits.mr = mrv;\
decoded->control_bits.asrc = asrcv;\
decoded->control_bits.bt = btv;\
decoded->control_bits.rdst = rdstv;\
decoded->control_bits.rw = rwv;
    
    switch (op) {
            // add
            // slt
            // sub
        case 0:
        {
            switch (func) {
                    // add
                case 10:
                    SET_CTRL_BITS(2, 0, 0, 0, 0, 0, 1, 1);
                    break;
                    // slt
                case 12:
                    SET_CTRL_BITS(6, 0, 0, 0, 0, 0, 1, 1);
                    break;
                    // sub
                case 17:
                    SET_CTRL_BITS(3, 0, 0, 0, 0, 0, 1, 1);
                    break;
                default:
                    assert(false && "Unknown func!");
                    break;
            }
        }
            break;
            // addi
        case 14:
            SET_CTRL_BITS(2, 0, 0, 0, 1, 0, 0, 1);
            break;
            // xori
        case 20:
            SET_CTRL_BITS(5, 0, 0, 0, 1, 0, 0, 1);
            break;
            // lw
        case 3:
            SET_CTRL_BITS(2, 0, 1, 1, 1, 0, 0, 1);
            break;
            // sw
        case 7:
            SET_CTRL_BITS(2, 1, 1, 1, 1, 0, UNUSED_CTRL_BIT, 0);
            break;
            // bne
        case 44:
            SET_CTRL_BITS(5, 0, UNUSED_CTRL_BIT, 0, 0, 3, UNUSED_CTRL_BIT, 0);
            break;
            // bge
        case 34:
            SET_CTRL_BITS(3, 0, UNUSED_CTRL_BIT, 0, 0, 2, UNUSED_CTRL_BIT, 0);
            break;
            // jal
        case 36:
            SET_CTRL_BITS(UNUSED_CTRL_BIT, 0, UNUSED_CTRL_BIT, 0, UNUSED_CTRL_BIT, 1, UNUSED_CTRL_BIT, 0);
            break;
        default:
            assert(false && "Unknown op!");
            break;
    }
}

static DecoderOp parse_r_format(uint32_t op, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f)
{
    DecoderOp decoded;
    bzero(&decoded, sizeof(decoded));
    
    decoded.op = op;
    decoded.func = f;
    
    decoded.format = RFormat;
    
    decoded.fields.rd = d;
    decoded.fields.rs = b;
    decoded.fields.rt = c;
    //decoded.fields.imm = UNUSED_FIELD;
    //decoded.fields.targ_address = UNUSED_FIELD;
    
    decode_control_bits(op, f, &decoded);
    
    return decoded;
}

static DecoderOp parse_i_format(uint32_t op, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f)
{
    DecoderOp decoded;
    bzero(&decoded, sizeof(decoded));
    
    decoded.op = op;
    decoded.func = 0;
    
    decoded.format = IFormat;
    
    //decoded.fields.rd = UNUSED_FIELD;
    decoded.fields.rs = b;
    decoded.fields.rt = c;
    decoded.fields.imm = (f|(e<<6)|(d<<11));
    //decoded.fields.targ_address = UNUSED_FIELD;
    
    decode_control_bits(op, 0, &decoded);
    
    return decoded;
}

static DecoderOp parse_j_format(uint32_t op, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f)
{
    DecoderOp decoded;
    bzero(&decoded, sizeof(decoded));
    
    decoded.op = op;
    decoded.func = 0;
    
    decoded.format = JFormat;
    
    //decoded.fields.rd = UNUSED_FIELD;
    //decoded.fields.rs = UNUSED_FIELD;
    //decoded.fields.rt = UNUSED_FIELD;
    //decoded.fields.imm = UNUSED_FIELD;
    decoded.fields.targ_address = (f|(e<<(6))|(d<<(6+5))|(c<<(6+5+5))|(b<<(6+5+5+5)));
    
    decode_control_bits(op, 0, &decoded);
    
    return decoded;
}

static string op_name(uint32_t op, uint32_t func)
{
    switch (op) {
            // add
            // slt
            // sub
        case 0:
        {
            switch (func) {
                    // add
                case 10:
                    return "add";
                    break;
                    // slt
                case 12:
                    return "slt";
                    break;
                    // sub
                case 17:
                    return "sub";
                    break;
                default:
                    assert(false && "Unknown func!");
                    break;
            }
        }
            break;
            // addi
        case 14:
            return "addi";
            break;
            // xori
        case 20:
            return "xori";
            break;
            // lw
        case 3:
            return "lw";
            break;
            // sw
        case 7:
            return "sw";
            break;
            // bne
        case 44:
            return "bne";
            break;
            // bge
        case 34:
            return "bge";
            break;
            // jal
        case 36:
            return "jal";
            break;
        default:
            assert(false && "Unknown op!");
            break;
    }
    
    return "---";
}

static string string_from_field(uint32_t field)
{
    char buf[64];
    snprintf(buf, 64, "%u", field);
    return string(buf);
}

static void print_decoder_op(DecoderOp op)
{
    printf("%s ", op_name(op.op, op.func).c_str());
    
    switch (op.format) {
        case RFormat:
            printf("$%d, $%d, $%d\n", op.fields.rd, op.fields.rs, op.fields.rt);
            break;
        case IFormat:
            printf("$%d, $%d, %d\n", op.fields.rt, op.fields.rs, *(int16_t *)(&op.fields.imm));
            break;
        case JFormat:
            printf("%d\n", op.fields.targ_address);
            break;
    }
    
    printf("Fields: {rd: %s, rs: %s, rt: %s, imm: %s, targ: %s}\n",
           string_from_field(op.fields.rd).c_str(),
           string_from_field(op.fields.rs).c_str(),
           string_from_field(op.fields.rt).c_str(),
           string_from_field(op.fields.imm).c_str(),
           string_from_field(op.fields.targ_address).c_str());
    
    printf("Control Bits: {alu: %s, mw: %s, mtr: %s, mr: %s, asrc: %s, bt: %s, rdst: %s, rw: %s}\n",
           string_from_field(op.control_bits.alu).c_str(),
           string_from_field(op.control_bits.mw).c_str(),
           string_from_field(op.control_bits.mtr).c_str(),
           string_from_field(op.control_bits.mr).c_str(),
           string_from_field(op.control_bits.asrc).c_str(),
           string_from_field(op.control_bits.bt).c_str(),
           string_from_field(op.control_bits.rdst).c_str(),
           string_from_field(op.control_bits.rw).c_str());
}

static string print_string_decoder_op(DecoderOp op)
{
    char buffer[512];
    bzero(buffer, sizeof(buffer));
    char *ptr = buffer;
    ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "%s ", op_name(op.op, op.func).c_str());
    
    switch (op.format) {
        case RFormat:
            ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "$%d, $%d, $%d", op.fields.rd, op.fields.rs, op.fields.rt);
            break;
        case IFormat:
            ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "$%d, $%d, %d", op.fields.rt, op.fields.rs, *(int16_t *)(&op.fields.imm));
            break;
        case JFormat:
            ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "%d", op.fields.targ_address);
            break;
    }
    
    return string(buffer);
}

static DecoderOp decode(uint32_t instruction)
{
    uint32_t op = (instruction>>(32-6))&SIX_BITS;
    uint32_t b = (instruction>>(32-6-5))&FIVE_BITS;
    uint32_t c = (instruction>>(32-6-5-5))&FIVE_BITS;
    uint32_t d = (instruction>>(32-6-5-5-5))&FIVE_BITS;
    uint32_t e = (instruction>>(32-6-5-5-5-5))&FIVE_BITS;
    uint32_t f = (instruction>>(32-6-5-5-5-5-6))&SIX_BITS;
    
    switch (op) {
            // add
            // slt
            // sub
        case OP_ADD_SLT_SUB:
        {
            return parse_r_format(op, b, c, d, e, f);
        }
            break;
            // addi
        case OP_ADDI:
            return parse_i_format(op, b, c, d, e, f);
            break;
            // xori
        case OP_XORI:
            return parse_i_format(op, b, c, d, e, f);
            break;
            // lw
        case OP_LW:
            return parse_i_format(op, b, c, d, e, f);
            break;
            // sw
        case OP_SW:
            return parse_i_format(op, b, c, d, e, f);
            break;
            // bne
        case OP_BNE:
            return parse_i_format(op, b, c, d, e, f);
            break;
            // bge
        case OP_BGE:
            return parse_i_format(op, b, c, d, e, f);
            break;
            // jal
        case OP_JAL:
            return parse_j_format(op, b, c, d, e, f);
            break;
        default:
            assert(false && "Unknown op!");
            break;
    }
}

class DesignFetchUnit {
private:
    int _program_counter; // Byte offset in instructions
    vector<uint32_t> _instructions;
public:
    
    // Initialize program
    DesignFetchUnit(const char *filename) {
        _program_counter = 0;
        
        FILE *file = fopen(filename, "r");
        if (!file) {
            fprintf(stderr, "File not found\n");
            exit(EXIT_FAILURE);
        }
        
        string line;
        
        while (!feof(file)) {
            char c;
            ssize_t nread = fread(&c, 1, 1, file);
            
            // EOF
            if (nread <= 0) {
                break;
            }
            
            // Finished line
            if (c == '\n') {
                if (line.size() > 0) {
                    _instructions.push_back(atoi(line.c_str()));
                }
                line.clear();
            }
            else {
                line.append(&c, 1);
            }
        }
        
        fclose(file);
        
        // Sentinel instruction; 0 is flag for exit
        _instructions.push_back(0);
    }
    
    ~DesignFetchUnit() {};
    
    // Increment program counter by 4
    uint32_t next_instruction() {
        uint32_t next = _instructions[_program_counter>>2];
        if (next) {
            _program_counter += 4;
        }
        return next;
    }
    
    bool is_end_of_program() { return _instructions[_program_counter>>2] == 0; };
    
#define BRANCH_NONE 0
#define BRANCH_BGE 2
#define BRANCH_BNE 3
#define BRANCH_JMP 1
    
    // Jump/branch support
    bool set_pc(int16_t *registers, uint32_t btype, int16_t zero_bit, int16_t offset, uint32_t jump_address) {
        switch (btype) {
            case BRANCH_NONE:
                break;
            case BRANCH_BGE:
                if ((zero_bit&(1<<15)) == 0) {
                    _program_counter += (offset<<2);
                    return true;
                }
                break;
            case BRANCH_BNE:
                if (zero_bit != 0) {
                    _program_counter += (offset<<2);
                    return true;
                }
                break;
            case BRANCH_JMP:
                registers[31] = _program_counter-4;
                _program_counter = (jump_address<<2);
                return true;
                break;
        }
        
        return false;
    }
};

#define ALU_OP_AND 0
#define ALU_OP_OR 1
#define ALU_OP_ADD 2
#define ALU_OP_SUB 3
#define ALU_OP_NOT 4
#define ALU_OP_XOR 5
#define ALU_OP_SLT 6

static int32_t alu(uint32_t alu_op, int32_t a, int32_t b)
{
    switch (alu_op) {
        case ALU_OP_AND:
            return (a && b);
            break;
        case ALU_OP_OR:
            return (a || b);
            break;
        case ALU_OP_ADD:
            return (a + b);
            break;
        case ALU_OP_SUB:
            return (a - b);
            break;
        case ALU_OP_NOT:
            return !a;
            break;
        case ALU_OP_XOR:
            return (a ^ b);
            break;
        case ALU_OP_SLT:
            return (a < b)?1:0;
            break;
    }
    
    // Unused ALU op
    return 0;
}

// We went a little overboard with formatting register output...
static void pad_minus_digits(int desired_chars, int val) {
    printf("%d", val);
    
    if (val < 0) {
        --desired_chars;
    }
    
    int nchar = 1;
    while (val/10 != 0) {
        val /= 10;
        ++nchar;
    }
    
    int remaining = desired_chars-nchar;
    
    for (int i = 0; i < remaining; ++i) {
        printf(" ");
    }
}

typedef enum {
    InstructionFetchState,
    InstructionDecodeState,
    InstructionExecuteState,
    InstructionMemoryState,
    InstructionWritebackState,
} InstructionState;

typedef enum {
    MemoryL1CacheStage,
    MemoryL2CacheStage,
    MemoryDRAMCacheStage,
} MemoryStage;

typedef struct {
    InstructionState state;
    
    // Debug output
    string description;
    
    // Fetch state
    uint32_t instruction;
    
    // Decode state
    DecoderOp decoded;
    
    // Execute state
    int32_t alu_result;
    
    // Data forwarding support
    int first_read_reg; // -1 if not needed
    int second_read_reg; // -1 if not needed
    
    bool forwarded_first_reg;
    int16_t forwarded_first_value;
    
    bool forwarded_second_reg;
    int16_t forwarded_second_value;
    
    unsigned int latest_forwarded_cycle;
    
    // Memory state
    MemoryStage memory_state;
    
    // L1 stage
    unsigned int l1_start_cycle;
    
    // L2 stage
    unsigned int l2_start_cycle;
    
    // Dram?
    unsigned int dram_start_cycle;
    
    // Writeback state
    int reg_to_write; // -1 if none
} PipelineEntry;

static list<PipelineEntry>::iterator oldest_with_state(list<PipelineEntry> &pipeline, InstructionState state, list<PipelineEntry>::iterator skip)
{
    list<PipelineEntry>::iterator itr;
    
    itr = pipeline.end();
    for (int i = ((int)pipeline.size())-1; i >= 0; --i) {
        --itr;
        
        if (itr->state == state && itr != skip) {
            return itr;
        }
    }
    
    return pipeline.end();
}

typedef struct {
    uint32_t address; // Bytes
    uint32_t word;    // Contents
    unsigned int last_access_cycle;
} CacheWordEntry;

static unsigned int bits_for_binary_num(unsigned int num)
{
    unsigned int bits = 0;
    
    while (num != 0) {
        num >>= 1;
        bits += 1;
    }
    
    return bits;
}

typedef struct {
    uint32_t full_address;
    
    unsigned int tag;
    unsigned int index;
    unsigned int block_offset;
    unsigned int byte_offset;
} DecodedAddress;

typedef struct {
    unsigned int index_bit_size;
    unsigned int block_offset_bit_size;
    unsigned int byte_offset_bit_size;
} CacheDecoderParameters;

static unsigned int mask_of_size(unsigned int bits)
{
    unsigned int mask = 0;
    
    for (unsigned int i = 0; i < bits; ++i) {
        mask |= (1<<i);
    }
    
    return mask;
}

// Note naming conventions:
// The blocksize is the size of the data,
// but we're calling the whole thing including
// valid bit, tag and data as a block
typedef struct {
    unsigned int access_cycle;
    bool valid;
    unsigned int index;
    unsigned int tag;
    vector<uint32_t> data;
} CacheBlock;

typedef struct {
    vector<CacheBlock> blocks;
} CacheLine;

static void init_cache_line(CacheLine *line, unsigned int words_per_block, unsigned int associativity)
{
    line->blocks.resize(associativity);
    
    for (unsigned int i = 0; i < associativity; ++i) {
        line->blocks[i].access_cycle = 0;
        line->blocks[i].valid = false;
        line->blocks[i].index = i;
        
        // Unnecessary, but zero everything initially
        line->blocks[i].tag = 0;
        line->blocks[i].data.resize(words_per_block);
    }
}

static DecodedAddress decode_cache_address(CacheDecoderParameters params, uint32_t address)
{
    DecodedAddress decoded;
    
    decoded.full_address = address;
    
    unsigned int byte_offset_mask = mask_of_size(params.byte_offset_bit_size);
    unsigned int block_offset_mask = mask_of_size(params.block_offset_bit_size);
    unsigned int index_mask = mask_of_size(params.index_bit_size);
    
    unsigned int walked_bits = 0;
    
    // Pull out byte_offset
    decoded.byte_offset = (((address&(byte_offset_mask<<walked_bits)))>>walked_bits);
    walked_bits += params.byte_offset_bit_size;
    assert(decoded.byte_offset == 0); // Only handle word-aligned lw/sw
    // Pull out block_offset
    decoded.block_offset = (((address&(block_offset_mask<<walked_bits)))>>walked_bits);
    walked_bits += params.block_offset_bit_size;
    
    // Pull out index
    decoded.index = (((address&(index_mask<<walked_bits)))>>walked_bits);
    walked_bits += params.index_bit_size;
    
    // Pull out tag
    decoded.tag = (address>>walked_bits);
    
    return decoded;
}

static uint32_t encode_block_cache_address(CacheDecoderParameters params, CacheBlock block)
{
    uint32_t address = 0;
    
    address |= (block.index<<(params.byte_offset_bit_size+params.block_offset_bit_size));
    address |= (block.tag<<(params.byte_offset_bit_size+params.block_offset_bit_size+params.index_bit_size));
    
    return address;
}

static CacheBlock *read_cache(unsigned int current_cycle, vector<CacheLine> *cache, DecodedAddress address)
{
    CacheLine& line = (*cache)[address.index];
    
    // Check all associativity levels for desired data
    for (unsigned int i = 0; i < line.blocks.size(); ++i) {
        if (line.blocks[i].valid &&
            line.blocks[i].tag == address.tag) {
            line.blocks[i].access_cycle = current_cycle;
            return &(line.blocks[i]);
        }
    }
    
    return NULL;
}

static CacheBlock write_cache(unsigned int current_cycle, vector<CacheLine> *cache, DecodedAddress address, uint32_t *line_data, unsigned int line_size)
{
    CacheLine& line = (*cache)[address.index];
    
    // Determine target block
    CacheBlock *target_block = NULL;
    for (unsigned int i = 0; i < line.blocks.size(); ++i) {
        if (!target_block ||
            line.blocks[i].valid == false ||
            target_block->access_cycle < line.blocks[i].access_cycle) {
            target_block = &line.blocks[i];
            if (target_block->valid == false) {
                break;
            }
        }
    }
    
    CacheBlock evicted = (*target_block);
    
    // Write to cache
    target_block->access_cycle = current_cycle;
    target_block->valid = true;
    target_block->tag = address.tag;
    
    memcpy(&(target_block->data[0]), line_data, sizeof(uint32_t)*line_size);
    
    return evicted;
}

static void write_dram(uint8_t *dram, uint32_t address, uint32_t *line_data, unsigned int line_size)
{
    memcpy(dram+address, line_data, sizeof(uint32_t)*line_size);
}

int main(int argc, const char * argv[])
{
    if (argc != 11 || strcmp(argv[1], "-file") != 0) {
        usage();
        return EXIT_FAILURE;
    }
    
    const char *filename = argv[2];
    
    unsigned int blocksize = atoi(argv[3]);
    unsigned int cachesize_1 = atoi(argv[4]);
    unsigned int cachesize_2 = atoi(argv[5]);
    unsigned int accesstime_L1 = atoi(argv[6]);
    unsigned int accesstime_L2 = atoi(argv[7]);
    unsigned int associativity_L1 = atoi(argv[8]);
    unsigned int associativity_L2 = atoi(argv[9]);
    unsigned int misspenalty = atoi(argv[10]);
    
    unsigned int num_words_per_block = (blocksize>>2);
    unsigned int num_lines_in_l1_cache = (cachesize_1/(blocksize*associativity_L1));
    unsigned int num_lines_in_l2_cache = (cachesize_2/(blocksize*associativity_L2));
    
    CacheDecoderParameters l1_params;
    CacheDecoderParameters l2_params;
    
    l1_params.index_bit_size = bits_for_binary_num(num_lines_in_l1_cache-1);
    l1_params.block_offset_bit_size = bits_for_binary_num(num_words_per_block-1);
    l1_params.byte_offset_bit_size = bits_for_binary_num(3); // 2 :) num bits to address a word
    
    l2_params.index_bit_size = bits_for_binary_num(num_lines_in_l2_cache-1);
    l2_params.block_offset_bit_size = bits_for_binary_num(num_words_per_block-1);
    l2_params.byte_offset_bit_size = bits_for_binary_num(3); // 2 :) num bits to address a word
    
    DesignFetchUnit fetcher(filename);
    
    // General registers
    int16_t registers[32];
    
    // Memory
    uint8_t memory[1024];
    
    vector<CacheLine> l1_cache;
    vector<CacheLine> l2_cache;
    
    // Init both caches
    l1_cache.resize(num_lines_in_l1_cache);
    for (unsigned int i = 0; i < num_lines_in_l1_cache; ++i) {
        init_cache_line(&(l1_cache[i]),
                        num_words_per_block,
                        associativity_L1);
    }
    l2_cache.resize(num_lines_in_l2_cache);
    for (unsigned int i = 0; i < num_lines_in_l2_cache; ++i) {
        init_cache_line(&(l2_cache[i]),
                        num_words_per_block,
                        associativity_L2);
    }
    
    // Initially zero all registers
    for (int i = 0; i < 32; ++i) {
        registers[i] = 0;
    }
    
    unsigned int cycle_num = 0;
    unsigned int instructions_completed = 0;
    unsigned int l1_accesses = 0;
    unsigned int l2_accesses = 0;
    unsigned int dram_accesses = 0;
    unsigned int l1_hits = 0;
    unsigned int l2_hits = 0;
    
    list<PipelineEntry> pipeline;
    
    // Run program
    while (!fetcher.is_end_of_program() || pipeline.size() > 0) {
        
        list<PipelineEntry>::iterator entry;
        
        string writeback_instruction;
        string memory_instruction;
        string execute_instruction;        
        string decode_instruction;
        string fetch_instruction;
        
        //
        //  Writeback State
        //
        
        entry = oldest_with_state(pipeline, InstructionWritebackState, pipeline.end());
        
        if (entry != pipeline.end()) {
            ++instructions_completed;
            writeback_instruction = entry->description;
            
            // Store ALU result from register
            if (entry->decoded.control_bits.rw == 1) {
                unsigned int reg_num = (entry->decoded.control_bits.rdst?entry->decoded.fields.rd:entry->decoded.fields.rt);
                
                registers[reg_num] = entry->alu_result;
            }
            
            pipeline.erase(entry);
        }
        
        list<PipelineEntry>::iterator memory_instruction_itr = pipeline.end();
        list<PipelineEntry>::iterator execute_instruction_itr = pipeline.end();
        list<PipelineEntry>::iterator decode_instruction_itr = pipeline.end();
        list<PipelineEntry>::iterator fetch_instruction_itr = pipeline.end();
        
        //
        //  Decode state
        //
        
        entry = oldest_with_state(pipeline, InstructionDecodeState, fetch_instruction_itr);
        //bool decode_had_forwarding = false;
        if (entry != pipeline.end()) {
            decode_instruction = entry->description;
            
            entry->decoded = decode(entry->instruction);
            
            // Declare any hazards
            entry->first_read_reg = entry->decoded.fields.rs;
            entry->second_read_reg  = entry->decoded.control_bits.asrc?-1:entry->decoded.fields.rt;
            
            if (entry->second_read_reg == -1 && entry->decoded.control_bits.mw) {
                entry->second_read_reg = entry->decoded.fields.rt;
            }
            
            if (entry->decoded.control_bits.rw && !entry->decoded.control_bits.mw) {
                entry->reg_to_write = (entry->decoded.control_bits.rdst?entry->decoded.fields.rd:entry->decoded.fields.rt);
            }
            else {
                entry->reg_to_write = -1;
            }
            
            //decode_had_forwarding = (entry->forwarded_first_reg || entry->forwarded_second_reg);
            decode_instruction_itr = entry;
        }
        
        //
        //  Execute state
        //
        
        entry = oldest_with_state(pipeline, InstructionExecuteState, decode_instruction_itr);
        
        // Do some trickery so we don't pick up the decode we just bumped
        if (entry != pipeline.end()) {
            execute_instruction = entry->description;
            
            int16_t effective_imm = *(int16_t *)(&(entry->decoded.fields.imm));
            
            int first_reg = entry->decoded.fields.rs;
            int second_reg = entry->decoded.control_bits.asrc?-1:entry->decoded.fields.rt;
            
            int32_t a = registers[first_reg];
            int32_t b = (entry->decoded.control_bits.asrc?effective_imm:registers[second_reg]);
            
            if (entry->forwarded_first_reg) {
                a = entry->forwarded_first_value;
            }
            
            if (!entry->decoded.control_bits.asrc && entry->forwarded_second_reg) {
                b = entry->forwarded_second_value;
            }
            
            entry->alu_result = alu(entry->decoded.control_bits.alu,
                                    a,
                                    b);
            
            // See if any other instructions needed our output register's value
            // DATA FORWARDING
            if (entry->reg_to_write != -1 && !entry->decoded.control_bits.mr) {
                list<PipelineEntry>::iterator itr = entry;
                
                while (1) {
                    if (itr == pipeline.begin()) {
                        break;
                    }
                    --itr;
                    /*
                     if (itr->state < InstructionDecodeState) {
                     continue;
                     }*/
                    
                    if (itr->first_read_reg == entry->reg_to_write) {
                        if (!itr->forwarded_first_reg) {
                            itr->forwarded_first_reg = true;
                            itr->forwarded_first_value = entry->alu_result;
                            itr->latest_forwarded_cycle = cycle_num;
                        }
                    }
                    
                    if (itr->second_read_reg == entry->reg_to_write) {
                        if (!itr->forwarded_second_reg) {
                            itr->forwarded_second_reg = true;
                            itr->forwarded_second_value = entry->alu_result;
                            itr->latest_forwarded_cycle = cycle_num;
                        }
                    }
                    
                    if (itr->reg_to_write == entry->reg_to_write) {
                        break;
                    }
                }
            }
            
            //entry->state = InstructionMemoryState;
            execute_instruction_itr = entry;
        }
        
        //
        //  Memory state
        //
        
        entry = oldest_with_state(pipeline, InstructionMemoryState, execute_instruction_itr);
        
        if (entry != pipeline.end()) {
            memory_instruction = entry->description;
            
            bool should_increment_state = false;
            bool has_value_to_forward = false;
            
            // Read from memory
            if (entry->decoded.control_bits.mr && entry->decoded.control_bits.mtr) {
                
                DecodedAddress l1_address = decode_cache_address(l1_params, entry->alu_result);
                DecodedAddress l2_address = decode_cache_address(l2_params, entry->alu_result);
                
                switch (entry->memory_state) {
                    case MemoryL1CacheStage:
                    {
                        if (entry->l1_start_cycle == 0) {
                            entry->l1_start_cycle = cycle_num;
                        }
                        
                        // Need to continue stalling
                        if (cycle_num-entry->l1_start_cycle < accesstime_L1-1) {
                            break;
                        }
                        
                        
                        // Ready to check L1
                        ++l1_accesses;
                        CacheBlock *l1_read = read_cache(cycle_num,
                                                        &l1_cache,
                                                        l1_address);
                        
                        if (!l1_read) {
                            entry->memory_state = MemoryL2CacheStage;
                            break;
                        }
                        
                        ++l1_hits;
                        
                        // Pass data
                        if (!entry->decoded.control_bits.mw) {
                            entry->alu_result = l1_read->data[l1_address.block_offset];
                        }
                        has_value_to_forward = true;
                        should_increment_state = true;
                    }
                        break;
                    case MemoryL2CacheStage:
                    {
                        if (entry->l2_start_cycle == 0) {
                            entry->l2_start_cycle = cycle_num;
                        }
                        
                        // Need to continue stalling
                        if (cycle_num-entry->l2_start_cycle < accesstime_L2-1) {
                            break;
                        }
                        
                                                
                        // Ready to check L2
                        ++l2_accesses;
                        CacheBlock *l2_read = read_cache(cycle_num,
                                                        &l2_cache,
                                                        l2_address);
                        
                        if (!l2_read) {
                            entry->memory_state = MemoryDRAMCacheStage;
                            break;
                        }
                        
                        ++l2_hits;
                        
                        // Promote to l1
                        uint32_t block_promote_address = encode_block_cache_address(l2_params, *l2_read);
                        DecodedAddress promote_address = decode_cache_address(l1_params, block_promote_address);
                        
                        CacheBlock evicted_from_l1 = write_cache(cycle_num,
                                                                 &l1_cache,
                                                                 promote_address,
                                                                 (uint32_t *)(&l2_read->data[0]),
                                                                 num_words_per_block);
                        
                        // Propogate to l2
                        if (evicted_from_l1.valid) {
                            uint32_t block_full_address = encode_block_cache_address(l1_params, evicted_from_l1);
                            DecodedAddress l2_target = decode_cache_address(l2_params, block_full_address);
                            
                            CacheBlock evicted_from_l2 = write_cache(cycle_num,
                                                                     &l2_cache,
                                                                     l2_target,
                                                                     (uint32_t *)(&evicted_from_l1.data[0]),
                                                                     num_words_per_block);
                            
                            if (evicted_from_l2.valid) {
                                uint32_t block_full_dram_address = encode_block_cache_address(l2_params, evicted_from_l2);
                                
                                write_dram(memory,
                                           block_full_dram_address,
                                           (uint32_t *)(&evicted_from_l2.data[0]),
                                           num_words_per_block);
                            }
                        }
                        
                        // Read word from our fetched L2 block
                        if (!entry->decoded.control_bits.mw) {
                            entry->alu_result = l2_read->data[l2_address.block_offset];
                        }
                        has_value_to_forward = true;
                        should_increment_state = true;
                    }
                        break;
                    case MemoryDRAMCacheStage:
                    {
                        if (entry->dram_start_cycle == 0) {
                            entry->dram_start_cycle = cycle_num;
                        }
                        
                        // Need to continue stalling
                        if (cycle_num-entry->dram_start_cycle < misspenalty-1) {
                            break;
                        }
                        
                        ++dram_accesses;
                        
                        // Populate L1 cache
                        CacheBlock evicted_from_l1 = write_cache(cycle_num,
                                                                 &l1_cache,
                                                                 l1_address,
                                                                 (uint32_t *)(memory+entry->alu_result),
                                                                 num_words_per_block);
                        
                        // Propogate to l2
                        if (evicted_from_l1.valid) {
                            uint32_t block_full_address = encode_block_cache_address(l1_params, evicted_from_l1);
                            DecodedAddress l2_target = decode_cache_address(l2_params, block_full_address);
                            
                            CacheBlock evicted_from_l2 = write_cache(cycle_num,
                                                                     &l2_cache,
                                                                     l2_target,
                                                                     (uint32_t *)(&evicted_from_l1.data[0]),
                                                                     num_words_per_block);
                            
                            if (evicted_from_l2.valid) {
                                uint32_t block_full_dram_address = encode_block_cache_address(l2_params, evicted_from_l2);
                                
                                write_dram(memory,
                                           block_full_dram_address,
                                           (uint32_t *)(&evicted_from_l2.data[0]),
                                           num_words_per_block);
                            }
                        }
                        
                        // Ready to read from memory
                        if (!entry->decoded.control_bits.mw) {
                            entry->alu_result = memory[entry->alu_result];
                        }
                        has_value_to_forward = true;
                        should_increment_state = true;
                    }
                        break;
                }
                
            }
            else {
                has_value_to_forward = true;
                should_increment_state = true;
            }
            
            // See if any other instructions needed our output register's value
            // DATA FORWARDING
            if (entry->reg_to_write != -1 && has_value_to_forward) {
                list<PipelineEntry>::iterator itr = entry;
                
                while (1) {
                    if (itr == pipeline.begin()) {
                        break;
                    }
                    --itr;
                    /*
                     if (itr->state < InstructionDecodeState) {
                     continue;
                     }*/
                    
                    if (itr->first_read_reg == entry->reg_to_write) {
                        if (!itr->forwarded_first_reg) {
                            itr->forwarded_first_reg = true;
                            itr->forwarded_first_value = entry->alu_result;
                            itr->latest_forwarded_cycle = cycle_num;
                        }
                    }
                    
                    if (itr->second_read_reg == entry->reg_to_write) {
                        if (!itr->forwarded_second_reg) {
                            itr->forwarded_second_reg = true;
                            itr->forwarded_second_value = entry->alu_result;
                            itr->latest_forwarded_cycle = cycle_num;
                        }
                    }
                    
                    if (itr->reg_to_write == entry->reg_to_write) {
                        break;
                    }
                }
            }
            
            if (should_increment_state) {
                
                // Write to memory
                if (entry->decoded.control_bits.mw) {
                    int16_t a = registers[entry->decoded.fields.rt];
                    
                    if (entry->forwarded_second_reg) {
                        a = entry->forwarded_second_value;
                    }
                    
                    DecodedAddress l1_address = decode_cache_address(l1_params, entry->alu_result);
                    CacheBlock *block = read_cache(cycle_num, &l1_cache, l1_address);
                    assert(block != NULL && "We just read this so it must be here");
                    
                    block->data[l1_address.block_offset] = a;
                }
                
                entry->state = InstructionWritebackState;
            }
            memory_instruction_itr = entry;
        }
        
        
        
        //
        //  Fetch state
        //
        
        if (oldest_with_state(pipeline, InstructionFetchState, pipeline.end()) == pipeline.end()){
            uint32_t instruction = fetcher.next_instruction();
            
            // If not end of program
            if (instruction != 0) {
                PipelineEntry entry;
                entry.state = InstructionFetchState;
                entry.description = print_string_decoder_op(decode(instruction));
                
                entry.instruction = instruction;
                
                entry.first_read_reg = -1;
                entry.second_read_reg = -1;
                entry.forwarded_first_reg = false;
                entry.forwarded_second_reg = false;
                entry.reg_to_write = -1;
                
                entry.latest_forwarded_cycle = 0;
                
                entry.memory_state = MemoryL1CacheStage;
                entry.l1_start_cycle = 0;
                entry.l2_start_cycle = 0;
                entry.dram_start_cycle = 0;
                
                fetch_instruction = entry.description;
                pipeline.insert(pipeline.begin(), entry);
            }
        }
        
        //
        //  Bump execute if possible
        //
        
        if (execute_instruction_itr != pipeline.end() &&
            oldest_with_state(pipeline, InstructionMemoryState, pipeline.end()) == pipeline.end()) {
            execute_instruction_itr->state = InstructionMemoryState;
        }
        
        //
        //  Bump decode into execute if needed
        //
        
        if (decode_instruction_itr != pipeline.end()) {
            entry = decode_instruction_itr;
            
            // Special case for branches -- Stall if the register we need will be written, or a register we need to write is
            // going to be written
            bool future_will_write_first = false;
            bool future_will_write_second = false;
            
            list<PipelineEntry>::iterator itr;
            for (itr = entry; itr != pipeline.end(); ++itr) {
                if (itr == entry) {
                    continue;
                }
                
                // Blocked on getting their value (data forwarding)
                if (itr->reg_to_write != -1) {
                    if (itr->reg_to_write == entry->first_read_reg) {
                        future_will_write_first = true;
                    }
                    
                    if (itr->reg_to_write == entry->second_read_reg) {
                        future_will_write_second = true;
                    }
                }
            }
            
            bool blocked_on_branch = false;
            
            if (entry->decoded.control_bits.bt&2) {
                if (future_will_write_first) {
                    if (!(entry->forwarded_first_reg && entry->latest_forwarded_cycle < cycle_num)) {
                        blocked_on_branch = true;
                    }
                }
                
                if (future_will_write_second) {
                    if (!(entry->forwarded_second_reg && entry->latest_forwarded_cycle < cycle_num)) {
                        blocked_on_branch = true;
                    }
                }
            }
            
            if (oldest_with_state(pipeline, InstructionExecuteState, pipeline.end()) == pipeline.end() &&
                !blocked_on_branch &&
                ((!future_will_write_first || entry->forwarded_first_reg) &&
                 (!future_will_write_second || entry->forwarded_second_reg))) {
                    
                    if (entry->decoded.control_bits.bt) {
                        int16_t effective_imm = *(int16_t *)(&(entry->decoded.fields.imm));
                        
                        if (entry->decoded.control_bits.bt&2) {
                            int32_t a = registers[entry->first_read_reg];
                            int32_t b = (entry->decoded.control_bits.asrc?effective_imm:registers[entry->second_read_reg]);
                            
                            if (entry->forwarded_first_reg) {
                                a = entry->forwarded_first_value;
                            }
                            
                            if (entry->forwarded_second_reg) {
                                b = entry->forwarded_second_value;
                            }
                            
                            entry->alu_result = alu(entry->decoded.control_bits.alu,
                                                    a,
                                                    b);
                        }
                        
                        // Jump/Branch Support
                        // Smash newer instructions
                        if (fetcher.set_pc(registers,
                                           entry->decoded.control_bits.bt,
                                           entry->alu_result,
                                           effective_imm-1,
                                           entry->decoded.fields.targ_address)) {
                            pipeline.erase(pipeline.begin(), entry);
                        }
                    }
                    
                    entry->state = InstructionExecuteState;
                }
        }
        
        //
        //  Increment fetch if possible
        //
        
        // Only move to decode if there's not one already there
        entry = oldest_with_state(pipeline, InstructionFetchState, pipeline.end());
        
        if (entry != pipeline.end()) {
            fetch_instruction = entry->description;
        }
        
        fetch_instruction_itr = entry;
        if (entry != pipeline.end() &&
            oldest_with_state(pipeline, InstructionDecodeState, pipeline.end()) == pipeline.end()) {
            
            entry->state = InstructionDecodeState;
        }
        
        //
        //  Print out all state
        //
        
        printf("Cycle %u:\n", cycle_num);
        printf("Fetch instruction: %s\n", fetch_instruction.c_str());
        printf("Decode instruction: %s\n", decode_instruction.c_str());
        printf("Execute instruction: %s\n", execute_instruction.c_str());
        printf("Memory instruction: %s\n", memory_instruction.c_str());
        printf("Writeback instruction: %s\n", writeback_instruction.c_str());
        
        // Hacky printing of registers
        int reg_index = 0;
        while (reg_index < 8) {
            for (int i = 0; i < 4; ++i) {
                printf("$%d:%s ", reg_index, (reg_index > 9)?"":" ");
                pad_minus_digits(4, registers[reg_index]);
                reg_index += 8;
            }
            printf("\n");
            
            reg_index -= 23+8;
        }
        
        printf("\n");
        
        // Next-state
        ++cycle_num;
    }
    
    printf("Cycles: %u\n", cycle_num);
    printf("Instructions Executed: %u\n", instructions_completed);
    printf("Number of cache accesses: %u\n", l1_accesses);
    printf("L1 hits: %u\n", l1_hits);
    printf("L2 hits: %u\n", l2_hits);
    printf("Miss rate of L1 cache: %f\n", 1.0-((float)l1_hits)/((float)l1_accesses));
    printf("Miss rate of L2 cache: %f\n", 1.0-((float)l2_hits)/((float)l2_accesses));
    printf("Average Memory Access Time: %f\n", ((float)(l1_accesses*accesstime_L1+l2_accesses*accesstime_L2+dram_accesses*misspenalty))/((float)l1_accesses));
    
    return EXIT_SUCCESS;
}
