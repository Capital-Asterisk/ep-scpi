#ifndef CMD_LENGTH_MAX
#define CMD_LENGTH_MAX 4
#endif

#ifndef VALUE_LENGTH_MAX
#define VALUE_LENGTH_MAX 16
#endif

#include <stdint.h>
#include <string.h>
#include <time.h> 

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
 * @return [0 for non-whitespace] [1 for whitespace] [2 for terminator]
 */
uint8_t char_nature(const char c);

/**
 * Feed SCPI data char-by-char into this function to parse it
 * @return [0 when OK] [1 for SYNTAX ERROR] [2 for COMMAND NOT FOUND]
 *         [3 for INVALID USE, ie. setting a non-settable]
 *         other error codes (perfer negative #) are returned by the commands
 */
int8_t epscpi_feed_char(struct epscpi_parser_t* parser, char in);

const struct epspi_command_t* spcom_find_command(
        struct epscpi_parser_t* parser,
        const char* str);

/*uint8_t is_separator(const char c)
{
    // accepted separators between commands
    switch(c)
    {
    case '\0':
    case '\n':
    case '\r':
    case ';':
        return 1;
    default:
        return 0;
    }
}

uint8_t is_whitespace(const char c)
{
    switch(c)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
        return 1;
    default:
        return 0;
    }
}*/
