#include "JohnsSpecialEasyTransfer.h"

/*
 * de begin functie initialized het object.
 * als deze functie een tweede keer gecalled word word die call genegeerd
 */

void JohnsSpecialEasyTransfer::begin(Stream *stream, uint8_t uint8_size, uint8_t int_size, uint8_t bool_size, Stream *debug_out = NULL)
{
    if(!did_init)
    {
        if (debug_out)
        {
            has_debug = true;
            debug_stream = debug_out;
        }
        else
        {
            has_debug = false;
        }

        _stream = stream;
        hash_array_raw_uint8_t[uint8_size];
        hash_array_raw_int[int_size];
        hash_array_raw_bool[bool_size];

        map_uint8_t = HashMap<char*,uint8_t>( hash_array_raw_uint8_t, uint8_size );
        map_int = HashMap<char*,int>( hash_array_raw_int, int_size );
        map_bool = HashMap<char*,bool>( hash_array_raw_bool, bool_size );

        uint8_t_remaining = uint8_size;
        int_remaining = int_size;
        bool_remaining = bool_size;

        did_init = true;
    }

}


/*
 * de add recieve functies zijn voor het toevoegen van vars aan de hashmaps.
 * als een naam niet in de hashmap staat dan worddat bericht genegeerd
 */
bool JohnsSpecialEasyTransfer::add_recieve_uint8(String name, uint8_t default_value = 0)
{
    if(uint8_t_remaining > 0)
    {
        map_uint8_t[uint8_t_idx](str_2_char(name), default_value);
        uint8_t_idx++;
        uint8_t_remaining--;
        return true;
    }
    else
    {
        return false;
    }
}



bool JohnsSpecialEasyTransfer::add_recieve_int(String name, int default_value = 0)
{
    if(int_remaining > 0)
    {
        map_int[int_idx](str_2_char(name), default_value);
        int_idx++;
        int_remaining--;
        return true;
    }
    else
    {
        return false;
    }
}

bool JohnsSpecialEasyTransfer::add_recieve_bool(String name, bool default_value = false)
{
    if(bool_remaining > 0)
    {
        map_bool[bool_idx](str_2_char(name), default_value);
        bool_idx++;
        bool_remaining--;
        return true;
    }
    else
    {
        return false;
    }
}


/*
 * haalt waardes uit de hashmap als de waarde bestaat.
 * als de waarde niet gevonden is word NULL gereturnt
 */
uint8_t JohnsSpecialEasyTransfer::get_uint8(String name)
{
    return map_uint8_t.getValueOf(str_2_char(name));
}

int JohnsSpecialEasyTransfer::get_int(String name)
{
    return map_int.getValueOf(str_2_char(name));
}

bool JohnsSpecialEasyTransfer::get_bool(String name)
{
    return map_bool.getValueOf(str_2_char(name));
}


/*
 * deze methodes versturen de waardes over de serial.
 * het zou mooi zijn als dit gerefactored zou kunnen worden zodat
 * het gernerics gebruikt in plaats van de types te hardcoden.
 * het probleem is dat de ontvanger het ook moet kunnen snappen,
 * de type chars moeten zowiezo gehardcode worden, want als dat niet matcht
 * snapt de parser het aan de andere kant niet.
 * dit zou ik natuurlijk kunnen veranderen, maar dat zou ontstettend veel
 * werk zijn, en is in dit geval niet nodig.
 */
void JohnsSpecialEasyTransfer::send_uint8(String name, uint8_t value)
{
    init_send();
    uint8_t msg_len = name.length() + SIZE_UINT8_T + TYPE_MARKER_SIZE;
    _stream->write(msg_len);
    _stream->write(type_chars._uint8);
    _stream->write(value);
    send_name(name);
}

void JohnsSpecialEasyTransfer::send_int(String name, int value)
{
    init_send();
    uint8_t msg_len = name.length() + SIZE_INT + TYPE_MARKER_SIZE;
    _stream->write(msg_len);
    _stream->write(type_chars._int);
    _stream->write(value);
    send_name(name);
}

