#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAX_LEN 100
#define MAX_LEN_FILENAME 256
#define MAX_SYMBOL_LENGTH 50

#define SCREEN_MEMORY_ADDRESS 16384
#define KBD_MEMORY_ADDRESS 24576
#define VARIABLE_BASE_ADDRESS 16

/* 
  Size of the table is number of addresses from (0 -> 16384 (2^14)) + 2 
  for the SCREEN base address and KEYBOARD base address
  NOTE: Total address space is 2^15
*/
#define SYMBOL_TABLE_ENTRIES 16386
#define NUM_PREDEFINED_REGISTERS 16
#define NUM_PREDEFINED_SYMBOLS 7
#define NUM_PREDEFINED_ENTRIES 23
// Instruction width is 16, so size of string is 16 + 2 for \n and \0
#define INSTRUCTION_WIDTH 18

#define MAX_MNEMONIC_SIZE 4
#define BASE_WRITE_INDEX 23

typedef struct 
{
  int value;
  char key[MAX_SYMBOL_LENGTH];
}KeyValuePair;

typedef struct 
{
  char opcode;
}InstructionType;

typedef struct
{
  char opcode;
  
  int value;
}AInstruction;

typedef struct
{
  char opcode;

  // Max field size
  char dest[MAX_MNEMONIC_SIZE];
  char comp[MAX_MNEMONIC_SIZE];
  char jump[MAX_MNEMONIC_SIZE];
}CInstruction;

union Instruction
{
  InstructionType type;
  AInstruction a;
  CInstruction c;
};

// Globals
static KeyValuePair g_symbol_table[SYMBOL_TABLE_ENTRIES];
static int g_line_number = 1;
static int g_instruction_number;

static int g_running_variable_address = VARIABLE_BASE_ADDRESS;
static int g_symbol_table_write_index = BASE_WRITE_INDEX;

bool CheckFileType(char *filename);
void GetOutputFilename(char *input_filename, char *output_filename);
void InitSymbolTable(KeyValuePair *symbol_table);
int FileReadLine(char *s, int max_length, FILE *file);
void ParseLine(char *line, int length, union Instruction *current_instruction);
void WriteInstructionToFile(FILE *output_file, union Instruction *instruction);

bool IsValidAInstruction(char *instruction, int length);
int ParseAInstruction(union Instruction *current_instruction, char *instruction, int length);
bool IsValidCInstruction(char *instruction, int length);
void ParseCInstruction(union Instruction *current_instruction, char *instruction, int length);
bool IsValidLabel(char *label, int length);

void AddLabelsToSymbolTable(FILE *file, KeyValuePair *symbol_table, char *line);

void ConvertDecimalToBinary(int value, char *output_buffer, int buffer_length);
void EnocdeCInstruction(union Instruction *instruction, char *output_buffer, int instruction_width);

void UpdateRunningCounters(int *variable_address, int *symbol_write_ptr);

int main(int argc, char **argv)
{
  char line[MAX_LEN] = {};
  union Instruction current_instruction = {};

  char *input_filename = argv[1];
  FILE *input_file = fopen(input_filename, "r");

  if (!input_file || !CheckFileType(input_filename))
    printf("File: %s cannot be opened.\n", input_filename);

  char output_filename[MAX_LEN_FILENAME];
  GetOutputFilename(input_filename, output_filename);
  FILE *output_file = fopen(output_filename, "w");

  InitSymbolTable(g_symbol_table);
  // Add labels to symbol table
  AddLabelsToSymbolTable(input_file, g_symbol_table, line);

  int line_length = 0;
  while ((line_length = FileReadLine(line, MAX_LEN, input_file)) > 0)
  {
    // An instruction has to be greater than length 1
    // If its not a comment or label
    if (line_length > 1 && !IsValidLabel(line, line_length))
    {
      ParseLine(line, line_length, &current_instruction);
      WriteInstructionToFile(output_file, &current_instruction);
    }
    UpdateRunningCounters(&g_running_variable_address, &g_symbol_table_write_index);
  }

  fclose(input_file);
  fclose(output_file);
}

bool CheckFileType(char *filename)
{
  int len = strlen(filename);
  char *input_filename_extension = ".asm";

  int i = len - 1;
  for (; filename[i] != '.' && i > 0; i--);

  for (int j = 0; filename[i] != '\0' && input_filename_extension[j] != '\0'; i++, j++)
  {
    if (tolower(filename[i]) != tolower(input_filename_extension[j]))
    {
      printf("Incompatible file.\n");
      return false;
    }
  }
  return true;
}

