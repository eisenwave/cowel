#ifndef MMML_UNICODE_HPP
#define MMML_UNICODE_HPP

#include "ulight/impl/unicode.hpp"

namespace mmml::utf8 {

using ulight::utf8::Code_Point_And_Length;
using ulight::utf8::Code_Point_Iterator;
using ulight::utf8::Code_Point_Iterator_Sentinel;
using ulight::utf8::Code_Point_View;
using ulight::utf8::code_points_unchecked;
using ulight::utf8::Code_Units_And_Length;
using ulight::utf8::decode;
using ulight::utf8::decode_and_length;
using ulight::utf8::decode_and_length_or_throw;
using ulight::utf8::decode_and_length_unchecked;
using ulight::utf8::decode_unchecked;
using ulight::utf8::encode8_unchecked;
using ulight::utf8::Error_Code;
using ulight::utf8::error_code_message;
using ulight::utf8::is_valid;
using ulight::utf8::sequence_length;
using ulight::utf8::Unicode_Error;

} // namespace mmml::utf8

#endif
