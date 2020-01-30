#include "epscpi.h" 

struct epscpi_parser_t;
struct epspi_cmd_t;

uint8_t char_nature(const char c)
{
    switch(c)
    {
    case '\0':
    case '\n':
    case '\r':
    case ';':
        return 2; // Terminator
    case ' ':
    case '\t':
    case '\v':
    case '\f':
        return 1; // Whitespace
    default:
        return 0;
    }

}

/**
 * Feed SCPI data char-by-char into this function to parse it
 * @return [0 when OK] [1 for SYNTAX ERROR] [2 for COMMAND NOT FOUND]
 *         [3 for INVALID USE, ie. setting a non-settable]
 *         other error codes (perfer negative #) are returned by the commands
 */
int8_t epscpi_feed_char(struct epscpi_parser_t* parser, char in)
{
    const struct epspi_command_t* command;
    enum cmdtype_e call = None;
    
    // 0 = normal, 1 = whitespace, 2 = terminator
    // if(inNature) if whitespace or terminator
    // if(inNature == 1) if whitespace only
    // ...
    uint8_t inNature = char_nature(in);
    
    switch (parser->m_state)
    {
    case SpaceTerminator:
        // A command has ended, now wait for a terminator before proceeding
        // to the next command.
        if (!inNature)
        {
            // some extra characters after a command, syntax error!
            parser->m_state = Error;
            return 1;
        }
        else if (inNature == 2)
        {
            // terminator, now start parsing the next command
            parser->m_state = SpaceNextCmd;
        }
        break;
        
    case SpaceNextCmd:
        // skip whitespace between commands, and ignore leading colons too
        if (in == ':' || inNature)
            break;
        // Start reading a command when non-whitespace
        parser->m_state = Command;
        parser->m_cmdStrLength = 0;
        memset(parser->m_cmdStr, 0, CMD_LENGTH_MAX);
        // no break, go straight to case: Command

    case Command:
        // In the middle of reading a Command. type of command (query, ...)
        // is currently unknown.
        if (in == '?')
        {
            call = Query; // ? detected, so it's a query
            break;
        }
        else if (in == ':')
        {
            // comma after a token indicates a subsystem  "subsystem:query?"
            // Since trees aren't implemented, ignore it
            parser->m_state = SpaceNextCmd;
            break;
        }
        else if (inNature)
        {
            // Command name is now known, it's either a Set or an Event
            parser->m_state = SpaceValue;
            // don't break and go directly to SpaceValue
        }
        else
        {
            // Stop caring when the command name gets too long
            if (parser->m_cmdStrLength == CMD_LENGTH_MAX)
                break;
            // save the character as UPPERCASE and increment counter
            // toggle bit 5 if letter is between a-z
            parser->m_cmdStr[parser->m_cmdStrLength++] 
                = in ^ (('a' <= in) && (in <= 'z')) << 5;
            break;
        }
        // no break, only from the "else if (isWhitespace)"
  
    case SpaceValue:
        if (inNature == 2)
        {
            // Command has ended with no value, which means it's an Event
            call = Event;
            break;
        }
        else if (inNature)
        {
            // ignore whitespace
            break;
        }
        
        // Non-whitespace characters means that it's reading a value now
        // no break, start reading value right away
        parser->m_state = Value;
        parser->m_valStrLength = 0;
        memset(parser->m_valStr, 0, VALUE_LENGTH_MAX);

    case Value:
        // Now reading a value.
        if (inNature)
        {
            // End of value, now call the set function
            call = Set;
            break;
        }
        else
        {
            // Stop caring when the command name gets too long
            if (parser->m_valStrLength == VALUE_LENGTH_MAX)
                break;
            // Save the character and increment counter
            parser->m_valStr[parser->m_valStrLength++] = in;
            break;
        }
        break;
    default:
        // ERROR!

        parser->m_state = Error;
        return 1; // error code 1 for syntax error
    }
    
    if (call != None)
    {
        // syntax error for 0-length command name
        if (!parser->m_cmdStrLength)
        {
            parser->m_state = Error;
            return 1;
        }
    
        // Listen for next command. It is required to have a terminator between
        // each command. If the current character is already a terminator, then
        // don't wait for one.
        if (inNature == 2)
        {
            parser->m_state = SpaceNextCmd;
        }
        else
        {
            parser->m_state = SpaceTerminator;
        }
        // Search for the command name
        command = spcom_find_command(parser, parser->m_cmdStr);
        if (command)
        {
            // Command found!
            return command->m_function(parser, call);
        }
        else
        {
            return 2; // error code 2 for command not found
        }
    }
    return 0;
}


const struct epspi_command_t* spcom_find_command(
        struct epscpi_parser_t* parser,
        const char* str)
{   
    // TODO: think of a better implementation, this below just isn't good

    // IEEE common commands starts with asterisks
    uint8_t isCommon = (str[0] == '*');
    // since common commands are listed first, skip them when not common
    for (uint8_t i = (!isCommon) * parser->m_commonCount;
         i < parser->m_cmdCount; i++)
    {
        // lazy test first char, then second, then third, ...
        if (str[0] != parser->m_commands[i].m_name[0])
            continue;
        if (str[1] != parser->m_commands[i].m_name[1])
            continue;
        if (str[2] != parser->m_commands[i].m_name[2])
            continue;
        if (str[3] != parser->m_commands[i].m_name[3])
            continue;
        return parser->m_commands + i;
    }
    return NULL;
}
