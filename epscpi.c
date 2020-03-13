// Embedded Partial SCPI Parser for the MMPS Setpoint Controller
// either "ippyskippy" or "episkippy" 
//
// Author   Neal Nicdao
// Date:    Jan 21 2020

// default values if not defined
// max length of value string, determines max digits of integers in query

#include "epscpi.h" 

struct epscpi_parser_t;
struct epspi_cmd_t;

static inline int8_t char_uppercase(char c)
{
    return c ^ (('a' <= c) && (c <= 'z')) << 5;
}

int8_t epscpi_char_nature(char c)
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
 *
 * @param   parser [in] Parser instance that parses the char
 * @param   charIn [in] Current char to feed
 *
 * @return [0 when OK] [1 for SYNTAX ERROR] [2 for COMMAND NOT FOUND]
 *         [3 for INVALID USE, ie. setting a non-settable]
 *         other error codes (perfer negative #) are returned by the commands
 */
int8_t epscpi_feed_char(struct epscpi_parser_t* parser, char charIn)
{
    const struct epspi_command_t* command;
    enum cmdtype_e call = None;
    
    // 0 = normal, 1 = whitespace, 2 = terminator
    // if(inNature) if whitespace or terminator
    // if(inNature == 1) if whitespace only
    // ...
    uint8_t inNature = epscpi_char_nature(charIn);
    
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
        if (charIn == ':' || inNature)
            break;
        // Start reading a command when non-whitespace
        parser->m_state = Command;
        parser->m_cmdStrLength = 0;
        memset(parser->m_cmdStr, 0, CMD_LENGTH_MAX);
        // no break, go straight to case: Command

    case Command:
        // In the middle of reading a Command. type of command (query, ...)
        // is currently unknown.
        if (charIn == '?')
        {
            call = Query; // ? detected, so it's a query
            break;
        }
        else if (charIn == ':')
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
            parser->m_cmdStr[parser->m_cmdStrLength++] 
                = char_uppercase(charIn);
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
            
            // Add a null terminator
            parser->m_valStr[parser->m_valStrLength] = '\0';
            break;
        }
        else
        {
            // Stop caring when the command name gets too long
            // reserve the last space for the null terminator
            if (parser->m_valStrLength == VALUE_LENGTH_MAX - 1)
                break;
            // Save the character and increment counter
            parser->m_valStr[parser->m_valStrLength++] = charIn;
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
        command = epscpi_command_find(parser, parser->m_cmdStr);
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

/**
 * Reset the parser
 * 
 * @param   parser [in] Parser to reset
 */
void epscpi_reset(struct epscpi_parser_t* parser)
{
    parser->m_state = SpaceNextCmd;
}

static inline uint8_t epscpi_command_compare(const char* strA,
                                             const char* strB)
{
    for (uint8_t i = 0; i < CMD_LENGTH_MAX; i ++)
    {
        if (strA[i] != strB[i])
            return 0;
    }
    return 1;
}

const struct epspi_command_t* epscpi_command_find(
        struct epscpi_parser_t* parser,
        const char* str)
{   
    // IEEE common commands starts with asterisks
    uint8_t isCommon = (str[0] == '*');
    // since common commands are listed first, skip them when not common
    
    for (uint8_t i = (!isCommon) * parser->m_commonCount;
         i < parser->m_cmdCount; i++)
    {
        // This commented solution actually performs really well, just that
        // there's spaghetti nonsense that is slightly unacceptable
        //for (uint8_t j = 0; j < CMD_LENGTH_MAX; j ++)
        //{
        //    if (str[j] != parser->m_commands[i].m_name[j])
        //       goto nextCom;
        //}
        //return parser->m_commands + i;
        //nextCom:
        //{
        //}
        
        // sane solution maybe
        if (epscpi_command_compare(parser->m_commands[i].m_name, str))
        {
            return parser->m_commands + i;
        }
        
        
        // lazy test first char, then second, then third, ...
        //if (str[0] != parser->m_commands[i].m_name[0])
        //    continue;
        //if (str[1] != parser->m_commands[i].m_name[1])
        //    continue;
        //if (str[2] != parser->m_commands[i].m_name[2])
        //    continue;
        //if (str[3] != parser->m_commands[i].m_name[3])
        //    continue;
        
        // compare with current command
        //if (!memcmp(str, parser->m_commands[i].m_name, CMD_LENGTH_MAX))
        //    continue;
        //if (*((uint32_t*)str) == *((uint32_t*)(parser->m_commands[i].m_name)))
        //    return parser->m_commands + i;
    }
    return NULL;
}

// # Value Parsing

int8_t epscpi_char_nature_number(const char c, uint8_t hex)
{
    // numbers are in order on the ascii table, so subtracting a number char by
    // '0' (decimal 48) will make it into a proper int, valid up to 9
    uint8_t number = (uint8_t)(c - '0');
    if (number <= 9)
    {
        // return 0..9 if number
        return number;
    }
    
    if (hex)
    {
        // same thing, so number will be 0 if A
        number = (uint8_t)(char_uppercase(c) - 'A');
        // if between 0..5 or A..F
        if (number <= 5)
        {
            return number + 10; // +10 because A = 10
        }
    }
    
    switch(c)
    {
    case '-':
        return -1;
    //case ',':
    //    return -2;
    case '#':
        return -6;
    case 'q':
    case 'Q':
        return -7;
    case 'b':
    case 'B':
        return -8;
    case '.':
        return -16;
    default:
        return -15;
    }
}

uint8_t epscpi_parse_bool(uint8_t* boolOut, const char* valueStr)
{
    //const char* currentCharPtr = valueStr;
    //char currentChar = ;
    
    // this is kind of lazy
    // test OFF or ON
    /*if (valueStr[0] == 'O')
    {
        //currentChar = valueStr[1];
        if (valueStr[1] == 'N')
        {
            *boolOut = 1;
            return 0;
        }
        else if (valueStr[1] == 'F'
                || valueStr[2] == 'F')
        {
            // OFF
            *boolOut = 0;
            return 0;
        }
        else
        {
            return 4; // nothing else starts with O, so error
        }
    }*/
    
    // rather save space than cpu cycles, parse as int
    int16_t integer;
    uint8_t parseStatus = epscpi_parse_int16(&integer, valueStr);
    if (!parseStatus)
    {
        // only change the value if success
        *boolOut = (integer != 0);
        return 0;
    }
    return parseStatus;
    
    // if it's just zeros, then it's false, any other number makes it true
    // val=fish   -> error!
    // val=100000 -> true
    // val=-25565 -> true
    // val=000000 -> false
    
    /*uint8_t numberEncountered = 0;
    
    // loop until null terminator
    while (currentChar = *(currentCharPtr++))
    {
        uint8_t nature = epscpi_char_nature_number(currentChar);
        if (nature > 0)
        {
           // another number means true
           *boolOut = 1;
           return 0;
        }
        numberEncountered = 1;
    }
    
    // all zeros, false
    *boolOut = 0;
    return 0;*/
}

uint8_t epscpi_parse_int16(int16_t* intOut, const char* valueStr)
{
    // decimal places used to parse ints
    // seriously, it takes more cycles to read from RAM then just multiply
    //static const uint16_t g_decimals[] = {1, 10, 100, 1000, 10000};
    
    //g_decimal
    int16_t output = 0;
    uint8_t negative = 0;
    uint8_t base = 10;
    
    uint8_t state = 1;
    
    const char* currentPtr = valueStr;
    int8_t currentNature;
    char current;
    
    // Loop through all characters left->right, MSB first because it's a string
    while ((current = *(currentPtr++)) && state)
    {
        //currentNature = epscpi_char_nature_number(current = *(currentPtr++))
        //if (current == '.')
        ///{
        //    break; // decimal point means stop
        //}
        currentNature = epscpi_char_nature_number(current, base == 16);
        
        //printf("fish %d\n", currentNature);
        
        // First time looping, 
        switch (state)
        {
        case 1: // initial looping
            //switch (currentNature)
            if (currentNature == 0) // skip leading zeros
            {
                break; 
            }
            else if (currentNature == -1)
            {
                // negative sign, toggle
                negative = !negative;
                break;
            }
            else if (currentNature == -6) // Hex
            {
                base = 16;
                state = 2;
                break;
            }
            else if (currentNature == -7) // Octal
            {
                base = 8;
                state = 2;
                break;
            }
            else if (currentNature == -8) // binary
            {
                base = 2;
                state = 2;
                break;
            }
            else if (currentNature == -15)
            {
                return 4; // error for weird characters in front of the number
            }
            // else
            state = 2; // first number to parse found
            // proceed to next state right away with no break 
        case 2: // actually read the number
        
            if (currentNature < 0)
            {
                // done when reaching a non-number
                state = 0;
                break;
            }
            else if (currentNature >= base)
            {
                // single digit is larger than base, error
                // triggers when reading an 8 or 9 in octal mode
                return 4;
            }
        
            // shift the output value by the base, like how x10 shifts digits
            output *= base;
            // Set the LSB, currentNature can only be less than the base
            output += currentNature;
            
            break;
        //case 2:
        //    
        //    break;
        }
    }
    
    if (negative)
    {
        output = -output;
    }
    
    (*intOut) = output;
    //(*intOut) = (output ^ -negative) + negative;
    
    return 0;
}


uint8_t epscpi_int_to_dec(char* strOut, int16_t printMe)
{
    char* currentPtr = strOut;
    int16_t decade = 10000;
    int8_t count = -1; // -1 means initial count, stays -1 for leading zeros
    int8_t len = 0;

    if (printMe < 0)
    {
        // absolute value
        printMe = -printMe;
        // add a negative sign and move to the next char
        *(currentPtr++) = '-';
        len ++;
    }
    else if (!printMe)
    {
        // number is zero
        strOut[0] = '0';
        return 1;
    }

    // see how many times decade can subtract into printMe 
    while (decade)
    {
        if (decade <= printMe)
        {
            printMe -= decade;
            
            // fires if count was -1, first non-zero number
            if (!(++count))
            {
                // increment twice to account for being -1
                count ++;
            }
        }
        else
        {
            // pick next decade
            // compiler should figure this out
            decade /= 10; 
            
            // do nothing for leading zeros
            if (count != -1)
            {
                // set number char then move to next
                *(currentPtr++) = ('0' + count);
                len ++;
                count = 0;
            }
        }
    }
    
    return len;
    
}