// TODO: Change to malloc and return the output name
void GetOutputFilename(char *input_filename, char *output_filename)
{
  int input_len = strlen(input_filename);
  char *output_filename_extension = ".hack";

  int i = input_len - 1;
  for (; input_filename[i] != '.' && i > 0; i--);
  input_len = i;

  for (; input_filename[i] != '/' && i >= 0; i--);
  i++;

  for (; i < input_len; i++)
    *output_filename++ = input_filename[i];
    
  while (*output_filename_extension)
    *output_filename++ = *output_filename_extension++;

  *output_filename = '\0';
}

void InitSymbolTable(KeyValuePair *symbol_table)
{
  char *predefined_symbols[] = {"SCREEN", "KBD", "SP", "LCL", "ARG", "THIS", "THAT"};

  int i; // Symbol table index
  for (i = 0; i < NUM_PREDEFINED_REGISTERS; i++)
  {
    symbol_table[i].key[0] = 'R';

    if (i < 10)
    {
      symbol_table[i].key[1] = '0' + i;
      symbol_table[i].key[2] = '\0';
    }
    else
    {
      symbol_table[i].key[1] = '0' + (i / 10);
      symbol_table[i].key[2] = '0' + (i % 10);
      symbol_table[i].key[3] = '\0';
    }

    symbol_table[i].value = i;
  }

  int j = 0; // Predefined symbol index

  strncpy(symbol_table[i].key, predefined_symbols[j++], MAX_SYMBOL_LENGTH);
  symbol_table[i++].value = SCREEN_MEMORY_ADDRESS;

  strncpy(symbol_table[i].key, predefined_symbols[j++], MAX_SYMBOL_LENGTH);
  symbol_table[i++].value = KBD_MEMORY_ADDRESS;

  for (int value = 0; j < NUM_PREDEFINED_SYMBOLS; j++, i++, value++)
  {
    strncpy(symbol_table[i].key, predefined_symbols[j], MAX_SYMBOL_LENGTH);
    symbol_table[i].value = value;
  }
}

// Updates a string with a line from a file until a newline or max_length is reached
// Returns the length of the string
int FileReadLine(char *line, int max_length, FILE *file)
{
  int i = 0, c = 0, nextc = 0;

  for (; i < max_length - 1 && (c = fgetc(file)) != EOF && c != '\n'; i++)
  {
    // If whitespace
    while (isspace(c) && c != EOF)
    {
      c = fgetc(file);
      if (c == '\n')
      {
        ungetc(c, file);
        break;
      }
    }
    // If comment skip rest of line
    if (c == '/' && ((nextc = fgetc(file)) == '/'))
    {
      while ((c = fgetc(file)) != EOF)
      {
        if (c == '\n')
        {
          ungetc(c, file);
          break;
        }
      }
    }
    line[i] = c;
  }
  // Remove trailing whitespace
  while (i >= 0 && isspace(line[i - 1]))
    i--;

  if (c == '\n')
    line[i++] = c;

  line[i] = '\0';

  return i;
}

void ParseLine(char *line, int length, union Instruction *current_instruction)
{
  if (IsValidAInstruction(line, length))
  {
    g_line_number++;
    g_instruction_number++;
    ParseAInstruction(current_instruction, line, length);
  }
  else if (IsValidCInstruction(line, length) && !IsValidLabel(line, length))
  {
    g_line_number++;
    g_instruction_number++;
    ParseCInstruction(current_instruction, line, length);
  }
  else if (!IsValidLabel(line, length))
  {
    printf("Unknown instruction (line number, instruction number: %d, %d): %s\n", g_line_number, g_instruction_number, line);
    //TODO: exit
  }
}


// Synthesize parsed pieces to machine code instruction
void SynthesizeAndEncode(union Instruction *instruction, char *output)
{
  output[0] = instruction->type.opcode;
  // Set output instruction to 0
  int i = 1;
  for (; i < INSTRUCTION_WIDTH - 2; i++)
    output[i] = '0';
  output[i++] = '\n';
  output[i] = '\0';
  // If a instruction set convert value to binary
  if (instruction->type.opcode == '0')
    ConvertDecimalToBinary(instruction->a.value, output, INSTRUCTION_WIDTH);
  else if (instruction->type.opcode == '1')
    EnocdeCInstruction(instruction, output, INSTRUCTION_WIDTH);
}

void WriteInstructionToFile(FILE *output_file, union Instruction *instruction)
{
  char output[INSTRUCTION_WIDTH];
  SynthesizeAndEncode(instruction, output);
  fputs(output, output_file);
}

