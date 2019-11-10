/*
 * liquidsfz - sfz sampler
 *
 * Copyright (C) 2019  Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "utils.hh"

#include <string>

namespace LiquidSFZInternal
{

void log_error (const char *fmt, ...) LIQUIDSFZ_PRINTF (1, 2);
void log_warning (const char *fmt, ...) LIQUIDSFZ_PRINTF (1, 2);
void log_info (const char *fmt, ...) LIQUIDSFZ_PRINTF (1, 2);
void log_debug (const char *fmt, ...) LIQUIDSFZ_PRINTF (1, 2);

std::string string_printf (const char *fmt, ...) LIQUIDSFZ_PRINTF (1, 2);

}