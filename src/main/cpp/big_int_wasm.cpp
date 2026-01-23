#include "cowel/big_int.hpp"
#include "cowel/settings.hpp"

#ifdef COWEL_EMSCRIPTEN
extern "C" {

COWEL_EXPORT
constinit cowel::Int128 cowel_big_int_small_result;

COWEL_EXPORT
constinit cowel_big_int_handle cowel_big_int_big_result;

COWEL_EXPORT
constinit cowel_big_int_div_result_t cowel_big_int_div_result {};
}
#endif
