/**
 * MIT License
 * 
 * Copyright (c) 2022, Elisha Kendagor
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **/

#pragma once

#ifdef _WIN32
#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif
#endif

#include <array>
#include <functional>
#include <string>

namespace ketrace
{
    enum class trace_level
    {
        info,
        warn,
        error,
    };

    enum class error_handler_result_t
    {
        result_continue,
        result_terminate,
        result_terminate_on_next_trace
    };

    class trace_t
    {
        using error_handler_t      = std::function<error_handler_result_t()>;

    private:
        const char* str_info  = "info:  ";
        const char* str_warn  = "warn:  ";
        const char* str_error = "error: ";
        const char* str_space = "       ";

        const char* where;

        error_handler_t error_handler;

        void trace( const std::string& message, const trace_level& level );
        void init( const std::string& context_id );

    public:
        trace_t( const char* function_name, const std::string context_id = std::string {} );
        trace_t( trace_t& rhs )        = delete;
        trace_t( trace_t&& rhs )       = delete;
        trace_t( const trace_t& rhs )  = delete;
        trace_t( const trace_t&& rhs ) = delete;

        ~trace_t();

        void info( const std::string& message );
        void warn( const std::string& message );
        void error( const std::string& message );
        void app_assert( const std::string& location, bool condition, const std::string& message );

        std::string& format_function_name( const std::string& pretty_function_name );

        void flush( const std::string& yuml_filename, const std::string& trace_filename );

        void set_error_handler( error_handler_t error_function );

        static error_handler_result_t get_error_handler_result();
        static void set_error_handler_result( error_handler_result_t error_result );
    };

#define TRACE                        trace_t trace( __PRETTY_FUNCTION__ )
#define ASSERT( condition, message ) trace.app_assert( __PRETTY_FUNCTION__, condition, message )

}
