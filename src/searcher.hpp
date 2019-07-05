#pragma once

#include <mutex>
#include <atomic>

#include "boost/regex.hpp"
namespace rx = boost;

#include "utils.hpp"
#include "searchoptions.hpp"

struct Stats {
    std::atomic_size_t matches = {0};
    std::atomic_size_t filesSearched = {0};
    std::atomic_size_t filesMatched = {0};
    std::atomic_size_t bytesRead = {0};

    std::atomic_llong t_recurse = {0}; // time to recurse directory
    std::atomic_llong t_read = {0};    // time to read files
    std::atomic_llong t_search = {0};  // time to search in files
    std::atomic_llong t_collect = {0}; // time to prepare matches for printing
    std::atomic_llong t_print = {0};   // time to print
};

struct Searcher {
    std::mutex m;
    std::string term;
    rx::regex regex;
    SearchOptions opts;
    Stats stats;

    using Iter = std::string_view::const_iterator;
    using Match = std::pair<Iter, Iter>;
    using Print = std::function<void()>;

    Searcher( const SearchOptions& opts ) : opts( opts ) {
        term = opts.term;

        // use regex only for complex searches
        if( opts.isRegex ) {
            rx::regex::flag_type flags = rx::regex::normal;

            if( opts.ignoreCase ) { flags ^= rx::regex::icase; }

            try {
                regex.assign( term, flags );
            } catch( const rx::regex_error& e ) {
                LOG( "Invalid regex: " << e.what() );
                exit( EXIT_FAILURE );
            }
        }
    }

    ~Searcher() {}

    void search( const sys_string& path );

    //! search with strcasestr
    std::vector<Match> caseInsensitiveSearch( const std::string_view& content );
    //! search with strstr or string_view::find
    std::vector<Match> caseSensitiveSearch( const std::string_view& content );
    //! search with boost::regex
    std::vector<Match> regexSearch( const std::string_view& content );
    //! collect what is printed
    std::vector<Print> collectPrints( const sys_string& path, const std::vector<Match>& matches, const std::string_view& content );
    //! call print functions locked
    void printPrints( const std::vector<Print>& prints );
};
