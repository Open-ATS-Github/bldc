/*
    Copyright 2022 Joel Svensson        svenssonjoel@yahoo.se
    Copyright 2022 Benjamin Vedder

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "extensions/array_extensions.h"

#include "extensions.h"
#include "symrepr.h"
#include "lbm_memory.h"

#include <math.h>

static lbm_uint little_endian = 0;
static lbm_uint big_endian = 0;

static lbm_value array_extension_unsafe_free_array(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_append_i8(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_append_i16(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_append_i32(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_append_u8(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_append_u16(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_append_u32(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_append_f32(lbm_value *args, lbm_uint argn);

static lbm_value array_extension_buffer_get_i8(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_get_i16(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_get_i32(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_get_u8(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_get_u16(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_get_u32(lbm_value *args, lbm_uint argn);
static lbm_value array_extension_buffer_get_f32(lbm_value *args, lbm_uint argn);

static lbm_value array_extension_buffer_length(lbm_value *args, lbm_uint argn);

bool lbm_array_extensions_init(void) {

  if (!lbm_get_symbol_by_name("little-endian", &little_endian)) {
    if (!lbm_add_symbol_const("little-endian", &little_endian)) {
      return false;
    }
  }
  if (!lbm_get_symbol_by_name("big-endian", &big_endian)) {
    if (!lbm_add_symbol_const("big-endian", &big_endian)) {
      return false;
    }
  }
  bool res = true;
  res = res && lbm_add_extension("free", array_extension_unsafe_free_array);
  res = res && lbm_add_extension("bufset-i8", array_extension_buffer_append_i8);
  res = res && lbm_add_extension("bufset-i16", array_extension_buffer_append_i16);
  res = res && lbm_add_extension("bufset-i32", array_extension_buffer_append_i32);
  res = res && lbm_add_extension("bufset-u8", array_extension_buffer_append_u8);
  res = res && lbm_add_extension("bufset-u16", array_extension_buffer_append_u16);
  res = res && lbm_add_extension("bufset-u32", array_extension_buffer_append_u32);
  res = res && lbm_add_extension("bufset-f32", array_extension_buffer_append_f32);

  res = res && lbm_add_extension("bufget-i8", array_extension_buffer_get_i8);
  res = res && lbm_add_extension("bufget-i16", array_extension_buffer_get_i16);
  res = res && lbm_add_extension("bufget-i32", array_extension_buffer_get_i32);
  res = res && lbm_add_extension("bufget-u8", array_extension_buffer_get_u8);
  res = res && lbm_add_extension("bufget-u16", array_extension_buffer_get_u16);
  res = res && lbm_add_extension("bufget-u32", array_extension_buffer_get_u32);
  res = res && lbm_add_extension("bufget-f32", array_extension_buffer_get_f32);

  res = res && lbm_add_extension("buflen",  array_extension_buffer_length);
  return res;
}

lbm_value array_extension_unsafe_free_array(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);
  if (argn != 1 ||
      lbm_type_of(args[0]) != LBM_TYPE_ARRAY) {
    return res;
  }
  lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
  if (lbm_memory_ptr_inside(array->data)) {
    lbm_memory_free((lbm_uint *)array->data);
    lbm_uint ptr = lbm_dec_ptr(args[0]);
    lbm_value cons_ptr = lbm_enc_cons_ptr(ptr);
    lbm_set_car(cons_ptr,lbm_enc_sym(SYM_NIL));
    lbm_set_cdr(cons_ptr,lbm_enc_sym(SYM_NIL));
    res = lbm_enc_sym(SYM_TRUE);
  }
  lbm_memory_free((lbm_uint *)array);
  return res;
}

lbm_value array_extension_buffer_append_i8(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);

  if (argn == 3) {
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1]) ||
       !lbm_is_number(args[2])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }
    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_int value = lbm_dec_as_i32(args[2]);

    if (index >= array->size) {
      return res;
    }

    uint8_t *data = (uint8_t*)array->data;
    data[index] = (uint8_t) value;
    res = lbm_enc_sym(SYM_TRUE);
  }
  return res;
}

lbm_value array_extension_buffer_append_i16(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 4:
    if (lbm_type_of(args[3]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[3]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 3:
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1]) ||
       !lbm_is_number(args[2])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }
    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_int value = lbm_dec_as_i32(args[2]);

    if (index+1 >= array->size) {
      return res;
    }

    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      data[index+1]  = (uint8_t)value;
      data[index]    = (uint8_t)(value >> 8);
    } else {
      data[index]    = (uint8_t)value;
      data[index +1] = (uint8_t)(value >> 8);
    }
    res = lbm_enc_sym(SYM_TRUE);
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_append_i32(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 4:
    if (lbm_type_of(args[3]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[3]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 3:
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1]) ||
       !lbm_is_number(args[2])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }
    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_int value = lbm_dec_as_i32(args[2]);

    if (index+3 >= array->size) {
      return res;
    }

    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      data[index+3]  = (uint8_t) value;
      data[index+2]  = (uint8_t) (value >> 8);
      data[index+1]  = (uint8_t) (value >> 16);
      data[index]    = (uint8_t) (value >> 24);
    } else {
      data[index]    = (uint8_t) value;
      data[index+1]  = (uint8_t) (value >> 8);
      data[index+2]  = (uint8_t) (value >> 16);
      data[index+3]  = (uint8_t) (value >> 24);
    }
    res = lbm_enc_sym(SYM_TRUE);
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_append_u8(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);

  switch(argn) {

  case 3:
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1]) ||
       !lbm_is_number(args[2])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }
    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_uint value = lbm_dec_as_u32(args[2]);

    if (index >= array->size) {
      return res;
    }

    uint8_t *data = (uint8_t*)array->data;
    data[index] = (uint8_t)value;
    res = lbm_enc_sym(SYM_TRUE);
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_append_u16(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 4:
    if (lbm_type_of(args[3]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[3]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 3:
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1]) ||
       !lbm_is_number(args[2])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }
    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_uint value = lbm_dec_as_u32(args[2]);

    if (index+1 >= array->size) {
      return res;
    }

    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      data[index+1]  = (uint8_t)value;
      data[index]    = (uint8_t)(value >> 8);
    } else {
      data[index]    = (uint8_t)value;
      data[index +1] = (uint8_t)(value >> 8);
    }
    res = lbm_enc_sym(SYM_TRUE);
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_append_u32(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 4:
    if (lbm_type_of(args[3]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[3]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 3:
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1]) ||
       !lbm_is_number(args[2])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }
    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_uint value = lbm_dec_as_u32(args[2]);

    if (index+3 >= array->size) {
      return res;
    }

    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      data[index+3]  = (uint8_t)value;
      data[index+2]  = (uint8_t)(value >> 8);
      data[index+1]  = (uint8_t)(value >> 16);
      data[index]    = (uint8_t)(value >> 24);
    } else {
      data[index]    = (uint8_t)value;
      data[index+1]  = (uint8_t)(value >> 8);
      data[index+2]  = (uint8_t)(value >> 16);
      data[index+3]  = (uint8_t)(value >> 24);
    }
    res = lbm_enc_sym(SYM_TRUE);
    break;
  default:
    break;
  }
  return res;
}

static lbm_uint float_to_u(float number) {
  // Set subnormal numbers to 0 as they are not handled properly
  // using this method.
  if (fabsf(number) < 1.5e-38) {
    number = 0.0;
  }

  int e = 0;
  float sig = frexpf(number, &e);
  float sig_abs = fabsf(sig);
  uint32_t sig_i = 0;

  if (sig_abs >= 0.5) {
    sig_i = (uint32_t)((sig_abs - 0.5f) * 2.0f * 8388608.0f);
    e += 126;
  }

  uint32_t res = ((e & 0xFF) << 23) | (sig_i & 0x7FFFFF);
  if (sig < 0) {
    res |= 1U << 31;
  }

  return res;
}

static lbm_float u_to_float(uint32_t v) {

  int e = (v >> 23) & 0xFF;
  uint32_t sig_i = v & 0x7FFFFF;
  bool neg = v & (1U << 31);

  float sig = 0.0;
  if (e != 0 || sig_i != 0) {
    sig = (float)sig_i / (8388608.0 * 2.0) + 0.5;
    e -= 126;
  }

  if (neg) {
    sig = -sig;
  }

  return ldexpf(sig, e);
}

lbm_value array_extension_buffer_append_f32(lbm_value *args, lbm_uint argn) {

  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 4:
    if (lbm_type_of(args[3]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[3]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 3:
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1]) ||
       !lbm_is_number(args[2])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }
    float f_value = (float)lbm_dec_as_float(args[2]);
    lbm_value value = float_to_u(f_value);
    lbm_uint index = lbm_dec_as_u32(args[1]);
    
    if (index+3 >= array->size) {
      return res;
    }

    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      data[index+3]  = (uint8_t)value;
      data[index+2]  = (uint8_t)(value >> 8);
      data[index+1]  = (uint8_t)(value >> 16);
      data[index]    = (uint8_t)(value >> 24);
    } else {
      data[index]    = (uint8_t)value;
      data[index+1]  = (uint8_t)(value >> 8);
      data[index+2]  = (uint8_t)(value >> 16);
      data[index+3]  = (uint8_t)(value >> 24);
    }
    res = lbm_enc_sym(SYM_TRUE);
    break;
  default:
    break;
  }
  return res;
}

/* (buffer-get-i8 buffer index) */
/* (buffer-get-i16 buffer index little-endian) */