// Returns 0 if found and updated
// Returns 1 if not found and added to symbol table
int ParseAInstruction(union Instruction *current_instruction, char *instruction, int length)
{
  current_instruction->type.opcode = '0';

  char temp[length - 1];

  int j = 0;
  for (int i = 1; i < length - 1; i++, j++)
    temp[j] = instruction[i];
  temp[j] = '\0';

  // If constant
  if (!isalpha(temp[0]))
    current_instruction->a.value = atoi(temp);
  else
  {
    // TODO: Move this to add to symbol table function
    for (int i = 0; i < SYMBOL_TABLE_ENTRIES; i++)
    {
      // If found 
      if (strncmp(g_symbol_table[i].key, temp, MAX_LEN) == 0)
      {
        current_instruction->a.value = g_symbol_table[i].value;
        return 0;
      }
    }
    // If not found
    strncpy(g_symbol_table[g_symbol_table_write_index].key, temp, length - 1);
    g_symbol_table[g_symbol_table_write_index].value = g_running_variable_address++ ;
    current_instruction->a.value = g_symbol_table[g_symbol_table_write_index++].value;
  }

  return 1;
}

void ParseCInstruction(union Instruction *current_instruction, char *instruction, int length)
{
  current_instruction->type.opcode = '1';
  // reset fields
  for (int i = 0; i < MAX_MNEMONIC_SIZE; i++)
  {
    current_instruction->c.dest[i] = '\0';
    current_instruction->c.comp[i] = '\0';
    current_instruction->c.jump[i] = '\0';
  }

  // Everything after ';' is a jump
  // Everything before '=' is dest
  // Everything between '=' and ';' is computation
  int comp_start = 0;
  int jump_start = 0;

  bool dest = false;
  bool jump = false;

  for (int i = 0; i < length - 2; i++)
  {
    if (instruction[i] == '=')
    {
      dest = true;
      comp_start = ++i;
    }
    else if (instruction[i] == ';')
    {
      jump = true;
      jump_start = ++i;
    }
  }

  // TODO: Move to function
  if (dest)
  {
    int j = 0;
    for (int i = 0; instruction[i] != '='; i++, j++)
      current_instruction->c.dest[j] = instruction[i];
    current_instruction->c.dest[j] = '\0';
  }
  else
  {
    
  }

  // comp
  int j = 0;
  int i = comp_start;
  for (; i < length - 1 && instruction[i] != '\n' && instruction[i] != ';'; i++, j++)
    current_instruction->c.comp[j] = instruction[i];
  current_instruction->c.comp[j] = '\0';

  if (jump)
  {
    int j = 0;
    for (int i = jump_start; i < length - 1 && instruction[i] != '\n'; i++, j++)
      current_instruction->c.jump[j] = instruction[i];
    current_instruction->c.jump[j] = '\0';
  }
}

bool IsValidAInstruction(char *instruction, int length)
{
  if (instruction[0] == '@')
    return true;
  return false;
}

bool IsValidCInstruction(char *instruction, int length)
{
  // TODO: Implement this
  // char valid_registers[3] = {'A', 'M', 'D'};
  // char valid_operations[3]
  // char valid_jumps[3]
  // if (length >= 2)

  // NOTE: Assume c instruction is always valid for now
  return true;
}

bool IsValidLabel(char *label, int length)
{
  // Label must be wrapped by parens and be at least 1 character long
  if (length >= 4 && label[0] == '(' && label[length - 2] == ')')
    return true;
  return false;
}

void AddLabelsToSymbolTable(FILE *file, KeyValuePair *symbol_table, char* line)
{
  int line_length = 0;
  while ((line_length = FileReadLine(line, MAX_LEN, file)) > 0)
  {
    // A label cannot be less than length 3
    if (IsValidLabel(line, line_length))
    {
      int length_no_parens = line_length - 3;
      // Remove parens
      char label[length_no_parens];
      int i, j;
      for (i = 0, j = 0; i < line_length - 1 && j < line_length - 1; i++)
      {
        if (!(line[i] == '(' || line[i] == ')'))
        {
          label[j++] = line[i];
        }
      }
      label[j] = '\0';

      strncpy(symbol_table[g_symbol_table_write_index].key, label, length_no_parens);
      symbol_table[g_symbol_table_write_index++].value = g_instruction_number;
    }
    else if (line_length > 1)
      g_instruction_number++;
  }
  g_instruction_number = 0;
  // Reset file pointer
  fseek(file, 0L, SEEK_SET);
}

