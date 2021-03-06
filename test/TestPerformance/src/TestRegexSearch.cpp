#include <regex>


#include "boost/test/unit_test.hpp"
#include "boost/xpressive/xpressive.hpp"
#include "boost/regex.hpp"

#if !BOOST_OS_WINDOWS
#include <regex.h>
#endif

#include "PerformanceUtils.hpp"
#include "utils.hpp"
#include "types.hpp"
#include "licence.hpp"

#if !BOOST_OS_WINDOWS
// http://pubs.opengroup.org/onlinepubs/9699919799/functions/regcomp.html
size_t posixRegex( const std::string& content, const std::string& term ) {

    regex_t re;
    regmatch_t pm;
    regcomp( &re, term.c_str(), 0 );

    const char* start = content.data();

    size_t count = 0;
    int error = regexec( &re, start, 1, &pm, 0 );

    while( error == 0 ) {
        ++count;
        start += pm.rm_eo;
        error = regexec( &re, start, 1, &pm, 0 );
    }

    regfree( &re );

    return count;
}
#endif

size_t boostRegex( const std::string& content, const std::string& term ) {

    boost::regex regex( term );

    size_t count = 0;

    auto begin = boost::cregex_iterator( &content.front(), 1 + &content.back(), regex );
    auto end   = boost::cregex_iterator();

    for( boost::cregex_iterator match = begin; match != end; ++match ) {
        ++count;
    }

    return count;
}

size_t boostXpressive( const std::string& content, const std::string& term ) {

    boost::xpressive::cregex regex = boost::xpressive::cregex::compile( term.cbegin(), term.cend() );

    size_t count = 0;

    auto begin = boost::xpressive::cregex_iterator( &content.front(), 1 + &content.back(), regex );
    auto end   = boost::xpressive::cregex_iterator();

    for( boost::xpressive::cregex_iterator match = begin; match != end; ++match ) {
        ++count;
    }

    return count;
}

size_t stdRegex( const std::string& content, const std::string& term ) {

    std::regex regex( term );

    size_t count = 0;

    auto begin = std::cregex_iterator( &content.front(), 1 + &content.back(), regex );
    auto end   = std::cregex_iterator();

    for( std::cregex_iterator match = begin; match != end; ++match ) {
        ++count;
    }

    return count;
}

BOOST_AUTO_TEST_CASE( Test_regex ) {
    printf( "Regex search\n" );

    size_t count = 0;
    std::string text( ( const char* )licence, sizeof( licence ) );
    std::string term = "[Ll]icense";

    auto check = [&] {
        // grep -Po '[Ll]icense' < LICENSE | wc -l
        BOOST_CHECK_EQUAL( count, 116 );
    };

    std::vector<Result> results = {
#if !BOOST_OS_WINDOWS
        timed1000( "posix", [&text, &term, &count] {
            count = posixRegex( text, term );
        }, check ),
#endif

        timed1000( "boost::regex", [&text, &term, &count] {
            count = boostRegex( text, term );
        }, check ),

        timed1000( "boost::xpressive", [&text, &term, &count] {
            count = boostXpressive( text, term );
        }, check ),

        timed1000( "std::regex", [&text, &term, &count] {
            count = stdRegex( text, term );
        }, check ),
    };

    printSorted( results );
    printf( "\n" );
}
