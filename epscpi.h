#ifndef _EPSCPI_H_
#define _EPSCPI_H_


// Embedded Partial SCPI Parser for the MMPS Setpoint Controller
// either "ippyskippy" or "episkippy" 
//
// Author   Neal Nicdao
// Date:    Jan 21 2020


// TODO: add some defines for magical error codes and natures

// default values if not defined
#ifndef CMD_LENGTH_MAX
#define CMD_LENGTH_MAX 4
#endif

// max length of value string, determines max digits of integers in query
#ifndef VALUE_LENGTH_MAX
#define VALUE_LENGTH_MAX 16
#endif

#include <stdint.h>
#include <string.h>
//#include <time.h> 

#define CHAR_NATURE_WHITESPACE 0x01
#define CHAR_NATURE_TERMINATOR 0x02

struct epscpi_parser_t;
struct epspi_cmd_t;

// State of a spscpi_parser
enum parsestate_e
{
    Error,          // Error. Parser needs to be reset, and m_error is set.
    SpaceTerminator,// Wait for a terminator before proceeding
    SpaceNextCmd,   // In space between commands
    Command,        // Currently reading a command
    SpaceValue,     // In space between Set command and value. or end of Event
    Value           // Currently reading a value
};

enum cmdtype_e { None, Event, Query, Set };

// Typedefs for function pointers. 
typedef uint8_t (*com_func_t)(struct epscpi_parser_t*, enum cmdtype_e);
//typedef uint8_t (*event_t)(struct epscpi_parser_t*);
//typedef uint8_t (*query_t)(struct epscpi_parser_t*);
//typedef uint8_t (*set_t)(struct epscpi_parser_t*);

struct epspi_command_t
{
    char m_name[CMD_LENGTH_MAX];
    com_func_t m_function;
    //event_t m_event;
    //query_t m_query;
    //set_t m_set;
};

struct epscpi_parser_t
{

    // set these during initialization
    const uint8_t m_cmdCount;
    const uint8_t m_commonCount;
    const struct epspi_command_t* m_commands;


    enum parsestate_e m_state;
    
    uint8_t m_cmdStrLength;
    uint8_t m_valStrLength;
    
    char m_cmdStr[CMD_LENGTH_MAX];
    
    union
    {
        char m_valStr[VALUE_LENGTH_MAX];
        uint8_t m_error;
    };
};

/**
 * Determine whether a char is whitespace, terminator, or just a normal char
 * Normal whitespace can be put between events and their values.
 * Terminators will always end commands.
 *
 * @param   c [in]    Char to check
 * 
 * @return [0 for non-whitespace] [1 for whitespace] [2 for terminator]
 */
int8_t epscpi_char_nature(char c);

/**
 * Feed SCPI data char-by-char into this function to parse it
 *
 * @param   parser [in] Parser instance that parses the char
 * @param   charIn [in] Current char to feed
 *
 * @return [0 when OK] [1 for SYNTAX ERROR] [2 for COMMAND NOT FOUND]
 *         [3 for INVALID USE, ie. setting a non-settable]
 *         other error codes (perfer negative #) are returned by the commands
 */
int8_t epscpi_feed_char(struct epscpi_parser_t* parser, char charIn);

/**
 * Reset the parser
 * 
 * @param   parser [in] Parser to reset
 */
void epscpi_reset(struct epscpi_parser_t* parser);

const struct epspi_command_t* epscpi_command_find(
        struct epscpi_parser_t* parser,
        const char* str);

// # Value parsing

/**
 * Determine whether a char is a number, or part of a number
 * @return [0..9 if digit] [10..15 if A..F and hex is true] 
 *         [-1 if '-'] [-6 if '#']
 *         [-7 if 'q' or 'Q'] [-8 for 'b' or 'B'] 
 *         [-15 everything else including null terminator] [-16 for '.']
 */
int8_t epscpi_char_nature_number(char c, uint8_t hex);

/**
 * Parse a bool from a stirng. Allows values like ON and OFF too
 *
 * @param   boolOut [out]   Pointer to bool to be modified
 * @param   valueStr [in]   String to parse
 *
 * @return [0 when OK] [4 for error]
 */
uint8_t epscpi_parse_bool(uint8_t* boolOut, const char* valueStr);

/**
 * Parse an int from a string. Allows decimal, hex, and octal
 *
 * @param   intOut [out]    Pointer to int16 to be modified
 * @param   valueStr [in]   String to parse
 *
 * @return [0 when OK] [4 for error]
 */
uint8_t epscpi_parse_int16(int16_t* intOut, const char* valueStr);
//uint8_t epscpi_parse_int16_decimal(int16_t* intIn, const char* value);
//uint8_t epscpi_parse_int16_hex(int16_t* intIn, const char* value);

/**
 * Print a 16 bit signed int onto a string. Make sure there's at least 6 chars
 * of space following strOut to print digits onto.
 *
 * @param   strOut [out]    Pointer to string to modify
 * @param   printMe [in]    16-Bit Integer to print
 *
 * @return Length of resulting string
 */
uint8_t epscpi_int_to_dec(char* strOut, int16_t printMe);

#endif
