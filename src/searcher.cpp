#include <iterator>

#include "searcher.hpp"

std::vector<Searcher::Match> Searcher::caseInsensitiveSearch( const std::string_view& content ) {
    std::vector<Match> matches;

    Iter pos = content.cbegin();
    Iter end = content.cend();
    const char* start = content.data();
    const char* ptr = start;

    while( pos != end ) {
        ptr = strcasestr( ptr, term.data() );

        if( ptr ) {
            Iter from = pos + ( ptr - start );
            Iter to = from + term.size();
            matches.emplace_back( from, to );
            ptr += term.size();
        } else {
            pos = end;
        }

    }

    return matches;
}

#if BOOST_OS_WINDOWS
std::vector<Searcher::Match> Searcher::caseSensitiveSearch( const std::string_view& content ) {
    std::vector<Match> matches;

    Iter pos = content.cbegin();
    Iter end = content.cend();
    const char* start = content.data();
    const char* ptr = start;

    while( pos != end ) {
        ptr = strstr( ptr, term.data() );

        if( ptr ) {
            Iter from = pos + ( ptr - start );
            Iter to = from + term.size();
            matches.emplace_back( from, to );
            ptr += term.size();
        } else {
            pos = end;
        }

    }

    return matches;
}
#else
std::vector<Searcher::Match> Searcher::caseSensitiveSearch( const std::string_view& content ) {
    std::vector<Match> matches;

    Iter pos = content.cbegin();
    Iter end = content.cend();
    size_t ptr = 0;

    while( pos != end ) {
        ptr = content.find( term, ptr );

        if( ptr != std::string_view::npos ) {
            Iter from = pos + ptr;
            Iter to = from + term.size();
            matches.emplace_back( from, to );
            ptr += term.size();
        } else {
            pos = end;
        }

    }

    return matches;
}
#endif

std::vector<Searcher::Match> Searcher::regexSearch( const std::string_view& content ) {
    std::vector<Match> matches;

    auto begin = rx::cregex_iterator( &content.front(), 1 + &content.back(), regex );
    auto end   = rx::cregex_iterator();

    for( rx::cregex_iterator match = begin; match != end; ++match ) {
        Iter from = content.cbegin() + match->position();
        Iter to = from + match->length();
        matches.emplace_back( from, to );
    }

    return matches;
}

std::vector<Searcher::Print> Searcher::collectPrints( const sys_string& path, const std::vector<Searcher::Match>& matches, const std::string_view& content ) {
    std::vector<std::function<void()>> prints;
    prints.reserve( 3 * matches.size() );

    // don't pipe colors
    Color cred   = opts.colorized ? Color::Red   : Color::Neutral;
    Color cblue  = opts.colorized ? Color::Blue  : Color::Neutral;
    Color cgreen = opts.colorized ? Color::Green : Color::Neutral;

    // print file path
#ifdef _WIN32
    prints.emplace_back( utils::printFunc( cgreen, std::string( path.cbegin(), path.cend() ) ) );
#else
    prints.emplace_back( utils::printFunc( cgreen, path ) );
#endif

    // parse file for newlines until last match
    long long stop = matches.back().second - content.cbegin();
    utils::Lines lines = utils::parseContent( content.data(), content.size(), stop );

    std::vector<Match>::const_iterator match = matches.cbegin();
    size_t size = lines.size();

    for( size_t i = 0; i < size; ++i ) {
        Iter from = lines[i].cbegin();
        Iter to   = lines[i].cend();
        bool printedLine = false;

        // find all matches in this line
        while( from <= match->first && match->first < to ) {

            // print line information in blue once per line
            if( !printedLine ) {
                printedLine = true;
                std::string number = utils::format( "\nL%4i : ", i + 1 );
                prints.emplace_back( utils::printFunc( cblue, number ) );
            }

            // print code in neutral
            prints.emplace_back( utils::printFunc( Color::Neutral, std::string( from, match->first ) ) );

            // print match in red
            prints.emplace_back( utils::printFunc( cred, std::string( match->first, match->second ) ) );

            // set from to end of match
            from = match->second;

            // if there are no more matches in this file, print rest of line in neutral
            // and exit search for this file
            if( std::next( match ) == matches.cend() ) {
                prints.emplace_back( utils::printFunc( Color::Neutral, std::string( from, to ) ) );
                goto end;
            }

            ++match;

            // if next match is within this line, print code in neutral until next match
            if( from <= match->first && match->first < to ) {
                prints.emplace_back( utils::printFunc( Color::Neutral, std::string( from, match->first ) ) );
                from = match->first;
            }
            // else print code in neutral until end and break to next line
            else {
                prints.emplace_back( utils::printFunc( Color::Neutral, std::string( from, to ) ) );
                break;
            }
        }
    }

end:
    // print newlines
    prints.emplace_back( utils::printFunc( Color::Neutral, "\n\n" ) );

    return prints;
}

void Searcher::printPrints( const std::vector<Searcher::Print>& prints ) {
    std::unique_lock<std::mutex> lock( m );

    for( const std::function<void()>& func : prints ) { func(); }
}

void Searcher::search( const sys_string& path ) {

    STOPWATCH
    START

#ifndef _WIN32
    utils::FileView view = utils::fromFileC( path );
#else
    utils::FileView view = utils::fromWinAPI( path );
#endif

    stats.bytesRead += view.size;
    STOP( stats.t_read )

    if( !view.size ) { return; }

    // collect matches
    START
    const std::string_view& content = view.content;
    std::vector<Match> matches;

    if( !opts.isRegex ) {
        if( opts.ignoreCase ) {
            matches = caseInsensitiveSearch( content );
        } else {
            matches = caseSensitiveSearch( content );
        }
    } else {
        matches = regexSearch( content );
    }

    STOP( stats.t_search );

    // handle matches
    if( !matches.empty() ) {
        stats.filesMatched++;
        stats.matches += matches.size();

        START
        std::vector<std::function<void()>> prints = collectPrints( path, matches, content );
        STOP( stats.t_collect );

        if( !opts.quiet ) {
            START
            printPrints( prints );
            STOP( stats.t_print );
        }
    }
}