void ConvertDecimalToBinary(int value, char *output_buffer, int instruction_width)
{
  // Start at least significant bit; skipping \n and \0
  for (int i = instruction_width - 3; i > 0; i--)
  {
    if (value)
    {
      int remainder = value % 2;
      output_buffer[i] = remainder + '0';
      value /= 2;
    }
  }
}


void EnocdeCInstruction(union Instruction *instruction, char *output_buffer, int instruction_width)
{
  #define KEY_SIZE 5
  #define MAX_CODE_SIZE 8

  #define DEST_CODE_SIZE 3
  #define COMP_CODE_SIZE 7
  #define JUMP_CODE_SIZE 3

  #define DEST_TABLE_SIZE 8
  #define COMP_TABLE_SIZE 28
  #define JUMP_TABLE_SIZE 8

  typedef struct
  {
    char key[KEY_SIZE];
    char code[MAX_CODE_SIZE];
  }pair;

  pair dest_table[DEST_TABLE_SIZE] = {{"", "000"}, {"M", "001"}, {"D", "010"}, {"MD", "011"}, {"A", "100"}, {"AM", "101"}, {"AD", "110"}, {"AMD", "111"}};
  pair comp_table[COMP_TABLE_SIZE] = {
                          {"0", "0101010"}, {"1", "0111111"}, {"-1", "0111010"}, {"D", "0001100"}, {"A", "0110000"}, {"M", "1110000"}, {"!D", "0001101"},
                          {"!A", "0110001"}, {"!M", "1110001"}, {"-D", "0001111"}, {"-A", "0110011"}, {"-M", "1110011"}, {"D+1", "0011111"}, {"A+1", "0110111"},
                          {"M+1", "1110111"}, {"D-1", "0001110"}, {"A-1", "0110010"}, {"M-1", "1110010"}, {"D+A", "0000010"}, {"D+M", "1000010"}, {"D-A", "0010011"},
                          {"D-M", "1010011"}, {"A-D", "0000111"}, {"M-D", "1000111"}, {"D&A", "0000000"}, {"D&M", "1000000"}, {"D|A", "0010101"}, {"D|M", "1010101"}
                        };
  pair jump_table[JUMP_TABLE_SIZE] = {{"", "000"}, {"JGT", "001"}, {"JEQ", "010"}, {"JGE", "011"}, {"JLT", "100"}, {"JNE", "101"}, {"JLE", "110"}, {"JMP", "111"}};

  // Set unused bits
  int buffer_index = 1;
  output_buffer[buffer_index++] = '1';
  output_buffer[buffer_index++] = '1';

  // TODO: Move to function
  // Get computation code
  for (int i = 0; i < COMP_TABLE_SIZE; i++)
  {
    if (strncmp(comp_table[i].key, instruction->c.comp, MAX_MNEMONIC_SIZE) == 0)
    {
      for (int j = 0; j < COMP_CODE_SIZE && buffer_index < instruction_width - 2 ; buffer_index++, j++)
      {
        output_buffer[buffer_index] = comp_table[i].code[j];
      }
      break;
    }
  }

  // Get dest code
  for (int i = 0; i < DEST_TABLE_SIZE; i++)
  {
    if (strncmp(dest_table[i].key, instruction->c.dest, MAX_MNEMONIC_SIZE) == 0)
    {
      for (int j = 0; j < DEST_CODE_SIZE && buffer_index < instruction_width - 2; buffer_index++, j++)
      {
        output_buffer[buffer_index] = dest_table[i].code[j];
      }
      break;
    }
  }

  // Get jump code
  for (int i = 0; i < JUMP_TABLE_SIZE; i++)
  {
    if (strncmp(jump_table[i].key, instruction->c.jump, MAX_MNEMONIC_SIZE) == 0)
    {
      for (int j = 0; j < JUMP_CODE_SIZE && buffer_index < instruction_width - 2; buffer_index++, j++)
      {
        output_buffer[buffer_index] = jump_table[i].code[j];
      }
      break;
    }
  }  
}

void UpdateRunningCounters(int *variable_address, int *symbol_write_ptr)
{
  (*variable_address) > SYMBOL_TABLE_ENTRIES - 3 ? (*variable_address) = VARIABLE_BASE_ADDRESS : 0;
  (*symbol_write_ptr) > SYMBOL_TABLE_ENTRIES - 3 ? (*symbol_write_ptr) = VARIABLE_BASE_ADDRESS : 0;
}
