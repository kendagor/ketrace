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

#include "trace.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <sys/prctl.h>
#include <unordered_map>
#include <unordered_set>

using namespace ketrace;

static std::unordered_map<std::string, std::unordered_set<std::string>> thread_callflow;
static std::unordered_map<std::string, std::queue<std::string>>         thread_trace_fifo;

std::mutex mutex_state_initialization;

thread_local std::stack<std::string>          code_coverage_stack;
thread_local std::string                      thread_context_id;
thread_local std::unordered_set<std::string>* local_callflow       = nullptr;
thread_local std::queue<std::string>*         local_trace_fifo     = nullptr;
thread_local error_handler_result_t           error_handler_result = error_handler_result_t::result_continue;

trace_t::trace_t( const char* const function_name, const std::string context_id )
{
    // If the last operation was fatal
    if ( error_handler_result == error_handler_result_t::result_terminate_on_next_trace )
    {
        std::exit(1);
    }

    where = function_name;

    app_assert(
        function_name,
        ( ( context_id.empty() && !thread_context_id.empty() ) || ( !context_id.empty() && thread_context_id.empty() ) ),
        "context_id and thread_context_id cannot both have values or be empty:"
        " context_id='"
            + context_id + "' thread_context_id='" + thread_context_id + "'" );

    if ( thread_context_id.empty() )
    {
        thread_context_id = context_id;
        app_assert( function_name, !thread_context_id.empty(), "thread name should not be empty" );

        init( thread_context_id );
    }

    std::string current_function_name = format_function_name( function_name );

    const std::string prev_function_name = code_coverage_stack.empty() ? "?" : code_coverage_stack.top();
    code_coverage_stack.push( current_function_name );

    std::stringstream ss;

    ss << "(" << prev_function_name << ")->(" << current_function_name << ")";

    app_assert( function_name, local_callflow != nullptr, "local_callflow cannot be nullptr" );
    local_callflow->insert( ss.str() );
}

trace_t::~trace_t()
{
    code_coverage_stack.pop();
}

void trace_t::init( const std::string& context_id )
{
    std::lock_guard<std::mutex> lock_initialize( mutex_state_initialization );

    auto pair_trace_fifo = thread_trace_fifo.emplace( thread_context_id, std::queue<std::string> {} );
    local_trace_fifo     = &pair_trace_fifo.first->second;
    auto pair_callflow   = thread_callflow.emplace( thread_context_id, std::unordered_set<std::string> {} );
    local_callflow       = &pair_callflow.first->second;
}

void trace_t::trace( const std::string& message, const trace_level& level )
{
    std::stringstream msg;
    msg << "[" << thread_context_id << "]";

    switch ( level )
    {
        case trace_level::info:
            msg << str_info;
            break;
        case trace_level::warn:
            msg << str_warn;
            break;
        case trace_level::error:
            msg << str_error;
            break;
    }
    msg << where << ":\n"
        << str_space << message << "\n\n";

    if ( local_trace_fifo == nullptr )
    {
        init( "Main - Uninitialized" );
    }
    if ( local_trace_fifo == nullptr )
    {
        msg << "local_trace_fifo cannot be nullptr";
        std::cerr << msg.str() << std::endl;
    }
    else
    {
        local_trace_fifo->push( msg.str() );
        std::cout << msg.str() << std::endl;
    }

    if ( level == trace_level::error )
    {
        if ( error_handler )
        {
            error_handler_result = error_handler();
        }
        else
        {
            error_handler_result = error_handler_result_t::result_terminate;
        }

        if (error_handler_result == error_handler_result_t::result_terminate)
        {
            std::exit(1);
        }
    }
}

void trace_t::info( const std::string& message )
{
    trace( message, trace_level::info );
}

void trace_t::warn( const std::string& message )
{
    trace( message, trace_level::warn );
}

void trace_t::error( const std::string& message )
{
    trace( message, trace_level::error );
}

void trace_t::app_assert( const std::string& location, bool condition, const std::string& message )
{
    if ( !condition )
    {
        std::stringstream ss;
        ss << "Assert at " << location << "\n"
           << str_space << message;

        error( ss.str() );
    }
}

void trace_t::flush( const std::string& yuml_filename, const std::string& trace_filename )
{
    std::ofstream ofs;
    ofs.open( yuml_filename, std::ios::trunc );
    ofs << "// {type:activity}\n"
           "// {direction:leftToRight}\n"
           "// {generate:true}\n";
    std::unordered_set<std::string> unique_calls;
    for ( const auto& per_thread_callflow : thread_callflow )
    {
        for ( const auto& call_str : per_thread_callflow.second )
        {
            if ( !unique_calls.contains( call_str ) )
            {
                ofs << call_str << "\n";
                unique_calls.insert( call_str );
            }
        }
    }
    ofs.close();

    auto flush_traces = [ & ]( std::queue<std::string>& trace_fifo )
    {
        bool failed_once = false;
        while ( !trace_fifo.empty() )
        {
            try
            {
                ofs << trace_fifo.front() << "\n";
                trace_fifo.pop();
            }
            catch ( ... )
            {
                if ( failed_once )
                {
                    break;
                }
                ofs << std::endl;
                failed_once = true;
            }
        }
    };

    ofs.open( trace_filename, std::ios::trunc );

    for ( auto& thread_trace : thread_trace_fifo )
    {
        flush_traces( thread_trace.second );
    }

    ofs.close();
}

std::string& trace_t::format_function_name( const std::string& pretty_function_name )
{
    thread_local std::unordered_map<std::string, std::string> format_cache;
    if ( !format_cache.contains( pretty_function_name ) )
    {
        std::regex  regex_function_name( R"(^([^\(\[]+).*$)" );
        std::smatch match;
        if ( std::regex_search( pretty_function_name, match, regex_function_name ) )
        {
            std::string match1                   = match[ 1 ];
            format_cache[ pretty_function_name ] = match[ 1 ];
        }
        else
        {
            error( "Unable to handle " + pretty_function_name );
        }
    }
    return format_cache.at( pretty_function_name );
}

void trace_t::set_error_handler( error_handler_t error_function )
{
    error_handler = error_function;
}

error_handler_result_t trace_t::get_error_handler_result()
{
    return error_handler_result;
}

void trace_t::set_error_handler_result( error_handler_result_t error_result )
{
    error_handler_result = error_result;
}