void JohnsSpecialEasyTransfer::send_bool(String name, bool value)
{
    init_send();
    uint8_t msg_len = name.length() + SIZE_BOOL + TYPE_MARKER_SIZE;
    _stream->write(msg_len);
    _stream->write(type_chars._bool);
    _stream->write((uint8_t) value);
    send_name(name);
}

void JohnsSpecialEasyTransfer::init_send()
{
    _stream->write(HEADER_1);
    _stream->write(HEADER_2);
}

void JohnsSpecialEasyTransfer::send_name(String name)
{
    // verstuurd de naam
    // de +1 is omdat de strings null terminated zijn
    int name_len = name.length();
    char buf[name_len+1];
    name.toCharArray(buf, name_len+1);
    for (int i=0; i < name_len; i++)
    {
        _stream->write(buf[i]);
    }
}




// todo is naam index nodig?
// is data length niet genoeg
void JohnsSpecialEasyTransfer::update()
{
    // deze methode leest de bytes 1 voor 1 uit de serial buffer.
    while (_stream->available() > 3)   // er is een rede voor de >3 want de andere lib deed het.
    {
        if (transfer_phase ==  READING_HEADER1)
        {
            if(_stream->read() == HEADER_1)
            {
                // als header byte gevonden next phase
                println_string_debug(String("header1"));
                transfer_phase = READING_HEADER2;
            }
            else
            {
                // als header bytes niet gevonden trash buffer todat je 'm wel vind
                transfer_phase = TRANSFER_FAILED;
                debug.trashed_bytes++;
            }
        }
        else if (transfer_phase ==  READING_HEADER2)
        {
            if(_stream->read() == HEADER_2)
            {
                // als header byte gevonden next phase
                println_string_debug(String("header2"));
                transfer_phase = READING_LEN;
            }
            else
            {
                // als header bytes niet gevonden trash buffer todat je 'm wel vind
                transfer_phase = TRANSFER_FAILED;
                debug.trashed_bytes + 2;
            }
        }
        else if (transfer_phase ==  READING_LEN)
        {
            // neemt de byte als len en gaat naar de volgende phase
            println_string_debug(String("len"));
            recieved.data_len = _stream->read();
            transfer_phase =  READING_TYPE;
        }
        else if (recieved.data_idx < recieved.data_len && transfer_phase > READING_LEN)
        {
            if (transfer_phase ==  READING_TYPE)
            {
                // pakt de eerstvolgede byte als het type.
                // als de byte niet van een type is word de transfer als failed verklaard;
                // als de byte valide is word de size ervan gepakt.
                println_string_debug(String("type"));
                char type = (char)_stream->read();
                if (type == type_chars._uint8 || type == type_chars._int || type == type_chars._bool)
                {
                    recieved.type_char = type;
                    if(type == type_chars._int)
                    {
                        recieved.type_len = SIZE_INT;
                    }
                    else if(type == type_chars._uint8)
                    {
                        recieved.type_len = SIZE_UINT8_T;
                    }
                    else if(type == type_chars._bool)
                    {
                        recieved.type_len = SIZE_BOOL;
                    }
                    recieved.name_len = recieved.data_len - recieved.type_len - TYPE_MARKER_SIZE;
                    transfer_phase = READING_VAL;
                    recieved.data_idx++;
                }
                else
                {
                    transfer_phase = TRANSFER_FAILED;
                    debug.wrong_type++;
                }
            }
            else if (transfer_phase ==  READING_VAL)
            {
                // leest de waarde uit de byte stream
                println_string_debug(String("val"));
                recieved.val[recieved.val_idx] = _stream->available();
                if(recieved.val_idx < recieved.type_len - 1)
                {
                    recieved.val_idx++;
                }
                else
                {
                    recieved.val_idx = 0;
                    recieved.name_idx = 0;
                    transfer_phase = READING_NAME;
                }
                recieved.data_idx++;
            }
            else if (transfer_phase ==  READING_NAME)
            {
                // leest de naam uit de byte stream
                println_string_debug(String("name"));
                recieved.name_buf[recieved.name_idx] = _stream->available();
                recieved.name_idx++;
                recieved.data_idx++;
            }
            else
            {

            }
        }
        else if (transfer_phase ==  TRANSFER_FAILED)
        {
            // als de transfer mislukt
            println_string_debug(String("failed"));
            debug.failed_transfers++;
            recieved.data_idx = 0;
            transfer_phase = READING_HEADER1;
        }
        else
        {
            if (transfer_phase == READING_NAME)
            {
                println_string_debug(String("success"));

                // sucsess update hashmaps
                // hier word de data naar het juiste type gecast
                String name = "";
                for (int i=0; i < recieved.name_idx; i++)
                {
                    name += recieved.name_buf[i];
                }

                if(recieved.type_char == type_chars._int)
                {
                    int idx = map_bool.getIndexOf(str_2_char(name));
                    if(idx < 255)
                    {
                        println_int_debug("idx", idx);
                        // plakt de twee byte aan elkaar zodat je een 16 bit int krijgt
                        int val = recieved.val[0] | (int)recieved.val[0] << 8;
                        map_bool[idx](str_2_char(name), val);
                        println_string_debug(String("updated"));
                    }
                    else
                    {
                        println_string_debug(String("no update"));
                    }
                }
                else if(recieved.type_char == type_chars._uint8)
                {
                    int idx = map_uint8_t.getIndexOf(str_2_char(name));
                    if(idx < 255)
                    {
                        println_int_debug("idx", idx);
                        map_bool[idx](str_2_char(name), recieved.val[0]);
                        println_string_debug(String("updated"));
                    }
                    else
                    {
                        println_string_debug(String("no update"));
                    }
                }
                else if(recieved.type_char == type_chars._bool)
                {
                    int idx = map_bool.getIndexOf(str_2_char(name));
                    if(idx < 255)
                    {
                        println_int_debug("idx", idx);
                        bool val = (bool)recieved.val[0];
                        map_bool[idx](str_2_char(name), val);
                        println_string_debug(String("updated"));
                    }
                    else
                    {
                        println_string_debug(String("no update"));
                    }
                }
                transfer_phase = READING_HEADER1;
            }
            else
            {
				println_string_debug(String("failed: at end of msg before name is read"));
				transfer_phase = TRANSFER_FAILED;
            }
        }
    }
}

