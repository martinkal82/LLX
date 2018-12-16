#ifndef __SEQUENTIAL_HPP__
#define __SEQUENTIAL_HPP__


// EBNF:
// =================================================================================================
// end                    = ? virtual end token (not part of the character set) ? ;
// non-zero-decimal-digit = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
// decimal-digit          = "0" | non-zero-decimal-digit ;
// whitespace             = " " | "\t" | "\n" ;
// decimal-whole-number   =  ["-"], ( "0" | non-zero-decimal-digit, {decimal-digit} ) ;
// separator              = { whitespace }- ;
// data-entry             = ( decimal-whole-number, ( ( separator, data-entry ) | end ) ) ) | end;
// data                   = { whitespace }, data-entry ;
// =================================================================================================


#include "LL1.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <deque>


namespace sequential
{
    constexpr auto is_separator        = LL1::is_space;
    constexpr auto is_sign             = LL1::is('-');
    constexpr auto is_number           = LL1::is_digit || is_sign;
    constexpr auto is_non_zero_number  = is_number && !LL1::is_zero; 
                                       
    constexpr auto read_sign           = LL1::next_if(is_sign, LL1::as(-1)) || 1;
    constexpr auto read_digit          = LL1::next(LL1::as_digit);
    constexpr auto read_digit_sequence = LL1::next_while(LL1::is_digit, LL1::as_digits);

    constexpr auto ignore_whitespace   = LL1::next_while(LL1::is_space);

    auto read_decimal_whole_number(std::istream& ins, LL1::code_position& pos)
    {    
        if (LL1::is_zero(ins))
            return read_digit(ins, pos);
        else if (is_non_zero_number(ins))
            return read_digit_sequence(ins, pos, read_sign(ins, pos) * read_digit(ins, pos));
    }

    std::vector<int> read_data_entry(std::istream& ins, LL1::code_position& pos)
    {
        std::vector<int> temperatures;

        if (is_number(ins))
        {
            temperatures.push_back(read_decimal_whole_number(ins, pos));
        
            if (is_separator(ins))
            {
                LL1::next(ins, pos);
                ignore_whitespace(ins, pos);
                const auto further_temperatures = read_data_entry(ins, pos);
                
                temperatures.insert(
                    std::end(temperatures),
                    std::begin(further_temperatures), std::end(further_temperatures)
                );
                
                return temperatures;
            }
            else if(LL1::is_end(ins))
            {
                return temperatures;
            }
        }
        else if (LL1::is_end(ins))
        {
            return temperatures;
        }
        else
        {
            throw LL1::unexpected_input{};
        }
    }

    auto read_data(std::istream& ins, LL1::code_position& pos)
    {
        ignore_whitespace(ins, pos);

        return read_data_entry(ins, pos);
    }

}

#endif /*__SEQUENTIAL_HPP__*/