lbm_value array_extension_buffer_get_i8(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);

  if (argn == 2) {
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }

    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_uint value = 0;

    if (index >= array->size) {
      return res;
    }
    uint8_t *data = (uint8_t*)array->data;

    value = data[index];
    res = lbm_enc_i((int8_t)value);
  }
  return res;
}


lbm_value array_extension_buffer_get_i16(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 3:
    if (lbm_type_of(args[2]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[2]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 2:
    if (lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
        !lbm_is_number(args[1])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }

    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_uint value = 0;

    if (index+1 >= array->size) {
      return res;
    }
    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      value =
        (lbm_uint) data[index+1] |
        (lbm_uint) data[index] << 8;
    } else {
      value =
        (lbm_uint) data[index] |
        (lbm_uint) data[index+1] << 8;
    }

    res = lbm_enc_i((int16_t)value);
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_get_i32(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 3:
    if (lbm_type_of(args[2]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[2]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 2:
    if (lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
        !lbm_is_number(args[1])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }

    lbm_uint index = lbm_dec_as_u32(args[1]);
    uint32_t value = 0;

    if (index+3 >= array->size) {
      return res;
    }
    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      value =
        (uint32_t) data[index+3] |
        (uint32_t) data[index+2] << 8 |
        (uint32_t) data[index+1] << 16 |
        (uint32_t) data[index] << 24;
    } else {
      value =
        (uint32_t) data[index] |
        (uint32_t) data[index+1] << 8 |
        (uint32_t) data[index+2] << 16 |
        (uint32_t) data[index+3] << 24;
    }

    res = lbm_enc_i32((int32_t)value);
    break;
  default:
    break;
  }
  return res;
}


lbm_value array_extension_buffer_get_u8(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);

  if (argn == 2) {
    if(lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
       !lbm_is_number(args[1])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }

    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_int value = 0;

    if (index >= array->size) {
      return res;
    }
    uint8_t *data = (uint8_t*)array->data;

    value = (lbm_int)data[index];
    res = lbm_enc_i(value);
  }
  return res;
}


lbm_value array_extension_buffer_get_u16(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 3:
    if (lbm_type_of(args[2]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[2]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 2:
    if (lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
        !lbm_is_number(args[1])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }

    lbm_uint index = lbm_dec_as_u32(args[1]);
    lbm_int value = 0;

    if (index+1 >= array->size) {
      return res;
    }
    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      value = (lbm_int)(
        (lbm_uint) data[index+1] |
        (lbm_uint) data[index] << 8);
    } else {
      value = (lbm_int)(
        (lbm_uint) data[index] |
        (lbm_uint) data[index+1] << 8);
    }

    res = lbm_enc_i(value);
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_get_u32(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 3:
    if (lbm_type_of(args[2]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[2]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 2:
    if (lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
        !lbm_is_number(args[1])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }

    lbm_uint index = lbm_dec_as_u32(args[1]);
    uint32_t value = 0;

    if (index+3 >= array->size) {
      return res;
    }
    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      value =
        (uint32_t) data[index+3] |
        (uint32_t) data[index+2] << 8 |
        (uint32_t) data[index+1] << 16 |
        (uint32_t) data[index] << 24;
    } else {
      value =
        (uint32_t) data[index] |
        (uint32_t) data[index+1] << 8 |
        (uint32_t) data[index+2] << 16 |
        (uint32_t) data[index+3] << 24;
    }

    res = lbm_enc_u32(value);
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_get_f32(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);
  bool be = true;

  switch(argn) {

  case 3:
    if (lbm_type_of(args[2]) == LBM_TYPE_SYMBOL &&
        lbm_dec_sym(args[2]) == little_endian) {
      be = false;
    }
    /* fall through */
  case 2:
    if (lbm_type_of(args[0]) != LBM_TYPE_ARRAY ||
        !lbm_is_number(args[1])) {
      return res;
    }
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    if (array->elt_type != LBM_TYPE_BYTE) {
      return res;
    }

    uint32_t index = (uint32_t)lbm_dec_as_u32(args[1]);
    uint32_t value = 0;

    if (index+3 >= array->size) {
      return res;
    }
    uint8_t *data = (uint8_t*)array->data;

    if (be) {
      value =
        (uint32_t) data[index+3] |
        (uint32_t) data[index+2] << 8 |
        (uint32_t) data[index+1] << 16 |
        (uint32_t) data[index] << 24;
    } else {
      value =
        (uint32_t) data[index] |
        (uint32_t) data[index+1] << 8 |
        (uint32_t) data[index+2] << 16 |
        (uint32_t) data[index+3] << 24;
    }

    res = lbm_enc_float((float)u_to_float(value));
    break;
  default:
    break;
  }
  return res;
}

lbm_value array_extension_buffer_length(lbm_value *args, lbm_uint argn) {
  lbm_value res = lbm_enc_sym(SYM_EERROR);
  if (argn == 1 &&
      lbm_type_of(args[0]) == LBM_TYPE_ARRAY) {
    printf("trying\n");
    lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(args[0]);
    switch(array->elt_type) {
    case LBM_TYPE_CHAR: /* Same as byte */
      res = lbm_enc_i((lbm_int)array->size);
      break;
    case LBM_TYPE_I: /* fall through */
    case LBM_TYPE_U:
    case LBM_TYPE_I32:
    case LBM_TYPE_U32:
    case LBM_TYPE_FLOAT:
      res = lbm_enc_i((lbm_int)array->size * 4);
      break;
    }
  }
  return res;
}