/*
 * debug print functies.
 */
void JohnsSpecialEasyTransfer::print_debug()
{
    println_int_debug("phase", (uint8_t)transfer_phase);
    println_int_debug("failed transfers", debug.failed_transfers);
    println_int_debug("trashed bytes", debug.trashed_bytes);
    println_int_debug("wrong_type", debug.wrong_type);
}

void JohnsSpecialEasyTransfer::println_string_debug(String str)
{
    if (debug_enabled)
    {
        str += "\n";
        uint16_t b_length = str.length() +1;
        char buf[b_length];
        str.toCharArray(buf, b_length);
        print_debug_buffer(buf);
    }


}

void JohnsSpecialEasyTransfer::println_int_debug(String name, int val)
{
    if (debug_enabled)
    {
        int b_size = name.length() + 10;
        char buffer[b_size];
        name += ":%d\n";
        sprintf(buffer, str_2_char(name), val);
        print_debug_buffer(buffer);

    }
}
void JohnsSpecialEasyTransfer::print_debug_buffer(char buffer[])
{
    if (debug_enabled)
    {
        int i = 0;
        char c = buffer[4];
        while(buffer[i] > 0)
        {
            debug_stream->write(buffer[i]);
            i++;
        }
    }
}

/*
 * convert aruduino string naar char*
 * ik weet niet zeker of dit segfaults gaat veroorzaken
 */
char* JohnsSpecialEasyTransfer::str_2_char(String str)
{
    if(str.length()!=0)
    {
        char *p = const_cast<char*>(str.c_str());
        return p;
    }
    return NULL;
}



